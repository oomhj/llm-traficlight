#!/usr/bin/env python3
"""
Daemon throughput stress test.
所有命令通过 daemon 队列发送，不直接操作串口。
"""
import subprocess
import time
import sys
import os

DAEMON = os.path.join(os.path.dirname(__file__), "..", "traflight-daemon.py")

def cmd(cmd_str):
    """通过 daemon 发送命令，返回耗时 (ms)"""
    start = time.perf_counter()
    subprocess.run(["python3", DAEMON, "send", cmd_str],
                   capture_output=True, timeout=10)
    return (time.perf_counter() - start) * 1000


def wait_queue_empty():
    """等待 daemon 处理完所有队列"""
    qdir = "/tmp/traflight-queue"
    for _ in range(100):
        if not os.listdir(qdir):
            return
        time.sleep(0.01)


print("=" * 60)
print("🚦 Daemon Stress Test")
print("=" * 60)
print()

# 确保 daemon 运行
subprocess.run(["python3", DAEMON, "start"], capture_output=True)
time.sleep(1)

# === 1. 单命令延迟 ===
print("── 1. Daemon send latency (avg of 20) ──")
for label, cmd_str in [
    ("light red",      "red"),
    ("light green",    "green"),
    ("light yellow",   "yellow"),
    ("light off",      "off"),
    ("status",         "status"),
    ("blink (3x500)",  "blink_red"),
]:
    times = []
    for _ in range(20):
        t = cmd(cmd_str)
        times.append(t)
    wait_queue_empty()
    avg = sum(times) / len(times)
    print(f"  {label:<20}  avg={avg:5.1f}ms  min={min(times):5.1f}ms  max={max(times):5.1f}ms")

# === 2. 吞吐量 ===
print()
print("── 2. Throughput (daemon send, 3 seconds) ──")
for label, cmd_str in [("light green", "green"), ("status", "status")]:
    count = 0
    deadline = time.time() + 3
    while time.time() < deadline:
        cmd(cmd_str)
        count += 1
    wait_queue_empty()
    elapsed = 3
    print(f"  {label:<20}  {count} commands in {elapsed}s = {count/elapsed:.0f}/s")

# === 3. 批量突发 ===
print()
print("── 3. Burst: 100 rapid alternations ──")
start = time.perf_counter()
cmds = ["red" if i % 2 == 0 else "green" for i in range(100)]
for c in cmds:
    cmd(c)
wait_queue_empty()
elapsed = time.perf_counter() - start
print(f"  100 switches in {elapsed:.1f}s = {100/elapsed:.0f}/s  (avg {(elapsed/100)*1000:.1f}ms each)")

# === 4. 大命令 ===
print()
print("── 4. Large: 50-step pattern ──")
steps = []
for i in range(50):
    color = "red" if i % 3 == 0 else "green" if i % 3 == 1 else "yellow"
    steps.append(f"{color}:0.1")
pattern_str = "pattern " + ",".join(steps)
t = cmd(pattern_str)
wait_queue_empty()
print(f"  50-step pattern: send={t:.1f}ms")

# === 5. 混合负载 ===
print()
print("── 5. Mixed: 200 commands ──")
mixed = []
for i in range(50):
    mixed += ["red", "status", "green", "status"]
mixed.append("blink_red")
start = time.perf_counter()
for c in mixed:
    cmd(c)
wait_queue_empty()
elapsed = time.perf_counter() - start
print(f"  201 commands in {elapsed:.1f}s = {201/elapsed:.0f}/s")

# === 6. 队列深度 ===
print()
print("── 6. Queue depth: 500 rapid sends ──")
start = time.perf_counter()
for i in range(500):
    cmd("status")
wait_queue_empty()
elapsed = time.perf_counter() - start
print(f"  500 commands in {elapsed:.1f}s = {500/elapsed:.0f}/s")

print()
print("=" * 60)
print("✅ Done")
print("=" * 60)
