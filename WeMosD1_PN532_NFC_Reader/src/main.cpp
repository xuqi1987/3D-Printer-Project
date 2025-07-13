#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"
#include "PN532.h"

/*
 * 硬件连接说明：
 * WeMos D1 Mini <-> PN532 NFC模块
 * D1 (GPIO5)  -> SDA
 * D2 (GPIO4)  -> SCL
 * 3.3V        -> VCC
 * GND         -> GND
 * 
 * 注意：
 * 1. PN532模块需要3.3V供电，不要使用5V
 * 2. 如果使用I2C通信，需要确保模块上的跳线帽设置正确
 * 3. 如果遇到通信问题，可以尝试降低I2C时钟频率
 * 4. PN532模块上的DIP开关设置：
 *    Switch 1: ON
 *    Switch 2: OFF
 */

// 定义引脚
#define SDA_PIN D1  // GPIO5
#define SCL_PIN D2  // GPIO4

// 全局变量
String lastTagId = "";
unsigned long lastTagTime = 0;
const unsigned long TAG_TIMEOUT = 2000; // 2秒内不重复读取同一标签

// 创建PN532对象
PN532 nfc(SDA_PIN, SCL_PIN);  // SDA, SCL

// 函数声明
void saveConfigCallback();
bool sendData(const char* data);

void setup() {
    Serial.begin(115200);
    Serial.println("\n初始化NFC读卡器...");
    
    // 初始化I2C
    Serial.println("初始化I2C...");
    Serial.print("SDA引脚: GPIO");
    Serial.println(D1);
    Serial.print("SCL引脚: GPIO");
    Serial.println(D2);
    
    Wire.begin(D1, D2);
    Wire.setClock(100000); // 降低I2C时钟频率以提高稳定性
    Serial.println("I2C初始化完成");
    
    // 扫描I2C设备
    Serial.println("扫描I2C设备...");
    bool foundPN532 = false;
    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("找到I2C设备，地址: 0x");
            Serial.println(addr, HEX);
            if (addr == PN532_I2C_ADDRESS) {
                foundPN532 = true;
            }
        }
    }
    
    if (!foundPN532) {
        Serial.println("未找到PN532设备！请检查：");
        Serial.println("1. DIP开关设置是否正确（I2C模式：Switch 1=ON, Switch 2=OFF）");
        Serial.println("2. 接线是否正确（SDA->D1, SCL->D2）");
        Serial.println("3. 电源是否正常（3.3V-5V）");
        while (1) {
            delay(1000);
        }
    }
    
    // 初始化PN532
    if (!nfc.begin()) {
        Serial.println("PN532初始化失败！");
        while (1) {
            delay(1000);
        }
    }
    
    // 配置SAM
    if (!nfc.SAMConfig()) {
        Serial.println("SAM配置失败！");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("NFC读卡器初始化完成！");
    
    // 初始化WiFiManager
    WiFiManager wifiManager;
    wifiManager.setTitle("NFC读卡器配置");
    wifiManager.setSuccessPage("配置已保存！设备将重启...");
    
    // 尝试连接WiFi
    if (!wifiManager.begin()) {
        Serial.println("WiFi连接失败，重启设备...");
        ESP.restart();
    }
    
    Serial.println("WiFi连接成功！");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    // 如果WiFi已连接，执行NFC读取
    if (WiFi.status() == WL_CONNECTED) {
        if (nfc.readNFC()) {
            Serial.println("检测到NFC标签，准备发送数据...");
            
            // 创建JSON数据
            StaticJsonDocument<200> doc;
            doc["device_id"] = "NFC_Reader_" + String(ESP.getChipId(), HEX);
            doc["timestamp"] = String(millis());
            
            String jsonString;
            serializeJson(doc, jsonString);
            
            if (sendData(jsonString.c_str())) {
                Serial.println("数据发送成功");
            } else {
                Serial.println("数据发送失败");
            }
            
            // 等待标签移开
            delay(1000);
        }
    }
}

void saveConfigCallback() {
    Serial.println("配置已保存");
}

bool sendData(const char* data) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi未连接，无法发送数据");
        return false;
    }
    
    Serial.println("正在发送数据到服务器...");
    WiFiClient client;
    HTTPClient http;
    
    String url = "http://your-server.com/api/nfc";
    Serial.println("目标URL: " + url);
    
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    
    StaticJsonDocument<200> doc;
    doc["deviceId"] = "NFC_Reader_" + String(ESP.getChipId(), HEX);
    doc["data"] = data;
    
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.println("发送数据: " + jsonString);
    
    int httpCode = http.POST(jsonString);
    Serial.println("HTTP响应码: " + String(httpCode));
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.println("服务器响应: " + response);
    }
    
    http.end();
    
    return httpCode == 200;
}
