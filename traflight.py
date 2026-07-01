#!/usr/bin/env python3
"""
traflight — LLM Traffic Light CLI (桥接层)
============================================
供 AI Agent 通过命令行控制 ESP8266 红绿灯。

用法:
    traflight red                  亮红灯
    traflight green                亮绿灯
    traflight yellow               亮黄灯
    traflight off                  关闭所有灯
    traflight status               查询状态
    traflight blink <color>        闪烁 (默认3次, 500ms)
    traflight blink <color> -n 5 -i 300  闪烁5次, 间隔300ms
    traflight pattern "red:3,green:5,yellow:1"  灯光序列
    traflight cycle                标准红绿灯周期
    traflight port                 显示当前串口
    traflight scan                 扫描可用串口
    traflight help                 显示帮助

退出码: 0=成功, 1=错误
"""

import sys
import json
import time
import argparse
import glob
import os


# ======================== 配置 ========================
SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 3         # 读超时(秒)
CMD_TIMEOUT = 10           # 等待命令完成(秒)
SERIAL_PATTERNS = [
    "/dev/ttyUSB*",         # Linux (CH340/CP2102)
    "/dev/ttyACM*",         # Linux (Arduino)
    "/dev/cu.usbserial*",   # macOS (CP2102)
    "/dev/cu.usbmodem*",    # macOS (Arduino)
    "/dev/cu.wchusbserial*",# macOS (CH340)
    "COM[0-9]",             # Windows
]


# ======================== 串口发现 ========================

def scan_ports():
    """扫描系统所有匹配的串口设备，返回完整路径列表"""
    ports = []
    for pattern in SERIAL_PATTERNS:
        # Windows: COM 端口用数字范围展开
        if pattern.startswith("COM"):
            for i in range(256):
                p = f"COM{i}"
                if os.path.exists(p):
                    ports.append(p)
        else:
            ports.extend(sorted(glob.glob(pattern)))
    return ports


def find_port():
    """自动发现串口设备，返回第一个可用端口"""
    ports = scan_ports()
    if not ports:
        return None
    return ports[0]


def print_scan():
    """打印所有可用串口"""
    ports = scan_ports()
    if ports:
        print("Available serial ports:")
        for p in ports:
            print(f"  {p}")
    else:
        print("No serial ports found.")
        print("Make sure ESP8266 is connected via USB.")
        print()
        print("Troubleshooting:")
        print("  Linux:   ls /dev/ttyUSB*")
        print("  macOS:   ls /dev/cu.usbserial* /dev/cu.usbmodem*")
        print("  Windows: Check Device Manager → Ports (COM & LPT)")


# ======================== 串口通信 ========================

class TrafficLight:
    """红绿灯控制器 — 封装串口通信"""

    def __init__(self, port=None):
        self.port = port or find_port()
        self.ser = None

    def connect(self):
        """连接串口 (带重试)"""
        if not self.port:
            print("❌ No serial port found.", file=sys.stderr)
            print("   Use 'traflight scan' to list available ports.", file=sys.stderr)
            print("   Or specify port manually: traflight --port /dev/ttyUSB0 <cmd>", file=sys.stderr)
            sys.exit(1)

        try:
            import serial
            self.ser = serial.Serial(
                self.port,
                SERIAL_BAUD,
                timeout=SERIAL_TIMEOUT,
                write_timeout=2
            )
            # 等待 ESP8266 就绪
            time.sleep(2)
            # 清空缓冲区 (丢弃启动日志)
            self.ser.reset_input_buffer()
            return True
        except ImportError:
            print("❌ pyserial not installed.", file=sys.stderr)
            print("   Run: pip install pyserial", file=sys.stderr)
            sys.exit(1)
        except serial.SerialException as e:
            print(f"❌ Cannot open {self.port}: {e}", file=sys.stderr)
            print("   Check:", file=sys.stderr)
            print("   • Is the ESP8266 plugged in?", file=sys.stderr)
            print("   • Is another program using the port? (e.g. monitor)", file=sys.stderr)
            if sys.platform != "win32":
                print(f"   • Try: sudo chmod 666 {self.port}", file=sys.stderr)
            sys.exit(1)

    def send_cmd(self, cmd_dict, quiet=False):
        """发送 JSON 命令，返回解析后的响应字典"""
        if not self.ser:
            self.connect()

        line = json.dumps(cmd_dict) + "\n"
        if not quiet:
            print(f"→ {line.strip()}")

        # 发送
        self.ser.write(line.encode("utf-8"))

        # 读取响应 (最多等 CMD_TIMEOUT 秒)
        deadline = time.time() + CMD_TIMEOUT
        while time.time() < deadline:
            if self.ser.in_waiting:
                raw = self.ser.readline().decode("utf-8", errors="replace").strip()
                if raw:
                    if not quiet:
                        print(f"← {raw}")
                    try:
                        return json.loads(raw)
                    except json.JSONDecodeError:
                        # 可能是启动日志，跳过
                        continue
            else:
                time.sleep(0.05)

        print("⚠️  No response (timeout)", file=sys.stderr)
        return {"status": "error", "message": "timeout"}

    def close(self):
        """关闭串口"""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.close()


# ======================== 高层命令 ========================

def cmd_set_light(tfl, color):
    """设置灯光"""
    resp = tfl.send_cmd({"cmd": "light", "value": color})
    if resp.get("status") != "ok":
        sys.exit(1)


def cmd_blink(tfl, color, times=3, interval=500):
    """闪烁指定灯"""
    resp = tfl.send_cmd({
        "cmd": "blink",
        "value": color,
        "times": times,
        "interval": interval,
    })
    if resp.get("status") != "ok":
        sys.exit(1)


