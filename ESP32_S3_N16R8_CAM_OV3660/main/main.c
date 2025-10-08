/**
 * @file main.c
 * @brief ESP32-S3 MJPEG RTSP摄像头服务器主程序
 * 
 * 该程序实现了一个基于ESP32-S3的MJPEG RTSP摄像头流媒体服务器
 * 专门使用ESP32摄像头的原生JPEG输出，通过MJPEG RTP进行流媒体传输
 * 
 * @author ESP32 Team
 * @date 2025
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_psram.h"
#include "esp_private/esp_clk.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "camera_manager.h"
#include "wifi_manager.h"
#include "mjpeg_http_server.h"
#include "system_monitor.h"

static const char *TAG = "main";

/* ==================== 配置参数 ==================== */
#define WIFI_SSID                   "TP-LINK_021966"
#define WIFI_PASSWORD               "xgm10503"
#define HTTP_PORT                   8080
#define STREAM_PATH                "/stream"
#define CAMERA_FRAME_RATE          8   // 8fps适应UXGA分辨率
#define SYSTEM_MONITOR_INTERVAL    5000

/* ==================== MJPEG HTTP 服务器配置 ==================== */
// ESP32-S3专用MJPEG流媒体服务器
// 使用摄像头的原生JPEG输出，通过HTTP multipart流传输
#define ENABLE_MJPEG_HTTP_MODE     1

#if ENABLE_MJPEG_HTTP_MODE
#pragma message("MJPEG HTTP Mode ENABLED - Camera JPEG frames will be streamed via HTTP")
#endif

/* ==================== 全局变量 ==================== */
static mjpeg_http_server_config_t s_http_config;
static camera_manager_config_t s_camera_config;
static wifi_manager_config_t s_wifi_config;
static system_monitor_config_t s_monitor_config;

static mjpeg_http_server_handle_t s_http_server = NULL;
static TaskHandle_t s_camera_task_handle = NULL;
static bool s_system_running = false;

/* ==================== 私有函数声明 ==================== */
static esp_err_t app_init_nvs(void);
static esp_err_t app_init_configs(void);
static esp_err_t app_init_modules(void);
static esp_err_t app_deinit_modules(void);
static void app_camera_task(void *pvParameters);
static void app_wifi_event_handler(wifi_state_t state, const wifi_connection_info_t *info);
static void app_system_monitor_handler(const system_resource_info_t *info);
static void app_print_system_info(void);

/* ==================== 主函数 ==================== */

void app_main(void)
{
#if ENABLE_H264_TEST_MODE
    ESP_LOGI(TAG, "ESP32-S3 H.264 RTP Test Server Starting...");
    ESP_LOGI(TAG, "Version: 3.1.0 (H.264 RTP Test Mode)");
    ESP_LOGW(TAG, "*** TEST MODE ENABLED ***");
    ESP_LOGW(TAG, "This version generates synthetic H.264 data for RTP testing");
    ESP_LOGW(TAG, "Disable ENABLE_H264_TEST_MODE for normal camera operation");
#else
    ESP_LOGI(TAG, "ESP32-S3 MJPEG HTTP Camera Server Starting...");
    ESP_LOGI(TAG, "Version: 3.1.0 (Production Mode)");
#endif
    ESP_LOGI(TAG, "Build time: %s %s", __DATE__, __TIME__);

    // 初始化NVS
    esp_err_t ret = app_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化配置
    ret = app_init_configs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize configs: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化所有模块
    ret = app_init_modules();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize modules: %s", esp_err_to_name(ret));
        app_deinit_modules();
        return;
    }

    // 打印系统信息
    app_print_system_info();
    
#if ENABLE_H264_TEST_MODE
    ESP_LOGI(TAG, "=== H.264 RTP Test Mode Active ===");
    ESP_LOGI(TAG, "• Generates synthetic H.264 NAL units (SPS, PPS, IDR, P-frames)");
    ESP_LOGI(TAG, "• Tests RFC 6184 RTP packetization (Single NAL and FU-A modes)");
    ESP_LOGI(TAG, "• Compatible with VLC, FFmpeg, GStreamer clients");
    ESP_LOGI(TAG, "• Frame rate: ~8 fps with I-frame every 30 frames - anti-tearing optimized");
    ESP_LOGI(TAG, "=====================================");
#endif

    // 启动WiFi连接
    ret = wifi_manager_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
        return;
    }

    // 等待WiFi连接
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    ret = wifi_manager_wait_for_connection(30000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed: %s", esp_err_to_name(ret));
        return;
    }

