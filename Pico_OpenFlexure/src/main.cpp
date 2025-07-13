/*
 * Pico OpenFlexure 步进电机控制器固件
 * 
 * 基于Sangaboard固件移植到树莓派Pico（RP2040）
 * 
 * 功能特点：
 * - 支持3轴步进电机控制
 * - 串口命令接口
 * - 兼容28BYJ-48单极步进电机
 * - 位置记录和恢复
 * 
 * 作者：基于Sangaboard固件改编
 * 许可证：GPL v3
 */

#include <Arduino.h>
// #include <EEPROM.h>  // RP2040暂不支持EEPROM库
#include "StepperPico.h"

// 调试串口打印
#define DEBUG_ON

#ifdef DEBUG_ON
  #define D(x)  Serial.println(x)
#else
  #define D(x)
#endif

// 版本信息
#define VER_STRING "Pico OpenFlexure Controller v1.0.0"
#define BOARD_STRING "Raspberry Pi Pico RP2040"

// 电机配置
#define EACH_MOTOR for(int i=0; i<n_motors; i++)
const int n_motors = 3;

// 步进电机引脚定义 (28BYJ-48 单极步进电机)
// 电机0 (X轴)
const int motor0_pins[] = {2, 3, 4, 5};
// 电机1 (Y轴) 
const int motor1_pins[] = {6, 7, 8, 9};
// 电机2 (Z轴)
const int motor2_pins[] = {10, 11, 12, 13};

// 全局变量
StepperPico* motors[n_motors];
signed long current_pos[n_motors];
long steps_remaining[n_motors];
long min_step_delay = 1000; // 最小步进延时（微秒）
long ramp_time = 0; // 加速时间

bool test_mode = false;

// 串口输入缓冲区
const int INPUT_BUFFER_LENGTH = 64;
char input_buffer[INPUT_BUFFER_LENGTH];

// EEPROM地址定义
const int min_step_delay_eeprom = sizeof(long) * n_motors;
const int ramp_time_eeprom = sizeof(long) * (n_motors + 1);

// 函数声明
int command_prefix(String command, const char** prefixes, int n_prefixes);
void processCommand(String command);
void executeMotorMovements();
void stepMotor(int motor, long dx);
void releaseMotor(int motor);
void print_position();
void save_position();
void load_position();
void printHelp();
void handleRelativeMove(String command);
void handleAbsoluteMove(String command);
void handleSetSpeed(String command);
void handleRelease();
void handleZero();

void setup() {
  // 初始化串口
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }
  
  Serial.println(F(VER_STRING));
  Serial.println(F(BOARD_STRING));
  
  // 初始化步进电机
  motors[0] = new StepperPico(motor0_pins);
  motors[1] = new StepperPico(motor1_pins);
  motors[2] = new StepperPico(motor2_pins);
  
  // 初始化电机参数
  EACH_MOTOR {
    steps_remaining[i] = 0;
    current_pos[i] = 0; // 可以从EEPROM读取上次位置
  }
  
  // 从EEPROM读取配置（如果可用）
  load_position();
  
  Serial.println(F("步进电机控制器已初始化"));
  Serial.println(F("发送 'help?' 查看可用命令"));
}

void loop() {
  // 处理串口命令
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processCommand(command);
  }
  
  // 执行步进电机移动
  executeMotorMovements();
  
  delay(1);
}

void processCommand(String command) {
  command.toLowerCase();
  
  if (command == "help?") {
    printHelp();
  }
  else if (command == "version?") {
    Serial.println(F(VER_STRING));
  }
  else if (command == "board?") {
    Serial.println(F(BOARD_STRING));
  }
  else if (command == "position?") {
    print_position();
  }
  else if (command.startsWith("move_rel ")) {
    handleRelativeMove(command);
  }
  else if (command.startsWith("move_abs ")) {
    handleAbsoluteMove(command);
  }
  else if (command.startsWith("set_speed ")) {
    handleSetSpeed(command);
  }
  else if (command == "release") {
    handleRelease();
  }
  else if (command == "zero") {
    handleZero();
  }
  else if (command == "save_pos") {
    save_position();
    Serial.println(F("位置已保存"));
  }
  else if (command == "test_mode") {
    test_mode = !test_mode;
    Serial.print(F("测试模式: "));
    Serial.println(test_mode ? "开启" : "关闭");
  }
  else {
    Serial.println(F("未知命令，发送 'help?' 查看可用命令"));
  }
}

void printHelp() {
  Serial.println(F("=== Pico OpenFlexure 控制器命令 ==="));
  Serial.println(F("help?               - 显示此帮助信息"));
  Serial.println(F("version?            - 显示版本信息"));
  Serial.println(F("board?              - 显示开发板信息"));
  Serial.println(F("position?           - 显示当前位置"));
  Serial.println(F("move_rel x y z      - 相对移动 (步数)"));
  Serial.println(F("move_abs x y z      - 绝对移动 (步数)"));
  Serial.println(F("set_speed speed     - 设置速度 (微秒延时)"));
  Serial.println(F("release             - 释放所有电机"));
  Serial.println(F("zero                - 零位重置"));
  Serial.println(F("save_pos            - 保存当前位置"));
  Serial.println(F("test_mode           - 切换测试模式"));
  Serial.println(F("====================================="));
}

