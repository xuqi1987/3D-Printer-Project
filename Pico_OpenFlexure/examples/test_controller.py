#!/usr/bin/env python3
"""
Pico OpenFlexure 控制器测试脚本

此脚本演示如何通过串口与Pico OpenFlexure控制器通信
"""

import serial
import time
import sys

def find_pico_port():
    """查找Pico设备的串口"""
    import serial.tools.list_ports
    
    for port in serial.tools.list_ports.comports():
        if "Pico" in port.description or "2E8A" in port.hwid:
            return port.device
    return None

def test_controller(port, baudrate=115200):
    """测试控制器功能"""
    try:
        # 连接串口
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(2)  # 等待连接建立
        
        print(f"已连接到 {port}")
        
        # 测试命令列表
        test_commands = [
            "version?",
            "board?", 
            "position?",
            "move_rel 50 0 0",
            "position?",
            "move_rel -50 0 0", 
            "position?",
            "set_speed 1500",
            "move_rel 0 100 0",
            "position?",
            "zero",
            "position?"
        ]
        
        for cmd in test_commands:
            print(f"\n发送命令: {cmd}")
            ser.write((cmd + '\n').encode())
            
            # 读取响应
            response = ""
            start_time = time.time()
            while time.time() - start_time < 1:
                if ser.in_waiting:
                    response += ser.read(ser.in_waiting).decode()
                    time.sleep(0.1)
                else:
                    break
            
            if response:
                print(f"响应: {response.strip()}")
            else:
                print("无响应")
            
            time.sleep(0.5)
        
        ser.close()
        print("\n测试完成!")
        
    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n测试被用户中断")
        if 'ser' in locals():
            ser.close()

def interactive_mode(port, baudrate=115200):
    """交互模式"""
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2)
        
        print(f"已连接到 {port}")
        print("交互模式已启动，输入 'quit' 退出")
        print("输入 'help?' 查看可用命令")
        
        while True:
            # 读取用户输入
            cmd = input(">>> ").strip()
            if cmd.lower() == 'quit':
                break
            
            if cmd:
                ser.write((cmd + '\n').encode())
                
                # 读取响应
                time.sleep(0.1)
                response = ""
                while ser.in_waiting:
                    response += ser.read(ser.in_waiting).decode()
                    time.sleep(0.05)
                
                if response:
                    print(response.strip())
        
        ser.close()
        print("已断开连接")
        
    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n已断开连接")
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    # 查找Pico端口
    port = find_pico_port()
    
    if not port:
        print("未找到Pico设备，请检查连接")
        sys.exit(1)
    
    print(f"找到Pico设备: {port}")
    
    # 选择模式
    if len(sys.argv) > 1 and sys.argv[1] == "-i":
        interactive_mode(port)
    else:
        test_controller(port)
