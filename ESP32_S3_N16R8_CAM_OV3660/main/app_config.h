/**
 * @file app_config.h
 * @brief 应用程序配置文件
 * 
 * 定义了应用程序的各种配置常量和参数
 * 
 * @author ESP32 Team
 * @date 2025
 */

#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== WiFi配置 ==================== */
#define APP_WIFI_SSID               "TP-LINK_021966"
#define APP_WIFI_PASSWORD           "xgm10503"
#define APP_WIFI_MAX_RETRY          5
#define APP_WIFI_CONNECT_TIMEOUT    30000  // 30秒

/* ==================== RTSP服务器配置 ==================== */
#define APP_RTSP_PORT               8554
#define APP_RTSP_STREAM_NAME        "live"
#define APP_RTSP_MAX_CLIENTS        3
#define APP_RTSP_SESSION_TIMEOUT    60     // 60秒

/* ==================== 摄像头配置 ==================== */
#define APP_CAMERA_FRAME_SIZE       FRAMESIZE_VGA    // 640x480
#define APP_CAMERA_PIXEL_FORMAT     PIXFORMAT_JPEG
#define APP_CAMERA_JPEG_QUALITY     12               // 0-63, 数字越小质量越高
#define APP_CAMERA_FB_COUNT         2
#define APP_CAMERA_XCLK_FREQ        20000000         // 20MHz
#define APP_CAMERA_FRAME_RATE       20               // 20 FPS

/* ==================== 系统监控配置 ==================== */
#define APP_MONITOR_INTERVAL        5000             // 5秒
#define APP_MEMORY_THRESHOLD        (50 * 1024)      // 50KB最小空闲内存
#define APP_TEMPERATURE_THRESHOLD   80               // 80°C温度告警

/* ==================== 双核负载均衡任务配置 ==================== */
// Core 1: 摄像头和数据处理任务（减轻Core0负载）
#define APP_CAMERA_TASK_PRIORITY    6        // 最高优先级 - 摄像头采集
#define APP_CAMERA_TASK_STACK_SIZE  8192
#define APP_CAMERA_TASK_CORE        1

#define APP_JPEG_PROC_PRIORITY      5        // JPEG处理任务，分担Core0负载
#define APP_JPEG_PROC_STACK_SIZE    6144
#define APP_JPEG_PROC_CORE          1        // 移到Core1，预处理JPEG数据

// Core 0: 专注网络传输任务
#define APP_RTSP_TASK_PRIORITY      5        // 高优先级 - RTSP协议处理
#define APP_RTSP_TASK_STACK_SIZE    8192     
#define APP_RTSP_TASK_CORE          0

#define APP_NET_SEND_PRIORITY       6        // 最高优先级 - 纯网络发送
#define APP_NET_SEND_STACK_SIZE     4096
#define APP_NET_SEND_CORE           0        // Core0专注快速网络发送

#define APP_MONITOR_TASK_PRIORITY   2        // 低优先级 - 系统监控
#define APP_MONITOR_TASK_STACK_SIZE 4096
#define APP_MONITOR_TASK_CORE       1        // 移到Core1，减轻Core0负载

/* ==================== 网络配置 ==================== */
#define APP_RTP_PORT_START          5000
#define APP_RTP_PORT_END            5100

/* ==================== 调试配置 ==================== */
#define APP_DEBUG_ENABLE            1
#define APP_LOG_LEVEL               ESP_LOG_INFO

/* ==================== 硬件配置 ==================== */
#define APP_LED_GPIO                2
#define APP_BUTTON_GPIO             0

#ifdef __cplusplus
}
#endif

#endif /* _APP_CONFIG_H_ */