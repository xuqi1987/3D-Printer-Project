#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.h>

#define SCREEN_WIDTH 128 // OLED 显示宽度，单位：像素
#define SCREEN_HEIGHT 32  // OLED 显示高度，单位：像素
#define OLED_RESET     -1 // 重置引脚（如果共享 Arduino 重置引脚，则为 -1）

#define IR_RECEIVE_PIN 15 // 红外接收引脚

// 创建 SSD1306 对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 创建红外接收对象
IRrecv irrecv(IR_RECEIVE_PIN);
decode_results results;

void setup() {
  Serial.begin(9600);

  // 初始化 OLED 显示器
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // 如果初始化失败，进入死循环
  }

  display.clearDisplay(); // 清空显示器
  display.setTextSize(1); // 设置文本大小
  display.setTextColor(SSD1306_WHITE); // 设置文本颜色
  display.setCursor(0, 0); // 设置光标位置
  display.println(F("Waiting for IR signal...")); // 显示提示信息
  display.display(); // 更新显示器

  irrecv.enableIRIn(); // 开始接收红外信号
}

void loop() {
  if (irrecv.decode(&results)) {
    display.clearDisplay(); // 清空显示器
    display.setTextSize(1); // 设置文本大小
    display.setTextColor(SSD1306_WHITE); // 设置文本颜色
    display.setCursor(0, 0); // 设置光标位置
    display.print(F("IR Signal: ")); // 显示文本
    display.println(results.value, HEX); // 以十六进制格式显示红外信号值
    display.display(); // 更新显示器

    Serial.print(F("Received IR Signal: ")); // 在串口监视器输出
    Serial.println(results.value, HEX); // 输出红外信号值
    irrecv.resume(); // 准备接收下一个信号
  }
}