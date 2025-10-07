/**
 * @file camera_webserver_main.c
 * @brief ESP32-S3摄像头Web服务器
 * 
 * 这个程序实现了一个基于ESP32-S3的摄像头Web服务器，功能包括：
 * - OV2640摄像头图像采集
 * - WiFi网络连接
 * - HTTP视频流服务
 */

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_camera.h>
#include <esp_mac.h>
#include <board_pins.h>
#include <sys/param.h>
#include <string.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include "custom_psram.h"  // 自定义的PSRAM处理

static const char *TAG = "camera_webserver";

/* WiFi配置 */
#define WIFI_SSID      "Henry'sStudy"
#define WIFI_PASS      "xgm10503"
#define WIFI_RETRY_NUM  5

/* HTTP服务器句柄 */
static httpd_handle_t server = NULL;

/**
 * @brief 系统监控任务
 */
static void monitor_task(void *pvParameters)
{
    uint32_t check_count = 0;
    
    // 等待系统其他部分完全初始化
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "监控任务开始运行");
    
    // 输出初始系统状态
    ESP_LOGI(TAG, "系统内存: 总=%lu KB, 空闲=%lu KB", 
             esp_get_free_heap_size() / 1024,
             esp_get_minimum_free_heap_size() / 1024);
    
    while (1) {
        // 打印系统信息
        ESP_LOGI(TAG, "空闲堆内存: %lu KB", esp_get_free_heap_size() / 1024);
        
        // 只有每15次循环才检查一次摄像头状态 (约5分钟一次)
        if (check_count % 15 == 0) {
            ESP_LOGI(TAG, "检查摄像头状态...");
            
            // 尝试获取摄像头帧
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) {
                ESP_LOGI(TAG, "摄像头工作正常，帧大小: %zu 字节", fb->len);
                esp_camera_fb_return(fb);
            } else {
                ESP_LOGW(TAG, "无法捕获摄像头画面");
            }
        }
        
        check_count++;
        
        // 每20秒检查一次
        for (int i = 0; i < 20; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// HTML页面
static const char STREAM_HTML[] = 
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>ESP32-CAM 视频流</title>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
    "body{font-family:'Microsoft YaHei', '微软雅黑', Arial, sans-serif;text-align:center;margin:0px auto;padding:20px;}"
    "img.original-size{width:auto !important;height:auto !important;max-width:none !important;max-height:none !important;}" /* 实际尺寸模式 */
    "img.fit-screen{width:auto;max-width:100%;height:auto;}" /* 适应屏幕模式 */
    "img{border:1px solid #ccc;box-shadow:0 0 10px rgba(0,0,0,0.1);transition:all 0.3s ease;}"
    ".container{display:flex;flex-direction:column;align-items:center;}"
    ".controls{margin:15px 0;display:flex;flex-wrap:wrap;justify-content:center;gap:10px;}"
    ".stream-container{position:relative;margin:10px 0;overflow:auto;max-width:100%;max-height:70vh;}" /* 添加滚动条 */
    ".error{color:red;font-weight:bold;}"
    ".status{color:green;font-size:14px;margin-top:5px;}"
    ".info{font-size:14px;margin-top:10px;color:#555;}"
    "button{padding:10px 20px;margin:5px;cursor:pointer;background-color:#4CAF50;color:white;border:none;border-radius:4px;font-size:14px;}"
    "button:hover{background-color:#45a049;}"
    "button.active{background-color:#2E7D32;box-shadow:0 0 5px rgba(0,0,0,0.3);}"
    "h1{color:#333;margin-bottom:20px;}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>ESP32-CAM 视频监控</h1>"
    "<div class=\"stream-container\" id=\"streamContainer\">"
    "<img id=\"stream\" class=\"fit-screen\" src=\"/stream\" onerror=\"streamError()\" onload=\"streamLoaded()\"/>"
    "<p id=\"status\" class=\"status\"></p>"
    "</div>"
    "<div class=\"info\" id=\"resolution\"></div>"
    "<div class=\"controls\">"
    "<button id=\"originalSize\" onclick=\"setOriginalSize()\">实际尺寸</button>"
    "<button id=\"fitScreen\" onclick=\"setFitScreen()\" class=\"active\">适应屏幕</button>"
    "<button onclick=\"refreshStream()\">刷新视频流</button>"
    "</div>"
    "</div>"
    "<script>"
    "let errorCount = 0;"
    "const statusElem = document.getElementById('status');"
    "const streamElem = document.getElementById('stream');"
    "const resolutionInfo = document.getElementById('resolution');"
    "const originalSizeBtn = document.getElementById('originalSize');"
    "const fitScreenBtn = document.getElementById('fitScreen');"
    "const streamContainer = document.getElementById('streamContainer');"
    
    "function streamError() {"
    "  errorCount++;"
    "  statusElem.innerHTML = '加载错误 #' + errorCount + ' - 5秒后自动重试...';"
    "  statusElem.className = 'error';"
    "  setTimeout(refreshStream, 5000);"
    "}"
    
    "function streamLoaded() {"
    "  statusElem.innerHTML = '视频流已连接';"
    "  statusElem.className = 'status';"
    "  errorCount = 0;"
    "  // 显示图像分辨率信息"
    "  setTimeout(() => {"
    "    resolutionInfo.innerHTML = `图像分辨率: ${streamElem.naturalWidth} × ${streamElem.naturalHeight}`;"
    "  }, 500);"
    "}"
    
    "function setOriginalSize() {"
    "  streamElem.className = 'original-size';"
    "  originalSizeBtn.className = 'active';"
    "  fitScreenBtn.className = '';"
    "  streamContainer.style.overflow = 'auto';"
    "}"
    
    "function setFitScreen() {"
    "  streamElem.className = 'fit-screen';"
    "  fitScreenBtn.className = 'active';"
    "  originalSizeBtn.className = '';"
    "  streamContainer.style.overflow = 'hidden';"
    "}"
    
    "function refreshStream() {"
    "  statusElem.innerHTML = '正在连接...';"
    "  statusElem.className = 'status';"
    "  streamElem.src = '/stream?t=' + new Date().getTime();"
    "}"
    "</script>"
    "</body>"
    "</html>";

// 函数声明
static esp_err_t init_camera(void);
static esp_err_t init_wifi(void);
static esp_err_t start_webserver(void);
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);

