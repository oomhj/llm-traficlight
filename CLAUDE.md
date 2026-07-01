# LLM Traffic Light

物理红绿灯 — Agent 状态指示器。

## 状态约定

| 灯光 | 含义 | 时机 |
|------|------|------|
| 🟡 黄灯 | 正在执行任务 | 编译、搜索、等待命令执行 |
| 🟢 绿灯 | 执行完成 | 任务成功结束 |
| 🔴 红灯 | 需要用户确认 | 需要决策、被阻塞 |
| 🔴 闪烁 | 错误 / 告警 | 需要用户注意 |

## 快速命令

```bash
# 状态指示
python3 traflight.py yellow    # 🟡 工作中
python3 traflight.py green     # 🟢 完成
python3 traflight.py red       # 🔴 需要输入
python3 traflight.py blink red -n 5  # ⚠️ 告警

# 查询
python3 traflight.py status
python3 traflight.py scan
```

## 项目文件

```
traflight.py                → CLI 控制
src/main.cpp                → ESP8266 固件
.claude/skills/traffic-light.md  → Skill 完整文档
```

## 编译烧录

```bash
pio run --target upload
```
