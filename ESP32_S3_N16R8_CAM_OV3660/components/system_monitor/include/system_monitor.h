/**
 * @file system_monitor.h
 * @brief 系统资源监控模块
 * 
 * 提供内存使用、CPU负载、温度等系统资源的监控功能
 * 
 * @author ESP32 Team  
 * @date 2025
 */

#ifndef _SYSTEM_MONITOR_H_
#define _SYSTEM_MONITOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统资源信息结构体
 */
typedef struct {
    uint32_t free_heap_size;           /*!< 空闲堆内存大小(字节) */
    uint32_t min_free_heap_size;       /*!< 最小空闲堆内存大小(字节) */
    uint32_t total_heap_size;          /*!< 总堆内存大小(字节) */
    uint32_t free_psram_size;          /*!< 空闲PSRAM大小(字节) */
    uint32_t total_psram_size;         /*!< 总PSRAM大小(字节) */
    float cpu_usage_percent;           /*!< CPU使用率(百分比) */
    int32_t temperature_celsius;       /*!< 芯片温度(摄氏度) */
    uint64_t uptime_ms;                /*!< 系统运行时间(毫秒) */
    uint32_t task_count;               /*!< 当前任务数量 */
} system_resource_info_t;

/**
 * @brief 任务信息结构体
 */
typedef struct {
    char name[16];                     /*!< 任务名称 */
    uint32_t stack_high_water_mark;    /*!< 栈剩余空间 */
    uint8_t priority;                  /*!< 任务优先级 */
    uint8_t state;                     /*!< 任务状态 */
    uint32_t runtime;                  /*!< 运行时间 */
} task_info_t;

/**
 * @brief 系统监控配置
 */
typedef struct {
    uint32_t monitor_interval_ms;      /*!< 监控间隔(毫秒) */
    bool enable_temperature;           /*!< 是否启用温度监控 */
    bool enable_cpu_usage;             /*!< 是否启用CPU使用率监控 */
    bool enable_task_monitor;          /*!< 是否启用任务监控 */
    uint8_t task_priority;             /*!< 监控任务优先级 */
    size_t task_stack_size;            /*!< 监控任务栈大小 */
} system_monitor_config_t;

/**
 * @brief 系统事件回调函数类型
 * 
 * @param info 系统资源信息
 */
typedef void (*system_monitor_callback_t)(const system_resource_info_t *info);

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化系统监控模块
 * 
 * @param config 监控配置
 * @param callback 监控数据回调函数
 * @return esp_err_t 
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t system_monitor_init(const system_monitor_config_t *config,
                             system_monitor_callback_t callback);

/**
 * @brief 去初始化系统监控模块
 * 
 * @return esp_err_t 
 *         - ESP_OK: 去初始化成功
 *         - ESP_ERR_INVALID_STATE: 模块未初始化
 */
esp_err_t system_monitor_deinit(void);

/**
 * @brief 启动系统监控
 * 
 * @return esp_err_t 
 *         - ESP_OK: 启动成功
 *         - ESP_ERR_INVALID_STATE: 模块未初始化
 */
esp_err_t system_monitor_start(void);

/**
 * @brief 停止系统监控
 * 
 * @return esp_err_t 
 *         - ESP_OK: 停止成功
 *         - ESP_ERR_INVALID_STATE: 监控未启动
 */
esp_err_t system_monitor_stop(void);

/**
 * @brief 获取当前系统资源信息
 * 
 * @param info 输出系统资源信息
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t system_monitor_get_info(system_resource_info_t *info);

/**
 * @brief 获取所有任务信息
 * 
 * @param tasks 输出任务信息数组
 * @param max_tasks 数组最大长度
 * @param actual_count 实际任务数量
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t system_monitor_get_tasks(task_info_t *tasks, 
                                  int max_tasks, 
                                  int *actual_count);

/**
 * @brief 设置内存告警阈值
 * 
 * @param min_free_heap 最小空闲堆内存阈值(字节)
 * @return esp_err_t 
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t system_monitor_set_memory_threshold(uint32_t min_free_heap);

/**
 * @brief 设置温度告警阈值
 * 
 * @param max_temperature 最高温度阈值(摄氏度)
 * @return esp_err_t 
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t system_monitor_set_temperature_threshold(int32_t max_temperature);

/**
 * @brief 重置统计信息
 * 
 * @return esp_err_t 
 *         - ESP_OK: 重置成功
 *         - ESP_ERR_INVALID_STATE: 模块未初始化
 */
esp_err_t system_monitor_reset_stats(void);

/**
 * @brief 获取默认配置
 * 
 * @return system_monitor_config_t 默认监控配置
 */
system_monitor_config_t system_monitor_get_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYSTEM_MONITOR_H_ */