// ST7789 240 x 240 display with no chip select line
#define USER_SETUP_ID 24

#define ST7789_DRIVER     // Configure all registers

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

//#define TFT_INVERSION_ON
#define TFT_INVERSION_OFF

// DSTIKE stepup
// #define TFT_DC    23
// #define TFT_RST   32
// #define TFT_MOSI  26
// #define TFT_SCLK  27

// Generic ESP32 setup
#define RP2040_PIO_CLK_DIV 1
#define TFT_SPI_PORT 1

#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS    13 // Not connected
#define TFT_DC    8
#define TFT_RST   9 // Connect reset to ensure display initialises

// For NodeMCU - use pin numbers in the form PIN_Dx where Dx is the NodeMCU pin designation
// #define TFT_CS   -1      // Define as not used
// #define TFT_DC   PIN_D1  // Data Command control pin
// #define TFT_RST  PIN_D4  // TFT reset pin (could connect to NodeMCU RST, see next line)
//#define TFT_RST  -1    // TFT reset pin connect to NodeMCU RST, must also then add 10K pull down to TFT SCK

// TFT_CS = 13   # 片选引脚 GP10 (Pin 14)
// TFT_RST = 9   # 复位引脚 GP9 (Pin 12)
// TFT_DC = 8    # 数据/命令引脚 GP8 (Pin 11)
// TFT_SDA = 11  # MOSI 引脚 GP11 (Pin 15)
// TFT_SCL = 10  # SCK 引脚 GP13 (Pin 17)

// #define TFT_CS   13     // Define as not used
// #define TFT_DC   8  // Data Command control pin
// #define TFT_RST  PIN_D4  // TFT reset pin (could connect to NodeMCU RST, see next line)
// #define TFT_RST  -1    // TFT reset pin connect to NodeMCU RST, must also then add 10K pull down to TFT SCK


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT


// #define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000

#define SPI_READ_FREQUENCY  20000000

#define SPI_TOUCH_FREQUENCY  2500000

// #define SUPPORT_TRANSACTIONS