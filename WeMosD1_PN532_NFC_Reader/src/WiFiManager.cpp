#include "WiFiManager.h"

WiFiManager::WiFiManager() : server(80), dnsServer() {
    shouldSaveConfig = false;
    configPortalTimeout = 180000; // 默认3分钟
    breakAfterConfig = true;
    configTitle = "设备配网";
    successPage = "<html><head><meta charset='UTF-8'><title>配网成功</title><meta http-equiv='refresh' content='3;url=http://192.168.4.1'></head><body><h1>配网成功！</h1><p>3秒后自动跳转到主页...</p></body></html>";
    saveConfigCallback = nullptr;
}

bool WiFiManager::begin() {
    // 尝试连接已保存的WiFi
    if (WiFi.SSID().length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        
        // 等待连接
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi已连接");
            Serial.print("IP地址: ");
            Serial.println(WiFi.localIP());
            return true;
        }
    }
    
    // 如果连接失败，启动AP模式
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("AutoConnectAP", "password");
    dnsServer.start(53, "*", WiFi.softAPIP());
    setupConfigPortal();
    
    Serial.println("\n进入配网模式");
    Serial.print("AP名称: AutoConnectAP\n");
    Serial.print("AP密码: password\n");
    Serial.print("IP地址: ");
    Serial.println(WiFi.softAPIP());
    
    return false;
}

void WiFiManager::handleClient() {
    dnsServer.processNextRequest();
    server.handleClient();
}

void WiFiManager::setSaveConfigCallback(void (*func)()) {
    saveConfigCallback = func;
}

void WiFiManager::setConfigPortalTimeout(unsigned long seconds) {
    configPortalTimeout = seconds * 1000;
}

void WiFiManager::setBreakAfterConfig(bool value) {
    breakAfterConfig = value;
}

void WiFiManager::setTitle(const char* title) {
    configTitle = title;
}

void WiFiManager::setSuccessPage(const char* page) {
    successPage = page;
}

void WiFiManager::resetSettings() {
    WiFi.disconnect();
    ESP.restart();
}

void WiFiManager::setupConfigPortal() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/wifi", HTTP_POST, [this]() { handleWifiSave(); });
    server.on("/scan", HTTP_GET, [this]() { handleWifiScan(); });
    server.on("/info", HTTP_GET, [this]() { handleInfo(); });
    server.on("/reset", HTTP_POST, [this]() { handleWifiReset(); });
    server.onNotFound([this]() { handleNotFound(); });
    server.begin();
}

void WiFiManager::handleRoot() {
    String page = "<html><head><meta charset='UTF-8'><title>";
    page += configTitle;
    page += "</title><style>";
    page += "body{font-family:Arial;margin:20px;}";
    page += ".container{max-width:800px;margin:0 auto;}";
    page += ".nav{background:#f8f9fa;padding:10px;margin-bottom:20px;}";
    page += ".nav a{color:#007bff;text-decoration:none;margin-right:15px;}";
    page += ".nav a:hover{text-decoration:underline;}";
    page += ".btn{background:#007bff;color:white;border:none;padding:8px 15px;cursor:pointer;}";
    page += ".btn-danger{background:#dc3545;}";
    page += "input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box;}";
    page += "</style></head><body>";
    page += "<div class='container'>";
    page += "<div class='nav'>";
    page += "<a href='/'>配置WiFi</a>";
    page += "<a href='/scan'>扫描网络</a>";
    page += "<a href='/info'>信息</a>";
    page += "</div>";
    page += getConfigPage();
    page += "</div></body></html>";
    server.send(200, "text/html", page);
}

void WiFiManager::handleWifiScan() {
    String page = "<html><head><meta charset='UTF-8'><title>扫描网络</title>";
    page += "<style>";
    page += "body{font-family:Arial;margin:20px;}";
    page += ".container{max-width:800px;margin:0 auto;}";
    page += ".nav{background:#f8f9fa;padding:10px;margin-bottom:20px;}";
    page += ".nav a{color:#007bff;text-decoration:none;margin-right:15px;}";
    page += ".nav a:hover{text-decoration:underline;}";
    page += ".network{background:#f8f9fa;padding:10px;margin:5px 0;cursor:pointer;}";
    page += ".network:hover{background:#e9ecef;}";
    page += "</style></head><body>";
    page += "<div class='container'>";
    page += "<div class='nav'>";
    page += "<a href='/'>配置WiFi</a>";
    page += "<a href='/scan'>扫描网络</a>";
    page += "<a href='/info'>信息</a>";
    page += "</div>";
    page += getScanPage();
    page += "</div></body></html>";
    server.send(200, "text/html", page);
}

