# 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                     🖥️ Claude Code                              │
│                                                                  │
│  PreToolUse  ──→ bash traflight-hook.sh before                  │
│  PostToolUse  ──→ bash traflight-hook.sh success                │
│  PostToolUseFailure ─→ bash traflight-hook.sh failure           │
│  Notification ──→ bash traflight-hook.sh notify                 │
└─────────────────────────┬────────────────────────────────────────┘
                          │
┌─────────────────────────▼────────────────────────────────────────┐
│                  traflight-hook.sh                               │
│  "before" → python3 traflight-daemon.py send yellow              │
│  "success" → python3 traflight-daemon.py send green              │
│  "failure" → python3 traflight-daemon.py send red                │
│  "notify"  → python3 traflight-daemon.py send blink_red          │
└─────────────────────────┬────────────────────────────────────────┘
                          │ 写文件到 /tmp/traflight-queue/
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│                Python Daemon (traflight-daemon.py)                │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 主线程:                                                     │  │
│  │  ① 轮询队列 (每50ms) → parse_cmd → send_cmd                │  │
│  │  ② 消费健康数据 → send_cmd                                  │  │
│  │  ③ 串口独占 (持久连接)                                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 健康采集线程 (daemon=True):                                 │  │
│  │  psutil.cpu_percent(interval=1)  →  共享变量                │  │
│  │  psutil.virtual_memory().percent →  共享变量                │  │
│  │  每秒一次                                                  │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────┬────────────────────────────────────────┘
                          │ USB 串口 115200 8N1
┌─────────────────────────▼────────────────────────────────────────┐
│                    🚦 ESP8266 NodeMCU                            │
│                                                                  │
│  main.cpp → loop():                                              │
│    readSerial()   →  收到 JSON → 解析                            │
│    updateBlink()  →  millis() 非阻塞闪烁                         │
│    updatePattern() → millis() 非阻塞序列                         │
│                                                                  │
│  ├─ light   → setLight() → 增量绘制 (~5ms)                       │
│  ├─ blink   → 启动闪烁状态机                                      │
│  ├─ pattern → 启动序列状态机                                      │
│  ├─ health  → 更新 CPU/MEM 20格条                                 │
│  └─ status  → 返回 JSON                                          │
└─────────────────────────┬────────────────────────────────────────┘
                          │ SPI
┌─────────────────────────▼────────────────────────────────────────┐
│                    📺 ST7735 TFT 128×128                         │
│                                                                  │
│  ┌──────────────────────────────────┐                            │
│  │ (🔴)        (🟡)        (🟢)     │ ← r=18                     │
│  │ CPU ████████████████░░░░ 73%    │ ← 20格 颜色随负载           │
│  │ MEM ████████░░░░░░░░░░░░ 42%   │                              │
│  └──────────────────────────────────┘                            │
└──────────────────────────────────────────────────────────────────┘
```

## 核心文件职责

| 文件 | 职责 | 使用者 |
|------|------|--------|
| `traflight-daemon.py` | 🧠 常驻进程，持串口+队列+健康采集 | 后台自动运行 |
| `traflight-hook.sh` | 🔌 Hook 回调入口，翻译命令 | Claude Code hooks |
| `traflight-daemon.sh` | 🧑‍💻 用户 CLI 入口 | 手动控制 |
| `traflight.py` | 📦 Python 桥接层（直开串口，备用） | 直接调试 |
| `scripts/install-hooks.py` | 🔧 安装 hooks 到 ~/.claude/settings.json | 首次配置 |
| `src/main.cpp` | 🔥 ESP8266 固件（TFT+协议） | 编译烧录 |

## 数据流摘要

```
所有命令 → FIFO 队列 → 常驻串口 → ESP8266
健康数据 → 后台线程 → 常驻串口 → ESP8266 (每秒)
```

串口由 daemon 独占，无竞争、无阻塞、全线速。

## 状态约定

| 事件 | 灯色 | 含义 |
|------|------|------|
| `PreToolUse` | 🟡 黄灯 | 正在执行任务 |
| `PostToolUse` | 🟢 绿灯 | 执行完成 |
| `PostToolUseFailure` | 🔴 红灯 | 命令失败，需关注 |
| `Notification` | 🔴 闪烁 | 桌面通知提醒 |
| 手动 `traflight red` | 🔴 红灯 | 需要用户输入 |
