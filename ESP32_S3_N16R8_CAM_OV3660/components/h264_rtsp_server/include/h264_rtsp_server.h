/**
 * @file h264_rtsp_server.h
 * @brief H.264 RTSP流媒体服务器模块
 * 
 * 该模块实现了一个专用于H.264视频流传输的RTSP服务器
 * 符合RFC 2326 (RTSP)和RFC 6184 (H.264 RTP载荷格式)标准
 * 
 * @author ESP32 Team
 * @date 2025
 */

#ifndef _H264_RTSP_SERVER_H_
#define _H264_RTSP_SERVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RTSP服务器句柄类型
 */
typedef void* h264_rtsp_server_handle_t;

/**
 * @brief RTSP传输协议类型
 */
typedef enum {
    RTSP_TRANSPORT_UDP = 0,            /*!< UDP传输 */
    RTSP_TRANSPORT_TCP,                /*!< TCP传输 */
    RTSP_TRANSPORT_AUTO                /*!< 自动选择 */
} rtsp_transport_t;

/**
 * @brief RTSP服务器状态
 */
typedef enum {
    RTSP_SERVER_STATE_STOPPED = 0,    /*!< 已停止 */
    RTSP_SERVER_STATE_STARTING,       /*!< 启动中 */
    RTSP_SERVER_STATE_RUNNING,        /*!< 运行中 */
    RTSP_SERVER_STATE_STOPPING,       /*!< 停止中 */
    RTSP_SERVER_STATE_ERROR            /*!< 错误状态 */
} rtsp_server_state_t;

/**
 * @brief RTSP服务器配置
 */
typedef struct {
    uint16_t port;                     /*!< 服务器监听端口 */
    const char *stream_name;           /*!< 流名称 */
    uint8_t max_clients;               /*!< 最大客户端连接数 */
    uint8_t task_priority;             /*!< 服务器任务优先级 */
    size_t task_stack_size;            /*!< 服务器任务栈大小 */
    int core_id;                       /*!< 运行的CPU核心ID */
    rtsp_transport_t transport;        /*!< 传输协议类型 */
    uint16_t rtp_port_start;           /*!< RTP端口范围起始 */
    uint16_t rtp_port_end;             /*!< RTP端口范围结束 */
    uint32_t session_timeout_sec;      /*!< 会话超时时间(秒) */
    bool enable_test_mode;             /*!< 启用H.264测试模式 */
} h264_rtsp_server_config_t;

/**
 * @brief H.264帧类型
 */
typedef enum {
    H264_FRAME_TYPE_I = 0,             /*!< I帧(关键帧) */
    H264_FRAME_TYPE_P,                 /*!< P帧(预测帧) */
    H264_FRAME_TYPE_B,                 /*!< B帧(双向预测帧) */
    H264_FRAME_TYPE_UNKNOWN            /*!< 未知类型 */
} h264_frame_type_t;

/**
 * @brief H.264帧数据结构
 */
typedef struct {
    const uint8_t *data;               /*!< H.264帧数据 */
    size_t size;                       /*!< 数据大小 */
    h264_frame_type_t type;            /*!< 帧类型 */
    int64_t timestamp_us;              /*!< 时间戳(微秒) */
} h264_frame_t;

/**
 * @brief 客户端连接信息
 */
typedef struct {
    uint32_t client_id;                /*!< 客户端ID */
    char client_ip[16];                /*!< 客户端IP地址 */
    uint16_t client_port;              /*!< 客户端端口 */
    rtsp_transport_t transport;        /*!< 传输协议 */
    bool is_playing;                   /*!< 是否正在播放 */
} rtsp_client_info_t;

/**
 * @brief 服务器统计信息
 */
typedef struct {
    uint32_t total_clients;            /*!< 总连接客户端数 */
    uint32_t active_clients;           /*!< 当前活跃客户端数 */
    uint64_t frames_sent;              /*!< 已发送帧数 */
    uint64_t frames_dropped;           /*!< 丢弃帧数 (网络拥塞导致) */
    uint64_t bytes_sent;               /*!< 已发送字节数 */
    float current_fps;                 /*!< 当前帧率 */
} rtsp_server_stats_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化H.264 RTSP服务器
 * 
 * @param config 服务器配置
 * @param handle 输出服务器句柄
 * @return esp_err_t 
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t h264_rtsp_server_init(const h264_rtsp_server_config_t *config,
                                h264_rtsp_server_handle_t *handle);