#if !ENABLE_H264_TEST_MODE
    // 启动摄像头（仅在非测试模式）
    ret = camera_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start camera: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Camera started successfully");
    
    // 等待传感器充分稳定后进一步优化设置
    vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒让传感器充分稳定
    
    // 直接通过esp_camera接口进一步优化亮度和图像方向
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        ESP_LOGI(TAG, "Applying enhanced settings...");
        // 应用稳定的亮度设置（确保系统稳定运行）
        s->set_brightness(s, 2);        // 适中亮度，确保稳定性
        s->set_contrast(s, 1);          // 适中对比度
        s->set_ae_level(s, 1);          // 温和曝光补偿
        s->set_agc_gain(s, 10);         // 保守增益值，确保稳定
        s->set_aec_value(s, 400);       // 保守曝光时间
        
        // 额外的同步优化设置
        s->set_saturation(s, 2);        // 增强饱和度
        s->set_wpc(s, 1);               // 白点校正
        s->set_raw_gma(s, 1);           // 伽马校正提升亮度
        
        ESP_LOGI(TAG, "NO-SOI optimization applied in runtime settings");
        
        // 确保图像方向正确（修复180度倒转）
        s->set_hmirror(s, 1);           // 水平镜像
        s->set_vflip(s, 1);             // 垂直翻转
        
        ESP_LOGI(TAG, "Enhanced brightness and orientation settings applied");
    }
#else
    ESP_LOGI(TAG, "Skipping camera initialization - H.264 test mode enabled");
#endif

    // 启动HTTP MJPEG服务器
    ret = mjpeg_http_server_start(s_http_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP MJPEG server: %s", esp_err_to_name(ret));
        return;
    }

    // 启动系统监控
    ret = system_monitor_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start system monitor: %s", esp_err_to_name(ret));
        return;
    }

    s_system_running = true;
    
#if ENABLE_MJPEG_HTTP_MODE
    // 创建摄像头任务（正常模式）- 使用Core 1进行高性能采集
    xTaskCreatePinnedToCore(
        app_camera_task,
        "camera_task",
        APP_CAMERA_TASK_STACK_SIZE,
        NULL,
        APP_CAMERA_TASK_PRIORITY,
        &s_camera_task_handle,
        APP_CAMERA_TASK_CORE  // 专用核心1用于摄像头高速采集
    );
    
    ESP_LOGI(TAG, "All modules started successfully!");
    ESP_LOGI(TAG, "HTTP MJPEG stream available at: http://<IP>:%d%s", 
             s_http_config.port, STREAM_PATH);
#endif

    // 主循环
    uint32_t loop_count = 0;
    while (s_system_running) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5秒检查一次
        loop_count++;
        
        // 检查系统状态
        if (wifi_manager_get_state() != WIFI_STATE_CONNECTED) {
            ESP_LOGW(TAG, "WiFi disconnected, attempting reconnection...");
        }
        
#if ENABLE_H264_TEST_MODE
        // 在测试模式下，每30秒打印一次统计信息
        if (loop_count % 6 == 0) {  // 6 * 5秒 = 30秒
            mjpeg_server_stats_t stats;
            if (mjpeg_http_server_get_stats(s_http_server, &stats) == ESP_OK) {
                ESP_LOGI(TAG, "MJPEG HTTP Stats: Clients=%lu, Frames=%llu, Bytes=%llu, FPS=%.1f", 
                         stats.active_clients, stats.frames_sent, stats.bytes_sent, stats.current_fps);
            }
        }
#endif
    }

    ESP_LOGI(TAG, "System shutting down...");
    app_deinit_modules();
}

/* ==================== 私有函数实现 ==================== */

