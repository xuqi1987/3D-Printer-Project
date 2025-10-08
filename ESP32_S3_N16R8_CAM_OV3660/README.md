# ESP32-S3 MJPEG RTSP Camera Server

## 项目概述

这是一个专为ESP32-S3设计的MJPEG RTSP摄像头服务器项目，通过WiFi提供实时视频流。该项目充分利用ESP32-S3的硬件能力，直接使用摄像头的原生JPEG输出，避免了软件编码的开销。

## 🚀 主要特性

- **MJPEG格式流媒体** - 使用RFC 2435标准，直接传输摄像头JPEG数据
- **RTSP协议支持** - 完整的RTSP 1.0实现，支持OPTIONS、DESCRIBE、SETUP、PLAY、TEARDOWN
- **TCP交错传输** - 使用TCP interleaved方式传输RTP数据，提高可靠性  
- **高性能优化** - 针对ESP32-S3硬件特性优化，37fps@640x480
- **内存管理** - 增强的内存配置，16KB主任务栈，8KB系统任务栈
- **WiFi连接管理** - 自动连接和重连机制
- **系统监控** - 实时FPS、内存使用、温度监控

## 📋 硬件要求

- **ESP32-S3 开发板** (推荐16MB Flash + 8MB PSRAM)
- **OV3660 摄像头模块**
- **WiFi网络环境**

## 🔧 快速开始

### 1. 环境配置
```bash
# 确保ESP-IDF v5.4.2已安装
source ~/esp/esp-idf/export.sh

# 克隆项目
git clone <repository_url>
cd ESP32_S3_N16R8_CAM_OV3660
```

### 2. 配置WiFi
编辑 `main/app_config.h` 文件，设置您的WiFi信息：
```c
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"  
```

### 3. 构建和刷写
```bash
# 使用便捷脚本（推荐）
./build_flash.sh

# 或手动构建
idf.py build
idf.py -p /dev/cu.usbserial-210 flash monitor
```

### 4. 测试连接
设备启动后会显示IP地址，使用以下命令测试：
```bash
# 使用FFmpeg测试
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live -t 10 -vcodec copy test.mp4

# 使用VLC播放器
vlc rtsp://192.168.31.104:8554/live

# 使用Python脚本测试
python test_rtsp_commands.py
```

## 📁 项目结构

```
├── main/                       # 主程序
│   ├── main.c                 # 应用入口
│   ├── app_config.h           # 配置文件
│   └── camera_pins.h          # 摄像头引脚定义
├── components/                 # 组件目录
│   ├── h264_rtsp_server/      # RTSP服务器（重命名保持兼容）
│   ├── camera_manager/        # 摄像头管理
│   ├── wifi_manager/          # WiFi管理
│   └── system_monitor/        # 系统监控
├── build_flash.sh             # 构建刷写脚本
├── test_rtsp_commands.py      # Python测试脚本
└── README.md                  # 项目文档
```

## 🔬 技术细节

### RTSP协议实现
- **SDP格式**: 使用MJPEG专用SDP描述（payload type 26）
- **传输方式**: TCP interleaved (RTP over TCP)
- **端口**: 8554 (标准RTSP端口)
- **会话管理**: 支持多客户端连接，自动会话超时

### 性能指标
- **分辨率**: 640x480 (VGA)
- **帧率**: 37 FPS
- **编码**: MJPEG (摄像头原生JPEG)
- **比特率**: 动态调整，质量等级12
- **延迟**: <100ms (局域网)

### 内存使用
- **总Flash**: 915KB (23% 使用)
- **可用PSRAM**: 8MB
- **运行时内存**: ~8MB可用堆内存
- **栈配置**: 主任务16KB，系统任务8KB

## 🛠️ 开发工具

### 构建脚本 `build_flash.sh`
```bash
# 构建
./build_flash.sh

# 构建并监控
./build_flash.sh monitor

# 清理构建
./build_flash.sh clean
```

### Python测试工具 `test_rtsp_commands.py`
- RTP包计数和统计
- 帧率测试
- 丢包率检测
- RTSP命令序列测试

## 🔍 故障排除

### 常见问题

1. **WiFi连接失败**
   - 检查SSID和密码配置
   - 确认2.4GHz网络（ESP32不支持5GHz）

2. **RTSP连接超时**
   - 确认防火墙设置
   - 检查端口8554是否被占用
   - 使用TCP传输模式

3. **视频卡顿**
   - 检查网络带宽
   - 调整视频质量参数
   - 监控ESP32内存使用

4. **构建错误**
   - 确认ESP-IDF版本为5.4.2
   - 检查串口权限和设备路径

### 调试模式
启用详细日志：
```c
// 在 app_config.h 中设置
#define CONFIG_LOG_DEFAULT_LEVEL ESP_LOG_DEBUG
```

## 🔮 未来改进

- [ ] 支持多种分辨率切换
- [ ] 添加认证机制
- [ ] 实现动态比特率调整
- [ ] 支持音频流
- [ ] Web配置界面
- [ ] OTA无线更新

## 📊 性能基准

| 指标 | 值 | 备注 |
|------|------|------|
| 分辨率 | 640x480 | VGA质量 |
| 帧率 | 37 FPS | 稳定输出 |
| 延迟 | <100ms | 局域网环境 |
| 内存占用 | <1MB | 运行时 |
| 功耗 | ~500mA | 含WiFi和摄像头 |
| 连接数 | 1-3个 | 同时客户端 |

## 📚 参考资料

- [RFC 2326 - Real Time Streaming Protocol (RTSP)](https://tools.ietf.org/html/rfc2326)
- [RFC 2435 - RTP Payload Format for JPEG](https://tools.ietf.org/html/rfc2435)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

## 🤝 贡献

欢迎提交Issue和Pull Request来改进这个项目！

## 📄 许可证

MIT License - 查看LICENSE文件了解详情

---
**项目状态**: 生产就绪 ✅  
**最后更新**: 2025年10月8日  
**版本**: 3.1.0