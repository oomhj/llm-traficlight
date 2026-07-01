#!/usr/bin/env python3
"""
traflight-health.py — 系统健康监控 (psutil)
采集 CPU、内存、磁盘、负载，可选设置红绿灯阈值。

用法:
    python3 traflight-health.py              # 打印 JSON 状态
    python3 traflight-health.py --light      # 打印 + 根据阈值设置灯
    python3 traflight-health.py --watch      # 每 5 秒监控一次
"""

import json
import os
import sys
import time

try:
    import psutil
except ImportError:
    print(json.dumps({"error": "psutil not installed. Run: pip install psutil"}))
    sys.exit(1)

# 阈值
THRESHOLDS = {
    "cpu_red": 80,      # CPU > 80% → 红灯
    "cpu_yellow": 50,    # CPU > 50% → 黄灯
    "mem_red": 85,       # 内存 > 85% → 红灯
    "mem_yellow": 70,    # 内存 > 70% → 黄灯
    "disk_red": 90,      # 磁盘 > 90% → 红灯
}


def collect():
    """采集系统健康数据"""
    cpu = psutil.cpu_percent(interval=0.5)
    mem = psutil.virtual_memory()
    disk = psutil.disk_usage("/")
    load = os.getloadavg() if hasattr(os, "getloadavg") else (0, 0, 0)

    data = {
        "cpu_percent": cpu,
        "mem_percent": mem.percent,
        "mem_used_gb": round(mem.used / 1024 ** 3, 1),
        "mem_total_gb": round(mem.total / 1024 ** 3, 1),
        "disk_percent": disk.percent,
        "disk_used_gb": round(disk.used / 1024 ** 3, 1),
        "disk_total_gb": round(disk.total / 1024 ** 3, 1),
        "load_1m": round(load[0], 2),
        "load_5m": round(load[1], 2),
        "load_15m": round(load[2], 2),
    }

    # 温度 (macOS 不支持 psutil.sensors_temperatures)
    try:
        temps = psutil.sensors_temperatures()
        if temps:
            data["temp"] = temps
    except Exception:
        pass

    return data


def evaluate(data):
    """根据阈值评估健康状态，返回颜色"""
    if data["cpu_percent"] > THRESHOLDS["cpu_red"]:
        return "red"
    if data["mem_percent"] > THRESHOLDS["mem_red"]:
        return "red"
    if data["disk_percent"] > THRESHOLDS["disk_red"]:
        return "red"
    if data["cpu_percent"] > THRESHOLDS["cpu_yellow"]:
        return "yellow"
    if data["mem_percent"] > THRESHOLDS["mem_yellow"]:
        return "yellow"
    return "green"


def set_light(color):
    """调用 traflight.py 设置灯色"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    traflight = os.path.join(script_dir, "traflight.py")
    port = "/dev/cu.usbserial-210"
    cmd = f"python3 {traflight} --port {port} {color}"
    os.system(f"{cmd} >/dev/null 2>&1")


def main():
    data = collect()
    color = evaluate(data)
    data["health"] = color

    # 是否设置了灯
    if "--light" in sys.argv or "--watch" in sys.argv:
        set_light(color)

    # 打印 JSON
    print(json.dumps(data, indent=2))

    # 循环监控
    if "--watch" in sys.argv:
        interval = 5
        try:
            while True:
                time.sleep(interval)
                data = collect()
                color = evaluate(data)
                data["health"] = color
                set_light(color)
                print(json.dumps(data, indent=2))
                print()
        except KeyboardInterrupt:
            print("\n👋 Monitor stopped")


if __name__ == "__main__":
    main()
