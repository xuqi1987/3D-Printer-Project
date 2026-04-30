#!/bin/bash

# Pico OpenFlexure 上传脚本
# 用于上传固件到Raspberry Pi Pico

echo "=== Pico OpenFlexure 上传脚本 ==="
echo

# 检查固件文件是否存在
UF2_FILE=".pio/build/pico/firmware.uf2"

if [ ! -f "$UF2_FILE" ]; then
    echo "错误: 固件文件不存在"
    echo "请先运行 ./build.sh 编译固件"
    exit 1
fi

echo "固件文件: $UF2_FILE"
echo "文件大小: $(du -h $UF2_FILE | cut -f1)"
echo

# 检查Pico是否在BOOTSEL模式
echo "正在检查Pico连接状态..."

# 查找RPI-RP2挂载点
RPI_MOUNT=$(ls -d /Volumes/RPI-RP2 2>/dev/null)

if [ -z "$RPI_MOUNT" ]; then
    echo
    echo "⚠️  未检测到Pico的BOOTSEL模式"
    echo
    echo "请按以下步骤操作:"
    echo "1. 按住Pico上的BOOTSEL按钮"
    echo "2. 插入USB线连接到电脑"
    echo "3. 松开BOOTSEL按钮"
    echo "4. 应该会看到RPI-RP2磁盘出现"
    echo
    echo "完成后按回车继续，或按Ctrl+C取消"
    read -p ""
    
    # 再次检查
    RPI_MOUNT=$(ls -d /Volumes/RPI-RP2 2>/dev/null)
    if [ -z "$RPI_MOUNT" ]; then
        echo "❌ 仍未检测到BOOTSEL模式，请重试"
        exit 1
    fi
fi

echo "✅ 检测到Pico BOOTSEL模式: $RPI_MOUNT"
echo

# 复制固件文件
echo "正在上传固件..."
cp "$UF2_FILE" "$RPI_MOUNT/"

if [ $? -eq 0 ]; then
    echo "✅ 固件上传成功!"
    echo
    echo "Pico将自动重启并运行新固件"
    echo
    
    # 等待Pico重启
    echo "等待Pico重启..."
    sleep 3
    
    # 检查串口设备
    echo "正在检查串口连接..."
    sleep 2
    
    PICO_PORT=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1)
    if [ -n "$PICO_PORT" ]; then
        echo "✅ Pico已重启，串口: $PICO_PORT"
        echo
        echo "测试连接:"
        echo "  pio device monitor"
        echo "或者:"
        echo "  screen $PICO_PORT 115200"
        echo
        echo "测试命令: version, board, help, p?"
    else
        echo "⚠️  暂未检测到串口，请稍等片刻"
        echo "如果仍无法连接，请重新插拔USB线"
    fi
else
    echo "❌ 固件上传失败!"
    exit 1
fi
