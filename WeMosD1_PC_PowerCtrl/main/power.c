#include "power.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "POWER";

/**
 * Drive the GPIO HIGH for the requested duration then back to LOW.
 * The MOSFET/relay module on the other side sees the rising edge as
 * "power button pressed" and shorts the motherboard PWR_BTN pins.
 */
static void pulse(int duration_ms)
{
    ESP_LOGW(TAG, "PWR_BTN pulse: %d ms", duration_ms);
    gpio_set_level(CONFIG_POWER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(CONFIG_POWER_GPIO, 0);
    ESP_LOGW(TAG, "PWR_BTN released");
}

void power_press_short(void)
{
    pulse(CONFIG_SHORT_PRESS_MS);
}

void power_press_long(void)
{
    pulse(CONFIG_LONG_PRESS_MS);
}
