# 🚦 LLM Traffic Light — ESP8266 串口红绿灯 Agent 接口

让 AI Agent (Claude 等) 通过 **USB 串口**控制 ST7735 TFT 屏幕上的红绿灯。

> 🔌 **无需 WiFi！** 只需一根 USB 线连接电脑，Agent 通过串口发送 JSON 命令即可控制。

## 通信方式

```
┌─────────────────┐   USB 串口   ┌──────────────────┐
│  AI Agent        │ ═══════════ │  ESP8266          │
│  (Claude/Python) │  JSON 命令  │  (USB-UART桥)     │
│                   │ ═══════════ │  ┌──────────────┐ │
│  → 发送 JSON 行   │ ──────────→  │  │  🚦 TFT 屏幕 │ │
│  ← 接收 JSON 行   │ ←──────────  │  │  🔴 🟡 🟢   │ │
└─────────────────┘              │  └──────────────┘ │
                                  └──────────────────┘
```

**工作原理：**
1. 电脑通过 USB 线连接 ESP8266（即烧录用的那根线）
2. Agent 打开对应的串口（如 `/dev/ttyUSB0` 或 `COM3`）
3. 发送一行 JSON 命令，以 `\n` 结尾
4. ESP8266 执行命令并在 TFT 上绘制红绿灯
5. 返回一行 JSON 响应

## 硬件清单

| 组件 | 数量 |
|------|------|
| ESP8266 (NodeMCU / Wemos D1 Mini) | 1 |
| ST7735 1.8" TFT 显示屏 (128x160) | 1 |
| USB 数据线 (Micro-B) | 1 |
| 面包板 + 杜邦线 (母对母) | 若干 |

## 接线图

```
ST7735 TFT 1.8"           ESP8266 NodeMCU
─────────────────         ────────────────

  VCC  ─────────────────   3.3V
  GND  ─────────────────   GND
  CS   ─────────────────   D2  (GPIO4)
  DC   ─────────────────   D1  (GPIO5)
  RST  ─────────────────   D4  (GPIO2)
  MOSI ─────────────────   D7  (GPIO13) ← HW SPI MOSI
  SCLK ─────────────────   D5  (GPIO14) ← HW SPI SCLK
  BL   ─────────────────   3.3V

  ┌──────────────────────────────────┐
  │  NodeMCU                         │
  │        USB 线 ───→ 电脑           │
  │  D1(DC)←──→ST7735 DC            │
  │  D2(CS)←──→ST7735 CS            │
  │  D4(RST)←──→ST7735 RST          │
  │  D5(SCK)←──→ST7735 SCLK         │
  │  D7(MOSI)←──→ST7735 MOSI        │
  │  3.3V──────→VCC, BL             │
  │  GND──────→GND                  │
  └──────────────────────────────────┘
```

> **注意：** 使用 ESP8266 **硬件 SPI**，MOSI 必须接 GPIO13(D7)，SCLK 必须接 GPIO14(D5)，不可更改。

## 编译与烧录

### 1. 安装依赖

```bash
pip install platformio
```

### 2. 修改屏幕配置 (如需自定义引脚)

编辑 `User_Setup_ST7735.h`：

```cpp
#define TFT_CS   D2   // GPIO4
#define TFT_DC   D1   // GPIO5
#define TFT_RST  D4   // GPIO2
// #define TFT_RGB_ORDER TFT_BGR  // 颜色反了取消注释
```

### 3. 编译烧录

```bash
# 编译
pio run

# 烧录到 ESP8266
pio run --target upload

# 打开串口监视器 (测试用)
pio device monitor
```

## 串口协议

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | 无 (N) |
| 停止位 | 1 |
| 命令格式 | **每行一条 JSON**，以 `\n` 结尾 |
| 响应格式 | 返回一行 JSON |

## 命令参考

### 文本快捷命令 (适合串口监视器手动测试)

只需输入单词即可控制：

```
red       → 亮红灯
yellow    → 亮黄灯
green     → 亮绿灯
off       → 关闭所有灯
status    → 查询当前状态
help      → 显示帮助信息
```

### JSON 命令 (适合 Agent 调用)

#### 🟢 light — 设置灯光

```json
{"cmd":"light","value":"red"}
```

| value 可选值 | 效果 |
|-------------|------|
| `"red"` | 🔴 红灯亮 |
| `"yellow"` | 🟡 黄灯亮 |
| `"green"` | 🟢 绿灯亮 |
| `"off"` | 全灭 |

**响应：**
```json
{"status":"ok","light":"red"}
```

---

#### 💡 blink — 闪烁

```json
{"cmd":"blink","value":"red","times":5,"interval":300}
```

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| value | string | — | 闪烁哪个灯 (red/yellow/green) |
| times | int | 3 | 闪烁次数 |
| interval | int | 500 | 间隔(毫秒) |

