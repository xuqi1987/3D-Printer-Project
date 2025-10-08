/**
 * @file wifi_manager.c
 * @brief WiFi连接管理器模块实现
 */

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_manager.h"

static const char *TAG = "wifi_mgr";

/* WiFi事件位定义 */
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

/* ==================== 私有变量 ==================== */
static wifi_state_t s_wifi_state = WIFI_STATE_DISCONNECTED;
static wifi_manager_config_t s_wifi_config = {0};
static wifi_event_callback_t s_event_callback = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static int s_retry_num = 0;
static bool s_initialized = false;

/* ==================== 私有函数声明 ==================== */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data);
static esp_err_t wifi_manager_setup_sta(void);
static void wifi_manager_notify_state_change(wifi_state_t new_state);
static bool wifi_manager_validate_config(const wifi_manager_config_t *config);

/* ==================== 公共函数实现 ==================== */

esp_err_t wifi_manager_init(const wifi_manager_config_t *config, wifi_event_callback_t event_cb)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!wifi_manager_validate_config(config)) {
        ESP_LOGE(TAG, "Invalid WiFi configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // 复制配置
    memcpy(&s_wifi_config, config, sizeof(wifi_manager_config_t));
    s_event_callback = event_cb;

    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建默认WiFi STA
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA");
        vEventGroupDelete(s_wifi_event_group);
        return ESP_FAIL;
    }

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        vEventGroupDelete(s_wifi_event_group);
        return ret;
    }

    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                             &ip_event_handler, NULL));

    // 设置WiFi模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // 优化WiFi性能 - 禁用省电模式以提高网络性能
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    // 设置WiFi带宽为40MHz (如果路由器支持)
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40));

    s_initialized = true;
    s_wifi_state = WIFI_STATE_DISCONNECTED;
    
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    ESP_LOGI(TAG, "SSID: %s, Max retry: %d", config->ssid, config->max_retry);
    
    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 断开连接
    if (s_wifi_state == WIFI_STATE_CONNECTED || s_wifi_state == WIFI_STATE_CONNECTING) {
        wifi_manager_disconnect();
    }

    // 注销事件处理器
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);

    // 去初始化WiFi
    esp_wifi_deinit();

    // 删除事件组
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_initialized = false;
    s_wifi_state = WIFI_STATE_DISCONNECTED;
    s_event_callback = NULL;
    
    ESP_LOGI(TAG, "WiFi manager deinitialized");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_wifi_state == WIFI_STATE_CONNECTED || s_wifi_state == WIFI_STATE_CONNECTING) {
        ESP_LOGW(TAG, "WiFi already connected or connecting");
        return ESP_OK;
    }

    // 配置WiFi连接
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strcpy((char*)wifi_config.sta.ssid, s_wifi_config.ssid);
    strcpy((char*)wifi_config.sta.password, s_wifi_config.password);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    s_retry_num = 0;
    s_wifi_state = WIFI_STATE_CONNECTING;
    wifi_manager_notify_state_change(s_wifi_state);

    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        s_wifi_state = WIFI_STATE_ERROR;
        wifi_manager_notify_state_change(s_wifi_state);
        return ret;
    }

    ESP_LOGI(TAG, "WiFi connection started");
    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_wifi_state == WIFI_STATE_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi already disconnected");
        return ESP_OK;
    }

    s_wifi_state = WIFI_STATE_DISCONNECTING;
    wifi_manager_notify_state_change(s_wifi_state);

    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi disconnection initiated");
    return ESP_OK;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

esp_err_t wifi_manager_get_connection_info(wifi_connection_info_t *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Info parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_wifi_state != WIFI_STATE_CONNECTED) {
        ESP_LOGE(TAG, "WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // 获取IP信息
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_sta_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
        return ret;
    }

    // 转换IP地址为字符串
    esp_ip4addr_ntoa(&ip_info.ip, info->ip_addr, sizeof(info->ip_addr));
    esp_ip4addr_ntoa(&ip_info.netmask, info->netmask, sizeof(info->netmask));
    esp_ip4addr_ntoa(&ip_info.gw, info->gateway, sizeof(info->gateway));

    // 获取WiFi信息
    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        info->rssi = ap_info.rssi;
        info->channel = ap_info.primary;
    } else {
        info->rssi = 0;
        info->channel = 0;
    }

    return ESP_OK;
}

esp_err_t wifi_manager_wait_for_connection(uint32_t timeout_ms)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_set_auto_reconnect(bool enable)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_wifi_config.auto_reconnect = enable;
    ESP_LOGI(TAG, "Auto reconnect %s", enable ? "enabled" : "disabled");
    
    return ESP_OK;
}

wifi_manager_config_t wifi_manager_get_default_config(void)
{
    wifi_manager_config_t config = {
        .ssid = "",
        .password = "",
        .max_retry = 5,
        .retry_delay_ms = 1000,
        .auto_reconnect = true
    };
    
    return config;
}

/* ==================== 私有函数实现 ==================== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < s_wifi_config.max_retry && s_wifi_config.auto_reconnect) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)", 
                     s_retry_num, s_wifi_config.max_retry);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_state = WIFI_STATE_DISCONNECTED;
            wifi_manager_notify_state_change(s_wifi_state);
            ESP_LOGE(TAG, "Failed to connect to AP");
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        
        s_retry_num = 0;
        s_wifi_state = WIFI_STATE_CONNECTED;
        wifi_manager_notify_state_change(s_wifi_state);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_manager_notify_state_change(wifi_state_t new_state)
{
    if (s_event_callback != NULL) {
        wifi_connection_info_t info = {0};
        if (new_state == WIFI_STATE_CONNECTED) {
            wifi_manager_get_connection_info(&info);
        }
        s_event_callback(new_state, &info);
    }
}

static bool wifi_manager_validate_config(const wifi_manager_config_t *config)
{
    if (strlen(config->ssid) == 0) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return false;
    }
    
    if (strlen(config->ssid) > 31) {
        ESP_LOGE(TAG, "SSID too long (max 31 characters)");
        return false;
    }
    
    if (strlen(config->password) > 63) {
        ESP_LOGE(TAG, "Password too long (max 63 characters)");
        return false;
    }
    
    if (config->max_retry == 0 || config->max_retry > 20) {
        ESP_LOGE(TAG, "Invalid max retry count (should be 1-20)");
        return false;
    }
    
    return true;
}