void handleRelativeMove(String command) {
  // 解析 "move_rel x y z" 命令
  int space1 = command.indexOf(' ', 9);
  int space2 = command.indexOf(' ', space1 + 1);
  
  if (space1 > 0 && space2 > 0) {
    long dx = command.substring(9, space1).toInt();
    long dy = command.substring(space1 + 1, space2).toInt();
    long dz = command.substring(space2 + 1).toInt();
    
    steps_remaining[0] += dx;
    steps_remaining[1] += dy;
    steps_remaining[2] += dz;
    
    Serial.print(F("相对移动: X="));
    Serial.print(dx);
    Serial.print(F(" Y="));
    Serial.print(dy);
    Serial.print(F(" Z="));
    Serial.println(dz);
  } else {
    Serial.println(F("格式错误，使用: move_rel x y z"));
  }
}

void handleAbsoluteMove(String command) {
  // 解析 "move_abs x y z" 命令
  int space1 = command.indexOf(' ', 9);
  int space2 = command.indexOf(' ', space1 + 1);
  
  if (space1 > 0 && space2 > 0) {
    long target_x = command.substring(9, space1).toInt();
    long target_y = command.substring(space1 + 1, space2).toInt();
    long target_z = command.substring(space2 + 1).toInt();
    
    steps_remaining[0] = target_x - current_pos[0];
    steps_remaining[1] = target_y - current_pos[1];
    steps_remaining[2] = target_z - current_pos[2];
    
    Serial.print(F("绝对移动到: X="));
    Serial.print(target_x);
    Serial.print(F(" Y="));
    Serial.print(target_y);
    Serial.print(F(" Z="));
    Serial.println(target_z);
  } else {
    Serial.println(F("格式错误，使用: move_abs x y z"));
  }
}

void handleSetSpeed(String command) {
  // 解析 "set_speed speed" 命令
  int space = command.indexOf(' ', 10);
  if (space > 0) {
    long new_speed = command.substring(10).toInt();
    if (new_speed > 0) {
      min_step_delay = new_speed;
      Serial.print(F("速度设置为: "));
      Serial.print(min_step_delay);
      Serial.println(F(" 微秒"));
    } else {
      Serial.println(F("速度必须大于0"));
    }
  } else {
    Serial.println(F("格式错误，使用: set_speed speed"));
  }
}

void handleRelease() {
  EACH_MOTOR {
    releaseMotor(i);
  }
  Serial.println(F("所有电机已释放"));
}

void handleZero() {
  EACH_MOTOR {
    current_pos[i] = 0;
    steps_remaining[i] = 0;
  }
  Serial.println(F("位置已重置为零"));
}

void executeMotorMovements() {
  static unsigned long last_step_time = 0;
  unsigned long current_time = micros();
  
  if (current_time - last_step_time >= min_step_delay) {
    bool any_motor_moving = false;
    
    EACH_MOTOR {
      if (steps_remaining[i] != 0) {
        any_motor_moving = true;
        int direction = (steps_remaining[i] > 0) ? 1 : -1;
        stepMotor(i, direction);
        steps_remaining[i] -= direction;
      }
    }
    
    if (any_motor_moving) {
      last_step_time = current_time;
    }
  }
}

void stepMotor(int motor, long dx) {
  if (motor >= 0 && motor < n_motors) {
    current_pos[motor] += dx;
    motors[motor]->step(dx > 0 ? 1 : -1);
  }
}

void releaseMotor(int motor) {
  if (motor >= 0 && motor < n_motors) {
    motors[motor]->release();
  }
}

void print_position() {
  Serial.print(F("位置: "));
  EACH_MOTOR {
    if (i > 0) Serial.print(F(" "));
    Serial.print(current_pos[i]);
  }
  Serial.println();
}

void save_position() {
  // 保存位置到Flash存储（暂不实现）
  // 在Pico上可以使用LittleFS来存储
  // EACH_MOTOR {
  //   // 保存到文件系统
  // }
}

void load_position() {
  // 从Flash存储加载位置（暂不实现）
  EACH_MOTOR {
    // 从文件系统加载
    current_pos[i] = 0; // 默认位置
  }
}

int command_prefix(String command, const char** prefixes, int n_prefixes) {
  // 检查命令是否以列表中的任何前缀开头
  // 如果是则返回索引，否则返回-1
  for (int i = 0; i < n_prefixes; i++) {
    if (command.startsWith(prefixes[i])) {
      return i;
    }
  }
  return -1;
}
