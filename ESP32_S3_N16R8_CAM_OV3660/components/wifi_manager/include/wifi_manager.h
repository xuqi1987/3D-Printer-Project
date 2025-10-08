/**
 * @file wifi_manager.h
 * @brief WiFi连接管理器模块
 * 
 * 提供WiFi连接、断开、状态监控等功能的统一接口
 * 
 * @author ESP32 Team
 * @date 2025
 */

#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi连接状态
 */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,      /*!< 未连接 */
    WIFI_STATE_CONNECTING,             /*!< 连接中 */
    WIFI_STATE_CONNECTED,              /*!< 已连接 */
    WIFI_STATE_DISCONNECTING,          /*!< 断开连接中 */
    WIFI_STATE_ERROR                   /*!< 错误状态 */
} wifi_state_t;

/**
 * @brief WiFi配置结构体
 */
typedef struct {
    char ssid[32];                     /*!< WiFi SSID */
    char password[64];                 /*!< WiFi密码 */
    uint8_t max_retry;                 /*!< 最大重试次数 */
    uint32_t retry_delay_ms;           /*!< 重试延迟时间(毫秒) */
    bool auto_reconnect;               /*!< 是否自动重连 */
} wifi_manager_config_t;

/**
 * @brief WiFi连接信息
 */
typedef struct {
    char ip_addr[16];                  /*!< IP地址 */
    char netmask[16];                  /*!< 子网掩码 */
    char gateway[16];                  /*!< 网关 */
    int8_t rssi;                       /*!< 信号强度 */
    uint8_t channel;                   /*!< 信道 */
} wifi_connection_info_t;

/**
 * @brief WiFi事件回调函数类型
 * 
 * @param state WiFi状态
 * @param info 连接信息(仅在连接成功时有效)
 */
typedef void (*wifi_event_callback_t)(wifi_state_t state, const wifi_connection_info_t *info);

/* ==================== 函数声明 ==================== */

/**
 * @brief WiFi管理器初始化
 * 
 * @param config WiFi配置
 * @param event_cb 事件回调函数
 * @return esp_err_t 
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config, wifi_event_callback_t event_cb);

/**
 * @brief WiFi管理器去初始化
 * 
 * @return esp_err_t 
 *         - ESP_OK: 去初始化成功
 *         - ESP_ERR_INVALID_STATE: 状态无效
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief 连接WiFi
 * 
 * @return esp_err_t 
 *         - ESP_OK: 连接启动成功
 *         - ESP_ERR_INVALID_STATE: WiFi未初始化
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief 断开WiFi连接
 * 
 * @return esp_err_t 
 *         - ESP_OK: 断开成功
 *         - ESP_ERR_INVALID_STATE: WiFi未连接
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief 获取WiFi连接状态
 * 
 * @return wifi_state_t 当前WiFi状态
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief 获取WiFi连接信息
 * 
 * @param info 输出连接信息
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_INVALID_STATE: WiFi未连接
 */
esp_err_t wifi_manager_get_connection_info(wifi_connection_info_t *info);

/**
 * @brief 等待WiFi连接完成
 * 
 * @param timeout_ms 超时时间(毫秒)
 * @return esp_err_t 
 *         - ESP_OK: 连接成功
 *         - ESP_ERR_TIMEOUT: 超时
 *         - ESP_FAIL: 连接失败
 */
esp_err_t wifi_manager_wait_for_connection(uint32_t timeout_ms);

/**
 * @brief 设置自动重连
 * 
 * @param enable 是否启用自动重连
 * @return esp_err_t 
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_STATE: WiFi未初始化
 */
esp_err_t wifi_manager_set_auto_reconnect(bool enable);

/**
 * @brief 获取默认配置
 * 
 * @return wifi_manager_config_t 默认WiFi配置
 */
wifi_manager_config_t wifi_manager_get_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIFI_MANAGER_H_ */