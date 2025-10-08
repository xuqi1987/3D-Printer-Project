/**
 * @file h264_rtsp_server.c
 * @brief H.264 RTSP流媒体服务器模块实现
 */

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "h264_rtsp_server.h"
#include "h264_test_data.h"
#include "h264_minimal_test.h"

static const char *TAG = "h264_rtsp";

/* ==================== H.264 RTP 常量定义 (RFC 6184) ==================== */
#define H264_RTP_PAYLOAD_TYPE           96
#define H264_MAX_MTU_SIZE              1400    // 典型以太网MTU减去IP/UDP头部
#define H264_MAX_RTP_PAYLOAD_SIZE      (H264_MAX_MTU_SIZE - 12)  // 减去RTP头部

// NAL单元类型 (RFC 6184)
typedef enum {
    H264_NAL_SLICE = 1,
    H264_NAL_DPA = 2,
    H264_NAL_DPB = 3,
    H264_NAL_DPC = 4,
    H264_NAL_IDR_SLICE = 5,
    H264_NAL_SEI = 6,
    H264_NAL_SPS = 7,
    H264_NAL_PPS = 8,
    H264_NAL_AUD = 9,
    H264_NAL_END_SEQUENCE = 10,
    H264_NAL_END_STREAM = 11,
    H264_NAL_FILLER_DATA = 12,
    H264_NAL_SPS_EXT = 13,
    // RTP打包模式 (RFC 6184)
    H264_NAL_STAP_A = 24,
    H264_NAL_STAP_B = 25,
    H264_NAL_MTAP16 = 26,
    H264_NAL_MTAP24 = 27,
    H264_NAL_FU_A = 28,
    H264_NAL_FU_B = 29
} h264_nal_type_t;

// FU-A 头部结构
typedef struct {
    uint8_t s : 1;  // Start bit
    uint8_t e : 1;  // End bit
    uint8_t r : 1;  // Reserved bit (must be 0)
    uint8_t type : 5;  // NAL unit type
} __attribute__((packed)) h264_fu_header_t;

// H.264 NAL单元信息
typedef struct {
    const uint8_t *data;
    size_t size;
    h264_nal_type_t type;
    uint8_t nri;  // nal_ref_idc
} h264_nal_unit_t;

/* ==================== 私有数据结构 ==================== */
typedef struct rtsp_client {
    int socket;
    uint32_t client_id;
    char client_ip[16];
    bool is_playing;
    uint16_t rtp_seq;
    uint32_t rtp_timestamp;
    int64_t last_frame_time_us;  // 上一帧的时间戳(微秒)
    // 网络状况监控
    uint32_t consecutive_failures; // 连续发送失败次数
    uint32_t total_frames_sent;    // 总发送帧数
    uint32_t total_frames_dropped; // 总丢弃帧数
    struct rtsp_client *next;
} rtsp_client_t;

typedef struct {
    h264_rtsp_server_config_t config;
    rtsp_server_state_t state;
    TaskHandle_t server_task;
    SemaphoreHandle_t mutex;
    rtsp_server_stats_t stats;
    uint32_t next_client_id;
    rtsp_client_t *clients;
    QueueHandle_t frame_queue;
    // FPS统计
    int64_t last_fps_update_time;
    uint32_t frames_since_last_update;
    // 测试模式相关
    TaskHandle_t test_task;
    bool test_mode_running;
    uint32_t test_frame_counter;
} h264_rtsp_server_t;

/* ==================== 私有函数声明 ==================== */
static void h264_rtsp_server_task(void *pvParameters);
static bool validate_config(const h264_rtsp_server_config_t *config);
static void handle_rtsp_session(h264_rtsp_server_t *server, int client_sock);
static void handle_rtsp_request(h264_rtsp_server_t *server, int client_sock, const char *request);
static esp_err_t add_client(h264_rtsp_server_t *server, int socket, const char *ip);
static void remove_client(h264_rtsp_server_t *server, int socket);
static rtsp_client_t *find_client(h264_rtsp_server_t *server, int socket);

// H.264 RTP 相关函数
static esp_err_t send_h264_frame(h264_rtsp_server_t *server, const h264_frame_t *frame);
static esp_err_t parse_h264_nals(const uint8_t *data, size_t size, h264_nal_unit_t *nals, size_t max_nals, size_t *nal_count);
static esp_err_t send_h264_nal_single(rtsp_client_t *client, const h264_nal_unit_t *nal, bool marker);
static esp_err_t send_h264_nal_fu_a(rtsp_client_t *client, const h264_nal_unit_t *nal, bool marker);
static esp_err_t send_h264_nal_stap_a(rtsp_client_t *client, const h264_nal_unit_t *nals, size_t nal_count, bool marker);
static esp_err_t send_rtp_h264_packet(rtsp_client_t *client, const uint8_t *payload, size_t payload_size, bool marker);

// 兼容性函数 (用于MJPEG输入)
static esp_err_t send_mjpeg_frame(h264_rtsp_server_t *server, const h264_frame_t *frame);
static esp_err_t send_rtp_packet(h264_rtsp_server_t *server, rtsp_client_t *client, const uint8_t *data, size_t len, bool marker);

// 测试模式函数
static void h264_test_mode_task(void *pvParameters);
static esp_err_t start_test_mode(h264_rtsp_server_t *server);
static esp_err_t stop_test_mode(h264_rtsp_server_t *server);

/* ==================== 公共函数实现 ==================== */

esp_err_t h264_rtsp_server_init(const h264_rtsp_server_config_t *config,
                                h264_rtsp_server_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (!validate_config(config)) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = calloc(1, sizeof(h264_rtsp_server_t));
    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for server");
        return ESP_ERR_NO_MEM;
    }

    // 复制配置
    memcpy(&server->config, config, sizeof(h264_rtsp_server_config_t));
    server->state = RTSP_SERVER_STATE_STOPPED;
    server->next_client_id = 1;

    // 创建互斥锁
    server->mutex = xSemaphoreCreateMutex();
    if (server->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(server);
        return ESP_ERR_NO_MEM;
    }

    // 创建帧队列
    server->frame_queue = xQueueCreate(10, sizeof(h264_frame_t));
    if (server->frame_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        vSemaphoreDelete(server->mutex);
        free(server);
        return ESP_ERR_NO_MEM;
    }

    server->clients = NULL;
    server->last_fps_update_time = esp_timer_get_time();
    server->frames_since_last_update = 0;
    *handle = (h264_rtsp_server_handle_t)server;
    
    ESP_LOGI(TAG, "H.264 RTSP server initialized on port %d", config->port);
    return ESP_OK;
}

esp_err_t h264_rtsp_server_deinit(h264_rtsp_server_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;

    // 停止服务器
    if (server->state != RTSP_SERVER_STATE_STOPPED) {
        h264_rtsp_server_stop(handle);
    }

    // 删除所有客户端
    while (server->clients != NULL) {
        rtsp_client_t *client = server->clients;
        server->clients = client->next;
        free(client);
    }

    // 删除帧队列
    if (server->frame_queue != NULL) {
        vQueueDelete(server->frame_queue);
    }

    // 删除互斥锁
    if (server->mutex != NULL) {
        vSemaphoreDelete(server->mutex);
    }

    free(server);
    ESP_LOGI(TAG, "H.264 RTSP server deinitialized");
    return ESP_OK;
}

esp_err_t h264_rtsp_server_start(h264_rtsp_server_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;

    if (server->state != RTSP_SERVER_STATE_STOPPED) {
        ESP_LOGW(TAG, "Server already started or starting");
        return ESP_ERR_INVALID_STATE;
    }

    server->state = RTSP_SERVER_STATE_STARTING;

    // 创建服务器任务
    BaseType_t ret = xTaskCreatePinnedToCore(
        h264_rtsp_server_task,
        "rtsp_server",
        server->config.task_stack_size,
        server,
        server->config.task_priority,
        &server->server_task,
        server->config.core_id
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create server task");
        server->state = RTSP_SERVER_STATE_ERROR;
        return ESP_FAIL;
    }

    server->state = RTSP_SERVER_STATE_RUNNING;
    
    // 如果启用了测试模式，启动测试任务
    if (server->config.enable_test_mode) {
        esp_err_t test_ret = start_test_mode(server);
        if (test_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start test mode: %s", esp_err_to_name(test_ret));
        }
    }
    
    ESP_LOGI(TAG, "H.264 RTSP server started");
    return ESP_OK;
}

esp_err_t h264_rtsp_server_stop(h264_rtsp_server_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;

    if (server->state != RTSP_SERVER_STATE_RUNNING) {
        ESP_LOGW(TAG, "Server not running");
        return ESP_ERR_INVALID_STATE;
    }

    server->state = RTSP_SERVER_STATE_STOPPING;

    // 停止测试模式
    if (server->config.enable_test_mode) {
        stop_test_mode(server);
    }

    // 删除服务器任务
    if (server->server_task != NULL) {
        vTaskDelete(server->server_task);
        server->server_task = NULL;
    }

    server->state = RTSP_SERVER_STATE_STOPPED;
    ESP_LOGI(TAG, "H.264 RTSP server stopped");
    return ESP_OK;
}

