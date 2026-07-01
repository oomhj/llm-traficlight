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
│  │ (Code)   │  │ 脚本     │  │ Agent     │                          │
│  └────┬─────┘  └────┬─────┘  └─────┬─────┘                          │
│       │             │              │                                │
│       └─────────────┼──────────────┘                                │
│                     │ 命令行调用                                     │
│                     │ traflight red / traflight blink ...           │
├─────────────────────┼───────────────────────────────────────────────┤
│  桥接层 (Python CLI + Auto Hooks)                                  │
│  ┌──────────────────▼────────────────────────────────────────────┐  │
│  │  traflight.py                   traflight-hook.sh              │  │
│  │  ┌───────────┐ ┌──────────┐    ┌──────────────┐               │  │
│  │  │ 串口发现   │ │ 命令解析  │    │ PreToolUse   │               │  │
│  │  │ JSON 封装  │ │ 响应解析  │    │ PostToolUse  │               │  │
│  │  │ 错误重试   │ │ CLI 解析  │    │ PostToolUse- │               │  │
│  │  └───────────┘ └──────────┘    │ Failure      │               │  │
│  │                                └──────────────┘               │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
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
│  │  │ (RX 缓冲)     │  │ (ArduinoJson)  │  │ (set/blink/   │   │  │
│  │  └──────────────┘  └────────────────┘  │  pattern)     │   │  │
│  │                                        └───────┬───────┘   │  │
│  │  ┌────────────────┐  ┌────────────────┐        │           │  │
│  │  │ TFT_eSPI 绘制   │←│ 状态查询        │←───────┘           │  │
│  │  │ ST7735 128×128 │  │ /status        │                    │  │
│  │  └────────────────┘  └────────────────┘                    │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 各层职责

| 层级 | 技术栈 | 职责 |
|------|--------|------|
| **Agent 层** | Claude / Python / 任意 LLM | 发出控制指令，接收状态反馈 |
| **桥接层** | Python 3 + pyserial + bash | 封装串口操作，自动发现设备，提供友好 CLI；通过 Claude Code hooks 自动触发 |
| **通信层** | USB 串口 (115200 8N1) | 物理传输 JSON 命令和响应 |
| **硬件层** | ESP8266 + TFT_eSPI | 接收指令、控制 TFT 显示、执行闪烁/序列 |

---

## 2. Agent 状态约定

红绿灯作为 Agent 的**物理状态指示器**：

| 状态 | 灯光 | 触发方式 | 时机 |
|------|------|---------|------|
| **Working** | 🟡 **黄灯** | Auto hook `PreToolUse[Bash]` | Agent 执行任何命令前 |
| **Done** | 🟢 **绿灯** | Auto hook `PostToolUse[Bash]` | 命令成功完成后 |
| **Need Input** | 🔴 **红灯** | 手动 `traflight red` | Agent 需要用户决策时 |
| **Error** | 🔴 **闪烁** | Auto hook `PostToolUseFailure` | 命令执行失败时 |

### 状态机

```
                  ┌────────────────────────────────────┐
                  │  Claude Code Auto Hooks            │
                  │  PreToolUse  → 🟡 yellow            │
                  │  PostToolUse → 🟢 green             │
                  │  PostToolUseFailure → 🔴 blink red  │
                  └────────────────────────────────────┘

  用户发消息 ──→ 🟡 Working ──→ 🟢 Done ──→ 等待下一消息
                      │
                      ├──→ (失败) → 🔴 blink red
                      │
                      └──→ (需要确认) → 🔴 red → 🟢 Done
```

---

## 3. 硬件设计

### 3.1 元器件清单

| 组件 | 型号 | 数量 | 作用 |
|------|------|------|------|
| 主控 | ESP8266 NodeMCU v3 | 1 | 接收串口指令 |
| 屏幕 | ST7735 1.8" TFT (128×128) | 1 | 显示红绿灯图形 |
| 线材 | 母对母杜邦线 | 8 根 | 连接屏幕和主控 |
| 电源 | USB 数据线 (Micro-B) | 1 | 供电 + 串口通信 |

### 3.2 接线表

| ST7735 引脚 | ESP8266 引脚 | 说明 |
|-------------|-------------|------|
| VCC | 3.3V | 电源 |
| GND | GND | 地线 |
| CS | GND | 片选直连 GND (软件禁用) |
| DC | D3 (GPIO0) | 数据/命令选择 |
| RST | D4 (GPIO2) | 复位 |
| MOSI | D7 (GPIO13) | **硬件 SPI MOSI** |
| SCLK | D5 (GPIO14) | **硬件 SPI 时钟** |
| BL (LEDA) | D0 (GPIO5) | 背光控制 (HIGH=亮) |
| LEDK | GND | 背光负极 |

> **约束:** MOSI 和 SCLK 必须接 ESP8266 的硬件 SPI 引脚 (GPIO13/GPIO14)，不可更改。

### 3.3 屏幕显示设计

ST7735 128×128 竖屏布局：

```
┌──── 屏幕 128×128 ────┐
│                        │
│  ┌────────────────┐    │  ← 深灰圆角外壳 (r=8)
│  │                │    │     BODY_X=29, BODY_Y=4
│  │    🔴 红灯      │    │     W=70, H=116
│  │    (r=14)      │    │
│  │                │    │     红: cx=64, cy=24
│  │    🟡 黄灯      │    │     黄: cx=64, cy=62
│  │    (r=14)      │    │     绿: cx=64, cy=100
│  │                │    │
│  │    🟢 绿灯      │    │     点亮: 主色 + 辉光 + 高光
│  │    (r=14)      │    │     熄灭: 深灰 + 内凹阴影
│  │                │    │
│  └────────────────┘    │
│                        │
│   背景: 黑色 (0x0000)  │
└────────────────────────┘
```