/**
 * @brief WiFi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        static int retry_num = 0;
        if (retry_num < WIFI_RETRY_NUM) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/**
 * @brief 主页处理函数
 */
static esp_err_t index_handler(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
    httpd_resp_set_type(req, "text/html");
    ret = httpd_resp_send(req, STREAM_HTML, strlen(STREAM_HTML));
    return ret;
}

/**
 * @brief 视频流处理函数
 */
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t ret = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];

    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    ESP_LOGI(TAG, "Stream handler started");

    // 设置HTTP响应类型
    ret = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set HTTP response type: %d", ret);
        return ret;
    }

    // 添加HTTP响应头
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "X-Framerate", "high"); // 添加自定义标头表明这是高帧率流

    int frame_count = 0;
    int empty_count = 0;
    while (1) {
        // 获取摄像头帧
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            empty_count++;
            if (empty_count > 5) {
                ret = ESP_FAIL;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        empty_count = 0;

        // 检查图像数据
        if (fb->len == 0 || fb->buf == NULL) {
            ESP_LOGE(TAG, "Empty camera frame received");
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 检查图像格式
        if (fb->format != PIXFORMAT_JPEG) {
            ESP_LOGE(TAG, "Camera frame not in JPEG format: %d", fb->format);
            
            // 如果需要，可以在这里转换格式
            bool converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            
            if (!converted) {
                ESP_LOGE(TAG, "JPEG conversion failed");
                if (_jpg_buf) {
                    free(_jpg_buf);
                    _jpg_buf = NULL;
                }
                ret = ESP_FAIL;
                break;
            }
        } else {
            _jpg_buf = fb->buf;
            _jpg_buf_len = fb->len;
        }

        ESP_LOGD(TAG, "JPEG image size: %u bytes", _jpg_buf_len);

        // 发送多部分响应
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, "--frame\r\n", 8);
        }
        if (ret == ESP_OK) {
            // 添加分辨率信息到HTTP头，帮助前端确定原始尺寸
            int width = 0;
            int height = 0;
            
            // 获取当前摄像头分辨率
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                framesize_t framesize = s->status.framesize;
                // 根据framesize获取分辨率
                switch (framesize) {
                    case FRAMESIZE_QXGA: width = 2048; height = 1536; break;
                    case FRAMESIZE_HD: width = 1280; height = 720; break;
                    case FRAMESIZE_XGA: width = 1024; height = 768; break;
                    case FRAMESIZE_SVGA: width = 800; height = 600; break;
                    case FRAMESIZE_VGA: width = 640; height = 480; break;
                    case FRAMESIZE_QVGA: width = 320; height = 240; break;
                    case FRAMESIZE_CIF: width = 400; height = 296; break;
                    case FRAMESIZE_QQVGA: width = 160; height = 120; break;
                    default: width = 0; height = 0; break;
                }
            }
            
            snprintf(part_buf, sizeof(part_buf), 
                    "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld\r\nX-Resolution: %dx%d\r\n\r\n", 
                    _jpg_buf_len, esp_timer_get_time(), width, height);
            ret = httpd_resp_send_chunk(req, part_buf, strlen(part_buf));
        }
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, "\r\n", 2);
        }

        // 如果使用了转换缓冲区，释放它
        if (fb == NULL && _jpg_buf != NULL) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        
        // 释放摄像头帧缓冲区
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send frame: %d", ret);
            break;
        }

        // 计算帧率
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        
        frame_count++;
        if (frame_count % 10 == 0) {
            ESP_LOGI(TAG, "Stream running - MJPEG: %uKB, %lums (%.1ffps)",
                    (unsigned int)(_jpg_buf_len / 1024),
                    (unsigned long)frame_time,
                    1000.0 / (float)frame_time);
        }
        
        // 短暂延时以防止CPU过载
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // 确保清理所有资源
    if (fb) {
        esp_camera_fb_return(fb);
    }
    if (_jpg_buf != NULL && fb == NULL) {
        free(_jpg_buf);
    }

    ESP_LOGI(TAG, "Stream handler exit");
    return ret;
}

