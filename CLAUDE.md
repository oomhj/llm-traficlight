# LLM Traffic Light Project

This project has a physical ESP8266 traffic light (ST7735 TFT 128x128) controlled via USB serial.

## Quick Reference

| Command | Description |
|---------|-------------|
| `python3 traflight.py red` | Red light |
| `python3 traflight.py green` | Green light |
| `python3 traflight.py yellow` | Yellow light |
| `python3 traflight.py off` | All off |
| `python3 traflight.py blink red -n 5` | Flash red 5 times |
| `python3 traflight.py pattern "red:3,green:5"` | Sequence |
| `python3 traflight.py cycle` | Standard cycle |
| `python3 traflight.py status` | Query state |
| `python3 traflight.py --port /dev/cu.usbserial-210 <cmd>` | Specify port |

Type `/traffic-light` for full skill documentation.

## Project Structure

```
.claude/skills/traffic-light.md     ← Skill definition
src/main.cpp                        ← ESP8266 firmware
traflight.py                        ← Python CLI bridge
User_Setup_ST7735.h                 ← TFT pin config
platformio.ini                      ← PlatformIO config
```

## Build & Flash

```bash
pio run --target upload
```
