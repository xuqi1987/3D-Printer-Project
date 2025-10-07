#ifndef __OV3660_H__
#define __OV3660_H__

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/ledc.h"
#include "esp_camera.h"  // 使用 ESP32 Camera 驱动的定义

// OV3660 寄存器地址
#define OV3660_ADDR          0x3C    // OV3660 I2C 地址
#define SCCB_FREQ           100000  // SCCB 频率 100KHz
#define WRITE_BIT           0x00    // I2C 写位
#define READ_BIT            0x01    // I2C 读位
#define ACK_CHECK_EN        0x1     // I2C 主机检查从机的 ACK
#define ACK_CHECK_DIS       0x0     // I2C 主机不检查从机的 ACK
#define ACK_VAL             0x0     // I2C ACK 值
#define NACK_VAL            0x1     // I2C NACK 值

// 使用 esp_camera.h 中的类型定义
typedef camera_fb_t fb_t;
typedef camera_sensor_info_t sensor_info_t;
typedef camera_pixformat_t pixformat_t;
typedef camera_framesize_t framesize_t;

// OV3660 配置结构体
typedef struct {
    int pin_pwdn;                  // 掉电控制引脚
    int pin_reset;                 // 复位控制引脚
    int pin_xclk;                  // XCLK 引脚
    int pin_sccb_sda;              // SCCB 数据线
    int pin_sccb_scl;              // SCCB 时钟线
    int pin_d0;                    // 数据位0
    int pin_d1;                    // 数据位1
    int pin_d2;                    // 数据位2
    int pin_d3;                    // 数据位3
    int pin_d4;                    // 数据位4
    int pin_d5;                    // 数据位5
    int pin_d6;                    // 数据位6
    int pin_d7;                    // 数据位7
    int pin_vsync;                 // 垂直同步
    int pin_href;                  // 水平参考
    int pin_pclk;                  // 像素时钟
    int xclk_freq_hz;              // XCLK 频率

    ledc_channel_t ledc_channel;    // LEDC 通道
    ledc_timer_t ledc_timer;        // LEDC 定时器

    pixformat_t pixel_format;       // 像素格式
    framesize_t frame_size;         // 帧大小
    int jpeg_quality;               // JPEG 质量 0-63
    int fb_count;                   // 帧缓冲区数量
    camera_grab_mode_t grab_mode;   // 抓取模式
    camera_fb_location_t fb_location; // 帧缓冲区位置
} camera_config_t;

/**
 * @brief 初始化 OV3660 摄像头
 * 
 * @param config 摄像头配置参数
 * @return esp_err_t 
 */
esp_err_t camera_init(const camera_config_t* config);

/**
 * @brief 获取一帧图像
 * 
 * @param fb 帧缓冲区指针的指针
 * @return esp_err_t 
 */
esp_err_t camera_get_frame(camera_fb_t** fb);

/**
 * @brief 返回帧缓冲区
 * 
 * @param fb 帧缓冲区指针
 * @return esp_err_t 
 */
esp_err_t camera_return_frame(camera_fb_t* fb);

/**
 * @brief 反初始化摄像头
 * 
 * @return esp_err_t 
 */
esp_err_t camera_deinit(void);

#endif /* __OV3660_H__ */ 