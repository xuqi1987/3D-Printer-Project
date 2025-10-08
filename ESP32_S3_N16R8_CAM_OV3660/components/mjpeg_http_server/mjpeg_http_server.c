/**
 * @file mjpeg_http_server.c
 * @brief HTTP MJPEG流媒体服务器实现
 * 
 * 该模块实现了一个高效的HTTP MJPEG流媒体服务器
 * 支持多客户端并发连接，使用multipart/x-mixed-replace格式
 * 
 * @author ESP32 Team
 * @date 2025
 */

#include "mjpeg_http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *TAG = "MJPEG_HTTP";

/* ==================== 内部数据结构 ==================== */

typedef struct {
    int socket_fd;                     /*!< 客户端socket */
    httpd_req_t *req;                  /*!< HTTP请求句柄 */
    bool is_active;                    /*!< 客户端是否活跃 */
    int64_t last_frame_time;           /*!< 上次帧时间 */
    uint32_t frame_count;              /*!< 发送帧数 */
    size_t bytes_sent;                 /*!< 发送字节数 */
    char client_ip[16];                /*!< 客户端IP */
} mjpeg_client_t;

typedef struct {
    httpd_handle_t server;             /*!< HTTP服务器句柄 */
    mjpeg_http_server_config_t config; /*!< 服务器配置 */
    
    mjpeg_client_t *clients;           /*!< 客户端数组 */
    SemaphoreHandle_t clients_mutex;   /*!< 客户端列表互斥锁 */
    
    QueueHandle_t frame_queue;         /*!< 帧队列 */
    TaskHandle_t stream_task;          /*!< 流媒体任务 */
    
    bool is_running;                   /*!< 服务器是否运行 */
    
    // 统计信息
    uint32_t total_clients;            /*!< 总连接数 */
    uint32_t active_clients;           /*!< 活跃连接数 */
    uint64_t frames_sent;              /*!< 发送帧数 */
    uint64_t bytes_sent;               /*!< 发送字节数 */
    float current_fps;                 /*!< 当前FPS */
    int64_t last_fps_time;             /*!< 上次FPS计算时间 */
    uint32_t frames_since_last_update; /*!< 自上次更新以来的帧数 */
} mjpeg_server_t;

/* ==================== 内部函数声明 ==================== */

static esp_err_t stream_handler(httpd_req_t *req);
static void stream_task(void *pvParameters);
static esp_err_t send_frame_to_client(mjpeg_client_t *client, const mjpeg_frame_t *frame);
static void cleanup_inactive_clients(mjpeg_server_t *server);
static void update_fps_stats(mjpeg_server_t *server);

/* ==================== HTTP头部模板 ==================== */
// HTTP MJPEG流使用multipart/x-mixed-replace格式

/* ==================== API实现 ==================== */

mjpeg_http_server_config_t mjpeg_http_server_get_default_config(void)
{
    mjpeg_http_server_config_t config = {
        .port = 8080,
        .max_clients = 4,
        .task_priority = 5,
        .task_stack_size = 8192,
        .core_id = 0,
        .frame_timeout_ms = 5000
    };
    return config;
}