esp_err_t h264_rtsp_server_push_frame(h264_rtsp_server_handle_t handle,
                                      const h264_frame_t *frame)
{
    if (handle == NULL || frame == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;

    if (server->state != RTSP_SERVER_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }

    // 检查是否有客户端在播放
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    bool has_playing_clients = false;
    rtsp_client_t *client = server->clients;
    while (client != NULL) {
        if (client->is_playing) {
            has_playing_clients = true;
            break;
        }
        client = client->next;
    }
    xSemaphoreGive(server->mutex);

    if (has_playing_clients) {
        esp_err_t ret = ESP_OK;
        
        // 检查数据格式并选择合适的发送方式
        if (frame->size > 4 && 
            frame->data[0] == 0x00 && frame->data[1] == 0x00 && 
            frame->data[2] == 0x00 && frame->data[3] == 0x01) {
            // H.264 NAL单元格式 (以0x00000001开头)
            ret = send_h264_frame(server, frame);
            ESP_LOGV(TAG, "Sent H.264 frame, size: %zu bytes", frame->size);
        } else if (frame->size > 2 && 
                   frame->data[0] == 0xFF && frame->data[1] == 0xD8) {
            // JPEG格式 (以0xFFD8开头) - 兼容性模式
            ret = send_mjpeg_frame(server, frame);
            ESP_LOGV(TAG, "Sent MJPEG frame (compat mode), size: %zu bytes", frame->size);
        } else {
            ESP_LOGW(TAG, "Unknown frame format, size: %zu, header: 0x%02X%02X%02X%02X", 
                     frame->size, 
                     frame->size > 0 ? frame->data[0] : 0,
                     frame->size > 1 ? frame->data[1] : 0,
                     frame->size > 2 ? frame->data[2] : 0,
                     frame->size > 3 ? frame->data[3] : 0);
            ret = ESP_ERR_INVALID_ARG;
        }
        
        // 更新统计信息
        xSemaphoreTake(server->mutex, portMAX_DELAY);
        server->stats.frames_sent++;
        server->stats.bytes_sent += frame->size;
        server->frames_since_last_update++;
        
        // 计算实际FPS (每秒更新一次)
        int64_t current_time = esp_timer_get_time();
        int64_t elapsed_us = current_time - server->last_fps_update_time;
        
        if (elapsed_us >= 1000000) {  // 1秒
            server->stats.current_fps = (float)server->frames_since_last_update * 1000000.0f / (float)elapsed_us;
            server->frames_since_last_update = 0;
            server->last_fps_update_time = current_time;
        }
        
        xSemaphoreGive(server->mutex);
        
        // 如果发送失败，不返回错误，避免摄像头任务频繁打印错误
        if (ret != ESP_OK) {
            static uint32_t error_count = 0;
            error_count++;
            if (error_count % 100 == 0) { // 每100次错误才打印一次
                ESP_LOGW(TAG, "Frame send failed %lu times", error_count);
            }
        }
        return ESP_OK; // 总是返回成功，避免频繁错误日志
    }

    return ESP_OK;
}

rtsp_server_state_t h264_rtsp_server_get_state(h264_rtsp_server_handle_t handle)
{
    if (handle == NULL) {
        return RTSP_SERVER_STATE_ERROR;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;
    return server->state;
}

int h264_rtsp_server_get_client_count(h264_rtsp_server_handle_t handle)
{
    if (handle == NULL) {
        return -1;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;
    
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    int count = server->stats.active_clients;
    xSemaphoreGive(server->mutex);
    
    return count;
}

esp_err_t h264_rtsp_server_get_clients(h264_rtsp_server_handle_t handle,
                                       rtsp_client_info_t *clients,
                                       int max_clients,
                                       int *actual_count)
{
    if (handle == NULL || clients == NULL || actual_count == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // 暂时返回0个客户端
    *actual_count = 0;
    return ESP_OK;
}

esp_err_t h264_rtsp_server_get_stats(h264_rtsp_server_handle_t handle,
                                     rtsp_server_stats_t *stats)
{
    if (handle == NULL || stats == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;
    
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    memcpy(stats, &server->stats, sizeof(rtsp_server_stats_t));
    xSemaphoreGive(server->mutex);
    
    return ESP_OK;
}

esp_err_t h264_rtsp_server_disconnect_client(h264_rtsp_server_handle_t handle,
                                             uint32_t client_id)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    // 暂时不实现
    ESP_LOGW(TAG, "Client disconnect not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

h264_rtsp_server_config_t h264_rtsp_server_get_default_config(void)
{
    h264_rtsp_server_config_t config = {
        .port = 8554,
        .stream_name = "live",
        .max_clients = 2,            // 减少客户端数量，优化性能
        .task_priority = 5,          // 提高RTSP任务优先级
        .task_stack_size = 8192,     // 增加栈大小
        .core_id = 0,                // 保持在Core0进行网络处理
        .transport = RTSP_TRANSPORT_AUTO,
        .rtp_port_start = 5000,
        .rtp_port_end = 5100,
        .session_timeout_sec = 60,
        .enable_test_mode = false
    };
    
    return config;
}

esp_err_t h264_rtsp_server_set_test_mode(h264_rtsp_server_handle_t handle, bool enable)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    h264_rtsp_server_t *server = (h264_rtsp_server_t *)handle;
    server->config.enable_test_mode = enable;
    
    ESP_LOGI(TAG, "H.264 test mode %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

/* ==================== 私有函数实现 ==================== */

static void h264_rtsp_server_task(void *pvParameters)
{
    h264_rtsp_server_t *server = (h264_rtsp_server_t *)pvParameters;
    
    ESP_LOGI(TAG, "RTSP server task started on core %d", xPortGetCoreID());
    
    // 等待状态变为RUNNING
    while (server->state == RTSP_SERVER_STATE_STARTING) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (server->state != RTSP_SERVER_STATE_RUNNING) {
        ESP_LOGE(TAG, "Server state error in task");
        vTaskDelete(NULL);
        return;
    }
    
    // 创建TCP socket
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        server->state = RTSP_SERVER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(server->config.port);
    
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        server->state = RTSP_SERVER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }
    
    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        server->state = RTSP_SERVER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "RTSP server listening on port %d", server->config.port);
    
    while (server->state == RTSP_SERVER_STATE_RUNNING) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        
        // 设置非阻塞接受，检查是否需要停止
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(listen_sock + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            ESP_LOGE(TAG, "Select error: errno %d", errno);
            break;
        }
        
        if (activity == 0) {
            // 超时，继续循环检查状态
            continue;
        }
        
        if (FD_ISSET(listen_sock, &read_fds)) {
            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                continue;
            }
            
            // 优化TCP socket参数以提高传输性能
            int tcp_nodelay = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &tcp_nodelay, sizeof(tcp_nodelay));
            
            int send_buffer = 65536; // 64KB发送缓冲区 - 增大以减少window full
            setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer));
            
            int recv_buffer = 16384;  // 16KB接收缓冲区
            setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buffer, sizeof(recv_buffer));
            
            // 设置TCP发送超时，避免长时间阻塞
            struct timeval send_timeout = {0, 500000};  // 500ms超时，更快响应
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
            
            // 获取并记录TCP窗口信息
            int actual_sndbuf = 0;
            socklen_t optlen = sizeof(actual_sndbuf);
            if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &actual_sndbuf, &optlen) == 0) {
                ESP_LOGI(TAG, "TCP send buffer size: %d bytes", actual_sndbuf);
            }
            
            char addr_str[128];
            inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "RTSP client connected from %s (TCP optimized)", addr_str);
            
            // 添加客户端到列表
            esp_err_t ret = add_client(server, sock, addr_str);
            if (ret == ESP_OK) {
                // 更新客户端计数
                xSemaphoreTake(server->mutex, portMAX_DELAY);
                server->stats.active_clients++;
                server->stats.total_clients++;
                xSemaphoreGive(server->mutex);
                
                // 处理RTSP会话
                handle_rtsp_session(server, sock);
                
                // 从客户端列表中移除
                remove_client(server, sock);
                
                // 更新客户端计数
                xSemaphoreTake(server->mutex, portMAX_DELAY);
                server->stats.active_clients--;
                xSemaphoreGive(server->mutex);
            }
            
            close(sock);
            ESP_LOGI(TAG, "RTSP client disconnected");
        }
    }
    
    close(listen_sock);
    ESP_LOGI(TAG, "RTSP server task stopped");
    vTaskDelete(NULL);
}

static bool validate_config(const h264_rtsp_server_config_t *config)
{
    if (config->port == 0 || config->port > 65535) {
        ESP_LOGE(TAG, "Invalid port number");
        return false;
    }
    
    if (config->stream_name == NULL || strlen(config->stream_name) == 0) {
        ESP_LOGE(TAG, "Invalid stream name");
        return false;
    }
    
    if (config->max_clients == 0 || config->max_clients > 10) {
        ESP_LOGE(TAG, "Invalid max clients (should be 1-10)");
        return false;
    }
    
    if (config->task_stack_size < 4096) {
        ESP_LOGE(TAG, "Task stack size too small (min 4096)");
        return false;
    }
    
    return true;
}

