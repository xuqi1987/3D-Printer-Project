#!/bin/bash

# ESP32-S3 MJPEG RTSP Camera - Build and Flash Script
# Usage: ./build_flash.sh [monitor]

set -e  # Exit on any error

PROJECT_DIR="/Users/xuqi/Proj/3D-Printer-Project/ESP32_S3_N16R8_CAM_OV3660"
ESP_IDF_PATH="/Users/xuqi/esp/v5.4.2/esp-idf"
SERIAL_PORT="/dev/cu.usbserial-210"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== ESP32-S3 MJPEG RTSP Camera Build Script ===${NC}"
echo -e "Project: $PROJECT_DIR"
echo -e "Serial Port: $SERIAL_PORT"
echo ""

# Check if serial port exists
if [ ! -e "$SERIAL_PORT" ]; then
    echo -e "${RED}Error: Serial port $SERIAL_PORT not found${NC}"
    echo "Available ports:"
    ls /dev/cu.* 2>/dev/null || echo "No serial ports found"
    exit 1
fi

# Navigate to project directory
cd "$PROJECT_DIR"

# Source ESP-IDF environment
echo -e "${YELLOW}Setting up ESP-IDF environment...${NC}"
source "$ESP_IDF_PATH/export.sh"

# Clean previous build (optional)
if [ "$1" = "clean" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    idf.py fullclean
fi

# Build project
echo -e "${YELLOW}Building project...${NC}"
if ! idf.py build; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Show build size
echo -e "${GREEN}Build completed successfully!${NC}"
ls -lh build/esp32_h264_rtsp_camera.bin

# Flash firmware
echo -e "${YELLOW}Flashing firmware to $SERIAL_PORT...${NC}"
if ! idf.py -p "$SERIAL_PORT" flash; then
    echo -e "${RED}Flash failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Firmware flashed successfully!${NC}"

# Start monitor if requested
if [ "$1" = "monitor" ] || [ "$2" = "monitor" ]; then
    echo -e "${YELLOW}Starting serial monitor...${NC}"
    echo -e "${BLUE}Press Ctrl+] to exit monitor${NC}"
    echo ""
    idf.py -p "$SERIAL_PORT" monitor
else
    echo -e "${BLUE}To view serial output, run: ./build_flash.sh monitor${NC}"
    echo -e "${BLUE}Or manually: idf.py -p $SERIAL_PORT monitor${NC}"
fi

echo -e "${GREEN}Done!${NC}"