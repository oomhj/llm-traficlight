---
name: traffic-light
description: >
  Physical agent status indicator — ESP8266 ST7735 TFT traffic light
  connected via USB serial. SIGNAL when working / done / need user input.
---

# 🚦 Traffic Light — Agent Status Indicator

## Convention

Use the traffic light to **signal your current state** to the user:

| State | Light | When |
|-------|-------|------|
| **Working** | 🟡 **Yellow** | Executing a task, processing, waiting for external command |
| **Done** | 🟢 **Green** | Task completed successfully, ready for next instruction |
| **Need Input** | 🔴 **Red** | Waiting for user confirmation, blocked, need decision |
| **Error / Alert** | 🔴 **Blink Red** | Something went wrong, needs attention |

## Quick Reference

```bash
# ——— Agent Status (primary use) ———
traflight yellow          # 🟡 I'm working on something
traflight green           # 🟢 Task done!
traflight red             # 🔴 Need your input
traflight blink red -n 5  # ⚠️ Alert / error

# ——— Manual Control ———
traflight off             # Turn off
traflight blink green -n 3  # Notify completion
traflight status          # Check current light
traflight scan            # Find serial port
traflight --port /dev/cu.usbserial-210 <cmd>  # Specify port
```

## Usage in Conversation

```
User: 帮我编译这个项目

You:
  traflight yellow          # ← 亮黄灯，表示正在执行
  pio run --target upload   # ← 编译烧录
  traflight green            # ← 完成，亮绿灯

  ✅ 编译烧录完成！
```

```
User: 这个配置是什么意思？

You: (需要用户先确认)
  traflight red              # ← 需要用户输入，亮红灯
  [解释配置]
  要用这个配置吗？
```

## State Machine

```
  ┌──────────┐  任务执行    ┌───────────┐  完成    ┌──────────┐
  │  🟢 Idle  │ ───────→  │  🟡 Working │ ─────→  │  🟢 Done  │
  └──────────┘            └────────────┘         └──────────┘
       ↑                        │                      │
       │                  需要输入                      │
       │                        ↓                      │
       │                  ┌──────────┐                 │
       │                  │  🔴 Wait  │                 │
       │                  │  (user)   │─────────────────┘
       │                  └──────────┘    用户确认后完成
       └───────────────────────────────────────────────┘
                   回到空闲
```

## Protocol

USB serial 115200 8N1, JSON line protocol:

```json
{"cmd":"light","value":"yellow"}
{"cmd":"blink","value":"red","times":3,"interval":500}
{"cmd":"status"}
```

## Hardware

- **MCU:** ESP8266 NodeMCU, USB serial (CP2102)
- **Display:** ST7735 128x128 TFT, black background, BGR
- **Firmware:** `src/main.cpp`, PlatformIO build
- **Port:** typically `/dev/cu.usbserial-210`

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Port not found | `traflight scan` |
| Permission denied | `sudo chmod 666 /dev/cu.usbserial-*` |
| Already in use | Close serial monitor / Arduino IDE |
