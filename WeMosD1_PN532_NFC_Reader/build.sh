#!/bin/bash

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息函数
print_message() {
    echo -e "${2}${1}${NC}"
}

# 检查命令是否成功执行
check_status() {
    if [ $? -eq 0 ]; then
        print_message "✓ $1" "$GREEN"
    else
        print_message "✗ $1" "$RED"
        exit 1
    fi
}

# 清理项目
print_message "正在清理项目..." "$YELLOW"
pio run -t clean
check_status "清理完成"

# 编译项目
print_message "正在编译项目..." "$YELLOW"
pio run
check_status "编译完成"

# 上传到设备
print_message "正在上传到设备..." "$YELLOW"
pio run -t upload
check_status "上传完成"

print_message "所有操作已完成！" "$GREEN" 