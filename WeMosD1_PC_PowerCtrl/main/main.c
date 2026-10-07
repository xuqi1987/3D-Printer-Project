#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "http_server.h"
#include "power.h"

static const char *TAG = "APP";

/**
 * Status LED blink task.
 *   - slow blink (1s on / 1s off)  = WiFi disconnected
 *   - solid ON                     = WiFi connected, idle
 *   - quick blink                  = power action just fired
 */
static void status_led_task(void *arg)
{
    const int led = CONFIG_STATUS_LED_GPIO;
    bool last_state = false;
    TickType_t last_blink = 0;

    while (1) {
        bool connected = wifi_is_connected();
        if (connected != last_state) {
            gpio_set_level(led, connected ? 1 : 0);
            last_state = connected;
        }
        if (!connected) {
            TickType_t now = xTaskGetTickCount();
            if (now - last_blink > pdMS_TO_TICKS(1000)) {
                gpio_set_level(led, 1 - gpio_get_level(led));
                last_blink = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // Power control GPIO — idle LOW
    gpio_reset_pin(CONFIG_POWER_GPIO);
    gpio_set_direction(CONFIG_POWER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_POWER_GPIO, 0);

    // Status LED
    gpio_reset_pin(CONFIG_STATUS_LED_GPIO);
    gpio_set_direction(CONFIG_STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_STATUS_LED_GPIO, 0);

    ESP_LOGI(TAG, "=== WeMos D1 PC Power Control ===");
    ESP_LOGI(TAG, "Power GPIO: %d | Status LED GPIO: %d", CONFIG_POWER_GPIO, CONFIG_STATUS_LED_GPIO);

    // WiFi (non-blocking)
    wifi_init_sta();

    // Status LED task
    xTaskCreate(status_led_task, "status_led", 1024, NULL, 1, NULL);

    // HTTP server
    http_server_start(power_press_short, power_press_long);
}
