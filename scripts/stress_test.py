#!/usr/bin/env python3
"""
Red-green traffic light stress test.
Tests single latency, burst throughput, and rapid switching.
"""

import serial
import json
import time
import sys

PORT = "/dev/cu.usbserial-210"
BAUD = 115200


def connect():
    ser = serial.Serial(PORT, BAUD, timeout=5)
    time.sleep(2)
    ser.reset_input_buffer()
    return ser


def send_raw(ser, cmd_dict):
    """Send JSON, return parsed response and round-trip time."""
    line = json.dumps(cmd_dict) + "\n"
    start = time.perf_counter()
    ser.write(line.encode())
    # Read until valid JSON
    deadline = time.time() + 3
    while time.time() < deadline:
        raw = ser.readline().decode(errors="replace").strip()
        if not raw:
            continue
        if raw.startswith("{"):
            try:
                resp = json.loads(raw)
                elapsed = (time.perf_counter() - start) * 1000  # ms
                return resp, elapsed
            except json.JSONDecodeError:
                continue
    return {"status": "error"}, None


def test_latency(ser, label, cmd_dict, n=10):
    """Measure single command latency over n repetitions."""
    times = []
    for _ in range(n):
        resp, ms = send_raw(ser, cmd_dict)
        if resp.get("status") == "ok":
            times.append(ms)
    avg = sum(times) / len(times)
    print(f"  {label:<30}  avg={avg:6.1f}ms  min={min(times):6.1f}ms  max={max(times):6.1f}ms  (n={len(times)})")


def test_throughput(ser, label, cmd_dict, duration=3):
    """Send as many commands as possible in `duration` seconds."""
    count = 0
    ok = 0
    deadline = time.time() + duration
    while time.time() < deadline:
        resp, ms = send_raw(ser, cmd_dict)
        count += 1
        if resp.get("status") == "ok":
            ok += 1
    rate = count / duration
    print(f"  {label:<30}  {count} commands in {duration}s = {rate:.0f}/s  (ok={ok})")


def test_burst(ser, label, cmd_list, delay=0):
    """Send commands in rapid succession, measure each."""
    times = []
    ok = 0
    for cmd in cmd_list:
        resp, ms = send_raw(ser, cmd)
        if resp.get("status") == "ok":
            ok += 1
        if ms:
            times.append(ms)
        if delay > 0:
            time.sleep(delay)
    avg = sum(times) / len(times) if times else 0
    print(f"  {label:<30}  avg={avg:6.1f}ms  ok={ok}/{len(cmd_list)}")


print("=" * 60)
print("🚦 Traffic Light Stress Test")
print("=" * 60)
print()

ser = connect()

# === 1. 单命令延迟 ===
print("── 1. Latency (single command, avg of 10) ──")
test_latency(ser, "light red",      {"cmd": "light", "value": "red"}, 10)
test_latency(ser, "light green",    {"cmd": "light", "value": "green"}, 10)
test_latency(ser, "light yellow",   {"cmd": "light", "value": "yellow"}, 10)
test_latency(ser, "light off",      {"cmd": "light", "value": "off"}, 10)
test_latency(ser, "status",         {"cmd": "status"}, 10)
test_latency(ser, "blink (3x500)",  {"cmd": "blink", "value": "red", "times": 3, "interval": 500}, 5)

# === 2. 吞吐量 ===
print()
print("── 2. Throughput (max commands in 3s) ──")
test_throughput(ser, "light green", {"cmd": "light", "value": "green"}, 3)
test_throughput(ser, "status",      {"cmd": "status"}, 3)

# === 3. 快速切换 ===
print()
print("── 3. Rapid alternation (red↔green, 100 switches) ──")
cmds = []
for i in range(50):
    cmds.append({"cmd": "light", "value": "red" if i % 2 == 0 else "green"})
test_burst(ser, "red↔green × 50 (no delay)", cmds, delay=0)

# === 4. 混合负载 ===
print()
print("── 4. Mixed load (interleave light/status/blink) ──")
cmds = []
for i in range(20):
    cmds.append({"cmd": "light", "value": "red"})
    cmds.append({"cmd": "status"})
    cmds.append({"cmd": "light", "value": "green"})
    cmds.append({"cmd": "status"})
cmds.append({"cmd": "blink", "value": "yellow", "times": 2, "interval": 200})
test_burst(ser, "80 mixed commands", cmds, delay=0.05)

# === 5. 大命令 ===
print()
print("── 5. Large command (50-step pattern) ──")
steps = [[("red" if i % 3 == 0 else "green" if i % 3 == 1 else "yellow"), 100] for i in range(50)]
resp, ms = send_raw(ser, {"cmd": "pattern", "steps": steps})
print(f"  50-step pattern:  {ms:.1f}ms  → {resp.get('status')}")

ser.close()
print()
print("=" * 60)
print("✅ Done")
print("=" * 60)
