#!/bin/bash

# H.264 RTP 测试模式切换脚本

set -e

MAIN_FILE="main/main.c"
DEFINE_LINE="#define ENABLE_H264_TEST_MODE"

if [ ! -f "$MAIN_FILE" ]; then
    echo "Error: $MAIN_FILE not found"
    exit 1
fi

# 检查当前状态
if grep -q "${DEFINE_LINE}.*1" "$MAIN_FILE"; then
    CURRENT_STATE="ENABLED"
else
    CURRENT_STATE="DISABLED"
fi

echo "H.264 RTP Test Mode Switcher"
echo "============================"
echo "Current state: $CURRENT_STATE"
echo

# 如果没有提供参数，显示帮助
if [ $# -eq 0 ]; then
    echo "Usage: $0 [enable|disable|status]"
    echo
    echo "Commands:"
    echo "  enable   - Enable H.264 test mode"
    echo "  disable  - Disable H.264 test mode (normal camera mode)"
    echo "  status   - Show current status"
    echo
    echo "Current state: $CURRENT_STATE"
    exit 0
fi

case "$1" in
    "enable")
        if [ "$CURRENT_STATE" = "ENABLED" ]; then
            echo "H.264 test mode is already enabled"
        else
            echo "Enabling H.264 test mode..."
            sed -i.bak "s/${DEFINE_LINE}.*/${DEFINE_LINE}      1/" "$MAIN_FILE"
            echo "✓ H.264 test mode enabled"
            echo "  - Synthetic H.264 data will be generated"
            echo "  - Camera hardware will be skipped"
            echo "  - Build and flash to test RTP implementation"
        fi
        ;;
    "disable")
        if [ "$CURRENT_STATE" = "DISABLED" ]; then
            echo "H.264 test mode is already disabled"
        else
            echo "Disabling H.264 test mode..."
            sed -i.bak "s/${DEFINE_LINE}.*/${DEFINE_LINE}      0/" "$MAIN_FILE"
            echo "✓ H.264 test mode disabled"
            echo "  - Normal camera operation restored"
            echo "  - Real camera data will be used"
            echo "  - Build and flash for production use"
        fi
        ;;
    "status")
        echo "Current H.264 test mode status: $CURRENT_STATE"
        if [ "$CURRENT_STATE" = "ENABLED" ]; then
            echo "  - Generates synthetic H.264 NAL units"
            echo "  - Tests RFC 6184 RTP packetization"
            echo "  - Camera hardware disabled"
        else
            echo "  - Uses real camera data"
            echo "  - Normal production operation"
            echo "  - Camera hardware enabled"
        fi
        ;;
    *)
        echo "Error: Unknown command '$1'"
        echo "Use: $0 [enable|disable|status]"
        exit 1
        ;;
esac

echo
echo "Next steps:"
echo "1. Build: idf.py build"
echo "2. Flash: idf.py flash"
echo "3. Test:  python3 test_h264_rtp.py"