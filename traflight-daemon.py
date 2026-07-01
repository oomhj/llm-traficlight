#!/usr/bin/env python3
"""
traflight-daemon.py — 常驻串口守护进程 (Python)

保持串口持续连接，通过 FIFO 队列接收命令，毫秒级响应。
每秒自动采集 CPU/内存使用率并更新显示。

所有 traflight / health / hooks 命令一律走队列，统一管理串口。

用法:
    python3 traflight-daemon.py start         启动后台守护进程
    python3 traflight-daemon.py stop          停止
    python3 traflight-daemon.py restart       重启
    python3 traflight-daemon.py status        查看状态
    python3 traflight-daemon.py send <cmd>    发送命令到队列
    python3 traflight-daemon.py run           前台运行 (调试用)
"""

import json
import os
import signal
import sys
import time
import glob
import threading

BAUD = 115200
QUEUE_DIR = "/tmp/traflight-queue"
PIDFILE = "/tmp/traflight-daemon.pid"

def find_port():
    """自动发现串口设备"""
    import glob
    patterns = ["/dev/ttyUSB*", "/dev/ttyACM*", "/dev/cu.usbserial*",
                "/dev/cu.usbmodem*", "/dev/cu.wchusbserial*"]
    for p in patterns:
        ports = sorted(glob.glob(p))
        if ports:
            return ports[0]
    return "/dev/cu.usbserial-210"  # fallback

PORT = find_port()


def parse_cmd(cmd_str):
    """将命令字符串解析为 JSON 命令字典"""
    parts = cmd_str.strip().split()
    if not parts:
        return None

    c = parts[0]

    if c == "yellow":
        return {"cmd": "light", "value": "yellow"}
    elif c == "green":
        return {"cmd": "light", "value": "green"}
    elif c == "red":
        return {"cmd": "light", "value": "red"}
    elif c == "off":
        return {"cmd": "light", "value": "off"}
    elif c == "status":
        return {"cmd": "status"}
    elif c == "blink":
        color = "red"
        times = 3
        interval = 500
        for i, p in enumerate(parts):
            if p == "-n" and i + 1 < len(parts):
                times = int(parts[i + 1])
            elif p == "-i" and i + 1 < len(parts):
                interval = int(parts[i + 1])
            elif p in ("red", "yellow", "green") and i > 0:
                color = p
        return {"cmd": "blink", "value": color, "times": times, "interval": interval}
    elif c == "health":
        cpu = mem = 0
        for i, p in enumerate(parts):
            if p == "--cpu" and i + 1 < len(parts):
                cpu = int(parts[i + 1])
            elif p == "--mem" and i + 1 < len(parts):
                mem = int(parts[i + 1])
        return {"cmd": "health", "cpu": cpu, "mem": mem}
    elif c == "notify":
        return {"cmd": "light", "value": "red"}
    elif c == "blink_red":
        return {"cmd": "blink", "value": "red", "times": 3, "interval": 500}
    elif c == "send_raw":
        # "send_raw <json_string>"
        raw = " ".join(parts[1:])
        try:
            return json.loads(raw)
        except json.JSONDecodeError:
            return None

    return None


def send_cmd(ser, cmd_dict):
    """通过持久串口连接发送 JSON 命令"""
    line = json.dumps(cmd_dict) + "\n"
    ser.write(line.encode())
    ser.flush()
    # 极短延时等响应到达，然后丢弃 (daemon 不需解析)
    time.sleep(0.01)
    while ser.in_waiting:
        ser.readline()


# 健康数据共享变量 (线程安全, Python int 赋值原子)
_health_cpu = 0
_health_mem = 0
_health_updated = False
_health_lock = threading.Lock()


def health_collector():
    """后台线程: 每秒采集 CPU(cpu_percent interval=1) + 内存"""
    try:
        import psutil
    except ImportError:
        print("[DAEMON] ⚠️ psutil not installed — health bars will show 0%")
        print("[DAEMON]    Run: pip install psutil")
        return

    global _health_cpu, _health_mem, _health_updated
    while True:
        cpu = int(psutil.cpu_percent(interval=1))
        mem = int(psutil.virtual_memory().percent)
        with _health_lock:
            _health_cpu = cpu
            _health_mem = mem
            _health_updated = True


