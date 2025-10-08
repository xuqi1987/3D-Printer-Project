#!/usr/bin/env python3
"""
详细的VLC兼容性诊断工具
模拟VLC的具体连接流程并诊断问题
"""

import socket
import time
import sys
import re

def detailed_vlc_simulation():
    """详细模拟VLC的连接流程"""
    
    esp32_ip = "192.168.31.104"
    rtsp_port = 8554
    
    print(f"=== VLC兼容性详细诊断 ===")
    print(f"目标: {esp32_ip}:{rtsp_port}")
    
    try:
        # 创建TCP socket连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((esp32_ip, rtsp_port))
        print("✅ TCP连接建立成功")
        
        # 1. OPTIONS - VLC首次请求
        print("\n--- 步骤1: OPTIONS请求 ---")
        options_request = f"""OPTIONS rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 1\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
\r
"""
        print("发送:", repr(options_request))
        sock.sendall(options_request.encode())
        
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b'\r\n\r\n' in response:
                    break
            except socket.timeout:
                break
        
        print("响应:", repr(response.decode('utf-8', errors='ignore')))
        
        if "200 OK" not in response.decode():
            print("❌ OPTIONS失败")
            return False
        
        # 2. DESCRIBE - 获取SDP
        print("\n--- 步骤2: DESCRIBE请求 ---")
        describe_request = f"""DESCRIBE rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 2\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
Accept: application/sdp\r
\r
"""
        print("发送:", repr(describe_request))
        sock.sendall(describe_request.encode())
        
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b'\r\n\r\n' in response:
                    break
            except socket.timeout:
                break
        
        response_text = response.decode('utf-8', errors='ignore')
        print("响应:", repr(response_text))
        
        if "200 OK" not in response_text:
            print("❌ DESCRIBE失败")
            return False
        
        # 3. 第一次SETUP尝试 - VLC通常先尝试UDP
        print("\n--- 步骤3a: SETUP请求 (UDP模拟VLC首次尝试) ---")
        setup_udp_request = f"""SETUP rtsp://{esp32_ip}:{rtsp_port}/live/track1 RTSP/1.0\r
CSeq: 3\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
Transport: RTP/AVP;unicast;client_port=50000-50001\r
\r
"""
        print("发送:", repr(setup_udp_request))
        sock.sendall(setup_udp_request.encode())
        
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b'\r\n\r\n' in response:
                    break
            except socket.timeout:
                break
        
        response_text = response.decode('utf-8', errors='ignore')
        print("响应:", repr(response_text))
        
        # 4. 第二次SETUP尝试 - VLC应该回退到TCP
        print("\n--- 步骤3b: SETUP请求 (TCP回退) ---")
        setup_tcp_request = f"""SETUP rtsp://{esp32_ip}:{rtsp_port}/live/track1 RTSP/1.0\r
CSeq: 4\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r
\r
"""
        print("发送:", repr(setup_tcp_request))
        sock.sendall(setup_tcp_request.encode())
        
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b'\r\n\r\n' in response:
                    break
            except socket.timeout:
                break
        
        response_text = response.decode('utf-8', errors='ignore')
        print("响应:", repr(response_text))
        
        if "200 OK" not in response_text:
            print("❌ TCP SETUP失败")
            return False
        
        # 提取Session ID
        session_match = re.search(r'Session:\s*([^;\r\n]+)', response_text)
        if not session_match:
            print("❌ 未找到Session ID")
            return False
        
        session_id = session_match.group(1).strip()
        print(f"✅ Session ID: {session_id}")
        
        # 5. PLAY请求
        print("\n--- 步骤4: PLAY请求 ---")
        play_request = f"""PLAY rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 5\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
Session: {session_id}\r
Range: npt=0.000-\r
\r
"""
        print("发送:", repr(play_request))
        sock.sendall(play_request.encode())
        
        response = b""
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b'\r\n\r\n' in response:
                    break
            except socket.timeout:
                break
                
        # 分离RTSP响应头和RTP数据
        response_end = response.find(b'\r\n\r\n')
        if response_end != -1:
            rtsp_response = response[:response_end + 4].decode('utf-8', errors='ignore')
            rtp_data_start = response[response_end + 4:]
            print("PLAY响应头:", repr(rtsp_response))
            print("RTP数据开始:", len(rtp_data_start), "bytes:", rtp_data_start[:20].hex() if rtp_data_start else "无")
        else:
            response_text = response.decode('utf-8', errors='ignore')
            print("PLAY响应头:", repr(response_text[:200]))
            rtp_data_start = b""
        
        if "200 OK" not in (rtsp_response if response_end != -1 else response_text):
            print("❌ PLAY失败")
            return False
        
        # 6. 检查RTP数据流
        print("\n--- 步骤5: 检查RTP数据流 ---")
        sock.settimeout(3)
        rtp_packets = 0
        jpeg_packets = 0
        
        # 首先处理PLAY响应中的RTP数据
        if rtp_data_start:
            print("处理PLAY响应中的RTP数据...")
            data = rtp_data_start
            if len(data) >= 12:
                # 检查TCP interleaved格式
                if len(data) >= 4 and data[0] == 0x24 and data[1] == 0x00:
                    rtp_packets += 1
                    print(f"  在PLAY响应中发现RTP包: {len(data)} bytes")
                
        for i in range(20):  # 检查20个数据包
            try:
                data = sock.recv(2048)
                if len(data) >= 12:
                    # 检查TCP interleaved格式
                    if len(data) >= 4 and data[0] == 0x24 and data[1] == 0x00:
                        rtp_packets += 1
                        rtp_length = (data[2] << 8) | data[3]
                        
                        if len(data) >= 4 + 12:  # TCP头 + RTP头
                            rtp_header = data[4:16]  # RTP头
                            payload_type = rtp_header[1] & 0x7F
                            seq_num = (rtp_header[2] << 8) | rtp_header[3]
                            timestamp = (rtp_header[4] << 24) | (rtp_header[5] << 16) | (rtp_header[6] << 8) | rtp_header[7]
                            
                            if payload_type == 26:  # MJPEG
                                jpeg_packets += 1
                                
                                # 检查JPEG payload
                                if len(data) >= 4 + 12 + 8:  # TCP头 + RTP头 + JPEG头
                                    jpeg_header = data[16:24]
                                    type_specific = jpeg_header[0]
                                    fragment_offset = (jpeg_header[1] << 16) | (jpeg_header[2] << 8) | jpeg_header[3]
                                    jpeg_type = jpeg_header[4]
                                    q = jpeg_header[5]
                                    width = jpeg_header[6] * 8
                                    height = jpeg_header[7] * 8
                                    
                                    if rtp_packets <= 3:
                                        print(f"  RTP包 #{rtp_packets}: PT={payload_type}, Seq={seq_num}, TS={timestamp}")
                                        print(f"    TCP Interleaved长度: {rtp_length}")
                                        print(f"    JPEG头: type={jpeg_type}, q={q}, 尺寸={width}x{height}, 偏移={fragment_offset}")
                                        
                                        # 检查JPEG数据
                                        payload_start = 4 + 12 + 8
                                        if fragment_offset == 0 and len(data) > payload_start + 2:
                                            if data[payload_start] == 0xFF and data[payload_start + 1] == 0xD8:
                                                print(f"    ✅ 检测到JPEG SOI标记")
                                            else:
                                                print(f"    ❌ 未检测到JPEG SOI标记: {data[payload_start:payload_start+8].hex()}")
                                
            except socket.timeout:
                break
                
        print(f"\n🔍 数据流统计:")
        print(f"  收到RTP包: {rtp_packets}")
        print(f"  MJPEG包: {jpeg_packets}")
        
        if rtp_packets == 0:
            print("❌ 未收到RTP数据包")
            return False
        elif jpeg_packets == 0:
            print("❌ 未收到MJPEG数据包")
            return False
        else:
            print("✅ RTP和MJPEG数据流正常")
        
        # 7. TEARDOWN
        print("\n--- 步骤6: TEARDOWN请求 ---")
        teardown_request = f"""TEARDOWN rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 6\r
User-Agent: VLC media player (LIVE555 Streaming Media v2021.05.22)\r
Session: {session_id}\r
\r
"""
        sock.sendall(teardown_request.encode())
        sock.close()
        
        return True
        
    except Exception as e:
        print(f"❌ 连接错误: {e}")
        return False

