/**
 * @file mjpeg_http_server.h
 * @brief HTTP MJPEG流媒体服务器模块
 * 
 * 该模块实现了一个简单的HTTP MJPEG流媒体服务器
 * 
 * @author ESP32 Team
 * @date 2025
 */

#ifndef _MJPEG_HTTP_SERVER_H_
#define _MJPEG_HTTP_SERVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HTTP MJPEG服务器句柄类型
 */
typedef void* mjpeg_http_server_handle_t;

/**
 * @brief HTTP MJPEG服务器配置
 */
typedef struct {
    uint16_t port;                     /*!< 服务器监听端口 */
    uint8_t max_clients;               /*!< 最大客户端连接数 */
    uint8_t task_priority;             /*!< 服务器任务优先级 */
    size_t task_stack_size;            /*!< 服务器任务栈大小 */
    int core_id;                       /*!< 运行的CPU核心ID */
    uint32_t frame_timeout_ms;         /*!< 帧超时时间(毫秒) */
} mjpeg_http_server_config_t;

/**
 * @brief JPEG帧数据结构
 */
typedef struct {
    const uint8_t *data;               /*!< JPEG帧数据 */
    size_t size;                       /*!< 数据大小 */
    int64_t timestamp_us;              /*!< 时间戳(微秒) */
} mjpeg_frame_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化HTTP MJPEG服务器
 */
esp_err_t mjpeg_http_server_init(const mjpeg_http_server_config_t *config,
                                 mjpeg_http_server_handle_t *handle);

/**
 * @brief 去初始化HTTP MJPEG服务器
 */
esp_err_t mjpeg_http_server_deinit(mjpeg_http_server_handle_t handle);

/**
 * @brief 启动服务器
 */
esp_err_t mjpeg_http_server_start(mjpeg_http_server_handle_t handle);

/**
 * @brief 停止服务器
 */
esp_err_t mjpeg_http_server_stop(mjpeg_http_server_handle_t handle);

/**
 * @brief 推送MJPEG帧
 */
esp_err_t mjpeg_http_server_push_frame(mjpeg_http_server_handle_t handle,
                                       const mjpeg_frame_t *frame);

/**
 * @brief HTTP MJPEG服务器统计信息
 */
typedef struct {
    uint32_t active_clients;           /*!< 活跃客户端数 */
    uint32_t total_clients;            /*!< 总连接数 */
    uint64_t frames_sent;              /*!< 发送帧数 */
    uint64_t bytes_sent;               /*!< 发送字节数 */
    float current_fps;                 /*!< 当前FPS */
} mjpeg_server_stats_t;

/**
 * @brief 获取服务器统计信息
 */
esp_err_t mjpeg_http_server_get_stats(mjpeg_http_server_handle_t handle,
                                     mjpeg_server_stats_t *stats);

/**
 * @brief 获取默认配置
 */
mjpeg_http_server_config_t mjpeg_http_server_get_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* _MJPEG_HTTP_SERVER_H_ */