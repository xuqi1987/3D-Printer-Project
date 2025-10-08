# FFmpeg H.264 RTSP 测试命令参考

## 基础测试命令

### 1. 流信息检测
```bash
# 快速检测流格式
ffprobe -rtsp_transport tcp rtsp://192.168.31.104:8554/live

# JSON格式输出（便于解析）
ffprobe -v quiet -print_format json -show_streams -show_format \
  -rtsp_transport tcp rtsp://192.168.31.104:8554/live

# 详细信息
ffprobe -v info -rtsp_transport tcp rtsp://192.168.31.104:8554/live
```

### 2. 播放测试
```bash
# 基本播放（窗口模式）
ffplay -rtsp_transport tcp rtsp://192.168.31.104:8554/live

# 10秒播放后自动退出
ffplay -rtsp_transport tcp -t 10 -autoexit rtsp://192.168.31.104:8554/live

# 全屏播放
ffplay -rtsp_transport tcp -fs rtsp://192.168.31.104:8554/live

# 静音播放
ffplay -rtsp_transport tcp -an rtsp://192.168.31.104:8554/live
```

### 3. 性能分析
```bash
# 网络带宽测试（不解码，只测传输）
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 30 -f null -

# 帧率分析
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 15 -f null - -v info 2>&1 | grep fps

# 详细统计
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -f null - -v error -stats
```

### 4. 数据保存
```bash
# 保存原始H.264流
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -c copy -f h264 raw_h264.h264

# 保存为MP4文件
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -c copy output.mp4

# 保存为MKV（更兼容H.264）
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -c copy output.mkv

# 保存关键帧截图
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -vframes 1 -q:v 2 screenshot.jpg
```

### 5. 转码测试
```bash
# 转码为不同分辨率
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -vf scale=320:240 -c:v libx264 small.mp4

# 转码为不同格式
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -c:v libvpx-vp9 output.webm

# 提取音频（如果有）
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 10 -vn -acodec copy audio.aac
```

### 6. 网络协议测试
```bash
# 强制TCP传输
ffmpeg -rtsp_transport tcp -i rtsp://192.168.31.104:8554/live \
  -t 5 -f null -

# 尝试UDP传输（如果支持）
ffmpeg -rtsp_transport udp -i rtsp://192.168.31.104:8554/live \
  -t 5 -f null -

# HTTP隧道（如果支持）
ffmpeg -rtsp_transport http -i rtsp://192.168.31.104:8554/live \
  -t 5 -f null -
```

### 7. 调试和故障排除
```bash
# 详细日志输出
ffmpeg -v debug -rtsp_transport tcp \
  -i rtsp://192.168.31.104:8554/live -t 5 -f null -

# 网络超时设置
ffmpeg -timeout 10000000 -rtsp_transport tcp \
  -i rtsp://192.168.31.104:8554/live -t 5 -f null -

# 缓冲区大小调整
ffmpeg -buffer_size 64k -rtsp_transport tcp \
  -i rtsp://192.168.31.104:8554/live -t 5 -f null -

# 连接重试
ffmpeg -reconnect 1 -reconnect_streamed 1 -rtsp_transport tcp \
  -i rtsp://192.168.31.104:8554/live -t 5 -f null -
```

## 预期结果分析

### 成功的输出示例：
```
Input #0, rtsp, from 'rtsp://192.168.31.104:8554/live':
  Metadata:
    title           : ESP32 H.264 Camera Stream
  Duration: N/A, start: 0.000000, bitrate: N/A
  Program 1 
    Stream #0:0: Video: h264, yuv420p, 640x480, 90k tbr, 90k tbn
```

### H.264特定信息：
- Codec: h264
- Profile: Baseline, Main, or High
- Level: 3.0 or higher  
- Pixel format: yuv420p
- Frame rate: ~20 fps
- Bitrate: 取决于内容复杂度

### 可能的错误和解决方法：

1. **连接被拒绝**：
   - 检查ESP32是否运行
   - 验证IP地址和端口
   - 确认WiFi连接

2. **协议不支持**：
   - 尝试不同的rtsp_transport选项
   - 检查RTSP版本兼容性

3. **解码错误**：
   - 可能是H.264流格式问题
   - 检查NAL单元完整性
   - 验证SPS/PPS参数

4. **网络超时**：
   - 增加timeout参数
   - 检查网络稳定性
   - 调整缓冲区大小

## 快速测试脚本

```bash
#!/bin/bash
RTSP_URL="rtsp://192.168.31.104:8554/live"

echo "Testing RTSP H.264 stream..."
echo "1. Basic connectivity test:"
timeout 10s ffprobe -rtsp_transport tcp "$RTSP_URL" 2>&1 | head -20

echo -e "\n2. 5-second playback test:"
timeout 10s ffplay -rtsp_transport tcp -t 5 -autoexit -v quiet "$RTSP_URL"

echo -e "\n3. Performance test (10s):"
timeout 15s ffmpeg -rtsp_transport tcp -i "$RTSP_URL" -t 10 -f null - -v error -stats

echo -e "\nTest complete!"
```