import socket
import time
import sys

def send_rtsp_request(sock, request):
    print(f"发送: {request.strip()}")
    sock.sendall(request.encode())
    time.sleep(0.5)
    
    response = b""
    sock.settimeout(2.0)
    try:
        # 读取响应，最多读取4KB
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            response += chunk
            if b'\r\n\r\n' in chunk:  # 检测到响应结束
                break
    except socket.timeout:
        print("接收响应超时")
    
    response_str = response.decode('utf-8', errors='ignore')
    print(f"接收: {response_str}")
    return response_str

def test_rtsp_commands():
    print("RTSP协议测试工具")
    server_ip = "192.168.31.104"
    port = 8554
    stream_path = "live"
    
    print(f"连接到 {server_ip}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        sock.connect((server_ip, port))
        print("成功连接到服务器")
        
        # 发送OPTIONS请求
        options_request = (
            f"OPTIONS rtsp://{server_ip}:{port}/{stream_path} RTSP/1.0\r\n"
            f"CSeq: 1\r\n"
            f"User-Agent: RTSP Tester\r\n\r\n"
        )
        options_response = send_rtsp_request(sock, options_request)
        
        # 发送DESCRIBE请求
        describe_request = (
            f"DESCRIBE rtsp://{server_ip}:{port}/{stream_path} RTSP/1.0\r\n"
            f"CSeq: 2\r\n"
            f"User-Agent: RTSP Tester\r\n"
            f"Accept: application/sdp\r\n\r\n"
        )
        describe_response = send_rtsp_request(sock, describe_request)
        
        # 发送SETUP请求 (使用TCP传输)
        setup_request = (
            f"SETUP rtsp://{server_ip}:{port}/{stream_path} RTSP/1.0\r\n"
            f"CSeq: 3\r\n"
            f"User-Agent: RTSP Tester\r\n"
            f"Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n"
        )
        setup_response = send_rtsp_request(sock, setup_request)
        
        # 从SETUP响应中提取会话ID
        session_id = None
        for line in setup_response.split('\r\n'):
            if line.startswith('Session:'):
                session_id = line.split(':', 1)[1].strip()
                if ';' in session_id:
                    session_id = session_id.split(';')[0]
                break
        
        if not session_id:
            print("警告: 无法从SETUP响应中提取会话ID")
            session_id = "dummy"
        
        # 发送PLAY请求
        play_request = (
            f"PLAY rtsp://{server_ip}:{port}/{stream_path} RTSP/1.0\r\n"
            f"CSeq: 4\r\n"
            f"User-Agent: RTSP Tester\r\n"
            f"Session: {session_id}\r\n"
            f"Range: npt=0.000-\r\n\r\n"
        )
        play_response = send_rtsp_request(sock, play_request)
        
        # 等待并尝试读取一些RTP数据
        print("尝试读取RTP数据包...")
        sock.settimeout(5.0)
        try:
            for i in range(3):
                rtp_data = sock.recv(2048)
                if rtp_data:
                    print(f"接收到数据包 {i+1}, 长度: {len(rtp_data)} 字节")
                    # 如果这是有效的RTP-over-RTSP数据，第一个字节应该是$
                    if rtp_data[0:1] == b'$':
                        channel = rtp_data[1]
                        length = (rtp_data[2] << 8) | rtp_data[3]
                        print(f"  RTP头: 通道={channel}, 长度={length}")
                        print(f"  RTP数据: {rtp_data[4:14].hex()}")
                    else:
                        print(f"  原始数据: {rtp_data[:20].hex()}")
                time.sleep(1)
        except socket.timeout:
            print("读取RTP数据超时")
            
        # 发送TEARDOWN请求
        teardown_request = (
            f"TEARDOWN rtsp://{server_ip}:{port}/{stream_path} RTSP/1.0\r\n"
            f"CSeq: 5\r\n"
            f"User-Agent: RTSP Tester\r\n"
            f"Session: {session_id}\r\n\r\n"
        )
        teardown_response = send_rtsp_request(sock, teardown_request)
        
    except socket.error as e:
        print(f"连接错误: {e}")
    finally:
        print("关闭连接")
        sock.close()

if __name__ == "__main__":
    test_rtsp_commands()