static void handle_rtsp_session(h264_rtsp_server_t *server, int client_sock)
{
    char *buffer = NULL;
    char *request_buffer = NULL;
    int request_len = 0;
    int timeout_count = 0;
    const int MAX_TIMEOUT = 300; // 30秒超时 (300 * 100ms)
    
    // 动态分配缓冲区以避免栈溢出
    buffer = malloc(2048);
    request_buffer = malloc(4096);
    
    if (buffer == NULL || request_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for buffers");
        if (buffer) free(buffer);
        if (request_buffer) free(request_buffer);
        return;
    }
    
    memset(request_buffer, 0, 4096);
    
    ESP_LOGI(TAG, "Starting RTSP session for client socket %d", client_sock);
    
    // 设置socket为非阻塞模式
    int flags = fcntl(client_sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
    }
    
    while (server->state == RTSP_SERVER_STATE_RUNNING && timeout_count < MAX_TIMEOUT) {
        // 使用非阻塞接收
        int bytes_received = recv(client_sock, buffer, 2047, 0);
        
        if (bytes_received > 0) {
            // 重置超时计数器
            timeout_count = 0;
            
            buffer[bytes_received] = '\0';
            
            // 添加到请求缓冲区
            if (request_len + bytes_received < 4095) {
                memcpy(request_buffer + request_len, buffer, bytes_received);
                request_len += bytes_received;
                request_buffer[request_len] = '\0';
            } else {
                ESP_LOGW(TAG, "Request too large, resetting buffer");
                request_len = 0;
                memset(request_buffer, 0, 4096);
                continue;
            }
            
            // 检查是否收到完整请求（以\r\n\r\n结束）
            if (strstr(request_buffer, "\r\n\r\n") != NULL) {
                ESP_LOGI(TAG, "Received RTSP request:\n%s", request_buffer);
                handle_rtsp_request(server, client_sock, request_buffer);
                
                // 重置缓冲区
                memset(request_buffer, 0, 4096);
                request_len = 0;
            }
        } else if (bytes_received == 0) {
            ESP_LOGI(TAG, "Client closed connection gracefully");
            break;
        } else {
            // 检查错误类型
            int err = errno;
            if (err == EAGAIN || err == EWOULDBLOCK) {
                // 没有数据可读，继续等待
                timeout_count++;
                vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms
            } else {
                ESP_LOGE(TAG, "Receive error: errno %d (%s)", err, strerror(err));
                break;
            }
        }
    }
    
    if (timeout_count >= MAX_TIMEOUT) {
        ESP_LOGW(TAG, "Client timeout, closing connection");
    }
    
    // 清理资源
    free(buffer);
    free(request_buffer);
}

static void handle_rtsp_request(h264_rtsp_server_t *server, int client_sock, const char *request)
{
    char response[2048];
    char cseq[32] = "1";
    char method[16] = "";
    char uri[256] = "";
    
    ESP_LOGI(TAG, "Processing RTSP request from socket %d", client_sock);
    ESP_LOGD(TAG, "Request content:\n%s", request);
    
    // 检查是否是RTSP响应而非请求（跳过响应）
    if (strncmp(request, "RTSP/", 5) == 0) {
        ESP_LOGD(TAG, "Skipping RTSP response (not a request)");
        return;
    }
    
    // 解析请求行 (METHOD URI RTSP/1.0)
    if (sscanf(request, "%15s %255s", method, uri) != 2) {
        ESP_LOGE(TAG, "Failed to parse RTSP request line");
        return;
    }
    
    // 验证是否为有效的RTSP方法
    if (strcmp(method, "OPTIONS") != 0 && strcmp(method, "DESCRIBE") != 0 && 
        strcmp(method, "SETUP") != 0 && strcmp(method, "PLAY") != 0 && 
        strcmp(method, "TEARDOWN") != 0 && strcmp(method, "PAUSE") != 0) {
        ESP_LOGW(TAG, "Ignoring invalid RTSP method: %s", method);
        return;
    }
    
    // URI兼容性处理 - 支持不同客户端的URI格式
    char normalized_uri[256];
    if (strstr(uri, "/stream=") || strstr(uri, "stream=")) {
        // VLC有时会使用stream=0格式，规范化为/live
        snprintf(normalized_uri, sizeof(normalized_uri), "rtsp://192.168.31.104:8554/live");
        ESP_LOGI(TAG, "Normalized URI from %s to %s", uri, normalized_uri);
    } else {
        strncpy(normalized_uri, uri, sizeof(normalized_uri) - 1);
        normalized_uri[sizeof(normalized_uri) - 1] = '\0';
    }
    
    // 提取CSeq
    const char *cseq_line = strstr(request, "CSeq:");
    if (cseq_line) {
        // 跳过 "CSeq:" 和可能的空格
        const char *cseq_start = cseq_line + 5;
        while (*cseq_start == ' ' || *cseq_start == '\t') cseq_start++;
        
        // 读取CSeq值直到遇到换行或回车
        int i = 0;
        while (i < 31 && cseq_start[i] && cseq_start[i] != '\r' && cseq_start[i] != '\n') {
            cseq[i] = cseq_start[i];
            i++;
        }
        cseq[i] = '\0';
        ESP_LOGI(TAG, "Found CSeq header: '%s'", cseq);
    } else {
        ESP_LOGW(TAG, "No CSeq header found in request");
    }
    
    ESP_LOGI(TAG, "RTSP Method: %s, URI: %s, CSeq: %s", method, uri, cseq);
    
    // 查找客户端
    rtsp_client_t *client = find_client(server, client_sock);
    
    // 处理不同的RTSP方法
    if (strcmp(method, "OPTIONS") == 0) {
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
            "\r\n", cseq);
        ESP_LOGI(TAG, "Handled OPTIONS request");
            
    } else if (strcmp(method, "DESCRIBE") == 0) {
        // 使用MJPEG格式的SDP (RFC 2435) - ESP32摄像头原生JPEG支持  
        const char *sdp = "v=0\r\n"
                         "o=- 0 0 IN IP4 192.168.31.104\r\n"
                         "s=ESP32 MJPEG Camera Stream\r\n" 
                         "c=IN IP4 0.0.0.0\r\n"
                         "t=0 0\r\n"
                         "m=video 0 RTP/AVP 26\r\n"
                         "a=rtpmap:26 JPEG/90000\r\n"
                         "a=control:track1\r\n";
        
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "Content-Type: application/sdp\r\n"
            "Content-Length: %d\r\n"
            "\r\n%s", cseq, (int)strlen(sdp), sdp);
        ESP_LOGI(TAG, "Handled DESCRIBE request - MJPEG format");
            
    } else if (strcmp(method, "SETUP") == 0) {
        // 生成唯一的会话ID
        uint32_t session_id = esp_timer_get_time() & 0xFFFFFFFF;
        
        // 解析Transport请求
        const char *transport_line = strstr(request, "Transport:");
        bool tcp_interleaved = false; // 默认尝试满足客户端要求
        char transport_response[256];
        
        if (transport_line) {
            ESP_LOGI(TAG, "Client requested transport: %.*s", 
                    (int)(strchr(transport_line, '\r') - transport_line), transport_line);
            
            if (strstr(transport_line, "TCP") || strstr(transport_line, "interleaved")) {
                // 客户端明确请求TCP interleaved
                tcp_interleaved = true;
                strcpy(transport_response, "RTP/AVP/TCP;unicast;interleaved=0-1");
                ESP_LOGI(TAG, "Using TCP interleaved transport as requested");
            } else if (strstr(transport_line, "client_port=")) {
                // 客户端请求UDP传输 - 回复错误，因为我们只支持TCP
                snprintf(response, sizeof(response),
                    "RTSP/1.0 461 Unsupported Transport\r\n"
                    "CSeq: %s\r\n"
                    "Server: ESP32-MJPEG-RTSP/1.0\r\n"
                    "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                    "\r\n", cseq);
                ESP_LOGW(TAG, "Client requested UDP transport - not supported, sending 461 error");
                goto send_response;
            } else {
                // 默认使用TCP interleaved
                tcp_interleaved = true;
                strcpy(transport_response, "RTP/AVP/TCP;unicast;interleaved=0-1");
                ESP_LOGI(TAG, "Using default TCP interleaved transport");
            }
        } else {
            // 没有Transport头，使用默认TCP
            tcp_interleaved = true;
            strcpy(transport_response, "RTP/AVP/TCP;unicast;interleaved=0-1");
        }
        
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "Session: %08" PRIX32 ";timeout=60\r\n"
            "Transport: %s\r\n"
            "\r\n", cseq, session_id, transport_response);
        ESP_LOGI(TAG, "Handled SETUP request, session: %08" PRIX32, session_id);
            
    } else if (strcmp(method, "PLAY") == 0) {
        // 从请求中提取会话ID
        char session_id[16] = "12345678";
        const char *session_line = strstr(request, "Session:");
        if (session_line) {
            sscanf(session_line, "Session: %15s", session_id);
        }
        
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "Session: %s\r\n"
            "Range: npt=0.000-\r\n"
            "RTP-Info: url=rtsp://192.168.31.104:8554/live/track1;seq=1;rtptime=0\r\n"
            "\r\n", cseq, session_id);
        
        // 标记需要在响应发送后启动播放
        if (client != NULL) {
            client->rtp_seq = 1;  // 重置RTP序列号
            client->rtp_timestamp = 0;  // 重置时间戳
            // 注意：is_playing将在响应发送后设置
        }
        ESP_LOGI(TAG, "Handled PLAY request, session: %s", session_id);
            
    } else if (strcmp(method, "PAUSE") == 0) {
        // 设置客户端为暂停状态
        if (client != NULL) {
            client->is_playing = false;
            ESP_LOGI(TAG, "Client %s paused", client->client_ip);
        }
        
        // 从请求中提取会话ID
        char session_id[16] = "12345678";
        const char *session_line = strstr(request, "Session:");
        if (session_line) {
            sscanf(session_line, "Session: %15s", session_id);
        }
        
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "Session: %s\r\n"
            "\r\n", cseq, session_id);
        ESP_LOGI(TAG, "Handled PAUSE request");
            
    } else if (strcmp(method, "TEARDOWN") == 0) {
        // 设置客户端为停止状态
        if (client != NULL) {
            client->is_playing = false;
            ESP_LOGI(TAG, "Client %s tore down", client->client_ip);
        }
        
        snprintf(response, sizeof(response),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "\r\n", cseq);
        ESP_LOGI(TAG, "Handled TEARDOWN request");
            
    } else {
        // 未支持的方法
        ESP_LOGW(TAG, "Unsupported RTSP method: %s", method);
        snprintf(response, sizeof(response),
            "RTSP/1.0 501 Not Implemented\r\n"
            "CSeq: %s\r\n"
            "Server: ESP32-MJPEG-RTSP/1.0\r\n"
            "\r\n", cseq);
    }
    
    send_response: // 标签用于UDP transport错误的跳转
    ESP_LOGI(TAG, "Sending RTSP response:\n%s", response);
    
    int response_len = strlen(response);
    int bytes_sent = 0;
    int total_sent = 0;
    
    // 循环发送，确保全部数据都发送完成
    while (total_sent < response_len) {
        bytes_sent = send(client_sock, response + total_sent, response_len - total_sent, MSG_NOSIGNAL);
        if (bytes_sent < 0) {
            int err = errno;
            if (err == EAGAIN || err == EWOULDBLOCK) {
                // 发送缓冲区满，等待一下再试
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            } else {
                ESP_LOGE(TAG, "Failed to send response: errno %d (%s)", err, strerror(err));
                break;
            }
        } else if (bytes_sent == 0) {
            ESP_LOGW(TAG, "Client closed connection during send");
            break;
        } else {
            total_sent += bytes_sent;
        }
    }
    
    if (total_sent == response_len) {
        ESP_LOGI(TAG, "Successfully sent %d bytes response", total_sent);
        
        // 检查是否是PLAY响应，如果是则启动RTP流
        if (strstr(response, "RTSP/1.0 200 OK") && strstr(response, "Range: npt=0.000-")) {
            if (client != NULL) {
                client->is_playing = true;
                ESP_LOGI(TAG, "Client %s started RTP streaming after PLAY response", client->client_ip);
            }
        }
        
        // 发送完成后小延时，确保数据正确传输并避免缓冲区问题
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_LOGW(TAG, "Partial send: %d/%d bytes", total_sent, response_len);
    }
}

