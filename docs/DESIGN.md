# 🚦 LLM Traffic Light — 方案设计文档

> 项目目标：**为 AI Agent 提供一个可通过软件控制的物理红绿灯**。
>
> Agent 通过串口发送 JSON 命令 → ESP8266 解析 → ST7735 TFT 绘制红绿灯。
> Agent 无需感知底层硬件细节，只需调用 `traflight` CLI 工具或直接发送串口命令。

---

## 1. 总体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│  Agent 层 (AI Agent)                                                │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐                          │
│  │ Claude   │  │ Python   │  │ 任何 LLM  │                          │
│  │ (MCP)    │  │ 脚本     │  │ Agent     │                          │
│  └────┬─────┘  └────┬─────┘  └─────┬─────┘                          │
│       │             │              │                                │
│       └─────────────┼──────────────┘                                │
│                     │ 命令行调用                                     │
│                     │ traflight red / traflight blink ...           │
├─────────────────────┼───────────────────────────────────────────────┤
│  桥接层 (Python CLI) │                                              │
│  ┌──────────────────▼────────────────────────────────────────────┐  │
│  │  traflight.py                                                 │  │
│  │  ┌───────────┐ ┌──────────┐ ┌─────────┐ ┌───────────────┐   │  │
│  │  │ 串口发现   │ │ 命令解析  │ │ 协议封装 │ │ 自动重试/恢复 │   │  │
│  │  └───────────┘ └──────────┘ └─────────┘ └───────────────┘   │  │
│  └──────────────────────────────┬───────────────────────────────┘  │
├─────────────────────────────────┼──────────────────────────────────┤
│  通信层 (USB 串口)              │                                  │
│                      ┌──────────▼──────────┐                      │
│                      │  USB-UART (CP2102)   │                      │
│                      │  115200 8N1          │                      │
│                      └──────────┬──────────┘                      │
├─────────────────────────────────┼──────────────────────────────────┤
│  硬件层 (ESP8266)              │                                  │
│  ┌──────────────────────────────▼──────────────────────────────┐  │
│  │  ESP8266 NodeMCU                                            │  │
│  │  ┌──────────────┐  ┌────────────────┐  ┌───────────────┐   │  │
│  │  │ 串口接收器    │→│ JSON 命令解析器  │→│ 灯光状态机     │   │  │
│  │  │ (RX 中断)     │  │ (ArduinoJson)  │  │ (set/blink/   │   │  │
│  │  └──────────────┘  └────────────────┘  │  pattern)     │   │  │
│  │                                        └───────┬───────┘   │  │
│  │  ┌────────────────┐  ┌────────────────┐        │           │  │
│  │  │ TFT_eSPI 绘制   │←│ 状态查询        │←───────┘           │  │
│  │  │ ST7735 128x160 │  │ /status        │                    │  │
│  │  └────────────────┘  └────────────────┘                    │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 各层职责

| 层级 | 技术栈 | 职责 |
|------|--------|------|
| **Agent 层** | Claude / Python / 任意 LLM | 发出控制指令，接收状态反馈 |
| **桥接层** | Python 3 + pyserial | 封装串口操作，自动发现设备，提供友好 CLI |
| **通信层** | USB 串口 (115200 8N1) | 物理传输 JSON 命令和响应 |
| **硬件层** | ESP8266 + TFT_eSPI | 接收指令、控制 TFT 显示、执行闪烁/序列 |

---

## 2. 硬件设计

### 2.1 元器件清单

| 组件 | 型号 | 数量 | 作用 |
|------|------|------|------|
| 主控 | ESP8266 NodeMCU v3 | 1 | WiFi SoC，接收串口指令 |
| 屏幕 | ST7735 1.8" TFT (128x160) | 1 | 显示红绿灯图形 |
| 线材 | 母对母杜邦线 | 8 根 | 连接屏幕和主控 |
| 电源 | USB 数据线 (Micro-B) | 1 | 供电 + 串口通信 |

### 2.2 接线表

