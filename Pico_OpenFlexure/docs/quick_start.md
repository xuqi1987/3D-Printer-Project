# 快速开始指南

## 安装开发环境

### 方法1：使用VS Code + PlatformIO（推荐）

1. 安装VS Code
2. 安装PlatformIO IDE扩展
3. 打开项目文件夹
4. 点击PlatformIO侧边栏中的"Build"构建项目

### 方法2：使用Arduino IDE

1. 安装Arduino IDE 2.0+
2. 添加RP2040支持包：
   - 文件 -> 首选项 -> 附加开发板管理器网址
   - 添加：`https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
3. 工具 -> 开发板 -> 开发板管理器，搜索"rp2040"并安装
4. 选择开发板：Raspberry Pi Pico
5. 复制src目录下的代码到Arduino IDE

## 硬件连接

### 基本连接方案

```
树莓派Pico GPIO -> 28BYJ-48步进电机

电机0 (X轴):
GP2  -> IN1 (橙色线)
GP3  -> IN2 (黄色线)  
GP4  -> IN3 (粉色线)
GP5  -> IN4 (蓝色线)

电机1 (Y轴):
GP6  -> IN1
GP7  -> IN2
GP8  -> IN3
GP9  -> IN4

电机2 (Z轴):
GP10 -> IN1
GP11 -> IN2
GP12 -> IN3
GP13 -> IN4

电源:
5V -> 所有电机VCC (红色线)
GND -> 所有电机GND (黑色线)
```

### 推荐使用ULN2003驱动板

每个28BYJ-48电机配一个ULN2003驱动板：
- 提供更好的驱动能力
- 保护Pico GPIO
- 更稳定的性能

## 上传固件

### PlatformIO
```bash
pio run --target upload
```

### Arduino IDE
1. 连接Pico到电脑（按住BOOTSEL按钮重新插入）
2. 选择端口（通常显示为"UF2 Board"）
3. 点击上传

## 测试控制器

### 使用串口监视器

1. 打开串口监视器（115200波特率）
2. 发送命令测试：

```
help?              # 查看帮助
version?           # 查看版本
position?          # 查看当前位置
move_rel 100 0 0   # X轴移动100步
move_rel 0 100 0   # Y轴移动100步
move_rel 0 0 100   # Z轴移动100步
zero               # 重置位置
```

### 使用Python测试脚本

```bash
cd examples
python3 test_controller.py          # 自动测试
python3 test_controller.py -i       # 交互模式
```

## 常见问题

### Q: 电机不转动
A: 
- 检查5V电源供应
- 确认接线正确
- 检查电机是否卡死

### Q: 转动不平稳
A:
- 降低速度：`set_speed 2000`
- 检查机械负载
- 确保接线牢固

### Q: 上传失败
A:
- 按住BOOTSEL重新连接
- 检查USB线缆
- 确认选择正确端口

## 高级配置

### 修改引脚定义

编辑`src/main.cpp`中的引脚定义：

```cpp
// 步进电机引脚定义
const int motor0_pins[] = {2, 3, 4, 5};    // 电机0
const int motor1_pins[] = {6, 7, 8, 9};    // 电机1 
const int motor2_pins[] = {10, 11, 12, 13}; // 电机2
```

### 调整速度参数

```cpp
long min_step_delay = 1000; // 步进延时（微秒）
```

数值越小速度越快，但可能导致失步。

### 添加终点开关

可在代码中添加终点开关支持：

```cpp
// 添加终点开关引脚定义
const int endstop_pins[] = {14, 15, 16}; // X, Y, Z轴终点开关
```

## 与OpenFlexure软件集成

本控制器兼容OpenFlexure控制软件，可以直接替换Sangaboard使用。

主要兼容的命令：
- `move_rel x y z` - 相对移动
- `move_abs x y z` - 绝对移动  
- `position?` - 查询位置
- `zero` - 零位重置

## 性能指标

- **控制精度**: 半步进，4096步/圈
- **最大速度**: ~15 RPM
- **响应延迟**: <1ms
- **USB通信**: 115200波特率
- **内存使用**: RAM 15.1%, Flash 0.2%
