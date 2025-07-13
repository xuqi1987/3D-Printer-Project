#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

class WiFiManager {
public:
    WiFiManager();
    bool begin();
    void handleClient();
    void setSaveConfigCallback(void (*func)());
    void setConfigPortalTimeout(unsigned long seconds);
    void setBreakAfterConfig(bool value);
    void setTitle(const char* title);
    void setSuccessPage(const char* page);
    void resetSettings();

private:
    ESP8266WebServer server;
    DNSServer dnsServer;
    bool shouldSaveConfig;
    unsigned long configPortalTimeout;
    bool breakAfterConfig;
    String configTitle;
    String successPage;
    void (*saveConfigCallback)();

    void setupConfigPortal();
    void handleRoot();
    void handleWifiSave();
    void handleWifiConnect();
    void handleWifiStatus();
    void handleWifiReset();
    void handleWifiScan();
    void handleInfo();
    void handleNotFound();
    bool connectWifi(String ssid, String pass);
    void startConfigPortal();
    void stopConfigPortal();
    String getConfigPage();
    String getInfoPage();
    String getScanPage();
};

#endif 