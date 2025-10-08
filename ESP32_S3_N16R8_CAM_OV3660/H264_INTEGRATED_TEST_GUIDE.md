# ESP32-S3 H.264 RTP 集成测试指南

## 概述

H.264 RTP 实现已经直接集成到主程序 (`main/main.c`) 中，通过编译时宏定义来控制测试模式。无需单独的测试文件。

## 测试模式控制

### 方法1: 使用切换脚本 (推荐)

```bash
# 查看当前状态
./toggle_test_mode.sh status

# 启用H.264测试模式
./toggle_test_mode.sh enable

# 禁用测试模式（恢复正常摄像头模式）
./toggle_test_mode.sh disable
```

### 方法2: 手动编辑

编辑 `main/main.c` 第 40 行附近：

```c
// 启用H.264测试模式
#define ENABLE_H264_TEST_MODE      1

// 或者禁用测试模式
#define ENABLE_H264_TEST_MODE      0
```

## 测试模式功能

### 🔄 启用测试模式时
- ✅ **跳过摄像头初始化** - 节省资源和避免硬件依赖
- ✅ **生成合成H.264数据** - 标准的SPS、PPS、IDR、P帧
- ✅ **20fps测试流** - 稳定的测试数据流
- ✅ **RFC 6184验证** - 测试单一NAL和FU-A分片模式
- ✅ **兼容性测试** - 支持VLC、FFmpeg等标准客户端

### 🎥 禁用测试模式时
- ✅ **正常摄像头操作** - 使用真实的摄像头数据
- ✅ **JPEG兼容性** - 继续支持MJPEG RTP
- ✅ **自动格式检测** - H.264和JPEG数据自适应
- ✅ **生产就绪** - 完整的摄像头流媒体功能

## 构建和测试流程

### 1. 准备环境
```bash
# 确保ESP-IDF环境已设置
source $HOME/esp/esp-idf/export.sh

# 或者设置IDF路径
export IDF_PATH='/Users/xuqi/esp/v5.4.2/esp-idf'
```

### 2. 启用测试模式
```bash
./toggle_test_mode.sh enable
```

### 3. 构建项目
```bash
idf.py build
```

### 4. 烧录固件
```bash
idf.py flash
```

### 5. 监控输出
```bash
idf.py monitor
```

期待的输出：
```
I (123) main: ESP32-S3 H.264 RTP Test Server Starting...
I (124) main: Version: 3.1.0 (H.264 RTP Test Mode)
W (125) main: *** TEST MODE ENABLED ***
I (126) main: Camera manager skipped - H.264 test mode enabled
I (127) main: H.264 Test Mode - Camera task disabled, using synthetic H.264 data
I (128) h264_rtsp: H.264 test mode started
```

### 6. 测试客户端连接

#### Python测试脚本
```bash
python3 test_h264_rtp.py
```

#### VLC播放器
```
rtsp://192.168.31.104:8554/live
```

#### FFmpeg命令行
```bash
ffplay -rtsp_transport tcp rtsp://192.168.31.104:8554/live
```

#### GStreamer管道
```bash
gst-launch-1.0 rtspsrc location=rtsp://192.168.31.104:8554/live ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink
```

## 测试验证点

### ✅ RTSP握手
- OPTIONS请求响应
- DESCRIBE返回H.264 SDP
- SETUP建立TCP interleaved传输
- PLAY开始RTP流

### ✅ H.264 RTP封装
- **单一NAL模式**: 小NAL单元直接封装
- **FU-A分片模式**: 大NAL单元分片传输
- **正确的RTP头部**: 序列号递增、时间戳计算
- **标记位处理**: 帧结束标记

### ✅ NAL单元类型
- **SPS (7)**: 序列参数集
- **PPS (8)**: 图像参数集  
- **IDR (5)**: 关键帧
- **P帧 (1)**: 预测帧
- **FU-A (28)**: 分片单元

### ✅ 时序特性
- **I帧间隔**: 每30帧一个关键帧
- **帧率**: 20 fps
- **时间戳**: 90kHz时钟
- **大NAL测试**: 每100帧测试FU-A分片

## 性能监控

### 日志输出示例
```
I (1234) h264_rtsp: Generated test I-frame #0
I (1235) h264_rtsp: H.264 RTP packet sent: seq=1, payload_size=67, marker=0
I (1236) h264_rtsp: H.264 RTP packet sent: seq=2, payload_size=8, marker=0  
I (1237) h264_rtsp: H.264 RTP packet sent: seq=3, payload_size=89, marker=1
I (5678) main: H.264 Test Stats: Clients=1, Frames=600, Bytes=89234, FPS=20.1
```

### 统计信息
- **active_clients**: 当前连接的客户端数
- **frames_sent**: 已发送的H.264帧数
- **bytes_sent**: 已发送的总字节数
- **current_fps**: 当前实际帧率

## 故障排除

### 问题1: 编译错误
```
Solution: 确保所有H.264相关文件都已正确添加到项目中
```

### 问题2: 客户端连接失败
```
Check: WiFi连接状态和IP地址
Check: 防火墙设置
Check: RTSP端口8554是否可访问
```

### 问题3: RTP流异常
```
Monitor: ESP32串口输出查看错误信息
Verify: Python测试脚本的详细分析
Debug: 启用ESP_LOG_DEBUG级别
```

### 问题4: 性能问题
```
Check: 内存使用情况
Adjust: 帧率和缓冲区设置
Monitor: CPU使用率
```

## 切换回生产模式

### 1. 禁用测试模式
```bash
./toggle_test_mode.sh disable
```

### 2. 重新构建
```bash
idf.py build && idf.py flash
```

### 3. 验证摄像头工作
确保摄像头硬件连接正常，程序将使用真实的摄像头数据。

## 高级配置

### 修改测试帧率
编辑 `h264_rtsp_server.c` 中 `h264_test_mode_task()` 函数：
```c
const TickType_t frame_delay = pdMS_TO_TICKS(50);  // 20fps
// 改为
const TickType_t frame_delay = pdMS_TO_TICKS(33);  // 30fps
```

### 修改I帧间隔
```c
if (server->test_frame_counter % 30 == 0) {  // 每30帧
// 改为
if (server->test_frame_counter % 60 == 0) {  // 每60帧
```

### 自定义SDP参数
编辑 `handle_rtsp_request()` 函数中的SDP字符串。

## 总结

✅ **集成测试模式提供了完整的H.264 RTP验证能力**
✅ **无需额外的测试文件，一个主程序支持两种模式**  
✅ **简单的宏定义切换，方便开发和部署**
✅ **标准兼容的H.264 RTP实现，支持主流播放器**

🎉 **这是一个完整的、生产就绪的H.264 RTSP流媒体解决方案！**