static esp_err_t add_client(h264_rtsp_server_t *server, int socket, const char *ip)
{
    rtsp_client_t *client = malloc(sizeof(rtsp_client_t));
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    client->socket = socket;
    client->client_id = server->next_client_id++;
    strncpy(client->client_ip, ip, sizeof(client->client_ip) - 1);
    client->client_ip[sizeof(client->client_ip) - 1] = '\0';
    client->is_playing = false;
    client->rtp_seq = 1;
    client->rtp_timestamp = 0;
    client->last_frame_time_us = 0;  // 初始化时间戳
    // 初始化网络状况统计
    client->consecutive_failures = 0;
    client->total_frames_sent = 0;
    client->total_frames_dropped = 0;
    
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    client->next = server->clients;
    server->clients = client;
    xSemaphoreGive(server->mutex);
    
    return ESP_OK;
}

static void remove_client(h264_rtsp_server_t *server, int socket)
{
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    
    rtsp_client_t **current = &server->clients;
    while (*current != NULL) {
        if ((*current)->socket == socket) {
            rtsp_client_t *to_remove = *current;
            *current = to_remove->next;
            free(to_remove);
            break;
        }
        current = &(*current)->next;
    }
    
    xSemaphoreGive(server->mutex);
}

static rtsp_client_t *find_client(h264_rtsp_server_t *server, int socket)
{
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    
    rtsp_client_t *client = server->clients;
    while (client != NULL) {
        if (client->socket == socket) {
            break;
        }
        client = client->next;
    }
    
    xSemaphoreGive(server->mutex);
    return client;
}

// RFC 2435兼容的JPEG解析结构
typedef struct {
    uint16_t width, height;
    uint8_t type;
    uint8_t q_factor;
    size_t scan_data_offset;
    size_t scan_data_length;
    bool has_quantization_tables;
    bool has_huffman_tables;
} jpeg_rtp_info_t;

static bool parse_jpeg_for_rtp(const uint8_t *data, size_t len, jpeg_rtp_info_t *info) {
    if (len < 20 || data[0] != 0xFF || data[1] != 0xD8) {
        ESP_LOGE(TAG, "Invalid JPEG: len=%zu, header=0x%02x%02x", len, data[0], data[1]);
        return false;
    }
    
    memset(info, 0, sizeof(jpeg_rtp_info_t));
    // 使用与摄像头配置匹配的质量因子
    info->q_factor = 12;   // 匹配摄像头的JPEG质量设置 
    info->type = 1;        // 默认YUV422，适用于大多数情况
    
    size_t i = 2;
    while (i < len - 2) {
        if (data[i] != 0xFF) {
            i++;
            continue;
        }
        
        uint8_t marker = data[i + 1];
        if (marker == 0xFF || marker == 0x00) {
            i += 2;
            continue;
        }
        
        // 获取段长度
        uint16_t segment_length = 0;
        if (i + 3 < len) {
            segment_length = (data[i + 2] << 8) | data[i + 3];
        }
        
        switch (marker) {
            case 0xC0: // SOF0 - Baseline DCT
            case 0xC1: // SOF1 - Extended sequential DCT
                if (i + 9 < len) {
                    uint8_t precision = data[i + 4];
                    info->height = (data[i + 5] << 8) | data[i + 6];
                    info->width = (data[i + 7] << 8) | data[i + 8];
                    uint8_t components = data[i + 9];
                    
                    ESP_LOGD(TAG, "JPEG SOF: %dx%d, precision=%d, components=%d", 
                             info->width, info->height, precision, components);
                    
                    if (precision != 8) {
                        ESP_LOGW(TAG, "Non-8bit precision (%d) not supported", precision);
                        return false;
                    }
                    
                    // 分析采样因子以确定正确的类型
                    if (components == 3 && i + 17 < len) {
                        uint8_t h1 = (data[i + 11] >> 4) & 0x0F;
                        uint8_t v1 = data[i + 11] & 0x0F;
                        uint8_t h2 = (data[i + 14] >> 4) & 0x0F;
                        uint8_t v2 = data[i + 14] & 0x0F;
                        uint8_t h3 = (data[i + 17] >> 4) & 0x0F;
                        uint8_t v3 = data[i + 17] & 0x0F;
                        
                        ESP_LOGD(TAG, "Sampling factors: Y(%dx%d), Cb(%dx%d), Cr(%dx%d)", 
                                 h1, v1, h2, v2, h3, v3);
                        
                        if (h1 == 2 && v1 == 2 && h2 == 1 && v2 == 1 && h3 == 1 && v3 == 1) {
                            info->type = 0; // YUV420
                        } else if (h1 == 2 && v1 == 1 && h2 == 1 && v2 == 1 && h3 == 1 && v3 == 1) {
                            info->type = 1; // YUV422  
                        } else {
                            // 其他采样格式，使用YUV420作为默认
                            info->type = 0; 
                            ESP_LOGW(TAG, "Unusual sampling factors, using YUV420");
                        }
                    } else if (components == 1) {
                        // 灰度图像
                        info->type = 0;
                    }
                }
                break;
                
            case 0xDB: // DQT - Define Quantization Table
                info->has_quantization_tables = true;
                break;
                
            case 0xC4: // DHT - Define Huffman Table  
                info->has_huffman_tables = true;
                break;
                
            case 0xDA: // SOS - Start of Scan
                // 扫描数据开始位置
                info->scan_data_offset = i + segment_length + 2;
                
                // 找到EOI计算扫描数据长度
                for (size_t j = info->scan_data_offset; j < len - 1; j++) {
                    if (data[j] == 0xFF && data[j + 1] == 0xD9) {
                        info->scan_data_length = j - info->scan_data_offset;
                        return true;
                    }
                }
                
                // 如果没找到EOI，使用剩余数据
                info->scan_data_length = len - info->scan_data_offset;
                return true;
        }
        
        i += 2 + segment_length;
    }
    
    return false;
}

