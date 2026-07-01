# 🚦 LLM Traffic Light — ESP8266 串口红绿灯 Agent 接口

让 AI Agent (Claude 等) 通过 **USB 串口**控制 ST7735 TFT 屏幕上的红绿灯。

> 🔌 **无需 WiFi！** 只需一根 USB 线连接电脑，Agent 通过串口发送 JSON 命令即可控制。
> 🤖 **自动 Hooks!** 命令执行时自动亮黄灯→绿灯，失败时自动闪烁红灯。

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
        │
        │ Auto Hooks (Claude Code):
        │ PreToolUse → 🟡 yellow
        │ PostToolUse → 🟢 green
        │ PostToolUseFailure → 🔴 blink red
        │ (通过守护进程 FIFO 队列避免串口竞争)
```

## 自动 Hook 安装

```bash
python3 scripts/install-hooks.py
```

安装后每次 Bash 命令执行时自动触发红绿灯，无需手动调用。

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

## Agent 调用示例 — traflight CLI (推荐)

```bash
# 安装依赖
pip install pyserial
chmod +x traflight.py        # 或 python3 traflight.py <cmd>

# 设置别名 (方便 Agent 调用)
alias traflight='python3 /path/to/traflight.py'
```

### 简单控制

```bash
# Agent 只需执行:
traflight green          # 亮绿灯
sleep 5                  # 等待 5 秒
traflight yellow         # 变黄
sleep 2
traflight red            # 变红
traflight off            # 关灯
```

### 闪烁

```bash
traflight blink red              # 红灯闪 3 次 (默认500ms)
traflight blink green -n 5       # 绿灯闪 5 次
traflight blink yellow -n 3 -i 300  # 黄灯闪 3 次, 每次300ms
```

### 灯光序列 (一行搞定)

```bash
# 格式: "灯色:秒数,灯色:秒数,..."
traflight pattern "green:5,yellow:2,red:5"

# 标准红绿灯周期
traflight cycle
```

### 查询与调试

```bash
traflight status    # 查看当前灯色和状态
traflight port      # 显示当前串口路径
traflight scan      # 扫描所有可用串口
```

### Agent 完整调用示例

```
User: 控制红绿灯执行一个完整周期

Agent:
  traflight cycle
  
  # 等待周期结束后红灯闪烁警示
  traflight blink red -n 3
```

```
User: 检查红绿灯当前状态

Agent:
  traflight status
  → 🔴 Light: red
  → ⏱ Uptime: 123s
```

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
├── traflight.py                      # Python CLI 桥接层
├── traflight-daemon.sh               # 串口守护进程 (FIFO 队列, 防并发竞争)
├── traflight-hook.sh                 # Claude Code hook 脚本
├── scripts/
│   ├── install-hooks.py              # Hook 安装脚本
│   └── stress_test.py                # 压测脚本
├── src/
│   └── main.cpp                      # ESP8266 固件 (串口协议 + TFT 显示)
├── docs/
│   ├── DESIGN.md                     # 方案设计文档
│   └── FIRMWARE_REVIEW.md            # 固件执行流分析
├── .claude/
│   ├── settings.local.json           # 项目权限配置
│   └── skills/traffic-light.md       # Skill 定义
├── User_Setup_ST7735.h               # TFT 屏幕引脚配置
├── platformio.ini                    # PlatformIO 编译配置
├── setup.py                          # Python 包安装
├── requirements.txt                  # Python 依赖
├── CLAUDE.md                         # 项目规则
└── README.md
```

## License

MIT
