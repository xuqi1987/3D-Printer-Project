#!/bin/bash

# 简单的ESP32 RTSP连接测试

ESP32_IP="192.168.31.104"
RTSP_PORT="8554"
STREAM_NAME="live"

echo "ESP32 H.264 RTSP Quick Test"
echo "==========================="
echo "Target: rtsp://${ESP32_IP}:${RTSP_PORT}/${STREAM_NAME}"
echo "Time: $(date)"
echo

# 1. 检查网络连通性
echo "1. Network connectivity test:"
if ping -c 1 -W 3000 "$ESP32_IP" >/dev/null 2>&1; then
    echo "   ✅ ESP32 is reachable at $ESP32_IP"
else
    echo "   ❌ ESP32 is not reachable at $ESP32_IP"
    echo "   💡 Check WiFi connection or IP address"
    exit 1
fi

# 2. 检查RTSP端口
echo "2. RTSP port test:"
if timeout 3s bash -c "</dev/tcp/$ESP32_IP/$RTSP_PORT" 2>/dev/null; then
    echo "   ✅ RTSP port $RTSP_PORT is open"
else
    echo "   ❌ RTSP port $RTSP_PORT is not accessible"
    echo "   💡 Check if RTSP server is running"
    exit 1
fi

# 3. FFmpeg可用性检查
echo "3. FFmpeg availability:"
if command -v ffmpeg >/dev/null 2>&1; then
    echo "   ✅ FFmpeg found: $(ffmpeg -version | head -n1)"
else
    echo "   ❌ FFmpeg not found"
    echo "   💡 Install FFmpeg first: brew install ffmpeg"
    exit 1
fi

# 4. 基本RTSP OPTIONS测试
echo "4. RTSP OPTIONS test:"
RTSP_URL="rtsp://${ESP32_IP}:${RTSP_PORT}/${STREAM_NAME}"
OPTIONS_RESULT=$(timeout 5s ffmpeg -v quiet -i "$RTSP_URL" -f null - 2>&1)
if [ $? -eq 0 ] || echo "$OPTIONS_RESULT" | grep -q "h264\|H264"; then
    echo "   ✅ RTSP stream detected"
else
    echo "   ⚠️  RTSP response received but may have issues"
    echo "   Details: $OPTIONS_RESULT"
fi

echo
echo "Ready for FFmpeg testing! Try these commands:"
echo
echo "Basic info:      ffprobe -rtsp_transport tcp '$RTSP_URL'"
echo "Quick play:      ffplay -rtsp_transport tcp '$RTSP_URL'"  
echo "Performance:     ffmpeg -rtsp_transport tcp -i '$RTSP_URL' -t 10 -f null -"
echo "Save stream:     ffmpeg -rtsp_transport tcp -i '$RTSP_URL' -t 10 -c copy test.mp4"
echo
echo "Full test suite: ./test_ffmpeg_rtsp.sh"