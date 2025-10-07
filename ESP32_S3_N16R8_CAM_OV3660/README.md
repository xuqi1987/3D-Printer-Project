| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# ESP32-S3 OV2640 摄像头驱动

这是一个基于 ESP32-S3 的 OV2640 摄像头驱动程序，提供了完整的摄像头初始化、图像采集和参数配置功能。

## 功能特点

- 支持多种图像格式：
  - RGB565（默认）
  - JPEG
- 支持多种分辨率：
  - QQVGA (160x120)
  - QVGA (320x240)
  - VGA (640x480)
- 可配置的 JPEG 压缩质量（0-63）
- 支持帧缓存管理
- 支持 SCCB（I2C）接口配置
- 支持硬件 XCLK 时钟生成

## 引脚定义

### 数据引脚
| 引脚名称 | GPIO编号 | 功能描述 |
|---------|---------|---------|
| D0/Y2   | GPIO11  | 数据位0 |
| D1/Y3   | GPIO9   | 数据位1 |
| D2/Y4   | GPIO8   | 数据位2 |
| D3/Y5   | GPIO10  | 数据位3 |
| D4/Y6   | GPIO12  | 数据位4 |
| D5/Y7   | GPIO18  | 数据位5 |
| D6/Y8   | GPIO17  | 数据位6 |
| D7/Y9   | GPIO16  | 数据位7 |
| PCLK    | GPIO13  | 像素时钟 |
| VSYNC   | GPIO6   | 垂直同步 |
| HREF    | GPIO7   | 水平参考 |

### 控制引脚
| 引脚名称 | GPIO编号 | 功能描述 |
|---------|---------|---------|
| XCLK    | GPIO15  | 系统时钟 |
| RESET   | NC      | 复位控制（未使用） |
| PWDN    | NC      | 掉电控制（未使用） |

### SCCB (I2C) 接口
| 引脚名称 | GPIO编号 | 功能描述 |
|---------|---------|---------|
| SIOD    | GPIO4   | SCCB 数据线 |
| SIOC    | GPIO5   | SCCB 时钟线 |

## 使用方法

1. 包含必要的头文件：
```c
#include "ov2640.h"
#include "board_pins.h"
```

2. 定义摄像头配置：
```c
ov2640_config_t camera_config = {
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
    .xclk_freq_hz = 20000000,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 10,
    .fb_count = 2
};
```

3. 初始化摄像头：
```c
esp_err_t ret = ov2640_init(&camera_config);
if (ret != ESP_OK) {
    // 处理错误
}
```

4. 获取图像：
```c
camera_fb_t *fb = NULL;
ret = ov2640_get_frame(&fb);
if (ret == ESP_OK && fb) {
    // 处理图像数据
    // fb->buf - 图像数据
    // fb->len - 数据长度
    // fb->width - 图像宽度
    // fb->height - 图像高度
    // fb->format - 像素格式
}
```

5. 配置参数：
```c
// 设置帧大小
ov2640_set_frame_size(FRAMESIZE_VGA);

// 设置像素格式
ov2640_set_pixel_format(PIXFORMAT_JPEG);

// 设置 JPEG 质量（仅在 JPEG 模式下有效）
ov2640_set_jpeg_quality(10);
```

6. 清理资源：
```c
ov2640_deinit();
```

## 注意事项

1. 确保所有引脚连接正确，特别是数据总线和时钟信号。
2. XCLK 频率建议设置在 10-20MHz 范围内。
3. 在使用 JPEG 格式时，建议根据实际需求调整压缩质量。
4. 使用 RGB565 格式时，确保有足够的内存存储图像数据。
5. 在高分辨率模式下，建议使用 DMA 传输以提高性能。

## 错误处理

驱动程序使用 ESP-IDF 的错误处理机制，可能返回以下错误码：

- `ESP_OK`: 操作成功
- `ESP_ERR_NO_MEM`: 内存分配失败
- `ESP_ERR_INVALID_STATE`: 驱动程序状态错误
- `ESP_ERR_NOT_FOUND`: 未检测到摄像头
- `ESP_FAIL`: 其他错误

## 依赖项

- ESP-IDF v5.4 或更高版本
- FreeRTOS
- ESP32-S3 芯片

# Hello World Example

Starts a FreeRTOS task to print "Hello World".

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## How to use example

Follow detailed instructions provided specifically for this example.

Select the instructions depending on Espressif chip installed on your development board:

- [ESP32 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)


## Example folder contents

The project **hello_world** contains one source file in C language [hello_world_main.c](main/hello_world_main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.
