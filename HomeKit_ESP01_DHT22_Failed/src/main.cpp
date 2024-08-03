#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino

// 需要的库
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <arduino_homekit_server.h>

// 用于LED状态
#include <Ticker.h>
Ticker ticker;

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

void my_homekit_setup();
void my_homekit_report();
void my_homekit_loop();
int random_value(int min, int max);

void tick()
{
  // 切换状态
  int state = digitalRead(LED_BUILTIN);  // 获取GPIO1引脚的当前状态
  digitalWrite(LED_BUILTIN, !state);     // 设置引脚为相反状态
}

// WiFiManager进入配置模式时调用
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("进入配置模式");
  Serial.println(WiFi.softAPIP());
  // 如果使用自动生成的SSID，打印出来
  Serial.println(myWiFiManager->getConfigPortalSSID());
  // 进入配置模式，使LED闪烁更快
  ticker.attach(0.2, tick);
}

void setup() {
  // 在此处放置你的初始化代码，只运行一次：
  Serial.begin(9600);
  
  // 设置LED引脚为输出模式
  pinMode(LED_BUILTIN, OUTPUT);
  // 以0.5秒的间隔启动ticker，因为我们在AP模式下启动并尝试连接
  ticker.attach(0.6, tick);

  // WiFiManager
  // 本地初始化。一旦完成工作，就不需要保留它
  WiFiManager wifiManager;
  
  // 重置设置 - 用于测试
  // wifiManager.resetSettings();

  // 设置回调，当连接到之前的WiFi失败并进入接入点模式时调用
  wifiManager.setAPCallback(configModeCallback);

  // 获取SSID和密码并尝试连接
  // 如果无法连接，则启动一个具有指定名称的接入点
  // 此处为 "AutoConnectAP"
  // 并进入一个阻塞循环，等待配置
  if (!wifiManager.autoConnect()) {
    Serial.println("连接失败并超时");
    // 重置并重试，或者可能进入深度睡眠
    ESP.reset();
    delay(1000);
  }

  // 如果到达这里，说明已经连接到WiFi
  Serial.println("已连接...耶 :)");
  ticker.detach();
  // 保持LED亮
  digitalWrite(LED_BUILTIN, LOW);

  my_homekit_setup();
}


void loop() {
	my_homekit_loop();
	delay(10);
}

//==============================
// Homekit setup and loop
//==============================

// access your homekit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_current_temperature;

static uint32_t next_heap_millis = 0;
static uint32_t next_report_millis = 0;

void my_homekit_setup() {
	arduino_homekit_setup(&config);
}

void my_homekit_loop() {
	arduino_homekit_loop();
	const uint32_t t = millis();
	if (t > next_report_millis) {
		// report sensor values every 10 seconds
		next_report_millis = t + 10 * 1000;
		my_homekit_report();
	}
	if (t > next_heap_millis) {
		// show heap info every 5 seconds
		next_heap_millis = t + 5 * 1000;
		LOG_D("Free heap: %d, HomeKit clients: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

	}
}

void my_homekit_report() {
	float temperature_value = random_value(10, 30); // FIXME, read your real sensor here.
	cha_current_temperature.value.float_value = temperature_value;
	LOG_D("Current temperature: %.1f", temperature_value);
	homekit_characteristic_notify(&cha_current_temperature, cha_current_temperature.value);
}

int random_value(int min, int max) {
	return min + random(max - min);
}