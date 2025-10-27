#!/usr/bin/env python3
"""
Simple MQTT Server using paho-mqtt for ESP32 Testing
监听192.168.158.148:1883
"""
import socket
import threading
import time
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class SimpleMQTTServer:
    def __init__(self, host='0.0.0.0', port=1883):
        self.host = host
        self.port = port
        self.clients = {}
        self.running = False
        
    def handle_client(self, client_socket, client_address):
        """处理MQTT客户端连接"""
        logger.info(f"ESP32 connected from {client_address}")
        self.clients[client_socket] = {
            'address': client_address,
            'connected': True
        }
        
        try:
            # 等待接收MQTT CONNECT包
            data = client_socket.recv(1024)
            if not data:
                logger.warning(f"No data received from {client_address}")
                return
                
            logger.info(f"Received MQTT packet from {client_address}: {data.hex()}")
            
            # 检查是否是CONNECT包 (0x10)
            if len(data) > 0 and data[0] == 0x10:
                logger.info("Valid MQTT CONNECT packet received")
                
                # 发送MQTT CONNACK响应 (0x20 0x02 0x00 0x00)
                connack = b'\x20\x02\x00\x00'  # CONNACK: 连接接受
                client_socket.send(connack)
                logger.info("Sent CONNACK response")
                
                # 继续处理其他MQTT包
                while self.running and self.clients[client_socket]['connected']:
                    data = client_socket.recv(1024)
                    if not data:
                        break
                    
                    # 记录接收到的MQTT数据包
                    if len(data) > 0:
                        logger.info(f"Received MQTT packet from {client_address}: {data.hex()}")
                        
                        # 如果是PUBLISH包，发送PUBACK
                        if data[0] & 0xF0 == 0x30:  # PUBLISH
                            packet_id = data[2:4] if len(data) > 4 else b'\x00\x01'
                            puback = b'\x40\x02' + packet_id  # PUBACK
                            client_socket.send(puback)
                            logger.info(f"Sent PUBACK for packet {packet_id.hex()}")
            else:
                logger.warning(f"Invalid MQTT packet or not CONNECT packet: {data.hex()}")
                        
        except Exception as e:
            logger.debug(f"Client {client_address} error: {e}")
        finally:
            self.disconnect_client(client_socket)
    
    def disconnect_client(self, client_socket):
        """断开客户端连接"""
        if client_socket in self.clients:
            client_info = self.clients[client_socket]
            logger.info(f"ESP32 {client_info['address']} disconnected")
            try:
                client_socket.close()
            except:
                pass
            del self.clients[client_socket]
    
    def start(self):
        """启动MQTT服务器"""
        self.running = True
        
        # 创建服务器socket
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            server_socket.bind((self.host, self.port))
            server_socket.listen(5)
            server_socket.settimeout(1.0)
            
            logger.info(f"MQTT Server started on {self.host}:{self.port}")
            logger.info("Waiting for ESP32 connections...")
            
            while self.running:
                try:
                    client_socket, client_address = server_socket.accept()
                    # 为新客户端创建线程
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, client_address)
                    )
                    client_thread.daemon = True
                    client_thread.start()
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        logger.error(f"Accept error: {e}")
                    break
                    
        except Exception as e:
            logger.error(f"Server error: {e}")
        finally:
            server_socket.close()
            self.stop()
    
    def stop(self):
        """停止服务器"""
        self.running = False
        for client_socket in list(self.clients.keys()):
            self.disconnect_client(client_socket)
        logger.info("MQTT Server stopped")

def main():
    server = SimpleMQTTServer()
    
    try:
        server.start()
    except KeyboardInterrupt:
        logger.info("Received interrupt signal, shutting down...")
        server.stop()
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        server.stop()

if __name__ == "__main__":
    main()