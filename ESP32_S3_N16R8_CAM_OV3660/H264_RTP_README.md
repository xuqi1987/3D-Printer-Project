# ESP32-S3 H.264 RTP 实现

本项目将原来的 MJPEG RTP 实现改造成了符合 RFC 6184 标准的真正的 H.264 RTP 实现。

## 改造内容

### 1. RTSP SDP 修改
- 从 `rtpmap:26 JPEG/90000` 改为 `rtpmap:96 H264/90000`
- 添加了 H.264 格式参数 (fmtp)
- 支持 profile-level-id 和 packetization-mode

### 2. H.264 NAL 单元解析
- 支持解析 H.264 起始码 (0x00000001 和 0x000001)
- 识别不同类型的 NAL 单元 (SPS, PPS, IDR, P帧等)
- 提取 NAL 单元类型和重要性指示符 (NRI)

### 3. RFC 6184 RTP 封装模式

#### 单一 NAL 单元模式 (Single NAL Unit Mode)
- 适用于小于 MTU 的 NAL 单元
- 直接发送 NAL 单元数据作为 RTP 载荷

#### FU-A 分片模式 (FU-A Fragmentation)
- 适用于大于 MTU 的 NAL 单元
- 将大 NAL 单元分片成多个 RTP 包
- 支持开始位 (S) 和结束位 (E) 标记

#### STAP-A 聚合模式 (预留)
- 将多个小 NAL 单元聚合成一个 RTP 包
- 目前暂未实现，可根据需要添加

### 4. 兼容性支持
- 自动检测输入数据格式 (H.264 或 JPEG)
- H.264 数据使用新的 RTP 实现
- JPEG 数据仍使用原来的 MJPEG RTP 实现

## 测试模式

### H.264 测试数据生成
项目包含了一个测试模式，可以生成合成的 H.264 数据用于验证 RTP 实现：

- **SPS/PPS**: 序列参数集和图像参数集
- **IDR 帧**: 关键帧 (I帧)
- **P 帧**: 预测帧
- **大 NAL 单元**: 用于测试 FU-A 分片

### 构建和运行测试

1. **构建测试版本**:
   ```bash
   ./build_h264_test.sh
   ```

2. **烧录固件**:
   ```bash
   idf.py flash
   ```

3. **监控输出**:
   ```bash
   idf.py monitor
   ```

4. **运行测试客户端**:
   ```bash
   python3 test_h264_rtp.py
   ```

### 测试客户端功能

`test_h264_rtp.py` 脚本提供以下测试功能：

- **RTSP 握手测试**: 验证 OPTIONS, DESCRIBE, SETUP, PLAY 流程
- **SDP 解析**: 检查 H.264 格式参数
- **RTP 包接收**: 捕获并分析 RTP 流
- **H.264 载荷分析**: 识别 NAL 单元类型和 RTP 封装模式
- **统计分析**: 包丢失检测、帧率计算、包大小统计

## 代码结构

### 核心文件

```
components/h264_rtsp_server/
├── h264_rtsp_server.c          # 主实现文件
├── include/
│   ├── h264_rtsp_server.h      # 公共接口
│   └── h264_test_data.h        # 测试数据生成
```

### 关键函数

- `send_h264_frame()`: H.264 帧发送入口
- `parse_h264_nals()`: NAL 单元解析
- `send_h264_nal_single()`: 单一 NAL 模式
- `send_h264_nal_fu_a()`: FU-A 分片模式
- `send_rtp_h264_packet()`: H.264 RTP 包发送

## H.264 RTP 包格式

### 单一 NAL 单元包
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                    NAL Unit Data                             |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### FU-A 分片包
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| FU indicator  |   FU header   |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
|                                                               |
|                      Fragment Data                           |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## 支持的客户端

改造后的实现应该兼容所有支持 H.264 RTP 的标准客户端：

- **VLC Media Player**
- **FFmpeg/ffplay**
- **GStreamer**
- **OpenCV**
- **其他支持 RTSP/H.264 的播放器**

## 使用示例

### VLC 播放
```
rtsp://192.168.31.104:8554/live
```

### FFmpeg 命令行
```bash
ffplay rtsp://192.168.31.104:8554/live
```

### GStreamer 管道
```bash
gst-launch-1.0 rtspsrc location=rtsp://192.168.31.104:8554/live ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink
```

## 性能特性

- **低延迟**: 针对实时流媒体优化
- **内存效率**: 避免不必要的数据拷贝
- **错误恢复**: 网络错误时的重试机制
- **兼容性**: 支持 TCP interleaved 和 UDP 传输 (计划中)

## 注意事项

1. **数据格式**: 当前实现需要输入符合 Annex B 格式的 H.264 数据
2. **起始码**: 支持 4 字节 (0x00000001) 和 3 字节 (0x000001) 起始码
3. **内存限制**: ESP32-S3 内存有限，大 NAL 单元会自动分片
4. **测试模式**: 实际应用中应关闭测试模式，使用真实摄像头数据

## 故障排除

### 常见问题

1. **客户端连接失败**
   - 检查 WiFi 连接和 IP 地址
   - 确认防火墙设置
   - 验证 RTSP 端口 (默认 8554)

2. **视频播放异常**
   - 检查 H.264 数据格式
   - 验证 NAL 单元完整性
   - 查看 ESP32 日志输出

3. **性能问题**
   - 调整帧率设置
   - 检查网络带宽
   - 监控 ESP32 内存使用

### 调试日志

启用详细日志：
```c
esp_log_level_set("h264_rtsp", ESP_LOG_DEBUG);
```

## 后续改进

1. **UDP 传输支持**
2. **STAP-A 聚合模式实现**  
3. **动态比特率调整**
4. **更多 H.264 profile 支持**
5. **硬件 H.264 编码集成**

## 参考资料

- [RFC 6184 - RTP Payload Format for H.264 Video](https://tools.ietf.org/html/rfc6184)
- [RFC 2326 - Real Time Streaming Protocol (RTSP)](https://tools.ietf.org/html/rfc2326)
- [RFC 3550 - RTP: A Transport Protocol for Real-Time Applications](https://tools.ietf.org/html/rfc3550)
- [ITU-T H.264 Standard](https://www.itu.int/rec/T-REC-H.264)