static esp_err_t send_rtp_packet(h264_rtsp_server_t *server, rtsp_client_t *client, const uint8_t *data, size_t len, bool marker)
{
    // 检查客户端连接状态
    if (!client || client->socket < 0) {
        ESP_LOGW(TAG, "Invalid client or socket for RTP transmission");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 使用SO_ERROR检查socket连接状态
    int socket_error = 0;
    socklen_t len_sockopt = sizeof(socket_error);
    if (getsockopt(client->socket, SOL_SOCKET, SO_ERROR, &socket_error, &len_sockopt) < 0 || socket_error != 0) {
        ESP_LOGW(TAG, "Socket error detected for client %s: errno=%d (%s)", 
                client->client_ip, socket_error, strerror(socket_error));
        client->is_playing = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    // 适度检查socket状态，但不要过于激进
    // 注释掉预先检测，因为可能导致过多丢帧
    /*
    fd_set write_fds;
    struct timeval timeout = {0, 0};
    FD_ZERO(&write_fds);
    FD_SET(client->socket, &write_fds);
    
    if (select(client->socket + 1, NULL, &write_fds, NULL, &timeout) <= 0) {
        ESP_LOGV(TAG, "Socket not writable - dropping frame");
        server->stats.frames_dropped++;
        return ESP_OK;
    }
    */
    
    jpeg_rtp_info_t jpeg_info;
    
    // 解析JPEG数据
    if (!parse_jpeg_for_rtp(data, len, &jpeg_info)) {
        ESP_LOGE(TAG, "Failed to parse JPEG data for RTP");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 验证解析结果
    if (jpeg_info.width == 0 || jpeg_info.height == 0 || jpeg_info.scan_data_length == 0) {
        ESP_LOGE(TAG, "Invalid JPEG parse result: %dx%d, scan_len=%zu", 
                 jpeg_info.width, jpeg_info.height, jpeg_info.scan_data_length);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "JPEG parsed: %dx%d, type=%d, scan_offset=%zu, scan_len=%zu", 
             jpeg_info.width, jpeg_info.height, jpeg_info.type, 
             jpeg_info.scan_data_offset, jpeg_info.scan_data_length);
    
    // 检查分辨率是否为8的倍数（JPEG RTP要求）
    if (jpeg_info.width % 8 != 0 || jpeg_info.height % 8 != 0) {
        ESP_LOGW(TAG, "JPEG dimensions not multiple of 8: %dx%d", jpeg_info.width, jpeg_info.height);
        // 调整为8的倍数
        jpeg_info.width = (jpeg_info.width + 7) & ~7;
        jpeg_info.height = (jpeg_info.height + 7) & ~7;
    }
    
    // RFC 2435 JPEG RTP载荷头部 (主头部: 8字节)
    uint8_t jpeg_header[8];
    jpeg_header[0] = 0;                              // Type-specific
    jpeg_header[1] = 0;                              // Fragment offset (23:16)
    jpeg_header[2] = 0;                              // Fragment offset (15:8)  
    jpeg_header[3] = 0;                              // Fragment offset (7:0)
    jpeg_header[4] = jpeg_info.type;                 // Type (0=YUV420, 1=YUV422)
    jpeg_header[5] = jpeg_info.q_factor;             // Q factor
    jpeg_header[6] = jpeg_info.width / 8;            // Width in 8-pixel units
    jpeg_header[7] = jpeg_info.height / 8;           // Height in 8-pixel units
    
    // RFC 2435 修正：发送完整JPEG数据，但去除JPEG文件头部标记
    // 这样可以确保解码器有完整的信息而不会产生花屏
    // 跳过SOI (0xFFD8) 标记，但保留其他所有数据直到EOI
    const uint8_t *jpeg_payload_data = data + 2; // 跳过SOI
    size_t jpeg_payload_length = len - 2; // 相应减少长度
    
    // 验证头部字段
    if (jpeg_header[6] == 0 || jpeg_header[7] == 0) {
        ESP_LOGE(TAG, "Invalid JPEG RTP dimensions: %d x %d units", jpeg_header[6], jpeg_header[7]);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "JPEG RTP: %dx%d, type=%d, q=%d, payload_len=%zu", 
             jpeg_info.width, jpeg_info.height, jpeg_info.type, jpeg_info.q_factor, jpeg_payload_length);
    
    // 检查是否有EOI标记，如果有则包含它  
    if (len >= 2 && data[len-2] == 0xFF && data[len-1] == 0xD9) {
        // JPEG数据完整，包含EOI
        ESP_LOGD(TAG, "Complete JPEG with EOI marker");
    } else {
        ESP_LOGW(TAG, "JPEG missing EOI marker, len=%zu", len);
    }
    
    // 计算RTP载荷大小 = JPEG RTP头部(8字节) + JPEG载荷数据
    size_t rtp_payload_size = 8 + jpeg_payload_length;
    
    // 检查载荷大小
    if (jpeg_payload_length < 10) {
        ESP_LOGE(TAG, "JPEG payload too small: %zu bytes", jpeg_payload_length);
        return ESP_ERR_INVALID_ARG;
    }
    
    const size_t MAX_RTP_PAYLOAD = 65000;
    if (rtp_payload_size > MAX_RTP_PAYLOAD) {
        ESP_LOGE(TAG, "RTP payload too large: %zu bytes (max: %zu)", rtp_payload_size, MAX_RTP_PAYLOAD);
        return ESP_ERR_INVALID_ARG;
    }
    
    // RTP over TCP (RFC 2326) - Interleaved format
    uint8_t rtp_header[4 + 12]; // 4 bytes interleaved header + 12 bytes RTP header
    
    // Interleaved header: $ + channel + length
    rtp_header[0] = '$';        // Magic byte
    rtp_header[1] = 0;          // Channel 0 (video)
    rtp_header[2] = (12 + rtp_payload_size) >> 8;    // Length high byte  
    rtp_header[3] = (12 + rtp_payload_size) & 0xFF;  // Length low byte
    
    // RTP header (12 bytes)
    rtp_header[4] = 0x80;       // Version=2, P=0, X=0, CC=0
    rtp_header[5] = 26 | (marker ? 0x80 : 0);  // M=marker, PT=26 (JPEG)
    rtp_header[6] = client->rtp_seq >> 8;      // Sequence number high
    rtp_header[7] = client->rtp_seq & 0xFF;    // Sequence number low
    
    // RTP timestamp (4 bytes)
    uint32_t ts = client->rtp_timestamp;
    rtp_header[8] = (ts >> 24) & 0xFF;
    rtp_header[9] = (ts >> 16) & 0xFF;
    rtp_header[10] = (ts >> 8) & 0xFF;
    rtp_header[11] = ts & 0xFF;
    
    // SSRC (4 bytes) - use client ID
    uint32_t ssrc = client->client_id;
    rtp_header[12] = (ssrc >> 24) & 0xFF;
    rtp_header[13] = (ssrc >> 16) & 0xFF;
    rtp_header[14] = (ssrc >> 8) & 0xFF;
    rtp_header[15] = ssrc & 0xFF;
    

    
    // Send interleaved header + RTP header with minimal retry for EAGAIN
    int sent = send(client->socket, rtp_header, 16, MSG_NOSIGNAL);
    if (sent != 16) {
        int err = errno;
        if (sent == -1 && (err == EAGAIN || err == EWOULDBLOCK)) {
            // TCP缓冲区满，给一次快速重试机会
            taskYIELD();  // 让其他任务运行，可能释放一些缓冲区
            sent = send(client->socket, rtp_header, 16, MSG_NOSIGNAL);
            if (sent != 16) {
                ESP_LOGW(TAG, "RTP header blocked by TCP window - dropping frame");
                client->rtp_seq++;
                server->stats.frames_dropped++;
                return ESP_OK;
            }
        } else if (sent == -1) {
            ESP_LOGW(TAG, "RTP header send failed: errno=%d (%s) - dropping frame", err, strerror(err));
            // 检查是否是连接断开
            if (err == ECONNRESET || err == EPIPE || err == ENOTCONN) {
                ESP_LOGW(TAG, "Client connection lost, marking client as inactive");
                client->is_playing = false;
                return ESP_FAIL;  // 连接断开时返回失败
            }
            client->rtp_seq++;
            server->stats.frames_dropped++;
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "RTP header partial send: sent=%d/16 - dropping frame", sent);
            client->rtp_seq++;
            server->stats.frames_dropped++;
            return ESP_OK;
        }
        client->rtp_seq++;  // 仍然递增序号，避免序列号混乱
        server->stats.frames_dropped++;  // 更新丢帧统计
        return ESP_OK;  // 丢帧但返回成功，继续处理下一帧
    }
    
    
    // Send JPEG RTP payload header with minimal retry for EAGAIN
    sent = send(client->socket, jpeg_header, 8, MSG_NOSIGNAL);
    if (sent != 8) {
        int err = errno;
        if (sent == -1 && (err == EAGAIN || err == EWOULDBLOCK)) {
            // TCP缓冲区满，给一次快速重试机会
            taskYIELD();  // 让其他任务运行，可能释放一些缓冲区
            sent = send(client->socket, jpeg_header, 8, MSG_NOSIGNAL);
            if (sent != 8) {
                ESP_LOGW(TAG, "JPEG header blocked by TCP window - dropping frame");
                client->rtp_seq++;
                server->stats.frames_dropped++;
                return ESP_OK;
            }
        } else if (sent == -1) {
            ESP_LOGW(TAG, "JPEG header send failed: errno=%d (%s) - dropping frame", err, strerror(err));
            // 检查是否是连接断开
            if (err == ECONNRESET || err == EPIPE || err == ENOTCONN) {
                ESP_LOGW(TAG, "Client connection lost during JPEG header send");
                client->is_playing = false;
                return ESP_FAIL;
            }
            client->rtp_seq++;
            server->stats.frames_dropped++;
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "JPEG header partial send: sent=%d/8 - dropping frame", sent);
            client->rtp_seq++;
            server->stats.frames_dropped++;
            return ESP_OK;
        }
    }
    
    // Send JPEG payload data with smart chunking based on TCP window limitations
    size_t total_sent = 0;
    
    /* TCP Window Limited 详细原因分析:
     * 
     * 1. TCP流量控制机制:
     *    - TCP使用滑动窗口控制数据流，防止接收方缓冲区溢出
     *    - 接收方在TCP头中通告窗口大小(Window Size)
     *    - 发送方不能超过这个窗口大小发送数据
     * 
     * 2. 观察到的5736字节限制可能来自:
     *    - VLC客户端接收缓冲区大小 (最可能)
     *    - 路由器/WiFi设备的缓冲区设置
     *    - 操作系统网络栈的限制
     *    - 网络MTU和聚合帧的影响: 5736 ≈ 4×1434字节
     * 
     * 3. VLC处理瓶颈:
     *    - MJPEG解码需要CPU时间
     *    - 视频渲染等待垂直同步
     *    - 多线程同步延迟
     *    - 网络接收线程处理不及时
     * 
     * 4. WiFi网络因素:
     *    - 半双工特性造成延迟
     *    - 无线信号质量影响传输
     *    - 802.11n/ac聚合机制
     */
    // 动态TCP窗口限制 - 基于历史发送成功的数据量
    static size_t max_successful_send = 0;
    static uint32_t send_attempts = 0;
    
    const size_t TCP_WINDOW_BASE = 5600;  // 基于观察到的5736字节限制，保守设置
    size_t tcp_window_limit = TCP_WINDOW_BASE;
    
    // 动态调整窗口大小
    if (max_successful_send > 0) {
        tcp_window_limit = (max_successful_send * 9) / 10;  // 使用90%的历史最大值
        if (tcp_window_limit < TCP_WINDOW_BASE) {
            tcp_window_limit = TCP_WINDOW_BASE;
        }
    }
    
    while (total_sent < jpeg_payload_length) {
        size_t remaining = jpeg_payload_length - total_sent;
        
        // 如果已发送接近TCP窗口限制，使用更小的块
        size_t chunk_size;
        if (total_sent > tcp_window_limit - 1400) {
            chunk_size = (remaining > 512) ? 512 : remaining;  // 小块发送
        } else {
            chunk_size = (remaining > 1400) ? 1400 : remaining;  // 正常块大小
        }
        
        int sent = send(client->socket, jpeg_payload_data + total_sent, chunk_size, MSG_NOSIGNAL);
        
        // 处理EAGAIN - 给一次快速重试机会
        if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            taskYIELD();  // 短暂让出CPU，让其他任务处理网络事件
            sent = send(client->socket, jpeg_payload_data + total_sent, chunk_size, MSG_NOSIGNAL);
        }
        
        if (sent > 0) {
            total_sent += sent;
            // 如果部分发送，给一次机会重试小块
            if (sent < chunk_size) {
                // 尝试发送剩余的小块
                size_t remaining_chunk = chunk_size - sent;
                if (remaining_chunk <= 256) {  // 只对小块重试
                    int retry_sent = send(client->socket, 
                                         jpeg_payload_data + total_sent - sent + sent, 
                                         remaining_chunk, MSG_NOSIGNAL);
                    if (retry_sent > 0) {
                        total_sent += retry_sent;
                    } else {
                        ESP_LOGW(TAG, "TCP congestion detected - dropping frame (sent %zu/%zu bytes)", 
                                 total_sent, jpeg_payload_length);
                        client->rtp_seq++;
                        server->stats.frames_dropped++;
                        return ESP_OK;
                    }
                } else {
                    ESP_LOGW(TAG, "TCP window limited - dropping frame (sent %zu/%zu bytes)", 
                             total_sent, jpeg_payload_length);
                    client->rtp_seq++;
                    server->stats.frames_dropped++;
                    return ESP_OK;
                }
            }
            // 基于TCP窗口限制主动丢帧
            if (total_sent >= tcp_window_limit && total_sent < jpeg_payload_length) {
                // 详细分析TCP窗口限制原因
                int sock_buf_size = 0;
                socklen_t opt_len = sizeof(sock_buf_size);
                
                // 获取当前socket发送缓冲区大小
                if (getsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, &sock_buf_size, &opt_len) == 0) {
                    ESP_LOGW(TAG, "TCP window limit reached: sent=%zu/%zu, socket_buf=%d, limit=%zu", 
                             total_sent, jpeg_payload_length, sock_buf_size, tcp_window_limit);
                } else {
                    ESP_LOGW(TAG, "TCP window limit reached: sent=%zu/%zu bytes (reason unknown)", 
                             total_sent, jpeg_payload_length);
                }
                
                // 统计窗口限制模式
                static uint32_t window_limit_count = 0;
                window_limit_count++;
                if (window_limit_count % 100 == 0) {
                    ESP_LOGI(TAG, "TCP Window Analysis: %lu frames limited by TCP window in last 100 frames", 
                             (unsigned long)window_limit_count);
                }
                
                client->rtp_seq++;
                server->stats.frames_dropped++;
                return ESP_OK;
            }
        } else {
            // 发送失败，检查错误类型
            int err = errno;
            
            // EAGAIN/EWOULDBLOCK 详细分析
            if (err == EAGAIN || err == EWOULDBLOCK) {
                static uint32_t eagain_count = 0;
                eagain_count++;
                
                // 获取TCP状态信息
                int sock_buf_size = 0;
                socklen_t opt_len = sizeof(sock_buf_size);
                getsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, &sock_buf_size, &opt_len);
                
                ESP_LOGW(TAG, "TCP EAGAIN #%lu: socket buffer full (sent %zu bytes, buf_size=%d)", 
                         (unsigned long)eagain_count, total_sent, sock_buf_size);
                         
                if (eagain_count % 50 == 0) {
                    ESP_LOGI(TAG, "EAGAIN Analysis: %lu EAGAIN errors, avg at %zu bytes sent", 
                             (unsigned long)eagain_count, total_sent);
                }
            } else {
                ESP_LOGW(TAG, "Data send failed: errno=%d (%s) - dropping frame (sent %zu/%zu bytes)", 
                         err, strerror(err), total_sent, jpeg_payload_length);
            }
            
            // 检查是否是连接断开
            if (err == ECONNRESET || err == EPIPE || err == ENOTCONN) {
                ESP_LOGW(TAG, "Client connection lost during data send");
                client->is_playing = false;
                return ESP_FAIL;
            }
            
            client->rtp_seq++;
            server->stats.frames_dropped++;
            return ESP_OK;
        }
    }
    
    // 更新发送统计和动态窗口大小
    send_attempts++;
    if (total_sent >= jpeg_payload_length) {
        // 成功发送完整帧，更新历史最大发送量
        if (total_sent > max_successful_send) {
            max_successful_send = total_sent;
            ESP_LOGI(TAG, "TCP Window: New max successful send = %zu bytes (attempt #%lu)", 
                     max_successful_send, (unsigned long)send_attempts);
        }
        
        // 每1000次成功发送报告一次窗口状态
        if (send_attempts % 1000 == 0) {
            ESP_LOGI(TAG, "TCP Window Stats: max=%zu, current_limit=%zu, success_rate=%.1f%%", 
                     max_successful_send, tcp_window_limit, 
                     (float)(send_attempts - server->stats.frames_dropped) * 100.0f / send_attempts);
        }
    }
    
    ESP_LOGV(TAG, "RTP packet sent: seq=%d, ts=%lu, bytes=%zu", 
             client->rtp_seq, client->rtp_timestamp, 16 + 8 + jpeg_payload_length);
    
    server->stats.bytes_sent += 16 + 8 + total_sent;  // 更新字节统计
    client->rtp_seq++;
    return ESP_OK;
}

