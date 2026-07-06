# LLM Traffic Light

物理红绿灯 — Agent 状态指示器。

> ⚠️ **每次执行任务必须通过 traffic-light skill 控制红绿灯。**

## 状态约定

| 灯色 | 含义 | 触发 | 时机 |
|------|------|------|------|
| 🟢 **绿灯** | 开始/完成 | Skill: `traflight green` | 开始任务或任务完成时 |
| 🟡 **黄灯** | Working | Skill: `traflight yellow` | 正在执行操作时 |
| 🔴 **红灯** | 等待输入 / 出错 | Skill: `traflight red` | 需要用户决策或遇到错误时 |

## 工作流

```
收到任务:
  → traflight green     🟢 开始

执行操作:
  → traflight yellow    🟡 工作中

需要用户决策:
  → traflight red       🔴 等回复
  → (用户回复后)
  → traflight green     🟢 继续

遇到错误:
  → traflight red       🔴 已记录

任务完成:
  → traflight green     🟢 结束
```

## Hook (仅保留通知和权限)

```
Notification            → daemon → 🔴 red
PermissionRequest[Bash] → daemon → 🔴 blink_all (弹窗等待)
PermissionDenied[Bash]  → daemon → 🔴 red   (被拒绝)
```

## 快速命令

```bash
bash traflight-cli.sh yellow     # 🟡
bash traflight-cli.sh green      # 🟢
bash traflight-cli.sh red        # 🔴
bash traflight-cli.sh status     # 查状态
bash traflight-cli.sh scan       # 扫串口
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
