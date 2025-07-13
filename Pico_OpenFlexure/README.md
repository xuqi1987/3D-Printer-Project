# Pico OpenFlexure 步进电机控制器

基于树莓派Pico（RP2040）的步进电机控制器，实现类似Sangaboard的功能，专为OpenFlexure显微镜设计。

## 硬件特性

- **微控制器**: 树莓派Pico (RP2040)
- **支持电机**: 28BYJ-48单极步进电机
- **控制轴数**: 3轴 (X, Y, Z)
- **通信接口**: USB串口
- **工作电压**: 3.3V/5V

## 引脚连接

### 步进电机连接
```
电机0 (X轴): GP2, GP3, GP4, GP5
电机1 (Y轴): GP6, GP7, GP8, GP9  
电机2 (Z轴): GP10, GP11, GP12, GP13
```

### 28BYJ-48接线说明
```
步进电机引脚 -> Pico引脚
IN1 -> GPx (第1个引脚)
IN2 -> GPx+1 (第2个引脚)
IN3 -> GPx+2 (第3个引脚)
IN4 -> GPx+3 (第4个引脚)
VCC -> 5V (外部电源)
GND -> GND
```

## 软件特性

- 串口命令控制界面
- 相对和绝对位置移动
- 可调速度控制
- 位置记录和恢复
- 测试模式
- 兼容Sangaboard命令集

## 开发环境

### PlatformIO (推荐)
1. 安装VS Code和PlatformIO扩展
2. 打开项目文件夹
3. 构建并上传到Pico

### Arduino IDE
1. 安装Arduino IDE 2.0+
2. 添加Raspberry Pi Pico支持包
3. 选择开发板：Raspberry Pi Pico
4. 编译并上传

## 命令接口

通过串口（115200波特率）发送命令：

### 基本命令
- `help?` - 显示帮助信息
- `version?` - 显示版本信息
- `board?` - 显示开发板信息
- `position?` - 显示当前位置

### 移动命令
- `move_rel x y z` - 相对移动（步数）
- `move_abs x y z` - 绝对移动到指定位置
- `zero` - 重置位置为零点

### 控制命令
- `set_speed speed` - 设置步进速度（微秒延时）
- `release` - 释放所有电机
- `save_pos` - 保存当前位置
- `test_mode` - 切换测试模式

### 使用示例
```
move_rel 100 0 0     # X轴正向移动100步
move_abs 0 0 500     # Z轴移动到500步位置
set_speed 2000       # 设置步进延时为2000微秒
position?            # 查询当前位置
```

## 兼容性

- **与Sangaboard兼容**: 支持大部分Sangaboard命令
- **OpenFlexure软件**: 兼容OpenFlexure控制软件
- **Python接口**: 可使用现有的Python控制库

## 性能参数

- **步进精度**: 半步进模式，4096步/圈
- **最大速度**: 约15 RPM（取决于负载）
- **响应时间**: < 1ms
- **位置精度**: ±1步

## 文件结构

```
src/
├── main.cpp          # 主程序文件
├── StepperPico.h     # 步进电机库头文件
└── StepperPico.cpp   # 步进电机库实现
platformio.ini        # PlatformIO配置
README.md             # 项目说明
```

## 许可证

本项目基于GPL v3许可证发布，继承自Sangaboard项目。

## 致谢

- 基于OpenFlexure项目的Sangaboard固件
- 感谢Richard Bowman和OpenFlexure团队的开源贡献