| ST7735 引脚 | ESP8266 引脚 | SPI 功能 |
|-------------|-------------|----------|
| VCC | 3.3V | 电源 |
| GND | GND | 地线 |
| CS | D2 (GPIO4) | SPI 片选 |
| DC | D1 (GPIO5) | 数据/命令选择 |
| RST | D4 (GPIO2) | 复位 |
| MOSI | D7 (GPIO13) | **硬件 SPI 主机输出** |
| SCLK | D5 (GPIO14) | **硬件 SPI 时钟** |
| BL | 3.3V | 背光 (常亮) |

> **约束:** MOSI 和 SCLK 必须接 ESP8266 的硬件 SPI 引脚 (GPIO13/GPIO14)，不可更改。

### 2.3 屏幕显示设计

ST7735 128x160 竖屏布局：

```
┌──── 屏幕 128x160 ────┐
│                        │
│  ┌────────────────┐    │  ← 深灰色圆角外壳
│  │                │    │     BODY_X=24, BODY_Y=6
│  │    🔴 红灯      │    │     w=80,  h=148
│  │    (r=15)      │    │
│  │                │    │     红: cx=64, cy=34
│  │    🟡 黄灯      │    │     黄: cx=64, cy=78
│  │    (r=15)      │    │     绿: cx=64, cy=122
│  │                │    │
│  │    🟢 绿灯      │    │     点亮: 主色 + 辉光 + 高光
│  │    (r=15)      │    │     熄灭: 深灰色 + 内凹阴影
│  │                │    │
│  └────────────────┘    │
│       ██ 灯柱          │  ← 灯柱底部
│                        │
│   背景: 柔和天空蓝     │
└────────────────────────┘
```

---

## 3. 串口通信协议

### 3.1 基本参数

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | 无 |
| 停止位 | 1 |
| 流控 | 无 |
| 编码 | UTF-8 |
| 帧格式 | 单行 JSON + `\n` |

### 3.2 协议状态图

```
┌──────────┐
│  IDLE     │ ←───────── off / pattern 结束 / blink 结束
└─────┬─────┘
      │ {"cmd":"light","value":"red"}
      ↓
┌──────────┐   5s后自动    ┌───────────┐
│  RED      │ ──────────→  │ GREEN      │
└──────────┘              └─────────────┘
      ↑                          │
      │  2s后自动                │ {"cmd":"light","value":"yellow"}
      │                          ↓
      │                    ┌───────────┐
      └───────────────────│  YELLOW    │
                          └───────────┘

Blink 状态:
  IDLE → {"cmd":"blink","value":"red","times":3}
       → RED(亮300ms) → OFF(灭300ms) → RED → OFF → RED → OFF → IDLE

Pattern 状态:
  IDLE → {"cmd":"pattern","steps":[["red",3000],["green",3000]]}
       → RED(3s) → GREEN(3s) → IDLE
```

### 3.3 命令列表

#### `light` — 设置灯光

最简单的操作，立即将红绿灯切换到指定颜色。

```json
{"cmd":"light","value":"red"}
```

| 字段 | 类型 | 必填 | 可选值 |
|------|------|------|--------|
| cmd | string | ✓ | `light` |
| value | string | ✓ | `red` / `yellow` / `green` / `off` |

**响应：**
```json
{"status":"ok","light":"red"}
```

**行为：** 停止当前 blink/pattern，立即切换。

---

#### `blink` — 闪烁

让指定灯闪烁指定次数，ESP8266 本地控制时序。

```json
{"cmd":"blink","value":"red","times":5,"interval":300}
```

| 字段 | 类型 | 必填 | 可选值 | 默认值 |
|------|------|------|--------|--------|
| cmd | string | ✓ | `blink` | — |
| value | string | ✓ | `red` / `yellow` / `green` | — |
| times | int | ✗ | 1–100 | 3 |
| interval | int | ✗ | 50–5000 (ms) | 500 |

**响应：**
```json
{"status":"ok","action":"blink","light":"red","times":5,"interval_ms":300}
```

---

#### `pattern` — 灯光序列

按顺序执行一系列灯色 + 持续时间，每个步骤自动切换。

```json
{"cmd":"pattern","steps":[["red",3000],["green",5000],["yellow",1000],["off",500]]}
```

