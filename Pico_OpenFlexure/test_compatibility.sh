#!/bin/bash

# Sangaboard兼容性测试脚本
# 使用screen命令与Pico通信

echo "=== Sangaboard兼容性测试 ==="
echo "请确保Pico已连接并上传了固件"
echo

# 查找Pico设备
PICO_PORT=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1)

if [ -z "$PICO_PORT" ]; then
    echo "错误: 未找到Pico设备"
    echo "请检查:"
    echo "1. Pico是否已连接"
    echo "2. 固件是否已上传"
    echo "3. 是否需要按BOOTSEL重新上传固件"
    exit 1
fi

echo "找到Pico设备: $PICO_PORT"
echo

# 测试命令列表
declare -a commands=(
    "version"
    "board" 
    "help"
    "p?"
    "zero"
    "p?"
    "mr 100 0 0"
    "p?"
    "mrx 50"
    "p?"
    "mry -30"
    "p?"
    "mrz 20"
    "p?"
    "dt 2000"
    "dt?"
    "ramp_time 5000"
    "ramp_time?"
    "release"
    "test_mode on"
    "test_mode off"
)

declare -a descriptions=(
    "获取版本信息"
    "获取开发板信息"
    "获取帮助信息"
    "获取位置"
    "重置位置"
    "验证位置重置"
    "X轴相对移动100步"
    "检查X轴位置"
    "X轴相对移动50步"
    "检查X轴位置"
    "Y轴反向移动30步"
    "检查Y轴位置"
    "Z轴移动20步"
    "检查Z轴位置"
    "设置最小步进延时为2000微秒"
    "查询最小步进延时"
    "设置加速时间为5000微秒"
    "查询加速时间"
    "释放电机"
    "开启测试模式"
    "关闭测试模式"
)

# 创建临时文件用于存储响应
TEMP_FILE="/tmp/pico_response.txt"

echo "开始兼容性测试..."
echo "按 Ctrl+C 可随时停止测试"
echo

for i in "${!commands[@]}"; do
    cmd="${commands[$i]}"
    desc="${descriptions[$i]}"
    
    echo "[$((i+1))/${#commands[@]}] 测试: $cmd - $desc"
    
    # 发送命令并等待响应
    echo "$cmd" > "$PICO_PORT"
    sleep 1
    
    # 读取响应 (这里只是简单延时，实际响应需要通过串口监视器查看)
    echo "  -> 命令已发送"
    echo
done

echo "=== 兼容性测试完成 ==="
echo
echo "要查看实际响应，请使用以下命令打开串口监视器："
echo "screen $PICO_PORT 115200"
echo
echo "或者使用PlatformIO的串口监视器："
echo "cd /Users/xuqi/Proj/3D-Printer-Project/Pico_OpenFlexure && pio device monitor"
echo
echo "手动测试命令示例："
echo "1. version  - 应返回版本信息"
echo "2. board    - 应返回 'pico_openflexure_v1.0'"
echo "3. help     - 应返回完整命令列表"
echo "4. p?       - 应返回三个数字 (当前位置)"
echo "5. mr 10 0 0 - X轴移动10步，应返回 'done.'"
