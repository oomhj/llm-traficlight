---
name: traffic-light
description: >
  Physical agent status indicator — ESP8266 ST7735 TFT traffic light
  connected via USB serial. SIGNAL when working / done / need user input.

  ⚠️ MANDATORY: Must be used on every task. See CLAUDE.md.

  Usage: `traflight yellow|green|red|status`
---

# 🚦 Traffic Light — Agent Status Indicator

## 硬性要求

每次执行任务**必须**调用此 skill 按顺序控制红绿灯：

```
开始      → 🟢 traflight green   → 开始工作了
需要输入  → 🔴 traflight red     → 等用户决策
遇到错误  → 🔴 traflight red     → 出错了
完成      → 🟢 traflight green   → 做完了
```

## 命令参考

```bash
bash traflight-cli.sh yellow       # 🟡 工作中
bash traflight-cli.sh green        # 🟢 完成/开始
bash traflight-cli.sh red          # 🔴 需要输入/出错
bash traflight-cli.sh status       # 查询当前状态
bash traflight-cli.sh blink red -n 3   # 闪烁
```

## 工作流

```
收到任务:
  → traflight green     🟢 开始

执行命令:
  → traflight yellow    🟡 正在做

需要用户决策:
  → traflight red       🔴 等回复
  → (用户回复后)
  → traflight green     🟢 继续

遇到错误:
  → traflight red       🔴 已记录

任务完成:
  → traflight green     🟢 结束
```

## Protocol

USB serial 115200 8N1, JSON line protocol.

## Files

```
traflight.py              → Python CLI
traflight-daemon.py       → 串口守护进程 (FIFO 队列)
traflight-hook.sh         → Hook 脚本 (仅保留通知/权限)
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