def cmd_pattern(tfl, steps_spec):
    """解析 "red:3,green:5,yellow:1" 格式并执行序列"""
    steps = []
    for part in steps_spec.split(","):
        part = part.strip()
        if ":" in part:
            color, secs = part.split(":", 1)
            color = color.strip().lower()
            try:
                duration = int(float(secs.strip()) * 1000)
            except ValueError:
                print(f"❌ Invalid duration: {secs}", file=sys.stderr)
                sys.exit(1)
        else:
            # 纯颜色名 → 默认 1 秒
            color = part.lower()
            duration = 1000

        if color not in ("red", "yellow", "green", "off"):
            print(f"❌ Invalid color: {color} (use: red/yellow/green/off)", file=sys.stderr)
            sys.exit(1)

        steps.append([color, duration])

    if not steps:
        print("❌ No steps specified", file=sys.stderr)
        sys.exit(1)

    print(f"📋 Pattern: {' → '.join(f'{s[0]}({s[1]}ms)' for s in steps)}")
    resp = tfl.send_cmd({"cmd": "pattern", "steps": steps})
    if resp.get("status") != "ok":
        sys.exit(1)


def cmd_cycle(tfl, green_s=5, yellow_s=2, red_s=5):
    """标准红绿灯周期"""
    print("🟢 Green 5s → 🟡 Yellow 2s → 🔴 Red 5s")
    cmd_pattern(tfl, f"green:{green_s},yellow:{yellow_s},red:{red_s}")


def cmd_status(tfl):
    """查询状态并显示"""
    resp = tfl.send_cmd({"cmd": "status"})
    if resp.get("status") == "ok":
        light = resp.get("light", "?")
        blink = " ⚡" if resp.get("blinking") else ""
        pat = " 🎬" if resp.get("pattern_active") else ""
        icon = {"red": "🔴", "yellow": "🟡", "green": "🟢", "off": "⚫"}.get(light, "❓")
        print(f"\n  {icon} Light: {light}{blink}{pat}")
        print(f"  ⏱ Uptime: {resp.get('uptime_ms', 0) // 1000}s")
    else:
        sys.exit(1)


# ======================== CLI 入口 ========================

def print_help():
    print(__doc__.strip())


def main():
    parser = argparse.ArgumentParser(
        description="🚦 LLM Traffic Light — 控制 ESP8266 红绿灯",
        add_help=False,
    )
    parser.add_argument("--port", "-p", help="串口设备路径 (默认自动发现)")
    parser.add_argument("--help", "-h", action="store_true", help="显示帮助")

    # 子命令
    sub = parser.add_subparsers(dest="command", metavar="<command>")

    p_red = sub.add_parser("red", help="亮红灯")
    p_yellow = sub.add_parser("yellow", help="亮黄灯")
    p_green = sub.add_parser("green", help="亮绿灯")
    p_off = sub.add_parser("off", help="关闭所有灯")

    p_status = sub.add_parser("status", help="查询状态")

    p_blink = sub.add_parser("blink", help="闪烁指定灯")
    p_blink.add_argument("color", nargs="?", default="red", help="灯色 (red/yellow/green)")
    p_blink.add_argument("-n", "--times", type=int, default=3, help="闪烁次数 (默认: 3)")
    p_blink.add_argument("-i", "--interval", type=int, default=500, help="间隔毫秒 (默认: 500)")

    p_pattern = sub.add_parser("pattern", help="灯光序列, 格式: red:3,green:5,yellow:1")
    p_pattern.add_argument("steps", help='序列描述, 如 "red:3,green:5,yellow:1"')

    p_cycle = sub.add_parser("cycle", help="标准红绿灯周期 (绿→黄→红)")

    p_port = sub.add_parser("port", help="显示当前串口设备")
    p_scan = sub.add_parser("scan", help="扫描可用串口")
    p_help = sub.add_parser("help", help="显示帮助")

    args, unknown = parser.parse_known_args()

    # 处理 --help / help
    if args.help or args.command == "help":
        print_help()
        return

    # scan: 不需要连串口
    if args.command == "scan":
        print_scan()
        return

    # port: 显示当前串口
    if args.command == "port":
        port = args.port or find_port()
        if port:
            print(port)
        else:
            print("No serial port found.", file=sys.stderr)
            sys.exit(1)
        return

    # 需要串口的命令
    if not args.command:
        print_help()
        print("\n" + "=" * 50)
        print("💡 Quick start:")
        print("   traflight green      ← 亮绿灯")
        print("   traflight status     ← 查状态")
        print("   traflight blink red  ← 闪烁")
        print("   traflight cycle      ← 标准周期")
        sys.exit(1)

    try:
        with TrafficLight(args.port) as tfl:
            if args.command == "red":
                cmd_set_light(tfl, "red")
            elif args.command == "yellow":
                cmd_set_light(tfl, "yellow")
            elif args.command == "green":
                cmd_set_light(tfl, "green")
            elif args.command == "off":
                cmd_set_light(tfl, "off")
            elif args.command == "blink":
                cmd_blink(tfl, args.color, args.times, args.interval)
            elif args.command == "pattern":
                cmd_pattern(tfl, args.steps)
            elif args.command == "cycle":
                cmd_cycle(tfl)
            elif args.command == "status":
                cmd_status(tfl)
    except KeyboardInterrupt:
        print("\n👋 Interrupted")
        sys.exit(130)


if __name__ == "__main__":
    main()
