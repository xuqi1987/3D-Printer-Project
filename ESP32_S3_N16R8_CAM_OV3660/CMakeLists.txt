# ESP32-S3 H.264 RTSP Camera Server Project
cmake_minimum_required(VERSION 3.16)

# 设置项目信息
set(PROJECT_NAME "esp32_h264_rtsp_camera")
set(PROJECT_VER "3.0.0")

# 设置组件目录
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)

# 添加编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

# 包含ESP-IDF构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})
