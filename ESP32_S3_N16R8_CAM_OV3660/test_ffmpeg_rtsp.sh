#!/bin/bash

# FFmpeg RTSP H.264 测试脚本集合

ESP32_IP="192.168.31.104"  # 修改为你的ESP32 IP地址
RTSP_PORT="8554"
STREAM_NAME="live"
RTSP_URL="rtsp://${ESP32_IP}:${RTSP_PORT}/${STREAM_NAME}"

echo "FFmpeg RTSP H.264 Test Suite"
echo "============================="
echo "Target: $RTSP_URL"
echo "Date: $(date)"
echo

# 测试1: 基本连接和信息探测
echo "Test 1: RTSP Stream Information"
echo "------------------------------"
ffprobe -v quiet -print_format json -show_streams -show_format "$RTSP_URL" 2>/dev/null || {
    echo "❌ Stream probe failed - checking with verbose output:"
    ffprobe -rtsp_transport tcp "$RTSP_URL"
}
echo

# 测试2: 10秒视频播放测试  
echo "Test 2: 10-second Playback Test"
echo "-------------------------------"
echo "Playing stream for 10 seconds..."
ffplay -rtsp_transport tcp -i "$RTSP_URL" -t 10 -autoexit -v quiet 2>/dev/null || {
    echo "❌ FFplay failed - trying with verbose output:"
    ffplay -rtsp_transport tcp -i "$RTSP_URL" -t 10 -autoexit
}
echo

# 测试3: 保存10秒H.264数据到文件
echo "Test 3: Save H.264 Stream to File"
echo "---------------------------------"
echo "Saving 10 seconds to test_output.h264..."
ffmpeg -y -rtsp_transport tcp -i "$RTSP_URL" -t 10 -c copy -f h264 test_output.h264 -v quiet || {
    echo "❌ Save failed - trying with verbose output:"
    ffmpeg -y -rtsp_transport tcp -i "$RTSP_URL" -t 10 -c copy -f h264 test_output.h264
}

if [ -f "test_output.h264" ]; then
    FILE_SIZE=$(wc -c < test_output.h264)
    echo "✅ Saved ${FILE_SIZE} bytes to test_output.h264"
    
    # 分析保存的H.264文件
    echo "Analyzing saved H.264 file..."
    ffprobe -v quiet -show_streams test_output.h264 2>/dev/null || echo "File analysis failed"
fi
echo

# 测试4: 帧率和质量分析
echo "Test 4: Frame Rate and Quality Analysis"
echo "--------------------------------------"
echo "Analyzing stream for 15 seconds..."
ffmpeg -rtsp_transport tcp -i "$RTSP_URL" -t 15 -f null - -v info 2>&1 | grep -E "(fps|frame=|bitrate)" || {
    echo "Analysis with full output:"
    ffmpeg -rtsp_transport tcp -i "$RTSP_URL" -t 15 -f null -
}
echo

# 测试5: 网络性能测试
echo "Test 5: Network Performance Test" 
echo "--------------------------------"
echo "Testing network throughput for 20 seconds..."
timeout 20s ffmpeg -rtsp_transport tcp -i "$RTSP_URL" -c copy -f null - -v error -stats 2>&1 | tail -5
echo

# 测试6: 错误恢复测试
echo "Test 6: Error Recovery Test"
echo "---------------------------"
echo "Testing connection resilience (multiple short connections)..."
for i in {1..3}; do
    echo "Connection attempt $i/3:"
    timeout 5s ffmpeg -rtsp_transport tcp -i "$RTSP_URL" -t 3 -f null - -v quiet 2>/dev/null && echo "  ✅ Success" || echo "  ❌ Failed"
    sleep 1
done
echo

echo "All tests completed!"
echo "===================="

if [ -f "test_output.h264" ]; then
    echo "Generated files:"
    echo "  test_output.h264 - Saved H.264 stream ($(wc -c < test_output.h264) bytes)"
    echo
    echo "To replay saved file:"
    echo "  ffplay test_output.h264"
fi

echo
echo "Manual test commands:"
echo "  Basic play:     ffplay -rtsp_transport tcp '$RTSP_URL'"
echo "  Info only:      ffprobe -rtsp_transport tcp '$RTSP_URL'"
echo "  Save stream:    ffmpeg -rtsp_transport tcp -i '$RTSP_URL' -c copy output.mp4"
echo "  Bandwidth test: ffmpeg -rtsp_transport tcp -i '$RTSP_URL' -t 30 -f null -"