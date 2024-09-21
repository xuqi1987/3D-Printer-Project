#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Gree.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// 使用手动指定的 SDA 和 SCK 引脚初始化 OLED 显示屏
#define OLED_SDA 4  // SDA 引脚
#define OLED_SCK 5  // SCK 引脚

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const uint16_t kIrLed = 16;  // 红外发射器连接到 GPIO 16 引脚
IRGreeAC ac(kIrLed);  // 设置红外发射引脚

// 按钮引脚
#define BUTTON_POWER   12
#define BUTTON_TEMP_UP 13
#define BUTTON_TEMP_DOWN 14
#define BUTTON_MODE    15

// 红外人体传感器引脚
#define PIR_SENSOR_PIN 17  // 根据实际情况选择合适的引脚

// 函数声明
void printState();
void togglePower();
void increaseTemperature();
void decreaseTemperature();
void changeMode();
void displayCommand(String command);
void displaySensorState(bool state);

void setup() {
  // 初始化红外发射
  ac.begin();
  // 初始化串口调试
  Serial.begin(115200);
  delay(200);

  // 手动初始化 I2C
  Wire.begin(OLED_SDA, OLED_SCK);
  
  // 初始化 OLED 显示屏
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED 显示屏初始化失败"));
    for (;;);  // 停止执行
  }
  display.clearDisplay();
  
  // 初始化按钮引脚
  pinMode(BUTTON_POWER, INPUT_PULLUP);
  pinMode(BUTTON_TEMP_UP, INPUT_PULLUP);
  pinMode(BUTTON_TEMP_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_MODE, INPUT_PULLUP);

  // 初始化红外人体传感器引脚
  pinMode(PIR_SENSOR_PIN, INPUT);

  displayCommand("System Ready");
  Serial.println("初始状态设置完成，未发送任何控制信号。");
}

void loop() {
  if (digitalRead(BUTTON_POWER) == LOW) {
    togglePower();
  } else if (digitalRead(BUTTON_TEMP_UP) == LOW) {
    increaseTemperature();
  } else if (digitalRead(BUTTON_TEMP_DOWN) == LOW) {
    decreaseTemperature();
  } else if (digitalRead(BUTTON_MODE) == LOW) {
    changeMode();
  }

  // 检测红外人体传感器状态
  bool sensorState = digitalRead(PIR_SENSOR_PIN) == HIGH;
  displaySensorState(sensorState);

  if (sensorState) {
    Serial.println("检测到人体移动");
    // 自动打开空调
    if (!ac.getPower()) {
      togglePower();
    }
  }

  delay(200);  // 防止误触发
}

void togglePower() {
  if (ac.getPower()) {
    ac.off();
    displayCommand("Power Off");
  } else {
    ac.on();
    displayCommand("Power On");
  }
  ac.send();
  printState();
}

void increaseTemperature() {
  ac.setTemp(ac.getTemp() + 1);
  ac.send();
  displayCommand("Temp Up");
  printState();
}

void decreaseTemperature() {
  ac.setTemp(ac.getTemp() - 1);
  ac.send();
  displayCommand("Temp Down");
  printState();
}

void changeMode() {
  uint8_t currentMode = ac.getMode();
  currentMode = (currentMode + 1) % 5;  // 循环切换模式
  ac.setMode(currentMode);
  ac.send();
  displayCommand("Mode Change");
  printState();
}

void printState() {
  // 显示当前空调状态
  Serial.println("GREE A/C 遥控器状态:");
  Serial.printf("  %s\n", ac.toString().c_str());
  // 显示编码的红外信号
  unsigned char* ir_code = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kGreeStateLength; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}

void displayCommand(String command) {
  // 在 OLED 显示屏上显示命令状态
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Command Sent:");
  display.println(command);
  display.display();  // 刷新显示内容
}

void displaySensorState(bool state) {
  // 在 OLED 显示屏上显示传感器状态（高或低）
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);  // 在屏幕中间显示状态
  if (state) {
    display.println("High");
  } else {
    display.println("Low");
  }
  display.display();  // 刷新显示内容
}