/**
 * @brief 去初始化H.264 RTSP服务器
 * 
 * @param handle 服务器句柄
 * @return esp_err_t 
 *         - ESP_OK: 去初始化成功
 *         - ESP_ERR_INVALID_ARG: 句柄无效
 */
esp_err_t h264_rtsp_server_deinit(h264_rtsp_server_handle_t handle);

/**
 * @brief 启动RTSP服务器
 * 
 * @param handle 服务器句柄
 * @return esp_err_t 
 *         - ESP_OK: 启动成功
 *         - ESP_ERR_INVALID_ARG: 句柄无效
 *         - ESP_ERR_INVALID_STATE: 服务器状态无效
 */
esp_err_t h264_rtsp_server_start(h264_rtsp_server_handle_t handle);

/**
 * @brief 停止RTSP服务器
 * 
 * @param handle 服务器句柄
 * @return esp_err_t 
 *         - ESP_OK: 停止成功
 *         - ESP_ERR_INVALID_ARG: 句柄无效
 */
esp_err_t h264_rtsp_server_stop(h264_rtsp_server_handle_t handle);

/**
 * @brief 推送H.264帧到所有客户端
 * 
 * @param handle 服务器句柄
 * @param frame H.264帧数据
 * @return esp_err_t 
 *         - ESP_OK: 推送成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_INVALID_STATE: 服务器未运行
 */
esp_err_t h264_rtsp_server_push_frame(h264_rtsp_server_handle_t handle,
                                      const h264_frame_t *frame);

/**
 * @brief 获取服务器状态
 * 
 * @param handle 服务器句柄
 * @return rtsp_server_state_t 服务器状态
 */
rtsp_server_state_t h264_rtsp_server_get_state(h264_rtsp_server_handle_t handle);

/**
 * @brief 获取活跃客户端数量
 * 
 * @param handle 服务器句柄
 * @return int 活跃客户端数量，-1表示错误
 */
int h264_rtsp_server_get_client_count(h264_rtsp_server_handle_t handle);

/**
 * @brief 获取所有客户端信息
 * 
 * @param handle 服务器句柄
 * @param clients 输出客户端信息数组
 * @param max_clients 数组最大长度
 * @param actual_count 实际客户端数量
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t h264_rtsp_server_get_clients(h264_rtsp_server_handle_t handle,
                                       rtsp_client_info_t *clients,
                                       int max_clients,
                                       int *actual_count);

/**
 * @brief 获取服务器统计信息
 * 
 * @param handle 服务器句柄
 * @param stats 输出统计信息
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t h264_rtsp_server_get_stats(h264_rtsp_server_handle_t handle,
                                     rtsp_server_stats_t *stats);

/**
 * @brief 断开指定客户端
 * 
 * @param handle 服务器句柄
 * @param client_id 客户端ID
 * @return esp_err_t 
 *         - ESP_OK: 断开成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NOT_FOUND: 客户端不存在
 */
esp_err_t h264_rtsp_server_disconnect_client(h264_rtsp_server_handle_t handle,
                                             uint32_t client_id);

/**
 * @brief 获取默认配置
 * 
 * @return h264_rtsp_server_config_t 默认服务器配置
 */
h264_rtsp_server_config_t h264_rtsp_server_get_default_config(void);

/**
 * @brief 启用H.264测试模式
 * 
 * 在测试模式下，服务器将生成合成的H.264帧用于测试RTP实现
 * 
 * @param handle 服务器句柄
 * @param enable 是否启用测试模式
 * @return esp_err_t 
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 句柄无效
 */
esp_err_t h264_rtsp_server_set_test_mode(h264_rtsp_server_handle_t handle, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* _H264_RTSP_SERVER_H_ */