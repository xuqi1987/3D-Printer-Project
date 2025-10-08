#!/usr/bin/env python3
"""
测试VLC兼容性的RTSP客户端
专门测试UDP transport请求的处理
"""

import socket
import time
import sys
import re

def send_rtsp_request(sock, request):
    """发送RTSP请求并获取响应"""
    print(f"\n=== 发送请求 ===")
    print(request)
    
    sock.sendall(request.encode())
    
    # 接收响应
    response = b""
    while True:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            # RTSP响应以\r\n\r\n结束
            if b'\r\n\r\n' in response:
                break
        except socket.timeout:
            break
    
    response_text = response.decode('utf-8', errors='ignore')
    print(f"\n=== 收到响应 ===")
    print(response_text)
    
    return response_text

def test_udp_transport_rejection():
    """测试UDP transport请求被正确拒绝"""
    
    esp32_ip = "192.168.31.104"
    rtsp_port = 8554
    
    print(f"连接到 ESP32 RTSP 服务器: {esp32_ip}:{rtsp_port}")
    
    try:
        # 创建TCP socket连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((esp32_ip, rtsp_port))
        print("✅ Socket连接成功")
        
        # 1. OPTIONS请求
        options_request = f"""OPTIONS rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 1\r
User-Agent: VLC-Compatibility-Test/1.0\r
\r
"""
        response = send_rtsp_request(sock, options_request)
        
        if "200 OK" not in response:
            print("❌ OPTIONS请求失败")
            return False
        
        # 2. DESCRIBE请求
        describe_request = f"""DESCRIBE rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 2\r
User-Agent: VLC-Compatibility-Test/1.0\r
Accept: application/sdp\r
\r
"""
        response = send_rtsp_request(sock, describe_request)
        
        if "200 OK" not in response:
            print("❌ DESCRIBE请求失败")
            return False
        
        # 3. SETUP请求 - 模拟VLC的UDP transport请求
        setup_request_udp = f"""SETUP rtsp://{esp32_ip}:{rtsp_port}/live/track1 RTSP/1.0\r
CSeq: 3\r
User-Agent: VLC-Compatibility-Test/1.0\r
Transport: RTP/AVP;unicast;client_port=9360-9361\r
\r
"""
        print("\n🔍 测试UDP transport请求（应该被拒绝）...")
        response = send_rtsp_request(sock, setup_request_udp)
        
        if "461 Unsupported Transport" in response:
            print("✅ UDP transport正确被拒绝，返回461错误")
            if "RTP/AVP/TCP;unicast;interleaved=0-1" in response:
                print("✅ 响应中包含支持的TCP transport信息")
            else:
                print("⚠️  响应中缺少TCP transport信息")
        elif "200 OK" in response:
            print("❌ UDP transport被错误接受！")
            return False
        else:
            print(f"❌ 意外的响应: {response[:100]}")
            return False
        
        # 4. SETUP请求 - 正确的TCP transport请求
        setup_request_tcp = f"""SETUP rtsp://{esp32_ip}:{rtsp_port}/live/track1 RTSP/1.0\r
CSeq: 4\r
User-Agent: VLC-Compatibility-Test/1.0\r
Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r
\r
"""
        print("\n🔍 测试TCP transport请求...")
        response = send_rtsp_request(sock, setup_request_tcp)
        
        if "200 OK" not in response:
            print("❌ TCP SETUP请求失败")
            return False
        
        # 提取Session ID
        session_match = re.search(r'Session:\s*([^;\r\n]+)', response)
        if not session_match:
            print("❌ 未找到Session ID")
            return False
        
        session_id = session_match.group(1).strip()
        print(f"✅ TCP SETUP成功，Session ID: {session_id}")
        
        # 5. PLAY请求
        play_request = f"""PLAY rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 5\r
User-Agent: VLC-Compatibility-Test/1.0\r
Session: {session_id}\r
Range: npt=0.000-\r
\r
"""
        response = send_rtsp_request(sock, play_request)
        
        if "200 OK" not in response:
            print("❌ PLAY请求失败")
            return False
        
        print("✅ PLAY请求成功")
        
        # 6. 接收一些RTP数据验证
        print("\n🔍 接收RTP数据包...")
        sock.settimeout(5)  # 5秒超时
        rtp_count = 0
        
        for i in range(10):  # 尝试接收10个包
            try:
                data = sock.recv(2048)
                if len(data) >= 12:  # RTP头最小长度
                    # 检查RTP magic number
                    if data[0] == 0x24 and data[1] == 0x00:  # TCP interleaved header
                        rtp_count += 1
                        if rtp_count == 1:
                            print(f"✅ 收到第一个RTP包: {len(data)} bytes")
                            print(f"   TCP Interleaved Header: ${data[0]:02x} ${data[1]:02x} {(data[2] << 8 | data[3]):04x}")
                        elif rtp_count <= 3:
                            print(f"✅ 收到RTP包 #{rtp_count}: {len(data)} bytes")
            except socket.timeout:
                break
        
        if rtp_count > 0:
            print(f"✅ 成功接收 {rtp_count} 个RTP数据包")
        else:
            print("⚠️  未接收到RTP数据包")
        
        # 7. TEARDOWN请求
        teardown_request = f"""TEARDOWN rtsp://{esp32_ip}:{rtsp_port}/live RTSP/1.0\r
CSeq: 6\r
User-Agent: VLC-Compatibility-Test/1.0\r
Session: {session_id}\r
\r
"""
        response = send_rtsp_request(sock, teardown_request)
        
        if "200 OK" not in response:
            print("❌ TEARDOWN请求失败")
            return False
        
        print("✅ TEARDOWN请求成功")
        
        sock.close()
        print("\n🎉 VLC兼容性测试完成！")
        return True
        
    except Exception as e:
        print(f"❌ 测试失败: {e}")
        return False

def main():
    print("=== VLC兼容性RTSP测试工具 ===")
    print("测试ESP32 MJPEG RTSP服务器的VLC兼容性")
    print("主要测试UDP transport请求的正确处理\n")
    
    success = test_udp_transport_rejection()
    
    if success:
        print("\n✅ 所有测试通过！ESP32应该与VLC兼容")
        print("\n📋 VLC使用说明:")
        print("1. 打开VLC媒体播放器")
        print("2. 选择 媒体 -> 打开网络串流")
        print("3. 输入URL: rtsp://192.168.31.104:8554/live")
        print("4. 点击播放")
        print("5. VLC现在应该能正确处理传输协商")
    else:
        print("\n❌ 测试失败，需要进一步调试")
        sys.exit(1)

if __name__ == "__main__":
    main()