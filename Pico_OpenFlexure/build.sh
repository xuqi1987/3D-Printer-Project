#!/bin/bash

# Pico OpenFlexure 编译脚本
# 用于编译Sangaboard兼容固件

echo "=== Pico OpenFlexure 编译脚本 ==="
echo

# 检查当前目录
if [ ! -f "platformio.ini" ]; then
    echo "错误: 请在项目根目录运行此脚本"
    echo "当前目录: $(pwd)"
    exit 1
fi

echo "开始编译固件..."
echo "项目目录: $(pwd)"
echo

# 清理之前的构建
echo "1. 清理之前的构建文件..."
pio run --target clean

echo
echo "2. 开始编译..."
pio run

# 检查编译结果
if [ $? -eq 0 ]; then
    echo
    echo "✅ 编译成功!"
    echo
    echo "生成的文件:"
    echo "  - firmware.uf2: $(pwd)/.pio/build/pico/firmware.uf2"
    echo "  - firmware.elf: $(pwd)/.pio/build/pico/firmware.elf"
    echo "  - firmware.bin: $(pwd)/.pio/build/pico/firmware.bin"
    echo
    echo "下一步: 运行 ./upload.sh 上传固件到Pico"
else
    echo
    echo "❌ 编译失败!"
    echo "请检查代码错误并重新编译"
    exit 1
fi
