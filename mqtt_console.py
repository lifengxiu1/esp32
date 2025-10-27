#!/usr/bin/env python3
# pip install paho-mqtt
import paho.mqtt.client as mqtt
import json, sys, threading

BROKER = "192.168.158.148"
PORT   = 1883
DOWN   = "rgb/ctrl"  # 下行：发控制
UP     = "rgb/ack"   # 上行：收回执

def on_connect(c, u, f, rc, properties):
    print(f"[MQTT] connected rc={rc}")
    c.subscribe(UP, qos=1)

def on_message(c, u, m):
    if m.topic != UP: return
    try:
        ack = json.loads(m.payload.decode("utf-8"))
    except Exception:
        print(f"[ACK] {m.payload!r}")
        return
    reason = ack.get("reason")
    lights = ack.get("lights", [])
    # 仅演示打印前几条
    sample = lights[:5]
    print(f"[ACK] reason={reason}, total={len(lights)}, sample={sample}")

def send(payload: dict):
    s = json.dumps(payload, separators=(",",":"))
    rc = client.publish(DOWN, s, qos=1)
    print(f"[SEND] {DOWN} {s} -> rc={rc.rc}")

client = mqtt.Client(client_id="pc-console", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.on_connect  = on_connect
client.on_message  = on_message
client.connect(BROKER, PORT, keepalive=30)
client.loop_start()

help_txt = """
命令示例（回车发送）：
  on <pixel> <sec>     亮 指定像素 <sec> 秒   （pixel 0=1号灯）
  off <pixel>          灭 指定像素
  all_on               全亮（不计时）
  all_off              全灭
  raw <json>           直接发送原始JSON到 rgb/ctrl
  q                    退出
"""
print(help_txt)

try:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        if line.lower() in ("q","quit","exit"):
            break
        if line.startswith("raw "):
            js = line[4:].strip()
            try:
                payload = json.loads(js)
            except Exception as e:
                print("JSON 解析失败：", e); continue
            send(payload)
            continue

        parts = line.split()
        cmd = parts[0].lower()

        if cmd == "on" and len(parts) >= 3:
            pixel = int(parts[1])         # 0基：0=1号灯
            sec   = int(parts[2])
            send({"cmd":"set","effect":"solid","pixel":pixel,"duration_ms":sec*1000})
        elif cmd == "off" and len(parts) >= 2:
            pixel = int(parts[1])
            send({"cmd":"set","effect":"off","pixel":pixel})
        elif cmd == "all_on":
            send({"cmd":"set","effect":"solid"})
        elif cmd == "all_off":
            send({"cmd":"set","effect":"off"})
        else:
            print("无效命令。", help_txt)
finally:
    client.loop_stop()
    client.disconnect()