---

## 4. 串口通信协议

### 4.1 基本参数

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | 无 |
| 停止位 | 1 |
| 流控 | 无 |
| 帧格式 | 单行 JSON + `\n` |

### 4.2 命令列表

#### `light` — 设置灯光

```json
{"cmd":"light","value":"red"}
```

| value | 效果 |
|-------|------|
| `red` | 🔴 红灯亮 (带红光辉光) |
| `yellow` | 🟡 黄灯亮 (带黄光辉光) |
| `green` | 🟢 绿灯亮 (带绿光辉光) |
| `off` | 全灭 |

**响应:** `{"status":"ok","light":"red"}`

---

#### `blink` — 闪烁

```json
{"cmd":"blink","value":"red","times":5,"interval":300}
```

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| value | string | — | 灯色 (red/yellow/green) |
| times | int | 3 | 闪烁次数 |
| interval | int | 500 | 间隔(毫秒) |

---

#### `pattern` — 灯光序列

```json
{"cmd":"pattern","steps":[["red",3000],["green",5000],["off",500]]}
```

每个 step 为 `[灯色, 持续时间ms]`，最多 50 步。

---

#### `status` — 查询状态

```json
{"cmd":"status"}
```

**响应:** `{"status":"ok","light":"red","blinking":false,"pattern_active":false,"uptime_ms":1234567}`

### 4.3 文本快捷命令

为方便串口监视器调试，同时支持纯文本命令：
`red` / `yellow` / `green` / `off` / `status` / `help`

---

## 5. 自动 Hooks 系统

Claude Code 通过 `~/.claude/settings.json` 配置 hooks，自动触发红绿灯：

```json
{
  "hooks": {
    "PreToolUse": [
      { "matcher": "Bash(*)", "hooks": [
          {"type":"command", "command":"bash traflight-hook.sh before",
           "async": true, "timeout": 5}
      ]}
    ],
    "PostToolUse": [
      { "matcher": "Bash(*)", "hooks": [
          {"type":"command", "command":"bash traflight-hook.sh success",
           "async": true, "timeout": 5}
      ]}
    ],
    "PostToolUseFailure": [
      { "matcher": "Bash(*)", "hooks": [
          {"type":"command", "command":"bash traflight-hook.sh failure",
           "async": true, "timeout": 5}
      ]}
    ]
  }
}
```

安装方式:

```bash
python3 scripts/install-hooks.py
```

---

## 6. ESP8266 固件设计

### 6.1 程序结构

```
main.cpp
├── setup()
│   ├── Serial.begin(115200)
│   ├── tft.init() + setRotation(0)
│   ├── self-test (红→黄→绿自检动画)
│   └── 打印启动信息
│
└── loop()
    ├── readSerial()            ← 非阻塞读取串口
    │   ├── 逐字符读取至 \n
    │   └── 触发 processCommand()
    │
    ├── processCommand()        ← JSON / 文本命令解析
    │   ├── light → setLight() + sendResponse()
    │   ├── blink → 启动闪烁状态机
    │   ├── pattern → 启动序列状态机
    │   └── status → sendStatus()
    │
    ├── updateBlink()           ← 闪烁状态机 (millis)
    └── updatePattern()         ← 序列状态机 (millis)
```

### 6.2 关键设计

- **串口响应纯净**: `sendResponse()` 先 `Serial.flush()` 再 `println(json)`，不混入调试日志
- **非阻塞**: 所有时序基于 `millis()`，不阻塞串口读取
- **CO_ BGR 色彩**: 配置 `TFT_RGB_ORDER TFT_BGR` 适配 ST7735
- **缓冲区保护**: 512 字节接收缓冲区，溢出时丢弃并重置

---

## 7. 项目文件结构

```
llm-traficlight/
├── .claude/
│   ├── settings.local.json         ← 项目权限配置
│   └── skills/traffic-light.md     ← Skill 定义
├── scripts/
│   └── install-hooks.py            ← Auto hooks 安装脚本
├── src/
│   └── main.cpp                    ← ESP8266 固件
├── traflight.py                    ← Python CLI 桥接层
├── traflight-hook.sh               ← Claude Code hook 脚本
├── User_Setup_ST7735.h             ← TFT 引脚配置 (用户自定义)
├── platformio.ini                  ← PlatformIO 编译配置
├── setup.py                        ← Python 包安装
├── requirements.txt                ← Python 依赖
├── CLAUDE.md                       ← 项目规则 (红绿灯强制使用)
├── docs/DESIGN.md                  ← 本文档
└── README.md
```

---

## 8. 部署与使用

### 编译烧录

```bash
pio run --target upload
```

### 安装 Python CLI

```bash
pip install pyserial
pip install -e .          # 可选: 安装 traflight 命令到 PATH
```

### 安装 Auto Hooks

```bash
python3 scripts/install-hooks.py
```

### Agent 状态指示

```bash
traflight yellow    # 🟡 开始工作
traflight green     # 🟢 完成
traflight red       # 🔴 需要输入
```

---

## 9. 扩展可能性

| 方向 | 方案 | 改动量 |
|------|------|--------|
| **背光 PWM** | `analogWrite(TFT_BL, brightness)` | 中 (固件 + 协议) |
| **渐变过渡** | fade 动画切换灯色 | 中 (固件) |
| **MCP Server** | 桥接串口到 Claude Desktop | 中 (Python 层) |
| **更多显示模式** | 文字、进度条、动画 | 大 (固件 + 协议) |
| **蜂鸣器** | GPIO 输出音频反馈 | 小 (固件 + 硬件) |
