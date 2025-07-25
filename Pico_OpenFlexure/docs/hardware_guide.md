# 硬件连接指南

## 所需器件

### 主要组件
- 树莓派Pico开发板 × 1
- 28BYJ-48步进电机 × 3
- ULN2003驱动板 × 3 (可选，推荐)
- 面包板或万能板
- 杜邦线若干

### 电源要求
- Pico供电：5V USB或3.3V
- 步进电机供电：5V (建议外部电源)

## 连接方案

### 方案1：直接驱动 (简单但驱动能力有限)
```
Pico GPIO -> 28BYJ-48
GP2  -> 电机0 IN1
GP3  -> 电机0 IN2  
GP4  -> 电机0 IN3
GP5  -> 电机0 IN4

GP6  -> 电机1 IN1
GP7  -> 电机1 IN2
GP8  -> 电机1 IN3  
GP9  -> 电机1 IN4

GP10 -> 电机2 IN1
GP11 -> 电机2 IN2
GP12 -> 电机2 IN3
GP13 -> 电机2 IN4

5V   -> 所有电机VCC
GND  -> 所有电机GND
```

### 方案2：ULN2003驱动 (推荐)
```
Pico GPIO -> ULN2003 -> 28BYJ-48
GP2  -> ULN2003_0 IN1 -> 电机0 IN1
GP3  -> ULN2003_0 IN2 -> 电机0 IN2
GP4  -> ULN2003_0 IN3 -> 电机0 IN3  
GP5  -> ULN2003_0 IN4 -> 电机0 IN4

(其他电机类似连接)

5V   -> ULN2003 VCC
GND  -> ULN2003 GND
```

## 电机特性

### 28BYJ-48参数
- 工作电压：5V DC
- 步进角度：5.625° (64步/圈)
- 减速比：1:64
- 实际步数：4096步/圈
- 扭矩：约34.3 mN·m

### 控制序列
半步进8步序列：
```
步骤 | IN1 | IN2 | IN3 | IN4
-----|-----|-----|-----|-----
  0  |  1  |  0  |  0  |  0
  1  |  1  |  1  |  0  |  0
  2  |  0  |  1  |  0  |  0
  3  |  0  |  1  |  1  |  0
  4  |  0  |  0  |  1  |  0
  5  |  0  |  0  |  1  |  1
  6  |  0  |  0  |  0  |  1
  7  |  1  |  0  |  0  |  1
```

## 接线图

```
     Pico                    28BYJ-48电机
   ┌─────────┐              ┌─────────────┐
   │   GP2   ├──────────────┤ IN1  (橙)   │
   │   GP3   ├──────────────┤ IN2  (黄)   │
   │   GP4   ├──────────────┤ IN3  (粉)   │
   │   GP5   ├──────────────┤ IN4  (蓝)   │
   │   5V    ├──────────────┤ VCC  (红)   │
   │   GND   ├──────────────┤ GND  (黑)   │
   └─────────┘              └─────────────┘
```

## 安装建议

### 机械安装
1. 将步进电机固定在合适的支架上
2. 确保电机轴与负载正确耦合
3. 避免机械卡死和过载

### 电气安装  
1. 使用足够粗的导线（≥22AWG）
2. 电源线要可靠连接
3. 信号线尽量短且远离电源线
4. 添加电源滤波电容（可选）

### 软件配置
1. 根据实际连接修改引脚定义
2. 调整步进速度和加速度
3. 校准机械行程限制

## 故障排除

### 电机不转
- 检查供电电压（应为5V）
- 检查接线是否正确
- 检查代码中引脚定义

### 转动不平稳
- 降低步进速度
- 检查机械负载
- 确认步进序列正确

### 位置不准确
- 检查机械间隙
- 避免失步（降低速度）
- 确认步数计算正确

### 发热严重
- 添加散热措施
- 减少持续通电时间
- 使用release()命令释放电机
