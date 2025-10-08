#!/bin/bash

# H.264 RTP 测试构建脚本

set -e

echo "=== ESP32 H.264 RTP Test Build Script ==="
echo "Current directory: $(pwd)"
echo "Date: $(date)"
echo

# 检查IDF环境
if [ -z "$IDF_PATH" ]; then
    echo "ERROR: IDF_PATH not set. Please source ESP-IDF environment first."
    echo "Example: source \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

echo "✓ ESP-IDF Path: $IDF_PATH"

# 检查当前main.c中的测试模式设置
echo "Checking H.264 test mode configuration in main.c..."
if grep -q "#define ENABLE_H264_TEST_MODE.*1" main/main.c; then
    echo "✓ H.264 test mode is already enabled in main.c"
else
    echo "✗ H.264 test mode is not enabled in main.c"
    echo "Please enable it by setting: #define ENABLE_H264_TEST_MODE 1"
    echo "in main/main.c around line 40"
    exit 1
fi

# 构建项目
echo
echo "Building H.264 RTP test version..."
idf.py build

if [ $? -eq 0 ]; then
    echo
    echo "✓ Build successful!"
    echo
    echo "Next steps:"
    echo "1. Flash the firmware:"
    echo "   idf.py flash"
    echo
    echo "2. Monitor the output:"
    echo "   idf.py monitor"
    echo
    echo "3. Run the test client:"
    echo "   python3 test_h264_rtp.py"
    echo
    echo "4. To disable test mode later, edit main/main.c:"
    echo "   Change: #define ENABLE_H264_TEST_MODE 1"
    echo "   To:     #define ENABLE_H264_TEST_MODE 0"
    echo
else
    echo
    echo "✗ Build failed!"
    echo "Check the error messages above for details"
    exit 1
fi