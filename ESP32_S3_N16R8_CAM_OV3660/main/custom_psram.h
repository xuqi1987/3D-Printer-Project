/**
 * @file custom_psram.h
 * @brief PSRAM处理的简化接口
 * 
 * 为ESP32-S3系列提供统一的PSRAM访问API
 */

#ifndef _CUSTOM_PSRAM_H_
#define _CUSTOM_PSRAM_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_system.h"
#include "esp_heap_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 检查PSRAM是否初始化
 * @return 如果PSRAM已初始化，返回true；否则返回false
 */
static inline bool custom_psram_is_available(void) {
    // 使用heap_caps_get_largest_free_block来检测PSRAM是否可用
    size_t psram_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    return (psram_size > 0);
}

/**
 * @brief 获取PSRAM大小
 * @return PSRAM大小（字节）
 */
static inline size_t custom_psram_get_size(void) {
    // 使用heap_caps_get_total_size获取PSRAM总大小
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

#ifdef __cplusplus
}
#endif

#endif /* _CUSTOM_PSRAM_H_ */ 