static esp_err_t app_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t app_init_configs(void)
{
    // 初始化摄像头配置 - 平衡分辨率和稳定性
    s_camera_config = camera_manager_get_default_config();
    s_camera_config.frame_size = FRAMESIZE_UXGA; // 1600x1200 - 高分辨率但易处理
    s_camera_config.pixel_format = PIXFORMAT_JPEG;
    s_camera_config.jpeg_quality = 12;   // 良好质量平衡
    s_camera_config.fb_count = 3;        // 适中缓冲区数量

    // 初始化WiFi配置
    s_wifi_config = wifi_manager_get_default_config();
    strncpy(s_wifi_config.ssid, WIFI_SSID, sizeof(s_wifi_config.ssid) - 1);
    strncpy(s_wifi_config.password, WIFI_PASSWORD, sizeof(s_wifi_config.password) - 1);
    s_wifi_config.max_retry = 5;
    s_wifi_config.auto_reconnect = true;

    // 初始化RTSP配置
    s_http_config = mjpeg_http_server_get_default_config();
    s_http_config.port = HTTP_PORT;
    s_http_config.max_clients = 4;
    s_http_config.core_id = APP_RTSP_TASK_CORE;
    s_http_config.task_priority = APP_RTSP_TASK_PRIORITY;
    s_http_config.frame_timeout_ms = 5000;
    
    ESP_LOGI(TAG, "MJPEG HTTP Server configured - Camera JPEG → HTTP multipart stream");
    ESP_LOGI(TAG, "Stream URL: http://[ESP32_IP]:%d%s", s_http_config.port, STREAM_PATH);

    // 初始化系统监控配置
    s_monitor_config = system_monitor_get_default_config();
    s_monitor_config.monitor_interval_ms = SYSTEM_MONITOR_INTERVAL;
    s_monitor_config.enable_temperature = true;
    s_monitor_config.enable_cpu_usage = true;
    s_monitor_config.enable_task_monitor = true;

    return ESP_OK;
}

static esp_err_t app_init_modules(void)
{
    esp_err_t ret;

#if !ENABLE_H264_TEST_MODE
    // 初始化摄像头管理器（仅在非测试模式）
    ret = camera_manager_init(&s_camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Camera manager initialized");
#else
    ESP_LOGI(TAG, "Camera manager skipped - H.264 test mode enabled");
#endif

    // 打印内存信息
    ESP_LOGI(TAG, "Free heap before WiFi init: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free internal heap: %lu bytes", esp_get_free_internal_heap_size());
    
    // 初始化WiFi管理器
    ret = wifi_manager_init(&s_wifi_config, app_wifi_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi manager initialized");
    
    // 打印内存信息
    ESP_LOGI(TAG, "Free heap after WiFi init: %lu bytes", esp_get_free_heap_size());

    // 初始化HTTP MJPEG服务器
    ret = mjpeg_http_server_init(&s_http_config, &s_http_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP MJPEG server init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "HTTP MJPEG server initialized");

    // 初始化系统监控
    ret = system_monitor_init(&s_monitor_config, app_system_monitor_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System monitor init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "System monitor initialized");

    return ESP_OK;
}

static esp_err_t app_deinit_modules(void)
{
    s_system_running = false;

    // 等待摄像头任务结束
    if (s_camera_task_handle != NULL) {
        vTaskDelete(s_camera_task_handle);
        s_camera_task_handle = NULL;
    }

    // 去初始化各模块
    system_monitor_deinit();
    if (s_http_server != NULL) {
        mjpeg_http_server_deinit(s_http_server);
        s_http_server = NULL;
    }
    wifi_manager_deinit();
    
#if !ENABLE_H264_TEST_MODE
    camera_manager_deinit();
#endif

    ESP_LOGI(TAG, "All modules deinitialized");
    return ESP_OK;
}

static void app_camera_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Camera task started");
    
    int64_t last_frame_time = esp_timer_get_time();
    uint32_t frame_count = 0;
    uint32_t failed_count = 0;

    while (s_system_running) {
        // UXGA抗撕裂优化：适中分辨率的合理准备时间
        vTaskDelay(pdMS_TO_TICKS(6));  // 6ms预等待，让传感器准备UXGA帧
        
        int64_t capture_start = esp_timer_get_time();
        
        // 获取摄像头帧（增加更长的超时时间适应低帧率）
        camera_fb_t *fb = camera_manager_capture_frame(500);  // 500ms超时适应5fps
        if (fb != NULL) {
            int64_t current_time = esp_timer_get_time();
            
            // 创建MJPEG帧结构，使用实际时间戳
            mjpeg_frame_t mjpeg_frame = {
                .data = fb->buf,
                .size = fb->len,
                .timestamp_us = current_time
            };

            // 推送到HTTP MJPEG服务器
            esp_err_t ret = mjpeg_http_server_push_frame(s_http_server, &mjpeg_frame);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "Failed to push frame: %s", esp_err_to_name(ret));
            }

            // 释放帧缓冲
            camera_manager_return_frame(fb);
            
            frame_count++;
            failed_count = 0;  // 重置失败计数
            
            // 计算实际帧间隔
            int64_t frame_interval = current_time - last_frame_time;
            last_frame_time = current_time;
            
            // 每100帧输出一次详细信息，包含双核信息
            if (frame_count % 100 == 0) {
                float actual_fps = 1000000.0f / (float)frame_interval;  // 微秒转fps
                ESP_LOGI(TAG, "Frame %lu: interval=%.1fms, fps=%.1f, capture_time=%.1fms [Core%d]", 
                         frame_count, frame_interval/1000.0f, actual_fps, 
                         (current_time - capture_start)/1000.0f, xPortGetCoreID());
            }
            
            
            // UXGA抗撕裂控制：目标8fps (125ms间隔) - 平衡性能和稳定性
            uint32_t frame_interval_ms = frame_interval / 1000;  // 转换为毫秒
            uint32_t target_interval_ms = 125; // 目标：8fps (1000/8 = 125ms)
            uint32_t min_interval_ms = 115;    // 最小间隔：115ms
            uint32_t max_interval_ms = 140;    // 最大间隔：140ms
            
            if (frame_interval_ms < min_interval_ms) {
                // 强制等待到最小间隔，确保传感器有足够时间稳定
                uint32_t delay_ms = min_interval_ms - frame_interval_ms;
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
            } else if (frame_interval_ms > max_interval_ms) {
                // 超时警告，但不阻塞
                ESP_LOGW(TAG, "Frame interval too long: %lu ms (target: %lu ms)", frame_interval_ms, target_interval_ms);
            }
            
            // UXGA稳定延迟 - 适中分辨率优化
            vTaskDelay(pdMS_TO_TICKS(8));  // 8ms延迟，确保UXGA分辨率稳定
            
        } else {
            failed_count++;
            ESP_LOGW(TAG, "Failed to capture camera frame (count: %lu)", failed_count);
            
            // 如果连续失败，增加延时避免CPU占用过高
            if (failed_count > 10) {
                vTaskDelay(pdMS_TO_TICKS(100));  // 失败时延时100ms
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));   // 短暂延时10ms
            }
        }
    }

    ESP_LOGI(TAG, "Camera task stopped, total frames: %lu", frame_count);
    vTaskDelete(NULL);
}