void WiFiManager::handleInfo() {
    String page = "<html><head><meta charset='UTF-8'><title>设备信息</title>";
    page += "<style>";
    page += "body{font-family:Arial;margin:20px;}";
    page += ".container{max-width:800px;margin:0 auto;}";
    page += ".nav{background:#f8f9fa;padding:10px;margin-bottom:20px;}";
    page += ".nav a{color:#007bff;text-decoration:none;margin-right:15px;}";
    page += ".nav a:hover{text-decoration:underline;}";
    page += ".btn{background:#007bff;color:white;border:none;padding:8px 15px;cursor:pointer;}";
    page += ".btn-danger{background:#dc3545;}";
    page += "</style></head><body>";
    page += "<div class='container'>";
    page += "<div class='nav'>";
    page += "<a href='/'>配置WiFi</a>";
    page += "<a href='/scan'>扫描网络</a>";
    page += "<a href='/info'>信息</a>";
    page += "</div>";
    page += getInfoPage();
    page += "</div></body></html>";
    server.send(200, "text/html", page);
}

String WiFiManager::getConfigPage() {
    String page = "<h1>配置WiFi</h1>";
    page += "<form action='/wifi' method='post'>";
    page += "<input type='text' name='ssid' placeholder='WiFi名称' required><br>";
    page += "<input type='password' name='pass' placeholder='WiFi密码'><br>";
    page += "<button type='submit' class='btn'>保存</button>";
    page += "</form>";
    return page;
}

String WiFiManager::getScanPage() {
    String page = "<h1>可用网络</h1>";
    int n = WiFi.scanNetworks();
    if (n == 0) {
        page += "<p>未找到网络</p>";
    } else {
        for (int i = 0; i < n; ++i) {
            page += "<div class='network' onclick='selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
            page += WiFi.SSID(i);
            page += " (" + String(WiFi.RSSI(i)) + "dBm)";
            page += "</div>";
        }
    }
    page += "<script>";
    page += "function selectNetwork(ssid) {";
    page += "  window.location.href = '/?ssid=' + encodeURIComponent(ssid);";
    page += "}";
    page += "</script>";
    return page;
}

String WiFiManager::getInfoPage() {
    String page = "<h1>设备信息</h1>";
    page += "<p>当前状态: ";
    if (WiFi.status() == WL_CONNECTED) {
        page += "已连接</p>";
        page += "<p>SSID: " + WiFi.SSID() + "</p>";
        page += "<p>IP地址: " + WiFi.localIP().toString() + "</p>";
    } else {
        page += "未连接</p>";
    }
    page += "<form action='/reset' method='post'>";
    page += "<button type='submit' class='btn btn-danger'>重置配置</button>";
    page += "</form>";
    return page;
}

void WiFiManager::handleWifiSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    
    if (connectWifi(ssid, pass)) {
        if (saveConfigCallback) {
            saveConfigCallback();
        }
        server.send(200, "text/html", successPage);
        if (breakAfterConfig) {
            stopConfigPortal();
        }
    } else {
        server.send(200, "text/html", "<html><head><meta charset='UTF-8'><title>连接失败</title></head><body><h1>连接失败</h1><p>请检查WiFi名称和密码是否正确</p><a href='/'>返回</a></body></html>");
    }
}

void WiFiManager::handleWifiReset() {
    resetSettings();
}

void WiFiManager::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

bool WiFiManager::connectWifi(String ssid, String pass) {
    if (ssid.length() == 0) {
        return false;
    }
    
    WiFi.begin(ssid.c_str(), pass.length() > 0 ? pass.c_str() : nullptr);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(100);
    }
    
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startConfigPortal() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("AutoConnectAP", "password");
    dnsServer.start(53, "*", WiFi.softAPIP());
    setupConfigPortal();
}

void WiFiManager::stopConfigPortal() {
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
} 