| 字段 | 类型 | 必填 | 约束 |
|------|------|------|------|
| cmd | string | ✓ | `pattern` |
| steps | array | ✓ | 最多 50 步 |

每个 step: `["灯色", 持续时间(ms)]`，灯色取值同 `value`。

**响应：**
```json
{"status":"ok","action":"pattern","steps":4}
```

---

#### `status` — 查询状态

```json
{"cmd":"status"}
```

**响应：**
```json
{"status":"ok","light":"red","blinking":false,"pattern_active":false,"uptime_ms":1234567}
```

| 响应字段 | 类型 | 说明 |
|---------|------|------|
| light | string | 当前灯色 |
| blinking | bool | 是否正在闪烁 |
| pattern_active | bool | 是否正在执行序列 |
| uptime_ms | uint | 设备启动以来的毫秒数 |

---

#### `help` — 帮助

```json
{"cmd":"help"}
```

在串口打印帮助文本，并返回确认 JSON。

---

### 3.4 错误处理

| 错误场景 | 响应 |
|---------|------|
| JSON 格式错误 | `{"status":"error","message":"Invalid JSON: ..."}` |
| 缺少 cmd 字段 | `{"status":"error","message":"Missing 'cmd' field"}` |
| 命令不存在 | `{"status":"error","message":"Unknown command: xxx"}` |
| light 值无效 | `{"status":"error","message":"Invalid value: red/yellow/green/off"}` |
| 命令过长 | `{"status":"error","message":"Command too long"}` (>512字节) |
| pattern 步数过多 | `{"status":"error","message":"Too many steps (max 50)"}` |

---

## 4. 桥接层设计 (traflight CLI)

### 4.1 设计目标

- Agent 只需执行 `traflight red`，无需手写 serial 代码
- 自动发现串口设备，无需人工指定端口号
- 统一的退出码和错误信息，Agent 可以判断是否成功

### 4.2 CLI 接口一览

```
Usage: traflight <command> [options]

Commands:
  red                 亮红灯
  yellow              亮黄灯
  green               亮绿灯
  off                 关闭所有灯
  status              查询状态
  blink <color>       闪烁指定灯
    [-n, --times N]   闪烁次数 (默认: 3)
    [-i, --interval]  间隔毫秒 (默认: 500)
  pattern <steps>     灯光序列 "red:3,green:5,yellow:1"
  cycle               标准红绿灯周期 (绿5s → 黄2s → 红5s)
  port                显示当前串口设备
  scan                扫描可用串口
  help                显示帮助
```

### 4.3 Agent 调用示例

```bash
# 简单控制
traflight green
traflight yellow
traflight red
traflight off

# 闪烁
traflight blink red -n 5 -i 300

# 序列 (用逗号分隔的 "灯色:秒数" 格式)
traflight pattern "red:2,green:3,yellow:1"

# 标准周期
traflight cycle

# 查询
traflight status
```

---

## 5. 场景流程

### 5.1 标准红绿灯周期

```
Agent                          traflight                     ESP8266
  │                               │                            │
  │──traflight cycle─────────────→│                            │
  │                               │──{"cmd":"light","value":"green"}──→│
  │                               │←──{"status":"ok","light":"green"}──│
  │                               │   (等待 5 秒)              │
  │                               │──{"cmd":"light","value":"yellow"}─→│
  │                               │←──{"status":"ok","light":"yellow"}─│
  │                               │   (等待 2 秒)              │
  │                               │──{"cmd":"light","value":"red"}───→│
  │                               │←──{"status":"ok","light":"red"}───│
  │                               │   (等待 5 秒)              │
  │←──cycle complete─────────────│                            │
```

### 5.2 闪烁警示

```
Agent                          traflight                     ESP8266
  │                               │                            │
  │──traflight blink red 3 300───→│                            │
  │                               │──{"cmd":"blink","value":"red",│
  │                               │   "times":3,"interval":300}──→│
  │                               │←──{"status":"ok","action":"blink"}│
  │                               │   红 → 灭 → 红 → 灭 → 红 → 灭  │
  │←──blink complete─────────────│                            │
```

