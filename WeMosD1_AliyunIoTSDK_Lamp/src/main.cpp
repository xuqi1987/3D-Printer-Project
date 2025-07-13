#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <AliyunIoTSDK.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  

#define PRODUCT_KEY "a1HLgn66PGI"
#define DEVICE_NAME "v5W9Czzs7Xdo3aPF2mBZ"
#define DEVICE_SECRET "101a9303aa56d34596c3217f15269f00"
#define REGION_ID "cn-shanghai"

#define WIFI_TIMEOUT 120 // seconds to run for
// Which pin on the Arduino is connected to the NeoPixels?
#define PIXEL_PIN        2 
#define TRIGGER_PIN      0
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 6 // Popular NeoPixel ring size

#define MAX_DEALY 20

static WiFiClient espClient;
AliyunIoTSDK iot;

// 在文件顶部添加全局变量
bool powerState = false;

// 添加 powerState 的回调函数
void powerStateCallback(JsonVariant p)
{
    powerState = p["powerstate"];
    // 这里可以添加控制开关的代码
    if (powerState) {
        // 开启设备的代码
        Serial.println("设备已开启");
    } else {
        // 关闭设备的代码
        Serial.println("设备已关闭");
    }
    AliyunIoTSDK::send("powerstate", powerState);
}

// 在文件顶部添加
#define LED_PIN LED_BUILTIN  // 使用板载LED

// 修改 wifiInit 函数
void wifiInit()
{
    pinMode(LED_PIN, OUTPUT);
    
    WiFiManager wifiManager;  
    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);
    
    // 在连接过程中闪烁LED
    while (!wifiManager.autoConnect("ESP_Wifi_Lamp")) 
    {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
        Serial.println("尝试连接WiFi...");
    }
    
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("连接失败，超时");
        delay(3000);
        ESP.restart();
        delay(5000);
    }
    
    Serial.println("已连接到AP");
    digitalWrite(LED_PIN, HIGH);  // 连接成功后保持LED常亮
}

void setup()
{
    Serial.begin(9600);

    wifiInit();

    AliyunIoTSDK::begin(espClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, REGION_ID);

    // 绑定属性回调
    AliyunIoTSDK::bindData("powerstate", powerStateCallback);
    // AliyunIoTSDK::bindData("RGBColor", RGBColor);

    // 更新 powerState 并发送到云端
    powerState = false;
    AliyunIoTSDK::send("powerstate", powerState);
}

// 在 loop 函数中添加网络状态检查
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        // 如果WiFi断开，重新闪烁LED
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
    } else {
        AliyunIoTSDK::loop();
        //AliyunIoTSDK::send("MotionAlarmState", 0);
        delay(1000);
    }
}