def daemon_loop():
    """主循环: 串口 + 队列 + 消费健康线程数据"""
    import serial

    os.makedirs(QUEUE_DIR, exist_ok=True)

    try:
        ser = serial.Serial(PORT, BAUD, timeout=2)
        ser.reset_input_buffer()
    except serial.SerialException as e:
        print(f"❌ Cannot open {PORT}: {e}")
        sys.exit(1)

    # 注册退出时关闭串口
    def cleanup(*a):
        try:
            ser.close()
        except Exception:
            pass
        sys.exit(0)
    signal.signal(signal.SIGTERM, cleanup)
    signal.signal(signal.SIGINT, signal.SIG_IGN)

    # 启动健康采集线程 (独立线程，不阻塞主循环)
    t = threading.Thread(target=health_collector, daemon=True)
    t.start()

    while True:
        now = time.time()

        # 消费健康线程的最新数据
        global _health_cpu, _health_mem, _health_updated
        if _health_updated:
            with _health_lock:
                cpu = _health_cpu
                mem = _health_mem
                _health_updated = False
            send_cmd(ser, {"cmd": "health", "cpu": cpu, "mem": mem})

        # 扫描 FIFO 队列
        for entry in sorted(glob.glob(os.path.join(QUEUE_DIR, "cmd_*"))):
            try:
                with open(entry) as f:
                    cmd_str = f.read().strip()
                os.unlink(entry)

                if cmd_str:
                    cmd_dict = parse_cmd(cmd_str)
                    if cmd_dict:
                        send_cmd(ser, cmd_dict)
            except Exception:
                try:
                    os.unlink(entry)
                except Exception:
                    pass

        time.sleep(0.05)


def write_pid(pid):
    with open(PIDFILE, "w") as f:
        f.write(str(pid))


def read_pid():
    if os.path.exists(PIDFILE):
        with open(PIDFILE) as f:
            return int(f.read().strip())
    return None


def is_running(pid):
    try:
        os.kill(pid, 0)
        return True
    except (OSError, ProcessLookupError):
        return False


def start_daemon():
    pid = read_pid()
    if pid and is_running(pid):
        print(f"already running (pid {pid})")
        return

    child_pid = os.fork()
    if child_pid == 0:
        # 子进程 (信号处理在 daemon_loop 内注册)
        daemon_loop()
    else:
        write_pid(child_pid)
        # 等待子进程就绪
        time.sleep(0.5)
        print(f"started (pid {child_pid})")


def stop_daemon():
    pid = read_pid()
    if pid:
        try:
            os.kill(pid, signal.SIGTERM)
            time.sleep(0.3)
        except ProcessLookupError:
            pass
        try:
            os.unlink(PIDFILE)
        except Exception:
            pass
    # 清理队列
    for f in glob.glob(os.path.join(QUEUE_DIR, "cmd_*")):
        try:
            os.unlink(f)
        except Exception:
            pass
    print("stopped")


def cmd_status():
    pid = read_pid()
    if pid and is_running(pid):
        print(f"running (pid {pid})")
    else:
        print("stopped")


def send_to_queue(cmd_str):
    """写入命令到 FIFO 队列"""
    os.makedirs(QUEUE_DIR, exist_ok=True)
    seq = int(time.time() * 1000000)
    entry = os.path.join(QUEUE_DIR, f"cmd_{seq}_{os.getpid()}")
    with open(entry, "w") as f:
        f.write(cmd_str)


def main():
    if len(sys.argv) < 2:
        print("Usage: traflight-daemon.py start|stop|status|send <cmd>|run")
        sys.exit(1)

    action = sys.argv[1]

    if action == "start":
        start_daemon()
    elif action == "stop":
        stop_daemon()
    elif action == "status":
        cmd_status()
    elif action == "restart":
        stop_daemon()
        time.sleep(0.5)
        start_daemon()
    elif action == "send":
        cmd_str = " ".join(sys.argv[2:])
        if cmd_str:
            send_to_queue(cmd_str)
    elif action == "run":
        daemon_loop()
    else:
        print(f"Unknown action: {action}")


if __name__ == "__main__":
    main()