esp_err_t mjpeg_http_server_init(const mjpeg_http_server_config_t *config,
                                 mjpeg_http_server_handle_t *handle)
{
    if (!config || !handle) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 分配服务器结构
    mjpeg_server_t *server = calloc(1, sizeof(mjpeg_server_t));
    if (!server) {
        ESP_LOGE(TAG, "Failed to allocate server memory");
        return ESP_ERR_NO_MEM;
    }
    
    // 复制配置
    memcpy(&server->config, config, sizeof(mjpeg_http_server_config_t));
    
    // 分配客户端数组
    server->clients = calloc(config->max_clients, sizeof(mjpeg_client_t));
    if (!server->clients) {
        ESP_LOGE(TAG, "Failed to allocate clients memory");
        free(server);
        return ESP_ERR_NO_MEM;
    }
    
    // 创建互斥锁
    server->clients_mutex = xSemaphoreCreateMutex();
    if (!server->clients_mutex) {
        ESP_LOGE(TAG, "Failed to create clients mutex");
        free(server->clients);
        free(server);
        return ESP_ERR_NO_MEM;
    }
    
    // 创建帧队列
    server->frame_queue = xQueueCreate(2, sizeof(mjpeg_frame_t));
    if (!server->frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        vSemaphoreDelete(server->clients_mutex);
        free(server->clients);
        free(server);
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化统计
    server->last_fps_time = esp_timer_get_time();
    
    *handle = server;
    ESP_LOGI(TAG, "HTTP MJPEG server initialized (port: %d, max clients: %d)", 
             config->port, config->max_clients);
    
    return ESP_OK;
}

esp_err_t mjpeg_http_server_start(mjpeg_http_server_handle_t handle)
{
    mjpeg_server_t *server = (mjpeg_server_t *)handle;
    if (!server) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (server->is_running) {
        ESP_LOGW(TAG, "Server is already running");
        return ESP_OK;
    }
    
    // 配置HTTP服务器
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = server->config.port;
    httpd_config.max_open_sockets = server->config.max_clients + 2;
    httpd_config.task_priority = server->config.task_priority;
    httpd_config.stack_size = server->config.task_stack_size;
    httpd_config.core_id = server->config.core_id;
    httpd_config.ctrl_port = httpd_config.server_port + 1;
    
    // 启动HTTP服务器
    esp_err_t ret = httpd_start(&server->server, &httpd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 注册流媒体处理程序
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = server
    };
    
    ret = httpd_register_uri_handler(server->server, &stream_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register stream handler: %s", esp_err_to_name(ret));
        httpd_stop(server->server);
        return ret;
    }
    
    // 关键修复：在创建任务之前设置is_running为true，防止竞态条件
    server->is_running = true;
    
    // 创建流媒体任务
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        stream_task,
        "mjpeg_stream",
        server->config.task_stack_size,
        server,
        server->config.task_priority,
        &server->stream_task,
        server->config.core_id
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stream task");
        server->is_running = false;  // 创建失败时重置标志
        httpd_stop(server->server);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "HTTP MJPEG server started on port %d", server->config.port);
    ESP_LOGI(TAG, "Stream URL: http://[device_ip]:%d/stream", server->config.port);
    
    return ESP_OK;
}

esp_err_t mjpeg_http_server_stop(mjpeg_http_server_handle_t handle)
{
    mjpeg_server_t *server = (mjpeg_server_t *)handle;
    if (!server || !server->is_running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    server->is_running = false;
    
    // 停止流媒体任务
    if (server->stream_task) {
        vTaskDelete(server->stream_task);
        server->stream_task = NULL;
    }
    
    // 停止HTTP服务器
    if (server->server) {
        httpd_stop(server->server);
        server->server = NULL;
    }
    
    // 清理客户端连接
    if (xSemaphoreTake(server->clients_mutex, portMAX_DELAY)) {
        for (int i = 0; i < server->config.max_clients; i++) {
            if (server->clients[i].is_active) {
                server->clients[i].is_active = false;
            }
        }
        xSemaphoreGive(server->clients_mutex);
    }
    
    ESP_LOGI(TAG, "HTTP MJPEG server stopped");
    return ESP_OK;
}

esp_err_t mjpeg_http_server_deinit(mjpeg_http_server_handle_t handle)
{
    mjpeg_server_t *server = (mjpeg_server_t *)handle;
    if (!server) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 先停止服务器
    mjpeg_http_server_stop(handle);
    
    // 清理资源
    if (server->frame_queue) {
        vQueueDelete(server->frame_queue);
    }
    
    if (server->clients_mutex) {
        vSemaphoreDelete(server->clients_mutex);
    }
    
    if (server->clients) {
        free(server->clients);
    }
    
    free(server);
    
    ESP_LOGI(TAG, "HTTP MJPEG server deinitialized");
    return ESP_OK;
}

esp_err_t mjpeg_http_server_push_frame(mjpeg_http_server_handle_t handle,
                                       const mjpeg_frame_t *frame)
{
    mjpeg_server_t *server = (mjpeg_server_t *)handle;
    if (!server || !frame || !server->is_running) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 尝试发送到队列，如果满了就丢弃旧帧
    if (xQueueSend(server->frame_queue, frame, 0) != pdTRUE) {
        // 队列满，丢弃最老的帧
        mjpeg_frame_t old_frame;
        xQueueReceive(server->frame_queue, &old_frame, 0);
        xQueueSend(server->frame_queue, frame, 0);
    }
    
    return ESP_OK;
}

/* ==================== 内部函数实现 ==================== */

static esp_err_t stream_handler(httpd_req_t *req)
{
    mjpeg_server_t *server = (mjpeg_server_t *)req->user_ctx;
    if (!server) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 设置MJPEG流响应头
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Connection", "close");
    
    ESP_LOGI(TAG, "New MJPEG client connected from %s", "browser");
    
    // 发送初始边界
    esp_err_t ret = httpd_resp_send_chunk(req, "\r\n--frame\r\n", strlen("\r\n--frame\r\n"));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send initial boundary");
        return ret;
    }
    
    // 主循环：持续发送MJPEG帧
    while (server->is_running) {
        mjpeg_frame_t frame;
        
        // 等待新帧，超时时间1秒
        if (xQueueReceive(server->frame_queue, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // 准备帧头部
            char frame_header[128];
            int header_len = snprintf(frame_header, sizeof(frame_header),
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %zu\r\n"
                "\r\n", frame.size);
            
            // 发送帧头部
            ret = httpd_resp_send_chunk(req, frame_header, header_len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send frame header: %s", esp_err_to_name(ret));
                break;
            }
            
            // 发送JPEG数据
            ret = httpd_resp_send_chunk(req, (const char*)frame.data, frame.size);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send JPEG data: %s", esp_err_to_name(ret));
                break;
            }
            
            // 发送帧边界
            ret = httpd_resp_send_chunk(req, "\r\n--frame\r\n", strlen("\r\n--frame\r\n"));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send frame boundary: %s", esp_err_to_name(ret));
                break;
            }
            
            // 更新统计
            server->frames_sent++;
            server->bytes_sent += frame.size;
            
        } else {
            // 超时，发送保活数据
            ret = httpd_resp_send_chunk(req, "", 0);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Client disconnected (keepalive failed)");
                break;
            }
        }
    }
    
    ESP_LOGI(TAG, "MJPEG client disconnected, sent %llu frames", server->frames_sent);
    
    // 结束响应
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static void stream_task(void *pvParameters)
{
    mjpeg_server_t *server = (mjpeg_server_t *)pvParameters;
    mjpeg_frame_t frame;
    
    ESP_LOGI(TAG, "Stream task started on core %d - FPS calculation only", xPortGetCoreID());
    
    while (server->is_running) {
        // 等待新帧进行FPS统计
        if (xQueueReceive(server->frame_queue, &frame, portMAX_DELAY) == pdTRUE) {
            
            // 更新FPS统计
            update_fps_stats(server);
            
            // 注意：帧实际发送由HTTP handler直接处理
            // 这里只是消费队列中的帧用于统计目的
            
            // 如果队列满了，清空一些旧帧以防内存积压
            mjpeg_frame_t dummy_frame;
            while (uxQueueMessagesWaiting(server->frame_queue) > 1) {
                xQueueReceive(server->frame_queue, &dummy_frame, 0);
            }
        }
    }
    
    ESP_LOGI(TAG, "Stream task ended");
    vTaskDelete(NULL);
}

