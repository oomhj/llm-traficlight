# LLM Traffic Light

> ⚠️ **MANDATORY**: Whenever you execute any task in this project, you MUST use the traffic light to signal your state. This is not optional.

## State Convention (必须遵守)

| 状态 | 灯光 | 时机 | 示例命令 |
|------|------|------|---------|
| 🟡 **Working** | **先亮黄灯** | 开始执行任何任务前 | `python3 traflight.py yellow` |
| 🟢 **Done** | **完成后亮绿灯** | 任务成功结束 | `python3 traflight.py green` |
| 🔴 **Need Input** | **需要输入时亮红灯** | 需要用户决策/确认 | `python3 traflight.py red` |
| 🔴 **Blink Alert** | **闪烁红灯** | 出错或需要关注 | `python3 traflight.py blink red -n 5` |

## Workflow

```
1. 收到任务 → 立即亮黄灯:  traflight yellow
2. 执行任务 → (pio run, 搜索代码, 修改文件...)
3. 需要用户确认? → 亮红灯:  traflight red
4. 完成后 → 亮绿灯:        traflight green
```

## Commands

```bash
# 如果已安装: pip install -e .
traflight yellow
traflight green
traflight red

# 如果未安装:
python3 traflight.py yellow
python3 traflight.py green
python3 traflight.py red
python3 traflight.py blink red -n 3
python3 traflight.py status
python3 traflight.py scan

# 指定串口:
python3 traflight.py --port /dev/cu.usbserial-210 <cmd>
```

## Install

```bash
pip install pyserial
pip install -e .          # optional: enables `traflight` command
```

## Build & Flash

```bash
pio run --target upload
```

## Files

```
traflight.py              → CLI tool
src/main.cpp              → ESP8266 firmware
.claude/skills/traffic-light.md → Full skill docs
```
