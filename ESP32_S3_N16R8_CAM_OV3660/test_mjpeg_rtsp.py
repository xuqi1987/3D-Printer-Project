#!/usr/bin/env python3
"""
ESP32-S3 MJPEG RTSP Camera Server Testing Script

This script provides comprehensive testing for the ESP32-S3 MJPEG RTSP camera server.
It tests RTSP protocol compliance, RTP packet reception, and stream quality.

Usage:
    python test_mjpeg_rtsp.py [ESP32_IP]

Features:
- RTSP protocol sequence testing
- RTP packet analysis
- Frame rate measurement
- Connection stability testing
- Error reporting and diagnostics

Author: ESP32 MJPEG RTSP Project
Date: 2025-10-08
"""

import socket
import time
import threading
import sys
import struct
import re
from datetime import datetime

class MJPEGRTSPTester:
    def __init__(self, host="192.168.31.104", port=8554):
        self.host = host
        self.port = port
        self.rtsp_url = f"rtsp://{host}:{port}/live"
        self.socket = None
        self.session_id = None
        self.cseq = 1
        
        # Statistics
        self.rtp_packets_received = 0
        self.test_start_time = None
        self.last_packet_time = None
        self.errors = []
        self.is_streaming = False
        
        print(f"🎥 ESP32-S3 MJPEG RTSP Tester")
        print(f"Target: {self.rtsp_url}")
        print("=" * 50)

    def connect(self):
        """Connect to RTSP server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            print(f"✅ Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            self.errors.append(f"Connection error: {e}")
            return False

    def send_rtsp_command(self, method, uri=None, headers=None):
        """Send RTSP command and return response"""
        if uri is None:
            uri = self.rtsp_url
        
        # Build request
        request = f"{method} {uri} RTSP/1.0\r\n"
        request += f"CSeq: {self.cseq}\r\n"
        request += "User-Agent: ESP32-MJPEG-Tester/1.0\r\n"
        
        if headers:
            for key, value in headers.items():
                request += f"{key}: {value}\r\n"
        
        request += "\r\n"
        
        print(f"📤 Sending {method} (CSeq: {self.cseq})")
        
        try:
            self.socket.send(request.encode())
            
            # Read response
            response = b""
            while b"\r\n\r\n" not in response:
                chunk = self.socket.recv(1024)
                if not chunk:
                    break
                response += chunk
                
            response_str = response.decode('utf-8', errors='ignore')
            
            # Parse response
            lines = response_str.split('\r\n')
            status_line = lines[0]
            
            print(f"📥 Response: {status_line}")
            
            # Extract session ID if present
            for line in lines:
                if line.startswith("Session:"):
                    self.session_id = line.split(":")[1].strip().split(";")[0]
                    print(f"🔑 Session ID: {self.session_id}")
            
            self.cseq += 1
            
            return response_str, status_line
            
        except Exception as e:
            print(f"❌ RTSP command failed: {e}")
            self.errors.append(f"RTSP {method} error: {e}")
            return None, None

    def test_options(self):
        """Test OPTIONS command"""
        print("\n🔍 Testing OPTIONS...")
        response, status = self.send_rtsp_command("OPTIONS")
        
        if response and "200 OK" in status:
            print("✅ OPTIONS successful")
            
            # Check for required methods
            if "Public:" in response:
                public_line = [line for line in response.split('\r\n') if line.startswith('Public:')][0]
                methods = public_line.split(':')[1].strip()
                print(f"📋 Supported methods: {methods}")
                
                required_methods = ["DESCRIBE", "SETUP", "PLAY", "TEARDOWN"]
                for method in required_methods:
                    if method in methods:
                        print(f"  ✅ {method} supported")
                    else:
                        print(f"  ❌ {method} missing")
                        self.errors.append(f"Missing method: {method}")
            
            return True
        else:
            print("❌ OPTIONS failed")
            return False

    def test_describe(self):
        """Test DESCRIBE command and analyze SDP"""
        print("\n🔍 Testing DESCRIBE...")
        headers = {"Accept": "application/sdp"}
        response, status = self.send_rtsp_command("DESCRIBE", headers=headers)
        
        if response and "200 OK" in status:
            print("✅ DESCRIBE successful")
            
            # Extract and analyze SDP
            sdp_start = response.find("\r\n\r\n") + 4
            sdp_content = response[sdp_start:].strip()
            
            print("📄 SDP Content:")
            for line in sdp_content.split('\r\n'):
                if line:
                    print(f"  {line}")
            
            # Verify MJPEG format
            if "m=video 0 RTP/AVP 26" in sdp_content:
                print("✅ MJPEG format detected (payload type 26)")
            else:
                print("❌ MJPEG format not detected")
                self.errors.append("MJPEG format not found in SDP")
            
            if "a=rtpmap:26 JPEG/90000" in sdp_content:
                print("✅ JPEG codec mapping correct")
            else:
                print("❌ JPEG codec mapping incorrect")
                self.errors.append("Incorrect JPEG codec mapping")
            
            return True
        else:
            print("❌ DESCRIBE failed")
            return False

    def test_setup(self):
        """Test SETUP command"""
        print("\n🔍 Testing SETUP...")
        setup_uri = f"{self.rtsp_url}/track1"
        headers = {"Transport": "RTP/AVP/TCP;unicast;interleaved=0-1"}
        response, status = self.send_rtsp_command("SETUP", setup_uri, headers)
        
        if response and "200 OK" in status:
            print("✅ SETUP successful")
            
            # Verify transport parameters
            for line in response.split('\r\n'):
                if line.startswith('Transport:'):
                    transport = line.split(':')[1].strip()
                    print(f"🚛 Transport: {transport}")
                    
                    if "RTP/AVP/TCP" in transport and "interleaved=0-1" in transport:
                        print("✅ TCP interleaved transport confirmed")
                    else:
                        print("❌ Transport parameters incorrect")
                        self.errors.append("Incorrect transport parameters")
            
            return True
        else:
            print("❌ SETUP failed")
            return False

    def test_play(self):
        """Test PLAY command"""
        print("\n🔍 Testing PLAY...")
        headers = {}
        if self.session_id:
            headers["Session"] = self.session_id
        headers["Range"] = "npt=0.000-"
        
        response, status = self.send_rtsp_command("PLAY", headers=headers)
        
        if response and "200 OK" in status:
            print("✅ PLAY successful - Stream should start")
            self.is_streaming = True
            return True
        else:
            print("❌ PLAY failed")
            return False

    def receive_rtp_data(self, duration=10):
        """Receive and analyze RTP data"""
        print(f"\n📡 Receiving RTP data for {duration} seconds...")
        
        self.socket.settimeout(1)
        self.test_start_time = time.time()
        end_time = self.test_start_time + duration
        
        jpeg_frames = 0
        total_rtp_data = 0
        
        while time.time() < end_time and self.is_streaming:
            try:
                # Read RTP over TCP (RFC 2326 Section 10.12)
                data = self.socket.recv(4096)
                if not data:
                    break
                
                # Parse interleaved data ($ + channel + length + data)
                i = 0
                while i < len(data):
                    if data[i:i+1] == b'$':
                        if i + 4 <= len(data):
                            channel = data[i+1]
                            length = struct.unpack('>H', data[i+2:i+4])[0]
                            
                            if i + 4 + length <= len(data):
                                rtp_packet = data[i+4:i+4+length]
                                
                                if len(rtp_packet) >= 12:  # Minimum RTP header size
                                    self.analyze_rtp_packet(rtp_packet)
                                    total_rtp_data += length
                                    
                                    # Check for JPEG markers
                                    if b'\xff\xd8' in rtp_packet:  # JPEG SOI
                                        jpeg_frames += 1
                                
                                i += 4 + length
                            else:
                                break
                        else:
                            break
                    else:
                        i += 1
                
                self.last_packet_time = time.time()
                
            except socket.timeout:
                continue
            except Exception as e:
                print(f"⚠️ RTP receive error: {e}")
                break
        
        duration_actual = time.time() - self.test_start_time
        
        # Statistics
        print(f"\n📊 RTP Statistics ({duration_actual:.1f}s):")
        print(f"  📦 Total RTP packets: {self.rtp_packets_received}")
        print(f"  🖼️ JPEG frames detected: {jpeg_frames}")
        print(f"  📈 Average packet rate: {self.rtp_packets_received/duration_actual:.1f} pps")
        print(f"  📊 Total data received: {total_rtp_data:,} bytes")
        print(f"  🚀 Average bitrate: {(total_rtp_data * 8 / duration_actual / 1000):.1f} kbps")
        
        if jpeg_frames > 0:
            fps = jpeg_frames / duration_actual
            print(f"  🎬 Estimated FPS: {fps:.1f}")
            
            if fps > 20:
                print("✅ Good frame rate")
            elif fps > 10:
                print("⚠️ Acceptable frame rate")
            else:
                print("❌ Low frame rate")
                self.errors.append(f"Low FPS: {fps:.1f}")
        
        return self.rtp_packets_received > 0

    def analyze_rtp_packet(self, packet):
        """Analyze RTP packet structure"""
        if len(packet) < 12:
            return
        
        self.rtp_packets_received += 1
        
        # Parse RTP header (RFC 3550)
        version = (packet[0] >> 6) & 0x3
        padding = (packet[0] >> 5) & 0x1
        extension = (packet[0] >> 4) & 0x1
        csrc_count = packet[0] & 0xF
        
        marker = (packet[1] >> 7) & 0x1
        payload_type = packet[1] & 0x7F
        
        sequence_num = struct.unpack('>H', packet[2:4])[0]
        timestamp = struct.unpack('>I', packet[4:8])[0]
        ssrc = struct.unpack('>I', packet[8:12])[0]
        
        # Log every 100th packet for debugging
        if self.rtp_packets_received % 100 == 0:
            print(f"  📦 RTP #{self.rtp_packets_received}: "
                  f"PT={payload_type}, Seq={sequence_num}, "
                  f"TS={timestamp}, Marker={marker}")
        
        # Verify MJPEG payload type
        if payload_type != 26:
            self.errors.append(f"Wrong payload type: {payload_type} (expected 26)")

    def test_teardown(self):
        """Test TEARDOWN command"""
        print("\n🔍 Testing TEARDOWN...")
        self.is_streaming = False
        
        headers = {}
        if self.session_id:
            headers["Session"] = self.session_id
        
        response, status = self.send_rtsp_command("TEARDOWN", headers=headers)
        
        if response and "200 OK" in status:
            print("✅ TEARDOWN successful")
            return True
        else:
            print("❌ TEARDOWN failed")
            return False

    def run_full_test(self, stream_duration=10):
        """Run complete RTSP test sequence"""
        print(f"🚀 Starting full MJPEG RTSP test sequence...")
        print(f"⏰ {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        
        success_count = 0
        total_tests = 6
        
        # Test sequence
        tests = [
            ("Connect", self.connect),
            ("OPTIONS", self.test_options),
            ("DESCRIBE", self.test_describe),
            ("SETUP", self.test_setup),
            ("PLAY", self.test_play),
            ("Stream Reception", lambda: self.receive_rtp_data(stream_duration)),
        ]
        
        for test_name, test_func in tests:
            print(f"\n{'='*50}")
            print(f"🧪 Test: {test_name}")
            
            try:
                if test_func():
                    success_count += 1
                    print(f"✅ {test_name} PASSED")
                else:
                    print(f"❌ {test_name} FAILED")
            except Exception as e:
                print(f"💥 {test_name} CRASHED: {e}")
                self.errors.append(f"{test_name} exception: {e}")
        
        # Final teardown
        if self.socket:
            self.test_teardown()
        
        # Final report
        self.print_final_report(success_count, total_tests)

    def print_final_report(self, success_count, total_tests):
        """Print comprehensive test report"""
        print(f"\n{'='*60}")
        print(f"📋 FINAL TEST REPORT")
        print(f"{'='*60}")
        
        print(f"🎯 Test Target: {self.rtsp_url}")
        print(f"⏰ Test Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"📊 Success Rate: {success_count}/{total_tests} ({success_count/total_tests*100:.1f}%)")
        
        if success_count == total_tests:
            print("🎉 ALL TESTS PASSED! ESP32 MJPEG RTSP server is working correctly!")
        elif success_count >= total_tests * 0.8:
            print("⚠️ Most tests passed, but there are some issues to address.")
        else:
            print("❌ Multiple test failures detected. Server needs debugging.")
        
        # Statistics
        if self.rtp_packets_received > 0:
            print(f"\n📈 Stream Statistics:")
            print(f"  📦 RTP packets received: {self.rtp_packets_received}")
            if self.test_start_time and self.last_packet_time:
                duration = self.last_packet_time - self.test_start_time
                print(f"  ⏱️ Stream duration: {duration:.1f}s")
                print(f"  📡 Packet rate: {self.rtp_packets_received/duration:.1f} pps")
        
        # Error summary
        if self.errors:
            print(f"\n🐛 Issues Found ({len(self.errors)}):")
            for i, error in enumerate(self.errors, 1):
                print(f"  {i}. {error}")
        else:
            print(f"\n✨ No errors detected!")
        
        # Recommendations
        self.print_recommendations(success_count, total_tests)
        
        print(f"{'='*60}")

    def print_recommendations(self, success_count, total_tests):
        """Print recommendations based on test results"""
        print(f"\n💡 Recommendations:")
        
        if success_count == total_tests:
            print("  🎯 System is working optimally!")
            print("  📱 Try connecting with VLC or FFmpeg for video playback")
            print("  🔧 Consider adding authentication for production use")
        else:
            if not self.socket or "Connection error" in str(self.errors):
                print("  🔌 Check ESP32 power and WiFi connection")
                print("  📡 Verify IP address and network connectivity")
                print("  🔥 Check ESP32 serial monitor for error messages")
            
            if "CSeq" in str(self.errors):
                print("  🔄 RTSP protocol synchronization issues detected")
                print("  ⚡ Try restarting the ESP32 device")
            
            if self.rtp_packets_received == 0:
                print("  📺 No video stream received")
                print("  📷 Check camera module connection and initialization")
                print("  🔋 Verify ESP32 has sufficient power for camera operation")
            
            if "Low FPS" in str(self.errors):
                print("  🐌 Frame rate is low - check network bandwidth")
                print("  ⚙️ Consider reducing video quality settings")

    def cleanup(self):
        """Clean up resources"""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        print("🧹 Cleanup completed")

def main():
    """Main function"""
    # Parse command line arguments
    if len(sys.argv) > 1:
        esp32_ip = sys.argv[1]
    else:
        esp32_ip = "192.168.31.104"  # Default IP
    
    print("🎬 ESP32-S3 MJPEG RTSP Camera Testing Tool")
    print("="*50)
    print("This tool tests the ESP32-S3 MJPEG RTSP camera server")
    print("for protocol compliance and streaming performance.")
    print("")
    
    tester = MJPEGRTSPTester(esp32_ip)
    
    try:
        # Run the full test with 15 seconds of streaming
        tester.run_full_test(stream_duration=15)
    except KeyboardInterrupt:
        print("\n\n⏹️ Test interrupted by user")
    except Exception as e:
        print(f"\n\n💥 Unexpected error: {e}")
    finally:
        tester.cleanup()

if __name__ == "__main__":
    main()