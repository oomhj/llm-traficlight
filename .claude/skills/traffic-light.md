---
name: traffic-light
description: >
  Control the physical ESP8266 ST7735 TFT traffic light connected via USB serial.
  Use `traflight.py` CLI to set lights, blink, run patterns, and query status.
---

# 🚦 Traffic Light Skill

This project has a **physical traffic light** (ESP8266 + ST7735 TFT 128x128) connected via USB serial. You can control it using the `traflight.py` CLI.

## Quick Start

```bash
# 1. Set the light color
traflight red
traflight yellow
traflight green
traflight off

# 2. Blink a light
traflight blink red -n 5 -i 300

# 3. Run a pattern sequence
traflight pattern "red:3,green:5,yellow:1"

# 4. Standard traffic light cycle (green→yellow→red)
traflight cycle

# 5. Query status
traflight status

# 6. Auto-detect serial port
traflight port
traflight scan

# If auto-detect fails, use:
traflight --port /dev/cu.usbserial-210 <command>
```

## Common Workflows

### Traffic Light Cycle
```
traflight cycle
```
Runs: 🟢 Green 5s → 🟡 Yellow 2s → 🔴 Red 5s

### Alert / Warning
```
traflight blink red -n 3 -i 500
```
Flashes red 3 times at 500ms intervals.

### Custom Sequence
```
traflight pattern "green:3,yellow:1,red:5,red:0.5"
```
Each step is `color:seconds`, colors: `red`/`yellow`/`green`/`off`.

## Protocol Details

The device uses a line-based JSON protocol over USB serial (115200 8N1).
You can also send raw JSON if needed:

```json
{"cmd":"light","value":"red"}
{"cmd":"blink","value":"red","times":3,"interval":500}
{"cmd":"pattern","steps":[["red",2000],["green",3000]]}
{"cmd":"status"}
```

## Hardware

- **MCU:** ESP8266 NodeMCU
- **Display:** ST7735 1.8" TFT, 128x128, BGR color order
- **Connection:** USB serial (CP2102), `/dev/cu.usbserial-210` typically
- **Backlight:** GPIO5 (HIGH = on), controlled by software
- **Config:** `User_Setup_ST7735.h` for TFT_eSPI pin mapping
- **Firmware:** `src/main.cpp`, compiled with PlatformIO

## Troubleshooting

| Problem | Fix |
|---------|-----|
| "No serial port found" | Run `traflight scan`, check USB cable |
| Port in use | Close other programs (monitor, Arduino IDE) |
| Permission denied | `sudo chmod 666 /dev/cu.usbserial-*` |
| No response | Wait 2s after plugging for ESP8266 boot |