static esp_err_t send_mjpeg_frame(h264_rtsp_server_t *server, const h264_frame_t *frame)
{
    esp_err_t result = ESP_OK;
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    
    rtsp_client_t *client = server->clients;
    
    while (client != NULL) {
        rtsp_client_t *next_client = client->next;
        
        if (client->is_playing) {
            // 减少日志输出频率，每100帧输出一次
            static uint32_t frame_counter = 0;
            frame_counter++;
            if (frame_counter % 100 == 0) {
                uint32_t total_attempts = client->total_frames_sent + client->total_frames_dropped;
                float success_rate = total_attempts > 0 ? (float)client->total_frames_sent / total_attempts * 100.0f : 100.0f;
                ESP_LOGI(TAG, "Client %s: %lu frames, size: %zu bytes, success: %.1f%%, consecutive_fails: %lu", 
                         client->client_ip, frame_counter, frame->size, success_rate, client->consecutive_failures);
            }
            
            // 计算基于实际时间的RTP时间戳
            int64_t current_time_us = frame->timestamp_us;
            uint32_t rtp_timestamp_increment = 0;
            
            // 根据实际帧间隔动态计算RTP时间戳，确保采集和发送速度一致
            if (client->last_frame_time_us > 0) {
                // 计算实际帧间隔(微秒)
                int64_t frame_interval_us = current_time_us - client->last_frame_time_us;
                
                // 转换为90kHz时钟单位 (1秒 = 90000 ticks, 1微秒 = 0.09 ticks)
                rtp_timestamp_increment = (uint32_t)((frame_interval_us * 90) / 1000);
                
                // 限制时间戳增量，适应15fps目标（10-20fps范围）
                if (rtp_timestamp_increment < 4500) {  // 最高20fps
                    rtp_timestamp_increment = 4500;
                } else if (rtp_timestamp_increment > 9000) {  // 最低10fps
                    rtp_timestamp_increment = 9000;
                }
                
                ESP_LOGD(TAG, "Frame interval: %lld us, RTP increment: %lu (%.1f fps)", 
                         frame_interval_us, rtp_timestamp_increment, 
                         90000.0f / rtp_timestamp_increment);
            } else {
                // 第一帧，使用默认增量 (目标15fps)
                rtp_timestamp_increment = 6000;  // 90000 / 15fps = 6000
            }
            
            client->last_frame_time_us = current_time_us;
            
            // 发送RTP JPEG包
            esp_err_t ret = send_rtp_packet(server, client, frame->data, frame->size, true);
            if (ret == ESP_OK) {
                // 发送成功，更新统计
                client->total_frames_sent++;
                client->consecutive_failures = 0;  // 重置失败计数
                ESP_LOGV(TAG, "RTP packet sent: client=%s, seq=%d, ts=%lu", 
                        client->client_ip, client->rtp_seq - 1, client->rtp_timestamp);
            } else if (ret == ESP_FAIL) {
                // 连接断开
                ESP_LOGE(TAG, "Client %s connection lost", client->client_ip);
                client->is_playing = false;
                result = ESP_FAIL;
            } else {
                // 发送失败（丢帧），更新统计
                client->total_frames_dropped++;
                client->consecutive_failures++;
                
                // 如果连续失败太多，考虑降低质量或帧率
                if (client->consecutive_failures > 10) {
                    ESP_LOGW(TAG, "Client %s: %lu consecutive failures, network congestion detected", 
                             client->client_ip, client->consecutive_failures);
                }
            }
            
            // 更新RTP时间戳（基于实际帧间隔）
            client->rtp_timestamp += rtp_timestamp_increment;
        }
        
        client = next_client;
    }
    
    xSemaphoreGive(server->mutex);
    return result;
}

