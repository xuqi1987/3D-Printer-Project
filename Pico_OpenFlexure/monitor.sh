#!/bin/bash

# Pico OpenFlexure 串口监视器脚本
# 用于连接和测试Pico固件

echo "=== Pico OpenFlexure 串口监视器 ==="
echo

# 查找Pico串口
echo "正在查找Pico设备..."
PICO_PORT=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1)

if [ -z "$PICO_PORT" ]; then
    echo "❌ 未找到Pico串口设备"
    echo
    echo "请检查:"
    echo "1. Pico是否已连接USB"
    echo "2. 固件是否已正确上传"
    echo "3. 是否需要重新插拔USB线"
    echo
    echo "可用串口设备:"
    ls /dev/tty.* | grep -E "(usb|acm)" 2>/dev/null || echo "  (无)"
    exit 1
fi

echo "✅ 找到Pico设备: $PICO_PORT"
echo

# 选择监视器类型
echo "选择串口监视器:"
echo "1. PlatformIO监视器 (推荐)"
echo "2. Screen终端"
echo "3. 简单测试命令"
echo

read -p "请选择 (1-3): " choice

case $choice in
    1)
        echo "启动PlatformIO串口监视器..."
        echo "按 Ctrl+C 退出"
        echo
        pio device monitor
        ;;
    2)
        echo "启动Screen串口监视器..."
        echo "按 Ctrl+A 然后按 K 退出"
        echo
        screen $PICO_PORT 115200
        ;;
    3)
        echo "执行简单测试..."
        echo
        
        # 测试基本命令
        test_commands=("version" "board" "p?" "help")
        
        for cmd in "${test_commands[@]}"; do
            echo "测试命令: $cmd"
            echo "$cmd" > $PICO_PORT
            sleep 1
            echo "  -> 命令已发送"
            echo
        done
        
        echo "测试完成！"
        echo "要查看响应，请选择选项1或2打开串口监视器"
        ;;
    *)
        echo "无效选择"
        exit 1
        ;;
esac