**响应：**
```json
{"status":"ok","action":"blink","light":"red","times":5,"interval_ms":300}
```

---

#### 🎬 pattern — 灯光序列

```json
{"cmd":"pattern","steps":[["red",3000],["green",5000],["yellow",1500],["off",500]]}
```

每个 step 为 `[灯色, 持续时间ms]`，最多 50 步。

**响应：**
```json
{"status":"ok","action":"pattern","steps":4}
```

---

#### 📊 status — 查询状态

```json
{"cmd":"status"}
```

**响应：**
```json
{"status":"ok","light":"red","blinking":false,"pattern_active":false,"uptime_ms":1234567}
```

## Agent 调用示例

### Claude / 任意 LLM Agent (Python 串口客户端)

```python
import serial
import json
import time

# 1. 打开串口 (Linux: /dev/ttyUSB0, Mac: /dev/cu.usbserial-xxx, Windows: COM3)
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=5)
time.sleep(2)  # 等待 ESP8266 重启完成

def send_cmd(cmd_dict):
    """发送 JSON 命令并读取响应"""
    line = json.dumps(cmd_dict) + '\n'
    ser.write(line.encode())
    time.sleep(0.1)
    resp = ser.readline().decode().strip()
    return json.loads(resp) if resp else {}

# 2. 查询状态
print("Status:", send_cmd({"cmd": "status"}))

# 3. 绿灯 5 秒
send_cmd({"cmd": "light", "value": "green"})
time.sleep(5)

# 4. 黄灯 2 秒
send_cmd({"cmd": "light", "value": "yellow"})
time.sleep(2)

# 5. 红灯闪烁警示
send_cmd({"cmd": "blink", "value": "red", "times": 3, "interval": 400})

# 6. 或者一步到位用 pattern 执行完整周期
send_cmd({
    "cmd": "pattern",
    "steps": [
        ["green", 5000],
        ["yellow", 2000],
        ["red", 5000]
    ]
})

ser.close()
```

### Agent 命令行测试 (screen / putty)

Mac/Linux 用 `screen` 直接测试：

```bash
# 连接串口
screen /dev/ttyUSB0 115200

# 输入命令（按回车发送）
red

# 或者用 echo 发送 JSON
echo '{"cmd":"light","value":"green"}' > /dev/ttyUSB0
```

Windows 用 PuTTY：选择 Serial，填写 COM3，波特率 115200。

### 在 Claude 中使用 MCP 控制

创建一个 MCP 工具，让 Claude Agent 通过串口控制红绿灯：

<details>
<summary>📋 MCP Tool 配置示例 (click to expand)</summary>

```json
{
  "traffic_light": {
    "description": "控制 ESP8266 红绿灯 (串口)",
    "input_schema": {
      "type": "object",
      "properties": {
        "command": {
          "type": "string",
          "enum": ["light", "blink", "pattern", "status"],
          "description": "命令类型"
        },
        "value": {
          "type": "string",
          "enum": ["red", "yellow", "green", "off"],
          "description": "灯色 (仅 light/blink)"
        },
        "times": {
          "type": "integer",
          "description": "闪烁次数 (仅 blink)"
        },
        "interval": {
          "type": "integer",
          "description": "闪烁间隔ms (仅 blink)"
        },
        "steps": {
          "type": "array",
          "description": "灯光序列 (仅 pattern)"
        }
      },
      "required": ["command"]
    }
  }
}
```

</details>

## TFT 显示效果

红绿灯在 ST7735 屏幕上的显示内容：

```
┌────────────────────────────┐
│  128 x 160 TFT 屏幕        │
│                            │
│  ┌─── 外壳 (深灰圆角) ───┐ │
│  │  ┌──────────────────┐  │ │
│  │  │   🔴 红灯(辉光)   │  │ │
│  │  └──────────────────┘  │ │
│  │  ┌──────────────────┐  │ │
│  │  │   🟡 黄灯(辉光)   │  │ │
│  │  └──────────────────┘  │ │
│  │  ┌──────────────────┐  │ │
│  │  │   🟢 绿灯(辉光)   │  │ │
│  │  └──────────────────┘  │ │
│  └────────────────────────┘ │
│       ██ 灯柱              │
│  天空蓝背景                │
└────────────────────────────┘
```

- **未点亮时**: 深灰色灯泡 + 内凹阴影
- **点亮时**: 主色 + 辉光 + 亮边 + 高光

## 项目结构

```
llm-traficlight/
├── platformio.ini                    # PlatformIO 配置
├── User_Setup_ST7735.h               # TFT 屏幕引脚配置
├── src/
│   └── main.cpp                      # 主固件 (串口协议 + TFT 显示)
└── README.md
```

## License

MIT