---

## 6. ESP8266 固件设计

### 6.1 文件结构

| 文件 | 作用 |
|------|------|
| `src/main.cpp` | 主程序：串口读取、命令解析、TFT 绘制、状态管理 |
| `User_Setup_ST7735.h` | TFT_eSPI 库的屏幕引脚配置 |

### 6.2 程序结构

```
main.cpp
├── setup()
│   ├── Serial.begin(115200)
│   ├── tft.init()
│   ├── self-test (红→黄→绿自检动画)
│   └── 打印启动信息
│
└── loop()
    ├── readSerial()          ← 非阻塞读取串口
    │   ├── 逐字符读取
    │   ├── 遇到 '\n' 触发 processCommand()
    │   └── 缓冲区溢出保护
    │
    ├── processCommand()      ← 命令解析
    │   ├── JSON 解析 (ArduinoJson)
    │   ├── light → setLight() + sendResponse()
    │   ├── blink → 启动闪烁状态机 + sendResponse()
    │   ├── pattern → 启动序列状态机 + sendResponse()
    │   └── status → sendStatus()
    │
    ├── updateBlink()         ← 闪烁状态机 (非阻塞)
    │   └── 基于 millis() 定时切换亮/灭
    │
    └── updatePattern()       ← 序列状态机 (非阻塞)
        └── 基于 millis() 逐步骤切换
```

### 6.3 状态变量

```cpp
String currentLight;       // 当前灯色 (off/red/yellow/green)
bool blinkingActive;        // 是否正在闪烁
int blinkRemaining;         // 剩余闪烁次数
bool patternActive;         // 是否正在执行序列
int patternIndex;           // 当前执行到第几步
```

### 6.4 核心约束

- **串口读取 + TFT 绘制不能阻塞**：所有时序控制基于 `millis()` 非阻塞定时
- **Pattern/Blink 优先于 Light**：正在闪烁/序列时，`light` 命令被忽略
- **缓冲区保护**：接收缓冲区 512 字节，溢出时丢弃整条命令

---

## 7. 配置与部署

### 7.1 编译烧录

```bash
# 一键编译烧录
pio run --target upload

# 查看串口日志 (Linux/Mac)
pio device monitor
```

### 7.2 安装 Python CLI

```bash
# 安装依赖
pip install pyserial

# 设置别名
alias traflight='python3 /path/to/traflight.py'
```

### 7.3 快速验证

```bash
# 1. 烧录固件
pio run --target upload

# 2. 打开串口监视器测试
pio device monitor
#   输入 help 查看帮助
#   输入 red / yellow / green 测试显示

# 3. 退出监视器，用 CLI 控制
traflight red
traflight blink green -n 5
traflight status
```

---

## 8. 扩展可能性

| 方向 | 方案 | 改动量 |
|------|------|--------|
| **+ 物理 LED** | TFT 显示同时，GPIO 也驱动 LED | 小 (改固件 + 加硬件) |
| **+ 声音** | 增加蜂鸣器，pattern 可带 beep | 小 |
| **+ 传感器** | 加按钮/光敏，Agent 可读取环境 | 中 |
| **+ MCP Server** | 封装为 MCP tool，Claude 原生调用 | 中 (加一层 Python) |
| **+ 无线** | 在 Python 桥接层上扩展 TCP/WebSocket | 小 (不改固件) |

---

## 9. 设计决策记录

| 决策 | 选项 | 选择 | 理由 |
|------|------|------|------|
| 通信方式 | WiFi HTTP / 串口 | **串口** | 简化硬件，无需网络配置 |
| 命令格式 | 二进制 / 文本 / JSON | **JSON** | Agent 天然擅长 JSON |
| 时序控制 | Agent 侧 / 硬件侧 | **硬件侧** | 避免 Agent 网络延迟影响时序 |
| 显示方式 | LED / TFT | **TFT** | 显示效果好，接线简单 |
| Agent 接口 | 直接串口 / Python CLI | **两者** | CLI 方便 Agent，直接串口方便调试 |
| 文本命令 | 支持 / 不支持 | **支持** | 方便人工调试 |
