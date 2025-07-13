# NFC读取器设计文档

## 1. 项目概述
本项目基于ESP8266和PN532 NFC模块，实现一个具有WiFi配网功能的NFC标签读取器。设备可以读取NFC标签信息，并通过网络将数据传输到服务器。

## 2. 硬件需求
- ESP8266 (WeMos D1 Mini)
- PN532 NFC模块
- 电源模块
- LED指示灯（可选，用于状态显示）

## 3. 软件架构

### 3.1 核心功能模块
1. WiFi管理模块
   - 配网模式（AP模式）
   - 正常工作模式（STA模式）
   - 配网状态管理

2. NFC读取模块
   - PN532初始化
   - NFC标签读取
   - 数据解析

3. 网络通信模块
   - HTTP客户端
   - 数据发送
   - 错误处理

### 3.2 工作流程
1. 设备启动流程
   - 检查WiFi连接状态
   - 如果未连接，进入配网模式
   - 如果已连接，进入正常工作模式

2. 配网流程
   - 创建AP热点（SSID: NFC_Reader_XXXX）
   - 启动Web服务器
   - 提供配网页面
   - 保存WiFi配置
   - 重启进入正常工作模式

3. NFC读取流程
   - 持续监听NFC标签
   - 检测到标签时读取数据
   - 格式化数据
   - 通过HTTP发送到服务器

## 4. 通信协议设计

### 4.1 数据格式
```json
{
    "device_id": "NFC_READER_001",
    "timestamp": "2024-03-21T10:30:00Z",
    "nfc_data": {
        "uid": "04:XX:XX:XX:XX:XX:XX",
        "type": "MIFARE_1KB",
        "data": "base64_encoded_data"
    }
}
```

### 4.2 API接口
- 端点：`http://your-server.com/api/nfc-data`
- 方法：POST
- 内容类型：application/json

## 5. 实现步骤

### 5.1 基础设置
1. 安装必要的库
   - ESP8266WiFi
   - ESP8266WebServer
   - DNSServer
   - Adafruit_PN532
   - ArduinoJson

2. 配置开发环境
   - 设置Arduino IDE
   - 配置ESP8266开发板
   - 安装依赖库

### 5.2 代码实现顺序
1. WiFi管理模块
2. Web配网服务器
3. NFC读取模块
4. 数据发送模块
5. 主程序逻辑

## 6. 测试计划
1. WiFi配网功能测试
2. NFC读取功能测试
3. 数据发送功能测试
4. 整体功能集成测试

## 7. 注意事项
1. 电源管理
2. 错误处理
3. 数据安全
4. 网络稳定性 