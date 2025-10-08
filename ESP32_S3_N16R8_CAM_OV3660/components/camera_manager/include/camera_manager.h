/**
 * @file camera_manager.h
 * @brief ESP32-S3 摄像头管理器模块
 * 
 * 该模块提供摄像头的初始化、配置、图像采集等功能的统一接口
 * 
 * @author ESP32 Team
 * @date 2025
 */

#ifndef _CAMERA_MANAGER_H_
#define _CAMERA_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 摄像头管理器配置结构体
 */
typedef struct {
    uint16_t frame_width;           /*!< 图像宽度 */
    uint16_t frame_height;          /*!< 图像高度 */
    pixformat_t pixel_format;       /*!< 像素格式 */
    framesize_t frame_size;         /*!< 帧大小 */
    uint8_t jpeg_quality;           /*!< JPEG质量 (0-63, 数字越小质量越高) */
    uint8_t fb_count;               /*!< 帧缓冲区数量 */
    uint32_t xclk_freq_hz;          /*!< XCLK时钟频率 */
    bool enable_psram;              /*!< 是否启用PSRAM */
} camera_manager_config_t;

/**
 * @brief 摄像头状态枚举
 */
typedef enum {
    CAMERA_STATE_UNINITIALIZED = 0,    /*!< 未初始化 */
    CAMERA_STATE_INITIALIZED,          /*!< 已初始化 */
    CAMERA_STATE_STARTED,              /*!< 已启动 */
    CAMERA_STATE_ERROR                 /*!< 错误状态 */
} camera_state_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 摄像头管理器初始化
 * 
 * @param config 摄像头配置参数
 * @return esp_err_t 
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NO_MEM: 内存不足
 *         - ESP_FAIL: 其他错误
 */
esp_err_t camera_manager_init(const camera_manager_config_t *config);

/**
 * @brief 摄像头管理器去初始化
 * 
 * @return esp_err_t 
 *         - ESP_OK: 去初始化成功
 *         - ESP_ERR_INVALID_STATE: 状态无效
 */
esp_err_t camera_manager_deinit(void);

/**
 * @brief 启动摄像头
 * 
 * @return esp_err_t 
 *         - ESP_OK: 启动成功
 *         - ESP_ERR_INVALID_STATE: 摄像头未初始化
 *         - ESP_FAIL: 启动失败
 */
esp_err_t camera_manager_start(void);

/**
 * @brief 停止摄像头
 * 
 * @return esp_err_t 
 *         - ESP_OK: 停止成功
 *         - ESP_ERR_INVALID_STATE: 摄像头未启动
 */
esp_err_t camera_manager_stop(void);

/**
 * @brief 获取摄像头帧
 * 
 * @param timeout_ms 超时时间(毫秒)
 * @return camera_fb_t* 摄像头帧缓冲区指针，NULL表示失败
 */
camera_fb_t *camera_manager_capture_frame(uint32_t timeout_ms);

/**
 * @brief 释放摄像头帧缓冲区
 * 
 * @param fb 摄像头帧缓冲区指针
 * @return esp_err_t 
 *         - ESP_OK: 释放成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t camera_manager_return_frame(camera_fb_t *fb);

/**
 * @brief 获取摄像头状态
 * 
 * @return camera_state_t 摄像头当前状态
 */
camera_state_t camera_manager_get_state(void);

/**
 * @brief 设置摄像头参数
 * 
 * @param param_id 参数ID
 * @param value 参数值
 * @return esp_err_t 
 *         - ESP_OK: 设置成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_INVALID_STATE: 摄像头未初始化
 */
esp_err_t camera_manager_set_param(int param_id, int value);

/**
 * @brief 获取摄像头参数
 * 
 * @param param_id 参数ID
 * @param value 输出参数值
 * @return esp_err_t 
 *         - ESP_OK: 获取成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_INVALID_STATE: 摄像头未初始化
 */
esp_err_t camera_manager_get_param(int param_id, int *value);

/**
 * @brief 获取默认配置
 * 
 * @return camera_manager_config_t 默认摄像头配置
 */
camera_manager_config_t camera_manager_get_default_config(void);

/**
 * @brief 获取当前帧率统计
 * 
 * @return float 当前帧率 (FPS)
 */
float camera_manager_get_fps(void);

#ifdef __cplusplus
}
#endif

#endif /* _CAMERA_MANAGER_H_ */