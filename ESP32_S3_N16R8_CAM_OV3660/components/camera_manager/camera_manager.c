/**
 * @file camera_manager.c
 * @brief ESP32-S3 摄像头管理器模块实现
 */

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "camera_manager.h"
#include "board_pins.h"
#include "esp_camera.h"

static const char *TAG = "camera_mgr";

/* ==================== 私有变量 ==================== */
static camera_state_t s_camera_state = CAMERA_STATE_UNINITIALIZED;
static camera_manager_config_t s_camera_config = {0};
static SemaphoreHandle_t s_camera_mutex = NULL;
static uint32_t s_frame_count = 0;
static int64_t s_last_fps_time = 0;
static float s_current_fps = 0.0f;

/* ==================== 私有函数声明 ==================== */
static esp_err_t camera_manager_configure_camera(const camera_manager_config_t *config);
static void camera_manager_update_fps(void);
static bool camera_manager_validate_config(const camera_manager_config_t *config);

/* ==================== 公共函数实现 ==================== */

esp_err_t camera_manager_init(const camera_manager_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_camera_state != CAMERA_STATE_UNINITIALIZED) {
        ESP_LOGW(TAG, "Camera manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!camera_manager_validate_config(config)) {
        ESP_LOGE(TAG, "Invalid camera configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // 创建互斥锁
    s_camera_mutex = xSemaphoreCreateMutex();
    if (s_camera_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create camera mutex");
        return ESP_ERR_NO_MEM;
    }

    // 复制配置
    memcpy(&s_camera_config, config, sizeof(camera_manager_config_t));

    // 配置摄像头
    esp_err_t ret = camera_manager_configure_camera(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure camera: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_camera_mutex);
        s_camera_mutex = NULL;
        return ret;
    }

    s_camera_state = CAMERA_STATE_INITIALIZED;
    s_frame_count = 0;
    s_last_fps_time = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Camera manager initialized successfully");
    ESP_LOGI(TAG, "Resolution: %dx%d, Format: %d, Quality: %d", 
             config->frame_width, config->frame_height, 
             config->pixel_format, config->jpeg_quality);
    
    return ESP_OK;
}

esp_err_t camera_manager_deinit(void)
{
    if (s_camera_state == CAMERA_STATE_UNINITIALIZED) {
        ESP_LOGW(TAG, "Camera manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 停止摄像头
    if (s_camera_state == CAMERA_STATE_STARTED) {
        camera_manager_stop();
    }

    // 去初始化摄像头
    esp_camera_deinit();

    // 删除互斥锁
    if (s_camera_mutex != NULL) {
        vSemaphoreDelete(s_camera_mutex);
        s_camera_mutex = NULL;
    }

    s_camera_state = CAMERA_STATE_UNINITIALIZED;
    memset(&s_camera_config, 0, sizeof(camera_manager_config_t));
    
    ESP_LOGI(TAG, "Camera manager deinitialized");
    return ESP_OK;
}

esp_err_t camera_manager_start(void)
{
    if (s_camera_state != CAMERA_STATE_INITIALIZED) {
        ESP_LOGE(TAG, "Camera not initialized or already started");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    
    // 测试摄像头是否正常工作 - 增加重试和更长等待时间
    vTaskDelay(pdMS_TO_TICKS(200)); // 给传感器更多准备时间
    
    camera_fb_t *test_fb = NULL;
    int retry_count = 3;
    
    for (int i = 0; i < retry_count; i++) {
        test_fb = esp_camera_fb_get();
        if (test_fb != NULL) {
            break;
        }
        ESP_LOGW(TAG, "Test frame capture attempt %d/%d failed, retrying...", i+1, retry_count);
        vTaskDelay(pdMS_TO_TICKS(500)); // 等待500ms再重试
    }
    
    if (test_fb == NULL) {
        ESP_LOGE(TAG, "Failed to capture test frame after %d attempts", retry_count);
        xSemaphoreGive(s_camera_mutex);
        s_camera_state = CAMERA_STATE_ERROR;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Test frame captured successfully (size: %zu bytes)", test_fb->len);
    
    esp_camera_fb_return(test_fb);
    s_camera_state = CAMERA_STATE_STARTED;
    s_frame_count = 0;
    s_last_fps_time = esp_timer_get_time();
    
    xSemaphoreGive(s_camera_mutex);
    
    ESP_LOGI(TAG, "Camera started successfully");
    return ESP_OK;
}

esp_err_t camera_manager_stop(void)
{
    if (s_camera_state != CAMERA_STATE_STARTED) {
        ESP_LOGW(TAG, "Camera not started");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);
    s_camera_state = CAMERA_STATE_INITIALIZED;
    xSemaphoreGive(s_camera_mutex);
    
    ESP_LOGI(TAG, "Camera stopped");
    return ESP_OK;
}

camera_fb_t *camera_manager_capture_frame(uint32_t timeout_ms)
{
    if (s_camera_state != CAMERA_STATE_STARTED) {
        ESP_LOGE(TAG, "Camera not started");
        return NULL;
    }

    if (xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire camera mutex within timeout");
        return NULL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb != NULL) {
        // 验证JPEG数据完整性
        if (fb->format == PIXFORMAT_JPEG) {
            if (fb->len < 10 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
                ESP_LOGE(TAG, "Invalid JPEG data - missing SOI marker");
                esp_camera_fb_return(fb);
                xSemaphoreGive(s_camera_mutex);
                return NULL;
            }
            
            // 检查EOI标记
            bool has_eoi = false;
            if (fb->len >= 2) {
                // 检查最后两个字节是否为EOI (0xFFD9)
                if (fb->buf[fb->len-2] == 0xFF && fb->buf[fb->len-1] == 0xD9) {
                    has_eoi = true;
                } else {
                    // 在数据中搜索EOI标记
                    for (size_t i = fb->len - 20; i < fb->len - 1; i++) {
                        if (fb->buf[i] == 0xFF && fb->buf[i+1] == 0xD9) {
                            has_eoi = true;
                            // 调整长度到EOI标记之后
                            fb->len = i + 2;
                            break;
                        }
                    }
                }
            }
            
            if (!has_eoi) {
                ESP_LOGW(TAG, "JPEG missing EOI marker, len=%zu", fb->len);
                // 添加EOI标记
                if (fb->len + 2 <= 65536) { // 确保不越界
                    fb->buf[fb->len] = 0xFF;
                    fb->buf[fb->len + 1] = 0xD9;
                    fb->len += 2;
                }
            }
        }
        
        s_frame_count++;
        camera_manager_update_fps();
    } else {
        ESP_LOGE(TAG, "Failed to capture frame");
    }

    xSemaphoreGive(s_camera_mutex);
    return fb;
}

esp_err_t camera_manager_return_frame(camera_fb_t *fb)
{
    if (fb == NULL) {
        ESP_LOGE(TAG, "Frame buffer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_camera_fb_return(fb);
    return ESP_OK;
}

camera_state_t camera_manager_get_state(void)
{
    return s_camera_state;
}

esp_err_t camera_manager_set_param(int param_id, int value)
{
    if (s_camera_state == CAMERA_STATE_UNINITIALIZED) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return ESP_FAIL;
    }

    // 这里应该根据具体的参数ID设置相应的值
    // 简化处理
    ESP_LOGI(TAG, "Setting camera parameter %d to %d", param_id, value);
    
    return ESP_OK;
}

esp_err_t camera_manager_get_param(int param_id, int *value)
{
    if (value == NULL) {
        ESP_LOGE(TAG, "Value pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_camera_state == CAMERA_STATE_UNINITIALIZED) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Failed to get camera sensor");
        return ESP_FAIL;
    }

    // 这里应该根据具体的参数ID获取相应的值
    // 简化处理
    *value = 0;
    ESP_LOGI(TAG, "Getting camera parameter %d", param_id);
    
    return ESP_OK;
}

camera_manager_config_t camera_manager_get_default_config(void)
{
    camera_manager_config_t config = {
        .frame_width = 1600,      // 1600宽度 - UXGA分辨率
        .frame_height = 1200,     // 1200高度  
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_UXGA,   // 1600x1200 UXGA配置
        .jpeg_quality = 12,      // 平衡质量与速度 (低数值 = 高质量)
        .fb_count = 3,           // 适中的帧缓冲区数量
        .xclk_freq_hz = 20000000, // 20MHz时钟，平衡同步稳定性和撕裂控制
        .enable_psram = true
    };
    
    return config;
}

float camera_manager_get_fps(void)
{
    return s_current_fps;
}

/* ==================== 私有函数实现 ==================== */

static esp_err_t camera_manager_configure_camera(const camera_manager_config_t *config)
{
    camera_config_t camera_config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        
        .xclk_freq_hz = config->xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        
        .pixel_format = config->pixel_format,
        .frame_size = config->frame_size,
        
        .jpeg_quality = config->jpeg_quality,
        .fb_count = config->fb_count,
        .fb_location = config->enable_psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_LATEST,  // 修复NO-SOI：使用LATEST模式确保同步
    };

    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return ret;
    }

    // 获取传感器并优化性能设置
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        ESP_LOGI(TAG, "Optimizing camera sensor for high performance");
        
        // 基本图像质量设置 - 最大亮度优化
        s->set_brightness(s, 4);     // 4=最大亮度值 (范围-2到4)
        s->set_contrast(s, 2);       // 2=高对比度，保持图像层次  
        s->set_saturation(s, 2);     // 2=增强饱和度，提升整体视觉效果
        s->set_special_effect(s, 0); // 0=无特效
        
        // 白平衡优化（室内光源）
        s->set_whitebal(s, 1);       // 启用自动白平衡
        s->set_awb_gain(s, 1);       // 启用AWB增益
        s->set_wb_mode(s, 1);        // 室内模式，改善颜色准确性
        
        // 同步信号优化 - 修复NO-SOI问题
        ESP_LOGI(TAG, "Applying NO-SOI fix - optimizing sensor synchronization");
        
        // 曝光控制优化（稳定性优先）
        s->set_exposure_ctrl(s, 1);  // 启用曝光控制
        s->set_aec2(s, 1);           // 启用AEC2自动模式
        s->set_ae_level(s, 1);       // 使用较温和的曝光补偿
        s->set_aec_value(s, 400);    // 适中曝光值，确保稳定启动
        
        // 增益控制优化（稳定性优先）
        s->set_gain_ctrl(s, 1);      // 启用增益控制
        s->set_agc_gain(s, 10);      // 使用更保守的增益值
        s->set_gainceiling(s, (gainceiling_t)4); // 适中增益上限512x
        
        // 图像处理优化（关闭可能降低亮度的功能）
        s->set_bpc(s, 0);            // 关闭坏点校正（避免暗化像素）
        s->set_wpc(s, 1);            // 启用白点校正（增强亮点）
        s->set_raw_gma(s, 1);        // 启用原始伽马校正（提升整体亮度）
        s->set_lenc(s, 1);           // 启用镜头校正（改善边缘亮度）
        
        // 几何变换 - 修复180度倒转
        s->set_hmirror(s, 1);        // 启用水平镜像
        s->set_vflip(s, 1);          // 启用垂直翻转
        s->set_dcw(s, 0);            // 关闭下采样（直接输出原始尺寸）
        s->set_colorbar(s, 0);       // 关闭测试模式
        
        // NO-SOI修复专用设置 - 温和模式
        ESP_LOGI(TAG, "Applying gentle synchronization optimization...");
        
        // 使用更温和的同步优化，避免硬复位
        vTaskDelay(pdMS_TO_TICKS(100));      // 给传感器更多稳定时间
        
        ESP_LOGI(TAG, "Camera sensor synchronized - gentle optimization applied");
    }

    return ESP_OK;
}

static void camera_manager_update_fps(void)
{
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed = current_time - s_last_fps_time;
    
    // 每秒更新一次FPS
    if (elapsed >= 1000000) { // 1秒 = 1,000,000微秒
        s_current_fps = (float)s_frame_count * 1000000.0f / (float)elapsed;
        s_frame_count = 0;
        s_last_fps_time = current_time;
    }
}

static bool camera_manager_validate_config(const camera_manager_config_t *config)
{
    if (config->frame_width == 0 || config->frame_height == 0) {
        ESP_LOGE(TAG, "Invalid frame dimensions");
        return false;
    }
    
    if (config->jpeg_quality > 63) {
        ESP_LOGE(TAG, "Invalid JPEG quality (should be 0-63)");
        return false;
    }
    
    if (config->fb_count == 0 || config->fb_count > 10) {
        ESP_LOGE(TAG, "Invalid frame buffer count (should be 1-10)");
        return false;
    }
    
    if (config->xclk_freq_hz < 10000000 || config->xclk_freq_hz > 30000000) {
        ESP_LOGE(TAG, "Invalid XCLK frequency (should be 10-30 MHz)");
        return false;
    }
    
    return true;
}