def test_vlc_command_line():
    """测试VLC命令行播放"""
    print("\n=== VLC命令行测试 ===")
    
    import subprocess
    
    # 测试VLC是否安装
    try:
        result = subprocess.run(['vlc', '--version'], 
                               capture_output=True, text=True, timeout=5)
        print(f"VLC版本: {result.stdout.split()[2] if len(result.stdout.split()) > 2 else '未知'}")
    except:
        print("❌ VLC未安装或不在PATH中")
        return False
    
    # 测试RTSP连接
    vlc_args = [
        'vlc',
        'rtsp://192.168.31.104:8554/live',
        '--intf', 'dummy',
        '--extraintf', 'logger',
        '--verbose', '2',
        '--rtsp-tcp',  # 强制使用TCP
        '--no-audio',
        '--run-time=5',
        '--play-and-exit'
    ]
    
    try:
        print("运行VLC测试...")
        result = subprocess.run(vlc_args, capture_output=True, text=True, timeout=15)
        print(f"VLC退出代码: {result.returncode}")
        print("VLC输出:")
        print(result.stdout)
        if result.stderr:
            print("VLC错误:")
            print(result.stderr)
            
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("❌ VLC测试超时")
        return False
    except Exception as e:
        print(f"❌ VLC测试错误: {e}")
        return False

def main():
    print("=== ESP32 MJPEG RTSP - VLC兼容性完整诊断 ===\n")
    
    # 1. 详细协议模拟
    print("🔍 步骤1: 详细协议流程模拟")
    protocol_ok = detailed_vlc_simulation()
    
    if not protocol_ok:
        print("\n❌ 协议层面存在问题，无法继续")
        return
    
    print("\n✅ 协议层面测试通过")
    
    # 2. VLC命令行测试
    print("\n🔍 步骤2: VLC应用层测试")
    vlc_ok = test_vlc_command_line()
    
    if vlc_ok:
        print("\n🎉 VLC完全兼容！")
        print("\n📋 建议的VLC设置:")
        print("1. 使用URL: rtsp://192.168.31.104:8554/live")
        print("2. 在VLC偏好设置中:")
        print("   - 输入/编解码器 → 网络 → 强制使用TCP传输")
        print("   - 输入/编解码器 → 视频编解码器 → FFmpeg → 启用硬件解码")
        print("3. 或者使用命令行: vlc rtsp://192.168.31.104:8554/live --rtsp-tcp")
    else:
        print("\n⚠️  协议兼容但VLC应用层可能有问题")
        print("建议:")
        print("1. 检查VLC版本 - 建议使用3.0.11或更高版本")
        print("2. 重置VLC偏好设置")
        print("3. 在VLC中手动配置RTSP-over-TCP")

if __name__ == "__main__":
    main()