#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

#define LED_PIN D2
#define LED_COUNT 12

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);
WiFiManager wifiManager;

bool isOn = false;
int currentMode = 0;
uint32_t currentColor = strip.Color(255, 255, 255);

// URLEncode 函数声明
String URLEncode(const char* msg);

// URLEncode 函数定义
String URLEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0') {
    if (('a' <= *msg && *msg <= 'z')
        || ('A' <= *msg && *msg <= 'Z')
        || ('0' <= *msg && *msg <= '9')
        || *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~') {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

// 函数声明
void handleRoot();
void handleControl();
void updateLights();
void rainbow();
void theaterChase(uint32_t color);
void breathe(uint32_t color);
void colorWipe(uint32_t color);
void rainbowBreath();
void timeBasedColor();

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("进入配置模式");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
  Serial.println("配置已保存");
  // 在这里可以添加保存自定义参数的代码
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();

  // 设置调试输出级别
  wifiManager.setDebugOutput(true);
  
  // 设置回调函数
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // 设置配置超时
  wifiManager.setConfigPortalTimeout(180);

  // WiFiManager配置
  if(!wifiManager.autoConnect("ESP8266_LED_Setup")) {
    Serial.println("连接失败并超时");
    // 重置并重试,或者进入深度睡
    ESP.reset();
    delay(1000);
  }

  // 设置mDNS响应器
  if (MDNS.begin("esp8266led")) {
    Serial.println("mDNS响应器已启动");
  }

  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.begin();

  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  
  // 打印设备可访问的URL
  Serial.println("您可��通过以下地址访问设备:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("或");
  Serial.println("http://esp8266led.local");
}

void loop() {
  MDNS.update();
  server.handleClient();
  updateLights();
}

void handleRoot() {
  String html = "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;}";
  html += "button{background-color:#4CAF50;border:none;color:white;padding:15px 32px;text-align:center;text-decoration:none;display:inline-block;font-size:16px;margin:4px 2px;cursor:pointer;}</style>";
  html += "</head><body>";
  html += "<h1>ESP8266 LED控制</h1>";
  html += "<p><button onclick='location.href=\"/control?action=toggle\"'>开关灯</button></p>";
  html += "<p>设置颜色: <input type='color' id='colorPicker' onchange='setColor(this.value)'></p>";
  html += "<p>模式: ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=1\"'>固定颜色</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=2\"'>彩虹</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=3\"'>流水</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=4\"'>呼吸</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=5\"'>颜色擦除</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=6\"'>彩色呼吸</button> ";
  html += "<button onclick='location.href=\"/control?action=mode&mode=7\"'>时间变色</button></p>";
  html += "<p><button onclick='location.href=\"/wifi\"'>WiFi设置</button></p>";
  html += "<script>function setColor(color) { location.href = '/control?action=color&hex=' + color.substring(1); }</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleControl() {
  String action = server.arg("action");
  
  if (action == "toggle") {
    isOn = !isOn;
  } else if (action == "color") {
    String hexColor = server.arg("hex");
    long number = strtol(hexColor.c_str(), NULL, 16);
    currentColor = strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
    currentMode = 1;
  } else if (action == "mode") {
    currentMode = server.arg("mode").toInt();
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void updateLights() {
  if (!isOn) {
    strip.clear();
    strip.show();
    return;
  }

  switch (currentMode) {
    case 1: // 固定颜色
      strip.fill(currentColor);
      break;
    case 2: // 彩虹
      rainbow();
      break;
    case 3: // 流水
      theaterChase(currentColor);
      break;
    case 4: // 呼吸
      breathe(currentColor);
      break;
    case 5: // 颜色擦除
      colorWipe(currentColor);
      break;
    case 6: // 彩色呼吸
      rainbowBreath();
      break;
    case 7: // 时间变色
      timeBasedColor();
      break;
  }
  strip.show();
}

// 彩虹效果
void rainbow() {
  static long firstPixelHue = 0;
  for(int i=0; i<strip.numPixels(); i++) {
    int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  firstPixelHue += 256;
  if(firstPixelHue >= 65536) {
    firstPixelHue = 0;
  }
  delay(10);
}

// 流水灯效果
void theaterChase(uint32_t color) {
  static int q = 0;
  for (int i = 0; i < strip.numPixels(); i += 3) {
    int pos = i + q;
    strip.setPixelColor(pos % strip.numPixels(), color);
  }
  for (int i = 0; i < strip.numPixels(); i += 3) {
    int pos = i + q - 1;
    if (pos < 0) pos += strip.numPixels();
    strip.setPixelColor(pos % strip.numPixels(), 0);
  }
  q++;
  if (q >= 3) q = 0;
  delay(50);
}

// 呼吸灯效果
void breathe(uint32_t color) {
  static int brightness = 0;
  static int fadeAmount = 5;
  
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(
      ((color >> 16) & 0xFF) * brightness / 255,
      ((color >> 8) & 0xFF) * brightness / 255,
      (color & 0xFF) * brightness / 255
    ));
  }
  
  brightness += fadeAmount;
  if(brightness <= 0 || brightness >= 255) {
    fadeAmount = -fadeAmount;
  }
  delay(20);
}

// 颜色擦除效果
void colorWipe(uint32_t color) {
  static int pixelIndex = 0;
  strip.setPixelColor(pixelIndex, color);
  pixelIndex = (pixelIndex + 1) % strip.numPixels();
  delay(50);
}

// 彩色呼吸灯效果
void rainbowBreath() {
  static int brightness = 0;
  static int fadeAmount = 5;
  static long hue = 0;

  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.ColorHSV(hue, 255, brightness));
  }

  brightness += fadeAmount;
  if(brightness <= 0 || brightness >= 255) {
    fadeAmount = -fadeAmount;
    if(brightness <= 0) {
      hue += 5000; // 改变色调
      if(hue >= 65536) hue = 0;
    }
  }
  delay(20);
}

// 基于时间的颜色变化
void timeBasedColor() {
  static unsigned long lastChange = 0;
  static int hue = 0;

  if (millis() - lastChange >= 1000) { // 每秒变化一次
    lastChange = millis();
    hue += 5461; // 大约每12秒循环一次所有颜色
    if(hue >= 65536) hue = 0;
    strip.fill(strip.ColorHSV(hue));
  }
}