/* ==================== H.264 RTP 实现函数 (RFC 6184) ==================== */

static esp_err_t send_h264_frame(h264_rtsp_server_t *server, const h264_frame_t *frame)
{
    esp_err_t result = ESP_OK;
    h264_nal_unit_t nals[32];  // 支持最多32个NAL单元
    size_t nal_count = 0;
    
    // 解析H.264 NAL单元
    esp_err_t ret = parse_h264_nals(frame->data, frame->size, nals, sizeof(nals)/sizeof(nals[0]), &nal_count);
    if (ret != ESP_OK || nal_count == 0) {
        ESP_LOGE(TAG, "Failed to parse H.264 NAL units");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Parsed %zu NAL units from H.264 frame", nal_count);
    
    xSemaphoreTake(server->mutex, portMAX_DELAY);
    
    rtsp_client_t *client = server->clients;
    while (client != NULL) {
        rtsp_client_t *next_client = client->next;
        
        if (client->is_playing) {
            // 计算RTP时间戳增量
            int64_t current_time_us = frame->timestamp_us;
            uint32_t rtp_timestamp_increment = 0;
            
            if (client->last_frame_time_us > 0) {
                int64_t frame_interval_us = current_time_us - client->last_frame_time_us;
                rtp_timestamp_increment = (uint32_t)((frame_interval_us * 90) / 1000);
                
                // 限制最小和最大时间戳增量，避免异常值
                if (rtp_timestamp_increment < 1800) {  // 最高50fps
                    rtp_timestamp_increment = 1800;
                } else if (rtp_timestamp_increment > 18000) {  // 最低5fps
                    rtp_timestamp_increment = 18000;
                }
            } else {
                rtp_timestamp_increment = 6000;  // 15fps默认
            }
            
            client->last_frame_time_us = current_time_us;
            client->rtp_timestamp += rtp_timestamp_increment;
            
            // 发送每个NAL单元
            for (size_t i = 0; i < nal_count; i++) {
                bool is_last_nal = (i == nal_count - 1);
                
                if (nals[i].size <= H264_MAX_RTP_PAYLOAD_SIZE) {
                    // 单一NAL单元模式
                    ret = send_h264_nal_single(client, &nals[i], is_last_nal);
                } else {
                    // FU-A 分片模式
                    ret = send_h264_nal_fu_a(client, &nals[i], is_last_nal);
                }
                
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to send NAL unit %zu to client %s", i, client->client_ip);
                    client->is_playing = false;
                    result = ESP_FAIL;
                    break;
                }
            }
            
            static uint32_t frame_counter = 0;
            frame_counter++;
            if (frame_counter % 100 == 0) {
                ESP_LOGI(TAG, "Sent %lu H.264 frames (%zu NALs) to client %s", 
                         frame_counter, nal_count, client->client_ip);
            }
        }
        
        client = next_client;
    }
    
    xSemaphoreGive(server->mutex);
    return result;
}

