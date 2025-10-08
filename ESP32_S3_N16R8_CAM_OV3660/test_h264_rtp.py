#!/usr/bin/env python3
"""
H.264 RTP 测试脚本
用于测试ESP32 H.264 RTSP服务器的RTP实现
"""

import socket
import struct
import time
import threading
from datetime import datetime

class RTSPClient:
    def __init__(self, host='192.168.31.104', port=8554, stream_name='live'):
        self.host = host
        self.port = port
        self.stream_name = stream_name
        self.socket = None
        self.cseq = 1
        self.session_id = None
        self.rtp_data = []
        self.running = False
        
    def connect(self):
        """连接到RTSP服务器"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            print(f"Connected to RTSP server at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False
    
    def send_request(self, method, uri='*', extra_headers=None):
        """发送RTSP请求"""
        request = f"{method} rtsp://{self.host}:{self.port}/{self.stream_name}{uri} RTSP/1.0\r\n"
        request += f"CSeq: {self.cseq}\r\n"
        
        if extra_headers:
            for header, value in extra_headers.items():
                request += f"{header}: {value}\r\n"
        
        request += "\r\n"
        
        print(f"Sending request:\n{request}")
        self.socket.send(request.encode())
        self.cseq += 1
        
        # 接收响应
        response = self.socket.recv(4096).decode()
        print(f"Received response:\n{response}")
        
        return response
    
    def parse_session_id(self, response):
        """从SETUP响应中解析会话ID"""
        lines = response.split('\r\n')
        for line in lines:
            if line.startswith('Session:'):
                # Session: 12345678;timeout=60
                self.session_id = line.split(':')[1].strip().split(';')[0]
                print(f"Session ID: {self.session_id}")
                break
    
    def test_rtsp_handshake(self):
        """测试RTSP握手流程"""
        print("=== Starting RTSP Handshake Test ===")
        
        # 1. OPTIONS
        print("\n1. Sending OPTIONS request...")
        response = self.send_request("OPTIONS")
        if "200 OK" not in response:
            print("OPTIONS failed!")
            return False
        
        # 2. DESCRIBE
        print("\n2. Sending DESCRIBE request...")
        response = self.send_request("DESCRIBE", "")
        if "200 OK" not in response:
            print("DESCRIBE failed!")
            return False
        
        # 检查SDP内容
        if "H264" in response:
            print("✓ H.264 SDP detected")
        else:
            print("✗ H.264 SDP not found")
        
        # 3. SETUP
        print("\n3. Sending SETUP request...")
        headers = {
            'Transport': 'RTP/AVP/TCP;unicast;interleaved=0-1'
        }
        response = self.send_request("SETUP", "/track1", headers)
        if "200 OK" not in response:
            print("SETUP failed!")
            return False
        
        self.parse_session_id(response)
        
        # 4. PLAY
        print("\n4. Sending PLAY request...")
        headers = {
            'Session': self.session_id
        }
        response = self.send_request("PLAY", "", headers)
        if "200 OK" not in response:
            print("PLAY failed!")
            return False
        
        print("✓ RTSP handshake completed successfully!")
        return True
    
    def start_rtp_receiver(self):
        """启动RTP数据接收"""
        print("\n=== Starting RTP Reception ===")
        self.running = True
        receiver_thread = threading.Thread(target=self._rtp_receiver)
        receiver_thread.daemon = True
        receiver_thread.start()
        
        # 接收10秒钟的数据
        time.sleep(10)
        self.running = False
        
        print(f"\nReceived {len(self.rtp_data)} RTP packets")
        self.analyze_rtp_packets()
    
    def _rtp_receiver(self):
        """RTP数据接收线程"""
        packet_count = 0
        h264_nal_types = {}
        
        while self.running:
            try:
                # 接收interleaved frame header: $ + channel + length
                header = self.socket.recv(4)
                if len(header) != 4:
                    continue
                
                if header[0] != ord('$'):
                    continue
                
                channel = header[1]
                length = struct.unpack('>H', header[2:4])[0]
                
                # 接收RTP包内容
                rtp_data = self.socket.recv(length)
                if len(rtp_data) != length:
                    continue
                
                packet_count += 1
                self.rtp_data.append(rtp_data)
                
                # 解析RTP头部
                if len(rtp_data) >= 12:
                    rtp_header = struct.unpack('>BBHII', rtp_data[:12])
                    version = (rtp_header[0] >> 6) & 3
                    payload_type = rtp_header[1] & 0x7F
                    marker = bool(rtp_header[1] & 0x80)
                    seq_num = rtp_header[2]
                    timestamp = rtp_header[3]
                    ssrc = rtp_header[4]
                    
                    # 解析H.264载荷
                    if len(rtp_data) > 12:
                        h264_payload = rtp_data[12:]
                        nal_type = self.analyze_h264_payload(h264_payload)
                        if nal_type:
                            h264_nal_types[nal_type] = h264_nal_types.get(nal_type, 0) + 1
                    
                    if packet_count % 10 == 0:
                        print(f"RTP packet #{packet_count}: PT={payload_type}, SEQ={seq_num}, "
                              f"TS={timestamp}, M={marker}, NAL={nal_type}")
            
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"RTP receive error: {e}")
                break
        
        print(f"H.264 NAL unit distribution: {h264_nal_types}")
    
    def analyze_h264_payload(self, payload):
        """分析H.264 RTP载荷"""
        if len(payload) == 0:
            return None
        
        nal_header = payload[0]
        nal_type = nal_header & 0x1F
        
        if nal_type <= 23:
            # 单一NAL单元模式
            return f"Single-{nal_type}"
        elif nal_type == 24:
            return "STAP-A"
        elif nal_type == 28:
            # FU-A 分片
            if len(payload) > 1:
                fu_header = payload[1]
                original_type = fu_header & 0x1F
                start_bit = bool(fu_header & 0x80)
                end_bit = bool(fu_header & 0x40)
                return f"FU-A-{original_type}-{'S' if start_bit else ''}{'E' if end_bit else ''}"
        
        return f"Unknown-{nal_type}"
    
    def analyze_rtp_packets(self):
        """分析接收到的RTP包"""
        if not self.rtp_data:
            print("No RTP packets received")
            return
        
        print(f"\n=== RTP Analysis ===")
        
        # 统计信息
        total_packets = len(self.rtp_data)
        total_bytes = sum(len(packet) for packet in self.rtp_data)
        
        # 解析序列号
        seq_numbers = []
        timestamps = []
        
        for packet in self.rtp_data:
            if len(packet) >= 12:
                rtp_header = struct.unpack('>BBHII', packet[:12])
                seq_numbers.append(rtp_header[2])
                timestamps.append(rtp_header[3])
        
        print(f"Total packets: {total_packets}")
        print(f"Total bytes: {total_bytes}")
        print(f"Average packet size: {total_bytes / total_packets:.1f} bytes")
        
        if seq_numbers:
            print(f"Sequence range: {min(seq_numbers)} - {max(seq_numbers)}")
            
            # 检查丢包
            expected_seq = list(range(min(seq_numbers), max(seq_numbers) + 1))
            missing = set(expected_seq) - set(seq_numbers)
            if missing:
                print(f"Missing packets: {len(missing)} ({missing})")
            else:
                print("✓ No packet loss detected")
        
        if len(timestamps) > 1:
            # 计算帧率
            time_diff = timestamps[-1] - timestamps[0]
            if time_diff > 0:
                fps = (len(timestamps) - 1) * 90000 / time_diff
                print(f"Estimated frame rate: {fps:.1f} fps")
    
    def run_test(self):
        """运行完整测试"""
        if not self.connect():
            return False
        
        try:
            # RTSP握手测试
            if not self.test_rtsp_handshake():
                return False
            
            # RTP数据接收测试
            self.start_rtp_receiver()
            
            return True
        
        except Exception as e:
            print(f"Test error: {e}")
            return False
        
        finally:
            if self.socket:
                self.socket.close()
                print("Connection closed")

def main():
    print("H.264 RTP RTSP Client Test")
    print("=" * 40)
    print(f"Test time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # 可以修改这些参数
    HOST = '192.168.31.104'  # ESP32 IP地址
    PORT = 8554
    STREAM = 'live'
    
    client = RTSPClient(HOST, PORT, STREAM)
    
    print(f"Testing RTSP server at rtsp://{HOST}:{PORT}/{STREAM}")
    
    success = client.run_test()
    
    if success:
        print("\n✓ Test completed successfully!")
    else:
        print("\n✗ Test failed!")

if __name__ == "__main__":
    main()