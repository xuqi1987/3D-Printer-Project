#ifndef __BOARD_PINS_H__
#define __BOARD_PINS_H__

// 电源和复位引脚
#define PWDN_GPIO_NUM     -1  // 不使用
#define RESET_GPIO_NUM    -1  // 不使用

// 时钟和控制引脚
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4   // SDA
#define SIOC_GPIO_NUM     5   // SCL

// 数据引脚
#define Y2_GPIO_NUM       11  // D0
#define Y3_GPIO_NUM       9   // D1
#define Y4_GPIO_NUM       8   // D2
#define Y5_GPIO_NUM       10  // D3
#define Y6_GPIO_NUM       12  // D4
#define Y7_GPIO_NUM       18  // D5
#define Y8_GPIO_NUM       17  // D6
#define Y9_GPIO_NUM       16  // D7

// 同步信号引脚
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

#endif /* __BOARD_PINS_H__ */ 