static esp_err_t parse_h264_nals(const uint8_t *data, size_t size, h264_nal_unit_t *nals, size_t max_nals, size_t *nal_count)
{
    *nal_count = 0;
    
    if (size < 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t pos = 0;
    
    while (pos < size && *nal_count < max_nals) {
        // 寻找NAL单元起始码 (0x00000001 或 0x000001)
        if (pos + 4 <= size && 
            data[pos] == 0x00 && data[pos+1] == 0x00 && 
            data[pos+2] == 0x00 && data[pos+3] == 0x01) {
            
            // 4字节起始码
            pos += 4;
            
        } else if (pos + 3 <= size && 
                   data[pos] == 0x00 && data[pos+1] == 0x00 && data[pos+2] == 0x01) {
            
            // 3字节起始码
            pos += 3;
            
        } else {
            // 没有找到起始码，跳过这个字节
            pos++;
            continue;
        }
        
        if (pos >= size) {
            break;
        }
        
        // NAL单元头部
        uint8_t nal_header = data[pos];
        h264_nal_type_t nal_type = (h264_nal_type_t)(nal_header & 0x1F);
        uint8_t nri = (nal_header >> 5) & 0x03;
        
        // 找到下一个NAL单元的开始位置
        size_t nal_end = pos;
        for (size_t i = pos + 1; i < size - 2; i++) {
            if ((data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) ||
                (i < size - 3 && data[i] == 0x00 && data[i+1] == 0x00 && 
                 data[i+2] == 0x00 && data[i+3] == 0x01)) {
                nal_end = i;
                break;
            }
        }
        
        if (nal_end == pos) {
            // 这是最后一个NAL单元
            nal_end = size;
        }
        
        // 存储NAL单元信息
        nals[*nal_count].data = data + pos;
        nals[*nal_count].size = nal_end - pos;
        nals[*nal_count].type = nal_type;
        nals[*nal_count].nri = nri;
        
        ESP_LOGD(TAG, "NAL unit %zu: type=%d, size=%zu, nri=%d", 
                 *nal_count, nal_type, nals[*nal_count].size, nri);
        
        (*nal_count)++;
        pos = nal_end;
    }
    
    return (*nal_count > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t send_h264_nal_single(rtsp_client_t *client, const h264_nal_unit_t *nal, bool marker)
{
    // 单一NAL单元模式：直接发送NAL单元数据
    return send_rtp_h264_packet(client, nal->data, nal->size, marker);
}

static esp_err_t send_h264_nal_fu_a(rtsp_client_t *client, const h264_nal_unit_t *nal, bool marker)
{
    // FU-A 分片模式 (RFC 6184 Section 5.8)
    
    if (nal->size <= 1) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t nal_header = nal->data[0];
    uint8_t nal_type = nal_header & 0x1F;
    uint8_t nri = (nal_header >> 5) & 0x03;
    
    // FU indicator: F=0, NRI=original_nri, Type=28 (FU-A)
    uint8_t fu_indicator = (nri << 5) | H264_NAL_FU_A;
    
    size_t payload_offset = 1;  // 跳过原始NAL头部
    size_t remaining = nal->size - payload_offset;
    size_t max_fragment_size = H264_MAX_RTP_PAYLOAD_SIZE - 2;  // 减去FU indicator和FU header
    
    bool first_fragment = true;
    
    while (remaining > 0) {
        size_t fragment_size = (remaining > max_fragment_size) ? max_fragment_size : remaining;
        bool last_fragment = (fragment_size == remaining);
        
        // FU header: S=start, E=end, R=0, Type=original_type
        uint8_t fu_header = (first_fragment ? 0x80 : 0) |  // S bit
                           (last_fragment ? 0x40 : 0) |    // E bit
                           nal_type;                        // Type
        
        // 构建RTP载荷: FU indicator + FU header + fragment data
        uint8_t *rtp_payload = malloc(2 + fragment_size);
        if (rtp_payload == NULL) {
            return ESP_ERR_NO_MEM;
        }
        
        rtp_payload[0] = fu_indicator;
        rtp_payload[1] = fu_header;
        memcpy(rtp_payload + 2, nal->data + payload_offset, fragment_size);
        
        // 发送RTP包，只在最后一个分片且是最后一个NAL单元时设置marker
        bool rtp_marker = last_fragment && marker;
        esp_err_t ret = send_rtp_h264_packet(client, rtp_payload, 2 + fragment_size, rtp_marker);
        
        free(rtp_payload);
        
        if (ret != ESP_OK) {
            return ret;
        }
        
        payload_offset += fragment_size;
        remaining -= fragment_size;
        first_fragment = false;
    }
    
    return ESP_OK;
}

static esp_err_t send_h264_nal_stap_a(rtsp_client_t *client, const h264_nal_unit_t *nals, size_t nal_count, bool marker)
{
    // STAP-A 聚合模式 (RFC 6184 Section 5.7.1)
    // 暂时不实现，因为ESP32内存限制，优先使用单一模式和FU-A模式
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t send_rtp_h264_packet(rtsp_client_t *client, const uint8_t *payload, size_t payload_size, bool marker)
{
    if (payload_size > H264_MAX_RTP_PAYLOAD_SIZE) {
        ESP_LOGE(TAG, "H.264 RTP payload too large: %zu bytes", payload_size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // RTP over TCP (RFC 2326) - Interleaved format
    size_t total_size = 4 + 12 + payload_size;  // Interleaved header + RTP header + payload
    uint8_t *packet = malloc(total_size);
    if (packet == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Interleaved header: $ + channel + length
    packet[0] = '$';        // Magic byte
    packet[1] = 0;          // Channel 0 (video)
    packet[2] = (12 + payload_size) >> 8;    // Length high byte  
    packet[3] = (12 + payload_size) & 0xFF;  // Length low byte
    
    // RTP header (12 bytes)
    packet[4] = 0x80;       // Version=2, P=0, X=0, CC=0
    packet[5] = H264_RTP_PAYLOAD_TYPE | (marker ? 0x80 : 0);  // M=marker, PT=96 (H.264)
    
    // Sequence number
    packet[6] = client->rtp_seq >> 8;
    packet[7] = client->rtp_seq & 0xFF;
    
    // RTP timestamp (4 bytes)
    uint32_t ts = client->rtp_timestamp;
    packet[8] = (ts >> 24) & 0xFF;
    packet[9] = (ts >> 16) & 0xFF;
    packet[10] = (ts >> 8) & 0xFF;
    packet[11] = ts & 0xFF;
    
    // SSRC (4 bytes) - use client ID
    uint32_t ssrc = client->client_id;
    packet[12] = (ssrc >> 24) & 0xFF;
    packet[13] = (ssrc >> 16) & 0xFF;
    packet[14] = (ssrc >> 8) & 0xFF;
    packet[15] = ssrc & 0xFF;
    
    // H.264 payload
    memcpy(packet + 16, payload, payload_size);
    
    // 发送完整的RTP包
    esp_err_t ret = ESP_OK;
    size_t sent = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 50;
    
    while (sent < total_size && retry_count < MAX_RETRIES) {
        int result = send(client->socket, packet + sent, total_size - sent, MSG_NOSIGNAL);
        if (result > 0) {
            sent += result;
            retry_count = 0;
        } else if (result == 0) {
            ESP_LOGW(TAG, "Client closed connection during H.264 RTP send");
            ret = ESP_FAIL;
            break;
        } else {
            int err = errno;
            if (err == EAGAIN || err == EWOULDBLOCK) {
                retry_count++;
                vTaskDelay(pdMS_TO_TICKS(10));
            } else {
                ESP_LOGE(TAG, "Failed to send H.264 RTP packet: errno=%d (%s)", err, strerror(err));
                ret = ESP_FAIL;
                break;
            }
        }
    }
    
    free(packet);
    
    if (ret == ESP_OK && sent == total_size) {
        client->rtp_seq++;
        ESP_LOGV(TAG, "H.264 RTP packet sent: seq=%d, payload_size=%zu, marker=%d", 
                 client->rtp_seq - 1, payload_size, marker);
    } else {
        ESP_LOGE(TAG, "H.264 RTP send failed: sent=%zu/%zu, retries=%d", sent, total_size, retry_count);
    }
    
    return ret;
}

/* ==================== H.264 测试模式实现 ==================== */

static esp_err_t start_test_mode(h264_rtsp_server_t *server)
{
    if (server->test_task != NULL) {
        ESP_LOGW(TAG, "Test mode already running");
        return ESP_OK;
    }
    
    server->test_mode_running = true;
    server->test_frame_counter = 0;
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        h264_test_mode_task,
        "h264_test",
        4096,
        server,
        server->config.task_priority - 1,  // 稍低优先级
        &server->test_task,
        server->config.core_id
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create test mode task");
        server->test_mode_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "H.264 test mode started");
    return ESP_OK;
}

static esp_err_t stop_test_mode(h264_rtsp_server_t *server)
{
    if (server->test_task == NULL) {
        return ESP_OK;
    }
    
    server->test_mode_running = false;
    
    // 等待任务自然退出
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (server->test_task != NULL) {
        vTaskDelete(server->test_task);
        server->test_task = NULL;
    }
    
    ESP_LOGI(TAG, "H.264 test mode stopped");
    return ESP_OK;
}

static void h264_test_mode_task(void *pvParameters)
{
    h264_rtsp_server_t *server = (h264_rtsp_server_t *)pvParameters;
    
    ESP_LOGI(TAG, "H.264 test mode task started on core %d", xPortGetCoreID());
    
    uint8_t *test_buffer = malloc(4096);  // 4KB缓冲区
    if (test_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate test buffer");
        server->test_mode_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    const TickType_t frame_delay = pdMS_TO_TICKS(50);  // 20fps
    uint8_t frame_num = 0;  // H.264 frame number for sequence
    
    while (server->test_mode_running && server->state == RTSP_SERVER_STATE_RUNNING) {
        // 检查是否有客户端在播放
        xSemaphoreTake(server->mutex, portMAX_DELAY);
        bool has_playing_clients = false;
        rtsp_client_t *client = server->clients;
        while (client != NULL) {
            if (client->is_playing) {
                has_playing_clients = true;
                break;
            }
            client = client->next;
        }
        xSemaphoreGive(server->mutex);
        
        if (has_playing_clients) {
            // 生成测试帧
            size_t frame_size = 0;
            h264_frame_type_t type = H264_FRAME_TYPE_P;
            
            if (server->test_frame_counter % 30 == 0) {
                // 每30帧发送一个关键帧 (SPS+PPS+IDR)
                frame_size = h264_create_test_frame(test_buffer, 4096, 0);
                type = H264_FRAME_TYPE_I;
                frame_num = 0;  // IDR帧重置frame_num
                ESP_LOGI(TAG, "Generated test I-frame #%lu (frame_num=%d)", server->test_frame_counter, frame_num);
            } else if (server->test_frame_counter % 100 == 50) {
                // 偶尔发送大NAL单元测试FU-A分片
                frame_size = h264_create_test_frame(test_buffer, 4096, 2);  
                type = H264_FRAME_TYPE_I;
                ESP_LOGI(TAG, "Generated large test frame #%lu (FU-A test)", server->test_frame_counter);
            } else {
                // 发送P帧
                frame_size = h264_create_test_frame(test_buffer, 4096, 1);
                type = H264_FRAME_TYPE_P;
                frame_num = (frame_num + 1) & 0x0F;  // frame_num循环 0-15
                
                // 动态修改P帧的frame_num字段来保持H.264序列一致性
                if (frame_size > 8) {  
                    // 简单修改P帧中的frame_num (位置可能需要调整)
                    test_buffer[8] = (test_buffer[8] & 0xF0) | (frame_num & 0x0F);
                }
            }
            
            if (frame_size > 0) {
                // 创建H.264帧结构
                h264_frame_t h264_frame = {
                    .data = test_buffer,
                    .size = frame_size,
                    .type = type,
                    .timestamp_us = esp_timer_get_time()
                };
                
                // 发送到RTP
                esp_err_t ret = send_h264_frame(server, &h264_frame);
                if (ret == ESP_OK) {
                    server->test_frame_counter++;
                    
                    // 每100帧输出统计信息
                    if (server->test_frame_counter % 100 == 0) {
                        ESP_LOGI(TAG, "Test mode: sent %lu frames", server->test_frame_counter);
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to send test frame: %s", esp_err_to_name(ret));
                }
            }
        }
        
        vTaskDelay(frame_delay);
    }
    
    free(test_buffer);
    server->test_task = NULL;
    ESP_LOGI(TAG, "H.264 test mode task finished, sent %lu frames total", server->test_frame_counter);
    vTaskDelete(NULL);
}