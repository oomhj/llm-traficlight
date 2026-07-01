---
name: traffic-light
description: >
  Physical agent status indicator — ESP8266 ST7735 TFT traffic light
  connected via USB serial. SIGNAL when working / done / need user input.

  ⚠️ MANDATORY: This skill MUST be invoked on every task. See CLAUDE.md.
---

# 🚦 Traffic Light — Agent Status Indicator

## ⚠️ 硬性要求

在**每次**执行任何任务时，都**必须**按以下顺序调用红绿灯：

```
1. 开始任务 → 🟡 黄灯    (traflight yellow)
2. 完成任务 → 🟢 绿灯    (traflight green)
3. 需要输入 → 🔴 红灯    (traflight red)
```

这是本项目的核心约定，不是建议。如果忘记调用，用户会指出来。

## Convention

| State | Light | When |
|-------|-------|------|
| **Working** | 🟡 **Yellow** | Executing a task, processing, waiting for command |
| **Done** | 🟢 **Green** | Task completed successfully |
| **Need Input** | 🔴 **Red** | Waiting for user confirmation, blocked |
| **Error / Alert** | 🔴 **Blink Red** | Something went wrong |

## Quick Reference

```bash
# ——— Agent Status (必须使用) ———
traflight yellow          # 🟡 开始工作
traflight green           # 🟢 完成
traflight red             # 🔴 需要输入
traflight blink red -n 5  # ⚠️ 告警

# ——— 未安装 pip 时用 python3 ———
python3 traflight.py yellow
python3 traflight.py green
python3 traflight.py red

# ——— 查询 ———
traflight status
traflight scan
```

## Usage Examples

```
User: 编译固件
→ 亮黄灯 traflight yellow
→ pio run --target upload
→ 亮绿灯 traflight green
→ "编译完成！"

User: 这个参数怎么配？
→ 亮红灯 traflight red
→ 解释配置选项
→ "你要用哪个？"
```

## State Machine

```
Idle ──→ 🟡 Working ──→ 🟢 Done ──→ Idle
               │
               ├──→ 🔴 Waiting for user ──→ 🟢 Done
               │
               └──→ 🔴 Blink(alert) ──→ 🟢 Done
```

## Protocol

USB serial 115200 8N1, JSON line protocol.

## Hardware

- **MCU:** ESP8266 NodeMCU, USB serial (CP2102)
- **Display:** ST7735 128x128 TFT, black background, BGR
- **Port:** typically `/dev/cu.usbserial-210`

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Port not found | `traflight scan` |
| Permission denied | `sudo chmod 666 /dev/cu.usbserial-*` |
| Already in use | Close serial monitor / Arduino IDE |
