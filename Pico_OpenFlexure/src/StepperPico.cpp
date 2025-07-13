/*
 * StepperPico.cpp - 树莓派Pico专用的步进电机控制库实现
 * 
 * 专为28BYJ-48单极步进电机设计
 * 基于Sangaboard的StepperF_alt库改编
 */

#include "StepperPico.h"

// 28BYJ-48步进电机的8步序列（半步进模式）
// 每个子数组表示一个步进状态下4个引脚的电平
const int StepperPico::step_sequence[8][4] = {
  {1, 0, 0, 0},  // 步骤 0
  {1, 1, 0, 0},  // 步骤 1
  {0, 1, 0, 0},  // 步骤 2
  {0, 1, 1, 0},  // 步骤 3
  {0, 0, 1, 0},  // 步骤 4
  {0, 0, 1, 1},  // 步骤 5
  {0, 0, 0, 1},  // 步骤 6
  {1, 0, 0, 1}   // 步骤 7
};

StepperPico::StepperPico(const int pins[4]) {
  // 复制引脚配置
  for (int i = 0; i < 4; i++) {
    motor_pins[i] = pins[i];
    pinMode(motor_pins[i], OUTPUT);
    digitalWrite(motor_pins[i], LOW);
  }
  
  current_step = 0;
  step_delay = 1000; // 默认1000微秒延时
  last_step_time = 0;
}

void StepperPico::setSpeed(long rpm) {
  // 计算步进延时
  // 28BYJ-48电机：64步/圈 * 64:1减速比 = 4096步/圈
  // 半步进模式：4096 * 2 = 8192步/圈
  if (rpm > 0) {
    step_delay = 60L * 1000L * 1000L / (8192L * rpm);
  }
}

void StepperPico::step(int direction) {
  unsigned long current_time = micros();
  
  // 检查是否到了下一步的时间
  if (current_time - last_step_time >= step_delay) {
    // 更新步序号
    if (direction > 0) {
      current_step++;
      if (current_step >= 8) {
        current_step = 0;
      }
    } else {
      current_step--;
      if (current_step < 0) {
        current_step = 7;
      }
    }
    
    // 执行步进
    stepMotor(current_step);
    last_step_time = current_time;
  }
}

void StepperPico::stepMotor(int step_sequence_index) {
  // 设置4个引脚的电平状态
  for (int i = 0; i < 4; i++) {
    digitalWrite(motor_pins[i], step_sequence[step_sequence_index][i]);
  }
}

void StepperPico::release() {
  // 释放电机 - 所有引脚设为低电平
  for (int i = 0; i < 4; i++) {
    digitalWrite(motor_pins[i], LOW);
  }
}

int StepperPico::getCurrentStep() {
  return current_step;
}
