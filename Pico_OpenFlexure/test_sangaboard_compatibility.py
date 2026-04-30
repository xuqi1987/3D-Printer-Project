#!/usr/bin/env python3
"""
测试Pico OpenFlexure与原版Sangaboard的兼容性
验证所有原版命令是否正确响应
"""

import serial
import time
import sys

def test_sangaboard_commands(port='/dev/tty.usbmodem*', baudrate=115200):
    """测试Sangaboard命令兼容性"""
    
    try:
        # 连接串口
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(2)  # 等待连接稳定
        
        # 清空缓冲区
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        print("=== Sangaboard命令兼容性测试 ===\n")
        
        # 测试命令列表
        test_commands = [
            ("version", "获取版本信息"),
            ("board", "获取开发板信息"),
            ("help", "获取帮助信息"),
            ("p?", "获取位置"),
            ("zero", "重置位置"),
            ("p?", "验证位置重置"),
            ("mr 100 0 0", "X轴相对移动100步"),
            ("p?", "检查X轴位置"),
            ("mrx 50", "X轴相对移动50步"),
            ("p?", "检查X轴位置"),
            ("mry -30", "Y轴反向移动30步"),
            ("p?", "检查Y轴位置"),
            ("mrz 20", "Z轴移动20步"),
            ("p?", "检查Z轴位置"),
            ("dt 2000", "设置最小步进延时为2000微秒"),
            ("dt?", "查询最小步进延时"),
            ("ramp_time 5000", "设置加速时间为5000微秒"),
            ("ramp_time?", "查询加速时间"),
            ("release", "释放电机"),
            ("test_mode on", "开启测试模式"),
            ("test_mode off", "关闭测试模式"),
        ]
        
        for command, description in test_commands:
            print(f"测试: {command} - {description}")
            
            # 发送命令
            ser.write((command + '\n').encode())
            time.sleep(0.5)
            
            # 读取响应
            response = ""
            start_time = time.time()
            while time.time() - start_time < 2:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    response += data
                    if response.endswith('\n') or response.endswith('\r\n'):
                        break
                time.sleep(0.1)
            
            print(f"响应: {response.strip()}")
            print("-" * 50)
            
        print("\n=== 兼容性测试完成 ===")
        
        # 验证help命令的详细输出
        print("\n=== 详细help命令测试 ===")
        ser.write(b'help\n')
        time.sleep(1)
        
        help_response = ""
        start_time = time.time()
        while time.time() - start_time < 5:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                help_response += data
                if "--END--" in help_response:
                    break
            time.sleep(0.1)
        
        print("完整help输出:")
        print(help_response)
        
        ser.close()
        print("\n测试完成！")
        
    except serial.SerialException as e:
        print(f"串口连接错误: {e}")
        print("请检查:")
        print("1. Pico是否已连接")
        print("2. 固件是否已上传")
        print("3. 串口号是否正确")
        sys.exit(1)
    except Exception as e:
        print(f"测试过程中发生错误: {e}")
        sys.exit(1)

def find_pico_port():
    """自动查找Pico的串口"""
    import glob
    
    # macOS下的USB串口模式
    patterns = [
        '/dev/tty.usbmodem*',
        '/dev/ttyACM*',
        '/dev/ttyUSB*'
    ]
    
    for pattern in patterns:
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    
    return None

if __name__ == "__main__":
    # 自动查找串口
    port = find_pico_port()
    
    if port:
        print(f"找到Pico串口: {port}")
        test_sangaboard_commands(port)
    else:
        print("未找到Pico串口，请手动指定:")
        print("python test_sangaboard_compatibility.py /dev/tty.usbmodem14201")
        
        if len(sys.argv) > 1:
            port = sys.argv[1]
            test_sangaboard_commands(port)
        else:
            print("\n可用串口:")
            import serial.tools.list_ports
            for port in serial.tools.list_ports.comports():
                print(f"  {port.device}: {port.description}")
