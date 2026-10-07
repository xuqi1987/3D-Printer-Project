#include "wifi.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "WIFI";

static volatile bool s_connected = false;
static volatile int  s_retry     = 0;
#define MAX_RETRY 10

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry < MAX_RETRY) {
            ESP_LOGW(TAG, "retry %d / %d", ++s_retry, MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "WiFi connect failed after %d retries", MAX_RETRY);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry = 0;
        s_connected = true;
    }
}

bool wifi_is_connected(void)
{
    return s_connected;
}

void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, event_handler, NULL);

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid,     CONFIG_WIFI_SSID,     sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, connecting to %s...", CONFIG_WIFI_SSID);
}
