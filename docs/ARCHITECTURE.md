# 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                     🖥️ Claude Code                              │
│                                                                  │
│  PreToolUse          ──→ bash traflight-hook.sh before           │
│  PostToolUse         ──→ bash traflight-hook.sh success          │
│  PostToolUseFailure  ──→ bash traflight-hook.sh failure          │
│  Notification        ──→ bash traflight-hook.sh notify           │
│  PermissionRequest   ──→ bash traflight-hook.sh permission       │
│  PermissionDenied    ──→ bash traflight-hook.sh denied           │
└─────────────────────────┬────────────────────────────────────────┘
                          │ 写 FIFO 文件到 /tmp/traflight-queue/
┌─────────────────────────▼────────────────────────────────────────┐
│               Python Daemon (traflight-daemon.py)                 │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 主线程:                                                     │  │
│  │  ① 轮询队列 (每50ms) → parse → send_cmd                    │  │
│  │  ② 消费健康数据 → send_cmd (每秒)                           │  │
│  │  ③ 串口独占 (持久连接, 无竞争)                              │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ 健康采集线程 (daemon=True):                                 │  │
│  │  psutil.cpu_percent(interval=1) → 共享变量                  │  │
│  │  psutil.virtual_memory().percent → 共享变量                 │  │
│  │  每秒一次, 不阻塞主循环                                     │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────┬────────────────────────────────────────┘
                          │ USB 串口 115200 8N1
┌─────────────────────────▼────────────────────────────────────────┐
│                    🚦 ESP8266 NodeMCU                            │
│                                                                  │
│  main.cpp → loop():                                              │
│    readSerial()     → JSON 解析 → command handler                │
│    updateBlinkAll() → 三灯齐闪 (持久闪烁直到下一指令)            │
│    updateBlink()    → 单灯闪烁状态机 (自动结束)                   │
│    updatePattern()  → 序列状态机                                  │
│                                                                  │
│  Commands:                                                       │
│    light   → drawTrafficLight() → 全屏重绘 (安全清除残留)         │
│    blink   → 启动单灯闪烁状态机                                   │
│    blink_all → 三灯齐闪 (持续闪烁)                                │
│    pattern → 启动序列状态机                                       │
│    health  → 更新 CPU/MEM 20格条                                  │
│    status  → 返回 JSON                                           │
│                                                                  │
│  ⚠️ 安全清除: 所有 command handler 使用 drawTrafficLight()        │
│     而非 setLight() 进入初始状态, 确保 blink_all 残留多灯        │
│     被清除, 避免多灯同时点亮。                                    │
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
| `traflight-daemon.py` | 🧠 常驻进程，持串口+队列+健康采集 | 后台自动运行 (fork daemon) |
| `traflight-hook.sh` | 🔌 Hook 回调入口，将 6 个 hook 事件翻译为 daemon 命令 | Claude Code hooks |
| `traflight.py` | 📦 Python CLI 桥接层（直开串口，备用手动控制） | 直接调试 / 非 hook 场景 |
| `scripts/install-hooks.py` | 🔧 安装 hooks 到 ~/.claude/settings.json | 首次配置 |
| `src/main.cpp` | 🔥 ESP8266 固件（TFT 绘制 + 串口协议） | 编译烧录 |

## 数据流摘要

```
所有控制命令 → FIFO 队列文件 → 常驻 daemon 串口 → ESP8266
健康数据 (CPU/MEM) → 后台线程 → 常驻 daemon 串口 → ESP8266 (每秒)
```

串口由 daemon 独占，无竞争、无阻塞、全线速。hook 脚本之间通过独立 FIFO 文件解耦，daemon 按文件名排序顺序消费。

## Hook 事件映射

| Hook 事件 | daemon 命令 | ESP8266 行为 |
|-----------|-------------|-------------|
| `PreToolUse` | `yellow` | 🟡 黄灯亮 |
| `PostToolUse` | `green` | 🟢 绿灯亮 |
| `PostToolUseFailure` | `red` | 🔴 红灯亮 |
| `Notification` | `blink_all` | 🔴 三灯闪烁 (桌面通知) |
| `PermissionRequest` | `blink_all` | 🔴 三灯闪烁 (弹窗等待) |
| `PermissionDenied` | `red` | 🔴 红灯亮 (被拒绝) |

## 命令协议

| 命令 | 格式 | 说明 |
|------|------|------|
| `light` | `{"cmd":"light","value":"red"}` | 设置灯光 (全屏重绘) |
| `blink` | `{"cmd":"blink","value":"red","times":3,"interval":500}` | 单灯闪烁 (自动结束) |
| `blink_all` | `{"cmd":"blink_all","times":3,"interval":500}` | 三灯齐闪 (持续闪烁直到下一指令) |
| `pattern` | `{"cmd":"pattern","steps":[["red",3000],["green",2000]]}` | 灯光序列 |
| `health` | `{"cmd":"health","cpu":73,"mem":65}` | 更新 CPU/MEM 显示条 |
| `status` | `{"cmd":"status"}` | 查询状态 |
| `off` | 文本快捷命令 | 全灭 |

## 多灯安全机制

`blink_all` 会交替点亮/熄灭三个灯。当下一个指令到达时，如果 blink_all 正处于"三灯全亮"相位，传统增量绘制 (`setLight`) 无法正确清除残留灯光，因为 `currentLight == "off"` 时 `lightX()` 返回 -1 跳过清除逻辑。

**修复方案：** 所有 command handler 在进入新状态前使用 `drawTrafficLight("off")` 全屏重绘，而非依赖增量 `setLight()`。
