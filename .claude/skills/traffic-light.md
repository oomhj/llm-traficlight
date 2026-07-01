---
name: traffic-light
description: >
  Physical agent status indicator — ESP8266 ST7735 TFT traffic light
  connected via USB serial. SIGNAL when working / done / need user input.

  ⚠️ MANDATORY: Must be used on every task. See CLAUDE.md.
---

# 🚦 Traffic Light — Agent Status Indicator

## 硬性要求

每次执行任务**必须**按顺序调用红绿灯：

```
开始 → 🟡 黄灯  (PreToolUse auto)
完成 → 🟢 绿灯  (PostToolUse auto)
失败 → 🔴 闪烁  (PostToolUseFailure auto)
需要输入 → 🔴 红灯  (manual)
```

## Quick Reference

```bash
# 状态指示
traflight yellow       # 🟡 工作中
traflight green        # 🟢 完成
traflight red          # 🔴 需要输入

# 手动控制
traflight blink red -n 3   # 闪烁
traflight status           # 查询
traflight scan             # 扫串口
traflight --port /dev/cu.usbserial-210 <cmd>
```

## Auto Hooks

命令执行时自动触发，通过守护进程（`traflight-daemon.sh`）处理串口并发：

```
命令 → PreToolUse → 写队列 "yellow" ─┐
                                     ├──→ daemon 顺序处理
     → PostToolUse → 写队列 "green" ─┘     yellow → green → ESP
```

安装: `python3 scripts/install-hooks.py`

## State Machine

```
Idle ──→ 🟡 Working ──→ 🟢 Done ──→ Idle
               │
               ├──→ 🔴 Waiting ──→ 🟢 Done
               │
               └──→ 🔴 Blink ──→ 🟢 Done
```

## Protocol

USB serial 115200 8N1, JSON line protocol.

## Files

```
traflight.py              → Python CLI
traflight-daemon.sh       → 串口守护进程 (FIFO 队列)
traflight-hook.sh         → Hook 脚本
scripts/install-hooks.py  → Hook 安装
src/main.cpp              → ESP8266 固件
```

## Hardware

- **MCU:** ESP8266 NodeMCU
- **Display:** ST7735 128x128 TFT, black bg, BGR
- **Port:** typically `/dev/cu.usbserial-210`

## Troubleshooting

| Problem | Fix |
|---------|------|
| Port not found | `traflight scan` |
| Permission denied | `sudo chmod 666 /dev/cu.usbserial-*` |
| Already in use | `bash traflight-daemon.sh stop` |
