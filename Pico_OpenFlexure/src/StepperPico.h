/*
 * StepperPico.h - 树莓派Pico专用的步进电机控制库
 * 
 * 专为28BYJ-48单极步进电机设计
 * 基于Sangaboard的StepperF_alt库改编
 */

#ifndef STEPPER_PICO_H
#define STEPPER_PICO_H

#include <Arduino.h>

class StepperPico {
  public:
    // 构造函数，输入4个控制引脚
    StepperPico(const int pins[4]);
    
    // 设置转速（RPM）
    void setSpeed(long rpm);
    
    // 执行指定步数，direction: 1=正向, -1=反向
    void step(int direction);
    
    // 释放电机（所有引脚设为低电平）
    void release();
    
    // 获取当前步序号
    int getCurrentStep();
    
  private:
    void stepMotor(int step_sequence);
    
    int motor_pins[4];           // 电机控制引脚
    int current_step;            // 当前步序号 (0-7)
    unsigned long step_delay;    // 步进延时（微秒）
    unsigned long last_step_time; // 上次步进时间
    
    // 28BYJ-48步进电机的8步序列（半步进模式）
    static const int step_sequence[8][4];
};

#endif