/**
 * @brief 启动Web服务器
 */
static esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // 启动HTTP服务器
    if (httpd_start(&server, &config) == ESP_OK) {
        // 设置URI处理函数
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        return ESP_OK;
    }

    return ESP_FAIL;
}

/**
 * @brief 初始化WiFi
 */
static esp_err_t init_wifi(void)
{
    esp_err_t ret = ESP_OK;
    
    // 初始化网络接口
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network interface");
        return ret;
    }
    
    // 创建默认事件循环
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop");
        return ret;
    }
    
    // 创建默认的wifi-sta netif
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA");
        return ESP_FAIL;
    }
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;
    cfg.wifi_task_core_id = 0;  // 设置WiFi任务在Core 0上运行
    
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return ret;
    }
    
    // 注册事件处理函数
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler");
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler");
        return ret;
    }
    
    // 配置WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // 设置WiFi模式
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return ret;
    }
    
    // 设置WiFi配置
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi configuration");
        return ret;
    }
    
    // 设置WiFi PS类型为NONE，避免省电模式
    ret = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi power save");
        return ret;
    }
    
    // 启动WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi");
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi initialization completed");
    return ret;
}

/**
 * @brief 初始化摄像头
 */
static esp_err_t init_camera(void) {
    camera_config_t config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,

        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer = LEDC_TIMER_0,
        .xclk_freq_hz = 24000000,
        
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QXGA,  // 提高分辨率到QXGA (2048x1536)
        .jpeg_quality = 1,            // 提高JPEG质量（值越小质量越好）
        .fb_count = 4,                 // 增加帧缓冲区数量
        .grab_mode = CAMERA_GRAB_LATEST,
        .fb_location = CAMERA_FB_IN_PSRAM  // 强制使用PSRAM
    };

    // 检查是否存在PSRAM
    bool psram_available = custom_psram_is_available();
    ESP_LOGI(TAG, "PSRAM status: %s", psram_available ? "Available" : "Not available");
    
    // 如果有PSRAM，调整配置
    if (psram_available) {
        config.jpeg_quality = 8;       // 更高的JPEG质量
        config.fb_count = 3;           // 更多的帧缓冲
        config.frame_size = FRAMESIZE_HD;  // 提升到HD分辨率 (1280x720)
        config.fb_location = CAMERA_FB_IN_PSRAM;
        ESP_LOGI(TAG, "Using PSRAM for camera buffer");
    } else {
        // 如果没有PSRAM，使用DRAM但保持较好的图像质量
        config.fb_location = CAMERA_FB_IN_DRAM;
        ESP_LOGI(TAG, "Using DRAM for camera buffer");
    }

    // 打印配置
    ESP_LOGI(TAG, "Camera configuration:");
    ESP_LOGI(TAG, "  - Resolution: %d", config.frame_size);
    ESP_LOGI(TAG, "  - JPEG quality: %d", config.jpeg_quality);
    ESP_LOGI(TAG, "  - FB count: %d", config.fb_count);
    ESP_LOGI(TAG, "  - Clock freq: %d Hz", config.xclk_freq_hz);

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }
    
    ESP_LOGI(TAG, "Camera init success");

    // 获取sensor对象并优化图像设置
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "Sensor ID: 0x%x (PID: 0x%x)", s->id.PID, s->id.VER);

        // 优化图像质量设置
        s->set_vflip(s, 1);        // 垂直翻转
        s->set_hmirror(s, 1);      // 水平镜像
        s->set_brightness(s, 1);    // 亮度调整
        s->set_contrast(s, 2);      // 增加对比度
        s->set_saturation(s, 2);    // 增加饱和度
        s->set_sharpness(s, 2);     // 增加锐度
        s->set_denoise(s, 1);       // 降噪
        s->set_quality(s, 10);      // 设置质量
        s->set_special_effect(s, 0); // 无特殊效果
        s->set_whitebal(s, 1);      // 启用自动白平衡
        s->set_awb_gain(s, 1);      // 启用自动白平衡增益
        s->set_wb_mode(s, 0);       // 自动白平衡模式
        s->set_exposure_ctrl(s, 1);  // 启用自动曝光
        s->set_aec2(s, 1);          // 启用自动曝光控制
        s->set_ae_level(s, 0);      // 自动曝光水平
        s->set_gain_ctrl(s, 1);     // 启用自动增益控制
        
        ESP_LOGI(TAG, "Camera sensor configured with optimized settings");
        
        // 测试摄像头
        vTaskDelay(pdMS_TO_TICKS(100));
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Camera test OK, captured %d bytes", fb->len);
        esp_camera_fb_return(fb);
    } else {
        ESP_LOGE(TAG, "Failed to get sensor object");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief 主函数
 */
void app_main(void)
{
    ESP_LOGI(TAG, "启动ESP32-S3摄像头Web服务器...");
    ESP_LOGI(TAG, "ESP-IDF版本: %s", esp_get_idf_version());
    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "MAC地址: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS初始化完成");

    // 检查PSRAM状态
    if (custom_psram_is_available()) {
        ESP_LOGI(TAG, "PSRAM已初始化，大小: %lu MB", 
            (unsigned long)(custom_psram_get_size() / 1024 / 1024));
    } else {
        ESP_LOGW(TAG, "PSRAM未初始化，将使用内部DRAM");
    }

    // 打印内存信息
    ESP_LOGI(TAG, "内存状态: 空闲=%lu KB", esp_get_free_heap_size() / 1024);

    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 初始化摄像头
    ESP_LOGI(TAG, "开始初始化摄像头...");
    ret = init_camera();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败，错误码: 0x%x", ret);
        return;
    }
    ESP_LOGI(TAG, "摄像头初始化完成");

    // 等待摄像头稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 初始化WiFi
    ESP_LOGI(TAG, "开始初始化WiFi...");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ret = init_wifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi初始化失败，错误码: 0x%x", ret);
        return;
    }
    ESP_LOGI(TAG, "WiFi初始化完成，正在连接...");

    // 等待WiFi连接稳定
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 启动Web服务器
    ESP_LOGI(TAG, "开始启动Web服务器...");
    ret = start_webserver();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web服务器启动失败，错误码: 0x%x", ret);
        return;
    }
    ESP_LOGI(TAG, "Web服务器启动完成");

    // 创建监控任务
    ESP_LOGI(TAG, "创建系统监控任务...");
    BaseType_t task_created = xTaskCreate(
        monitor_task,        // 任务函数
        "monitor_task",      // 任务名称
        4096,                // 堆栈大小
        NULL,                // 任务参数
        1,                   // 任务优先级 (降低优先级)
        NULL                 // 任务句柄
    );
    
    if (task_created != pdPASS) {
        ESP_LOGW(TAG, "监控任务创建失败");
    } else {
        ESP_LOGI(TAG, "监控任务创建成功");
    }
    
    // 打印启动完成信息
    ESP_LOGI(TAG, "系统启动完成 - 请访问 http://<device-ip>/ 查看视频流");
} 