// 简化的帧发送 - 现在由stream_handler直接处理
// 这个函数保留用于向后兼容，但实际不再使用
static esp_err_t send_frame_to_client(mjpeg_client_t *client, const mjpeg_frame_t *frame)
{
    // 此函数现在不再使用，因为帧发送由stream_handler直接处理
    return ESP_OK;
}

static void cleanup_inactive_clients(mjpeg_server_t *server)
{
    int64_t current_time = esp_timer_get_time();
    int64_t timeout_us = server->config.frame_timeout_ms * 1000;
    
    if (xSemaphoreTake(server->clients_mutex, portMAX_DELAY)) {
        for (int i = 0; i < server->config.max_clients; i++) {
            if (server->clients[i].is_active) {
                if (current_time - server->clients[i].last_frame_time > timeout_us) {
                    ESP_LOGW(TAG, "Client %d timeout, disconnecting", i);
                    server->clients[i].is_active = false;
                    server->active_clients--;
                }
            }
        }
        xSemaphoreGive(server->clients_mutex);
    }
}

static void update_fps_stats(mjpeg_server_t *server)
{
    int64_t current_time = esp_timer_get_time();
    server->frames_since_last_update++;
    
    // 每秒更新一次FPS
    int64_t elapsed_us = current_time - server->last_fps_time;
    if (elapsed_us >= 1000000) { // 1秒
        server->current_fps = (float)server->frames_since_last_update * 1000000.0f / (float)elapsed_us;
        server->frames_since_last_update = 0;
        server->last_fps_time = current_time;
    }
}

esp_err_t mjpeg_http_server_get_stats(mjpeg_http_server_handle_t handle,
                                     mjpeg_server_stats_t *stats)
{
    mjpeg_server_t *server = (mjpeg_server_t *)handle;
    if (!server || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    stats->active_clients = server->active_clients;
    stats->total_clients = server->total_clients;
    stats->frames_sent = server->frames_sent;
    stats->bytes_sent = server->bytes_sent;
    stats->current_fps = server->current_fps;
    
    return ESP_OK;
}