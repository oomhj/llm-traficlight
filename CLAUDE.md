# LLM Traffic Light

物理红绿灯 — Agent 状态指示器。

> ⚠️ **MANDATORY**: 每次执行任务必须用红绿灯指示状态。

## 状态约定

| 灯色 | 含义 | 触发 | 时机 |
|------|------|------|------|
| 🟡 **黄灯** | Working | Auto hook (PreToolUse) | 执行任何 Bash 命令前 |
| 🟢 **绿灯** | Done | Auto hook (PostToolUse) | 命令成功完成后 |
| 🔴 **红灯** | Need Input | 手动 `traflight red` | 需要用户决策时 |
| 🔴 **闪红** | Error | Auto hook (PostToolUseFailure) | 命令执行失败时 |

## 快速命令

```bash
# 如果已安装
traflight yellow     # 🟡 工作中
traflight green      # 🟢 完成
traflight red        # 🔴 需要输入
traflight status     # 查询状态
traflight scan       # 扫描串口

# 未安装 pip 时
python3 traflight.py yellow
python3 traflight.py status
```

## 自动 Hooks

命令执行时自动触发（通过守护进程避免串口竞争）：

```
Bash 命令 → PreToolUse → daemon → 🟡 yellow
         → 命令执行
         → PostToolUse → daemon → 🟢 green
         → (失败) PostToolUseFailure → daemon → 🔴 blink red
```

安装: `python3 scripts/install-hooks.py`

守护进程: 自动启停，无需手动管理

## 项目文件

```
traflight.py              → Python CLI 桥接层
traflight-hook.sh         → Claude Code hook 脚本
traflight-daemon.sh       → 串口守护进程 (解决并发竞争)
scripts/install-hooks.py  → hook 安装脚本
src/main.cpp              → ESP8266 固件
.claude/skills/traffic-light.md → Skill 完整文档
docs/DESIGN.md            → 方案设计文档
docs/FIRMWARE_REVIEW.md   → 固件执行流分析
scripts/stress_test.py    → 压测脚本
```

## 编译烧录

```bash
pio run --target upload
```
