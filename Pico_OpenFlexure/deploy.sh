#!/bin/bash

# Pico OpenFlexure 一键构建和上传脚本
# 编译、上传、测试一条龙服务

echo "=== Pico OpenFlexure 一键部署脚本 ==="
echo

# 检查项目目录
if [ ! -f "platformio.ini" ]; then
    echo "错误: 请在项目根目录运行此脚本"
    exit 1
fi

echo "🔨 步骤1: 编译固件"
echo "========================="
./build.sh

if [ $? -ne 0 ]; then
    echo "编译失败，停止执行"
    exit 1
fi

echo
echo "📤 步骤2: 上传固件"
echo "========================="
echo "请按BOOTSEL按钮连接Pico..."
echo
read -p "准备好后按回车继续，或按Ctrl+C取消: "

./upload.sh

if [ $? -ne 0 ]; then
    echo "上传失败，停止执行"
    exit 1
fi

echo
echo "🔍 步骤3: 测试连接"
echo "========================="
echo "等待Pico重启..."
sleep 5

# 检查串口
PICO_PORT=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1)
if [ -n "$PICO_PORT" ]; then
    echo "✅ 检测到串口: $PICO_PORT"
    echo
    echo "自动测试基本命令..."
    
    # 简单测试
    sleep 2
    echo "version" > $PICO_PORT
    sleep 1
    echo "board" > $PICO_PORT  
    sleep 1
    echo "p?" > $PICO_PORT
    sleep 1
    
    echo "✅ 测试命令已发送"
    echo
    echo "🎉 部署完成!"
    echo
    echo "下一步操作:"
    echo "1. 运行 ./monitor.sh 查看串口输出"
    echo "2. 运行 ./test_compatibility.sh 进行完整测试"
    echo "3. 开始使用Sangaboard兼容命令控制步进电机"
    echo
    echo "常用命令:"
    echo "  version  - 查看版本"
    echo "  help     - 查看帮助"
    echo "  p?       - 查看位置"
    echo "  mr 10 0 0 - X轴移动10步"
    echo "  release  - 释放电机"
else
    echo "⚠️  未检测到串口，请稍后手动测试"
fi
