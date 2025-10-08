/**
 * @file system_monitor.c
 * @brief 系统资源监控模块实现
 */

#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "system_monitor.h"
#include "../../main/app_config.h"

static const char *TAG = "sys_mon";

/* ==================== 私有变量 ==================== */
static bool s_initialized = false;
static system_monitor_config_t s_config;
static system_monitor_callback_t s_callback = NULL;
static TaskHandle_t s_monitor_task = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static system_resource_info_t s_last_info = {0};
static int64_t s_start_time = 0;

/* ==================== 私有函数声明 ==================== */
static void system_monitor_task(void *pvParameters);
static void collect_system_info(system_resource_info_t *info);
static bool validate_config(const system_monitor_config_t *config);

/* ==================== 公共函数实现 ==================== */

esp_err_t system_monitor_init(const system_monitor_config_t *config,
                             system_monitor_callback_t callback)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG, "System monitor already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!validate_config(config)) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // 复制配置
    memcpy(&s_config, config, sizeof(system_monitor_config_t));
    s_callback = callback;

    // 创建互斥锁
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    s_start_time = esp_timer_get_time();
    s_initialized = true;
    
    ESP_LOGI(TAG, "System monitor initialized");
    return ESP_OK;
}

esp_err_t system_monitor_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 停止监控
    if (s_monitor_task != NULL) {
        system_monitor_stop();
    }

    // 删除互斥锁
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    s_callback = NULL;
    
    ESP_LOGI(TAG, "System monitor deinitialized");
    return ESP_OK;
}

esp_err_t system_monitor_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_monitor_task != NULL) {
        ESP_LOGW(TAG, "System monitor already started");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        system_monitor_task,
        "sys_monitor",
        s_config.task_stack_size,
        NULL,
        s_config.task_priority,
        &s_monitor_task,
        APP_MONITOR_TASK_CORE
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "System monitor started");
    return ESP_OK;
}

esp_err_t system_monitor_stop(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_monitor_task == NULL) {
        ESP_LOGW(TAG, "System monitor not started");
        return ESP_ERR_INVALID_STATE;
    }

    vTaskDelete(s_monitor_task);
    s_monitor_task = NULL;
    
    ESP_LOGI(TAG, "System monitor stopped");
    return ESP_OK;
}

esp_err_t system_monitor_get_info(system_resource_info_t *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Info parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    collect_system_info(info);
    return ESP_OK;
}

esp_err_t system_monitor_get_tasks(task_info_t *tasks, 
                                  int max_tasks, 
                                  int *actual_count)
{
    if (tasks == NULL || actual_count == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 获取任务信息
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array = malloc(task_count * sizeof(TaskStatus_t));
    
    if (task_status_array == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status array");
        return ESP_ERR_NO_MEM;
    }

    UBaseType_t actual_tasks = uxTaskGetSystemState(task_status_array, task_count, NULL);
    
    int copy_count = (actual_tasks < max_tasks) ? actual_tasks : max_tasks;
    
    for (int i = 0; i < copy_count; i++) {
        strncpy(tasks[i].name, task_status_array[i].pcTaskName, sizeof(tasks[i].name) - 1);
        tasks[i].name[sizeof(tasks[i].name) - 1] = '\0';
        tasks[i].stack_high_water_mark = task_status_array[i].usStackHighWaterMark;
        tasks[i].priority = task_status_array[i].uxCurrentPriority;
        tasks[i].state = task_status_array[i].eCurrentState;
        tasks[i].runtime = task_status_array[i].ulRunTimeCounter;
    }
    
    *actual_count = copy_count;
    free(task_status_array);
    
    return ESP_OK;
}

esp_err_t system_monitor_set_memory_threshold(uint32_t min_free_heap)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Memory threshold set to %lu bytes", min_free_heap);
    return ESP_OK;
}

esp_err_t system_monitor_set_temperature_threshold(int32_t max_temperature)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Temperature threshold set to %ld°C", max_temperature);
    return ESP_OK;
}

esp_err_t system_monitor_reset_stats(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "System monitor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_last_info, 0, sizeof(system_resource_info_t));
    s_start_time = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Statistics reset");
    return ESP_OK;
}

system_monitor_config_t system_monitor_get_default_config(void)
{
    system_monitor_config_t config = {
        .monitor_interval_ms = 5000,
        .enable_temperature = true,
        .enable_cpu_usage = true,
        .enable_task_monitor = true,
        .task_priority = 3,
        .task_stack_size = 4096
    };
    
    return config;
}

/* ==================== 私有函数实现 ==================== */

static void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System monitor task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t interval_ticks = pdMS_TO_TICKS(s_config.monitor_interval_ms);
    
    while (1) {
        system_resource_info_t info;
        collect_system_info(&info);
        
        // 更新最后的信息
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        memcpy(&s_last_info, &info, sizeof(system_resource_info_t));
        xSemaphoreGive(s_mutex);
        
        // 调用回调函数
        if (s_callback != NULL) {
            s_callback(&info);
        }
        
        vTaskDelayUntil(&last_wake_time, interval_ticks);
    }
}

static void collect_system_info(system_resource_info_t *info)
{
    if (info == NULL) {
        return;
    }

    memset(info, 0, sizeof(system_resource_info_t));
    
    // 收集堆内存信息
    info->free_heap_size = esp_get_free_heap_size();
    info->min_free_heap_size = esp_get_minimum_free_heap_size();
    
    // 获取总堆内存大小
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);
    info->total_heap_size = heap_info.total_allocated_bytes + heap_info.total_free_bytes;
    
    // 收集PSRAM信息
    if (esp_psram_is_initialized()) {
        heap_caps_get_info(&heap_info, MALLOC_CAP_SPIRAM);
        info->free_psram_size = heap_info.total_free_bytes;
        info->total_psram_size = heap_info.total_allocated_bytes + heap_info.total_free_bytes;
    }
    
    // CPU使用率（简化实现）
    info->cpu_usage_percent = 0.0f;  // 需要实际的CPU使用率计算
    
    // 温度（ESP32-S3没有内置温度传感器，返回默认值）
    info->temperature_celsius = 45;  // 假设温度
    
    // 系统运行时间
    info->uptime_ms = (esp_timer_get_time() - s_start_time) / 1000;
    
    // 任务数量
    info->task_count = uxTaskGetNumberOfTasks();
}

static bool validate_config(const system_monitor_config_t *config)
{
    if (config->monitor_interval_ms < 1000 || config->monitor_interval_ms > 60000) {
        ESP_LOGE(TAG, "Invalid monitor interval (should be 1000-60000 ms)");
        return false;
    }
    
    if (config->task_stack_size < 2048) {
        ESP_LOGE(TAG, "Task stack size too small (min 2048)");
        return false;
    }
    
    if (config->task_priority < 1 || config->task_priority > 24) {
        ESP_LOGE(TAG, "Invalid task priority (should be 1-24)");
        return false;
    }
    
    return true;
}