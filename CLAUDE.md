# LLM Traffic Light

物理红绿灯 — Agent 状态指示器。

> ⚠️ **每次执行任务必须用红绿灯指示状态。**

## 状态约定

| 灯色 | 含义 | 触发 | 时机 |
|------|------|------|------|
| 🟡 **黄灯** | Working | Auto hook (PreToolUse) | 执行任何 Bash 命令前 |
| 🟢 **绿灯** | Done | Auto hook (PostToolUse) | 命令成功完成后 |
| 🔴 **红灯** | Need Input | **手动 `traflight red`** | 需要用户决策时 |
| 🔴 **闪红** | Error | Auto hook (PostToolUseFailure) | 命令执行失败时 |
| 🔴 **红灯** | Notification | Auto hook (Notification) | 桌面通知时 |

## 工作流

```
收到任务:
  traflight yellow    ← 🟡 开始工作
  执行命令...            ← auto hooks 自动切换
  traflight green     ← 🟢 完成

需要用户输入:
  traflight red       ← 🔴 等用户决策
  (解释问题，等回复)

遇到错误:
  auto → 🔴 闪烁      ← auto hook 自动触发
```

## Auto Hooks

命令执行时自动触发（通过守护进程 FIFO 队列避免串口竞争）：

```
PreToolUse[Bash]       → daemon → 🟡 yellow
PostToolUse[Bash]      → daemon → 🟢 green
PostToolUseFailure     → daemon → 🔴 blink red
Notification           → daemon → 🔴 red
```

安装: `python3 scripts/install-hooks.py`

## 快速命令

```bash
# 已安装 pip
traflight yellow     # 🟡
traflight green      # 🟢
traflight red        # 🔴
traflight status     # 查状态
traflight scan       # 扫串口

# 未安装
python3 traflight.py yellow
```

## 系统健康监控

```bash
bash traflight-daemon.sh health      # 更新 CPU/MEM 显示条
bash traflight-daemon.sh healthd     # 后台持续监控 (每5秒)
bash traflight-daemon.sh health-stop # 停止后台监控
```

## 编译烧录

```bash
pio run --target upload
```
