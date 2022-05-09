#include <ESP8266WiFi.h>

static WiFiClient espClient;
#include <ArduinoJson.h>

#include <AliyunIoTSDK.h>
AliyunIoTSDK iot;

#define PRODUCT_KEY "a16PlBcvRUx"
#define DEVICE_NAME "Switch_01"
#define DEVICE_SECRET "e5d9f462f6f20dc4484c6596f7958ed0"
#define REGION_ID "cn-shanghai"

#define WIFI_SSID "TP-LINK_xuqi3"
#define WIFI_PASSWD "xgm10503"

void powerCallback(JsonVariant p)
{
    int PowerSwitch = p["PowerSwitch"];
    if (PowerSwitch == 1)
    {
        //
    }
    else
    {
        //
    }
}

void wifiInit(const char *ssid, const char *passphrase)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passphrase);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
    Serial.println("Connected to AP");
}

void setup()
{
    Serial.begin(9600);

    wifiInit(WIFI_SSID, WIFI_PASSWD);

    AliyunIoTSDK::begin(espClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, REGION_ID);

    // 绑定属性回调
    AliyunIoTSDK::bindData("PowerSwitch", powerCallback);
}

unsigned long lastMsMain = 0;
void loop()
{
    AliyunIoTSDK::loop();
    if (millis() - lastMsMain >= 5000)
    {
        lastMsMain = millis();
        // 发送事件到阿里云平台
        AliyunIoTSDK::sendEvent("xxx");
        // 发送模型属性到阿里云平台
        AliyunIoTSDK::send("CurrentTemperature", 30);
    }
}
