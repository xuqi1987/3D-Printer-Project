/*
 * WeMosD1_PC_PowerCtrl
 *
 * 通过局域网 HTTP 接口控制 PC 主机电源：
 *   - GET /            → Web 控制页面
 *   - GET /power?action=press  → 短按电源键（300ms，开机/唤醒）
 *   - GET /power?action=long   → 长按电源键（5s，强制断电）
 *   - GET /status      → JSON: {"wifi":true,"ip":"..."}
 *
 * 硬件接线：
 *   - D1 (GPIO5) → LR7843 MOS 模块 PWM 端
 *   - GND        → LR7843 GND
 *   - 板载 LED (GPIO2) = 状态指示
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>

// ============== User Configuration ==============
#ifndef WIFI_SSID
  #define WIFI_SSID       "MyWiFi"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD   "MyPassword"
#endif

#define HOSTNAME         "pc-power"
#define POWER_GPIO       D1        // WeMos D1: D1 = GPIO5
#define STATUS_LED_GPIO  LED_BUILTIN  // WeMos D1: on-board LED = GPIO2

#ifndef SHORT_PRESS_MS
  #define SHORT_PRESS_MS  300      // 短按：开机/唤醒
#endif
#ifndef LONG_PRESS_MS
  #define LONG_PRESS_MS   5000     // 长按：>4s 多数主板强制关机
#endif
// ================================================

ESP8266WebServer server(80);
Ticker ledTicker;

static volatile bool s_wifiConnected = false;

// ====== Power control ======================================================

/**
 * Drive the power control GPIO HIGH for the requested duration then LOW.
 * The MOSFET/relay module on the other side sees this as a power-button press.
 */
static void pulsePowerButton(int durationMs)
{
    Serial.printf("[POWER] pulse: %d ms\n", durationMs);
    digitalWrite(POWER_GPIO, HIGH);
    delay(durationMs);
    digitalWrite(POWER_GPIO, LOW);
    Serial.println("[POWER] released");
}

// ====== HTTP Handlers ======================================================

static const char kIndexHtml[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>PC Power</title>"
    "<style>"
    "body{font-family:-apple-system,sans-serif;text-align:center;margin-top:60px;"
    "background:#1e1e1e;color:#eee;}"
    "h1{color:#4CAF50;}"
    ".btn{display:inline-block;padding:24px 48px;margin:12px;font-size:20px;"
    "border:none;border-radius:10px;color:#fff;cursor:pointer;text-decoration:none;}"
    ".btn-on{background:#4CAF50;}.btn-on:hover{background:#45a049;}"
    ".btn-off{background:#f44336;}.btn-off:hover{background:#da190b;}"
    ".status{margin-top:30px;font-size:14px;color:#888;}"
    "</style></head><body>"
    "<h1>PC Power Control</h1>"
    "<p><a href='/power?action=press' class='btn btn-on'>POWER ON / WAKE</a></p>"
    "<p><a href='/power?action=long' class='btn btn-off'>FORCE OFF (5s)</a></p>"
    "<p class='status' id='status'>loading...</p>"
    "<script>"
    "fetch('/status').then(r=>r.json()).then(s=>{"
    "  document.getElementById('status').textContent="
    "    'WiFi: ' + (s.wifi?'connected':'disconnected') + ' | IP: ' + s.ip;"
    "}).catch(()=>{});"
    "</script>"
    "</body></html>";

static void handleRoot()
{
    server.send(200, "text/html", kIndexHtml);
}

static void handlePower()
{
    String action = "press";
    if (server.hasArg("action")) {
        action = server.arg("action");
    }
    Serial.printf("[HTTP] /power action=%s\n", action.c_str());

    if (action == "long") {
        pulsePowerButton(LONG_PRESS_MS);
    } else {
        pulsePowerButton(SHORT_PRESS_MS);
    }

    String body = "{\"ok\":true,\"action\":\"" + action + "\"}";
    server.send(200, "application/json", body);
}

static void handleStatus()
{
    String ip = s_wifiConnected ? WiFi.localIP().toString() : "0.0.0.0";
    String body = "{\"wifi\":" + String(s_wifiConnected ? "true" : "false") +
                  ",\"ip\":\"" + ip + "\"}";
    server.send(200, "application/json", body);
}

// ====== Status LED =========================================================

/**
 * Status LED:
 *   - blink 1Hz  = WiFi not connected
 *   - solid ON   = WiFi connected
 */
static void statusLedTick()
{
    if (!s_wifiConnected) {
        digitalWrite(STATUS_LED_GPIO, !digitalRead(STATUS_LED_GPIO));
    } else {
        digitalWrite(STATUS_LED_GPIO, HIGH);
    }
}

// ====== WiFi events ========================================================

static void onWifiConnected(const WiFiEventStationModeGotIP &)
{
    s_wifiConnected = true;
    Serial.printf("[WIFI] connected, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] open: http://%s.local/\n", HOSTNAME);
}

static void onWifiDisconnected(const WiFiEventStationModeDisconnected &)
{
    s_wifiConnected = false;
    Serial.println("[WIFI] disconnected, retrying...");
}

// ====== Arduino setup/loop =================================================

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== WeMos D1 PC Power Control ===");

    // Power control GPIO — idle LOW
    pinMode(POWER_GPIO, OUTPUT);
    digitalWrite(POWER_GPIO, LOW);

    // Status LED
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, LOW);

    // WiFi events
    WiFi.onStationModeGotIP(onWifiConnected);
    WiFi.onStationModeDisconnected(onWifiDisconnected);

    Serial.printf("[WIFI] connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setAutoReconnect(true);

    // mDNS
    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[MDNS] http://%s.local/\n", HOSTNAME);
    }

    // HTTP routes
    server.on("/",         HTTP_GET, handleRoot);
    server.on("/power",    HTTP_GET, handlePower);
    server.on("/status",   HTTP_GET, handleStatus);
    server.begin();
    Serial.println("[HTTP] listening on port 80");

    // Status LED ticker
    ledTicker.attach(0.5, statusLedTick);
}

void loop()
{
    server.handleClient();
    MDNS.update();
}
