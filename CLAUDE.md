# LLM Traffic Light

Physical traffic light — Agent status indicator via USB serial.

## State Convention

| Light | Meaning | When |
|-------|---------|------|
| 🟡 **Yellow** | Working | Executing a task, compiling, searching |
| 🟢 **Green** | Done | Task completed successfully |
| 🔴 **Red** | Need input | Waiting for user decision |
| 🔴 **Blink** | Alert / Error | Something went wrong |

## Commands

```bash
# ——— Status signals ———
traflight yellow          # 🟡 working
traflight green           # 🟢 done
traflight red             # 🔴 need input
traflight blink red -n 5  # ⚠️ alert

# ——— Queries ———
traflight status          # check state
traflight scan            # find serial port

# ——— If not installed via pip ———
python3 traflight.py yellow
python3 traflight.py status
```

## Install (optional)

```bash
pip install -e .          # enables `traflight` command
pip install pyserial      # minimum dependency
```

## Build & Flash

```bash
pio run --target upload
```

## Files

```
traflight.py              → CLI tool
src/main.cpp              → ESP8266 firmware
.claude/skills/traffic-light.md → Skill docs
```