static void app_wifi_event_handler(wifi_state_t state, const wifi_connection_info_t *info)
{
    switch (state) {
        case WIFI_STATE_CONNECTING:
            ESP_LOGI(TAG, "WiFi connecting...");
            break;
        
        case WIFI_STATE_CONNECTED:
            if (info != NULL) {
                ESP_LOGI(TAG, "WiFi connected!");
                ESP_LOGI(TAG, "IP Address: %s", info->ip_addr);
                ESP_LOGI(TAG, "RSSI: %d dBm", info->rssi);
                ESP_LOGI(TAG, "Channel: %d", info->channel);
            }
            break;
        
        case WIFI_STATE_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected");
            break;
        
        case WIFI_STATE_ERROR:
            ESP_LOGE(TAG, "WiFi connection error");
            break;
        
        default:
            break;
    }
}

static void app_system_monitor_handler(const system_resource_info_t *info)
{
    ESP_LOGI(TAG, "System Status: Free heap: %lu KB | Min free heap: %lu KB | PSRAM free: %lu KB | CPU usage: %.1f%% | Temperature: %ld°C | Uptime: %llu ms | Tasks: %lu", 
             info->free_heap_size / 1024, info->min_free_heap_size / 1024, info->free_psram_size / 1024, 
             info->cpu_usage_percent, info->temperature_celsius, info->uptime_ms, info->task_count);
    
    if (s_http_server != NULL) {
        mjpeg_server_stats_t stats;
        if (mjpeg_http_server_get_stats(s_http_server, &stats) == ESP_OK) {
            ESP_LOGI(TAG, "HTTP MJPEG clients: %lu, FPS: %.1f, Total: %llu frames, %llu bytes", 
                     stats.active_clients, stats.current_fps, stats.frames_sent, stats.bytes_sent);
        }
    }
    
    ESP_LOGI(TAG, "Camera FPS: %.1f", camera_manager_get_fps());
}

static void app_print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    ESP_LOGI(TAG, "=== System Information ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "Chip revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "CPU frequency: %u MHz", (unsigned int)(esp_clk_cpu_freq() / 1000000));
    
    // Get Flash size from spi_flash module
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash size: %u MB", (unsigned int)(flash_size / (1024 * 1024)));
    ESP_LOGI(TAG, "PSRAM available: %s", esp_psram_is_initialized() ? "Yes" : "No");
}