# NFC读取器项目

基于ESP8266和PN532的NFC标签读取器，支持WiFi配网和数据上传功能。

## 功能特点

1. WiFi配网功能
   - 首次使用时自动创建配网热点
   - 通过网页界面配置WiFi
   - 配置信息保存在EEPROM中

2. NFC读取功能
   - 支持MIFARE 1KB标签
   - 自动读取标签UID
   - 数据格式化输出

3. 数据上传
   - 使用HTTP POST请求
   - JSON格式数据
   - 支持自定义服务器地址

## 硬件要求

- ESP8266 (WeMos D1 Mini)
- PN532 NFC模块
- 电源模块
- 连接线

## 接线说明

PN532模块与ESP8266的连接：
- PN532 IRQ -> D1
- PN532 RST -> D2
- PN532 SDA -> D4
- PN532 SCL -> D5
- VCC -> 3.3V
- GND -> GND

## 安装步骤

1. 安装Arduino IDE
2. 安装ESP8266开发板支持
   - 打开Arduino IDE
   - 进入"文件 > 首选项"
   - 在"附加开发板管理器网址"中添加：`http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - 进入"工具 > 开发板 > 开发板管理器"
   - 搜索并安装"ESP8266"

3. 安装必要的库
   - ESP8266WiFi
   - ESP8266WebServer
   - DNSServer
   - Adafruit_PN532
   - ArduinoJson

4. 配置项目
   - 打开`src/main.cpp`
   - 修改服务器地址（`http://your-server.com/api/nfc-data`）
   - 可选：修改AP模式的SSID和密码

5. 上传代码
   - 选择正确的开发板（WeMos D1 Mini）
   - 选择正确的端口
   - 点击上传

## 使用方法

1. 首次使用
   - 设备启动后会自动创建名为"NFC_Reader_Config"的WiFi热点
   - 使用手机连接该热点（密码：12345678）
   - 打开浏览器，访问任意网址，将自动跳转到配网页面
   - 输入您的WiFi名称和密码
   - 点击保存，设备将自动重启并连接到您的WiFi

2. 正常使用
   - 设备启动后会自动连接到已配置的WiFi
   - 将NFC标签靠近读取器
   - 数据将自动发送到配置的服务器

## 数据格式

```json
{
    "device_id": "NFC_READER_001",
    "timestamp": "1234567890",
    "nfc_data": {
        "uid": "04:XX:XX:XX:XX:XX:XX",
        "type": "MIFARE_1KB"
    }
}
```

## 故障排除

1. 无法连接到WiFi
   - 检查WiFi名称和密码是否正确
   - 确保WiFi信号强度足够
   - 尝试重新配网

2. NFC读取失败
   - 检查接线是否正确
   - 确保标签类型兼容
   - 检查标签是否损坏

3. 数据上传失败
   - 检查网络连接
   - 确认服务器地址正确
   - 检查服务器是否正常运行

## 注意事项

1. 电源要求
   - 建议使用稳定的3.3V电源
   - 确保电流足够（建议>500mA）

2. 安全建议
   - 修改默认的AP密码
   - 使用HTTPS进行数据传输
   - 定期更新固件

## 许可证

MIT License 