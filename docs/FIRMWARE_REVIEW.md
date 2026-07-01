# ESP8266 Firmware Review — 执行流与并发分析

> 审查 `src/main.cpp` 的事件接收、屏幕渲染、请求响应流程，
> 识别阻塞点和并发风险。

---

## 1. 执行流总览

```
loop() 每次迭代:
  ┌─────────────────────────────────────────────────────┐
  │  1. readSerial()         ← 逐字符读取串口数据到 rxBuf │
  │  2. 遇到 '\n' 触发 processCommand()                  │
  │  3. updateBlink()        ← 闪烁状态机               │
  │  4. updatePattern()      ← 序列状态机               │
  └─────────────────────────────────────────────────────┘
```

没有 RTOS，单线程顺序执行。所有操作在 `loop()` 中串行完成。

## 2. 单条命令的完整路径

以 `{"cmd":"light","value":"red"}` 为例：

```
串口到达 '\n'
  │
  ▼
readSerial()                          耗时: ~1μs
  │  逐字符读入 rxBuf[512]
  │  '\n' 触发 processCommand()
  │
  ▼
processCommand("red")                 耗时: ~1-5ms
  │  JSON 解析 (deserializeJson)
  │  参数校验
  │  blinkingActive = false
  │  patternActive = false
  │  setLight("red")
  │    │
  │    ▼
  │  drawTrafficLight("red")          耗时: ~50-100ms ⚠️
  │    ├─ drawHousing()
  │    │    ├─ fillScreen(COL_BG)         ← 全屏刷黑
  │    │    └─ fillRoundRect + draw       ← 外壳
  │    ├─ drawLightOff(LIGHT_R1)   ← 红灯灭
  │    ├─ drawLightOff(LIGHT_R2)   ← 黄灯灭
  │    ├─ drawLightOff(LIGHT_R3)   ← 绿灯灭
  │    └─ drawLightOn(LIGHT_R1)    ← 红灯亮 + 辉光 + 高光
  │
  └─ sendOk()                            耗时: ~5ms
       Serial.flush() + println(json)
```

### 各阶段耗时分布

| 阶段 | 近似耗时 | 类型 |
|------|---------|------|
| `readSerial()` (无数据) | ~1μs | 非阻塞 |
| `readSerial()` + `processCommand()` | ~1-10ms | **阻塞** |
| `drawTrafficLight()` 全屏重绘 | **~50-100ms** | ⚠️ **最大阻塞点** |
| `sendOk()` 串口 flush | ~5ms | **阻塞** |
| `updateBlink()` (不活跃) | ~1μs | 非阻塞 |
| `updatePattern()` (不活跃) | ~1μs | 非阻塞 |
| `updateBlink()` (切换瞬间) | ~50-100ms | ⚠️ 同 draw |

## 3. 并发风险分析

ESP8266 是单核 MCU，无 RTOS，没有真正并发。但存在以下时序问题：

### 3.1 RX 串口缓冲溢出 🔴

```
100ms 重绘期间:
  ┌─ drawTrafficLight() ─────────────────── 50-100ms ──┐
  │                                                    │
  │  串口 115200 baud = ~11,500 字节/秒               │
  │  100ms 内到达 ~1,150 字节                         │
  │  UART 硬件 FIFO 仅 128 字节                       │
  │  → 超出部分静默丢失                                │
  └────────────────────────────────────────────────────┘
```

**后果**：快速连续发送多条命令时，中间的命令可能被静默丢弃。

**现有防护**：`RX_BUFFER=512` 是软件缓冲区，但软件只有在 `readSerial()` 运行时才从 FIFO 取数据。如果 `drawTrafficLight()` 阻塞 100ms 期间没有调用 `readSerial()`，硬件 FIFO 128 字节填满后溢出。

### 3.2 Light 命令被静默丢弃 🟡

```cpp
void setLight(const String& color) {
    if (blinkingActive || patternActive) return;  // ← 静默返回
    ...
}
```

如果在 blink 或 pattern 执行期间发送 `light` 命令，会被静默丢弃（返回 ok 但灯不变）。

### 3.3 Pattern 时序累积漂移 🟡

```cpp
if (now - patternStepStartTime >= duration) {
    patternIndex++;
}
```

每次步骤切换都执行 `drawTrafficLight()` (~50-100ms)，这段耗时**不计入** duration。

```
期望: red(2000ms) → green(2000ms)
实际: red(2000+50ms) → green(2000+50ms)
50 步后累积漂移可达 5 秒
```

### 3.4 Blink 切换延迟 🟢

Blink 在 `loop()` 中检查 `millis()` 决定切换时机。但如果主循环正阻塞在 `drawTrafficLight()` 中，切换会延迟最多 100ms。对 500ms 间隔影响不大。

## 4. 优化方案

### 4.1 增量绘制（最高优先级）

**问题**：每次 `drawTrafficLight()` 重绘**整个**屏幕，包括不变的外壳和两个不亮的灯。

**方案**：只改变需要变更的部分：

```
修改前（全量重绘 50-100ms）:
  drawTrafficLight(color):
    drawHousing()            ← 全屏 + 外壳
    drawLightOff(R1, R2, R3) ← 3 个暗灯
    drawLightOn(target)      ← 1 个亮灯

修改后（增量绘制 ~5ms）:
  setLight(color):
    if (previous != color):
      drawLightOff(previous)  ← 只熄灭前一个灯
      drawLightOn(color)      ← 只点亮新灯
```

效果：50-100ms → **~5ms**，阻塞窗口缩小 90%+。

### 4.2 增大串口缓冲区

```cpp
Serial.setRxBufferSize(256);  // 默认 128 → 256
```

但这是软件缓冲，不解决硬件 FIFO 溢出。根本方案仍是减少阻塞时间。

### 4.3 Pattern 时序补偿

```cpp
// 记录步骤切换的实际耗时，从下一步 duration 中减去
unsigned long stepOverhead = millis() - patternStepStartTime;
// duration 不变，但下次判断时补偿
```

### 4.4 命令队列

将 `readSerial()` 改为只入队不处理，`loop()` 中逐条消费：

```cpp
struct Command {
    char buf[512];
};
std::vector<Command> cmdQueue;

void readSerial() {
    // 只入队
    if (遇到 '\n') cmdQueue.push_back(rxBuf);
}

void loop() {
    readSerial();
    if (!cmdQueue.empty()) processCommand(cmdQueue.front());
    updateBlink();
    updatePattern();
}
```

但 ESP8266 内存有限（~40KB 可用），队列不宜过大。

---

## 5. 总结

| 风险 | 严重程度 | 方案 | 优先级 |
|------|---------|------|--------|
| RX 溢出丢命令 | 🔴 中 | 增量绘制减少阻塞窗口 | 🥇 |
| 全量重绘 100ms 阻塞 | 🔴 中 | 增量绘制 ~5ms | 🥇 |
| Light 被静默丢弃 | 🟡 低 | 加日志 / 返回错误 | 🥈 |
| Pattern 时序漂移 | 🟡 低 | 补偿绘制耗时 | 🥈 |
| Blink 切换延迟 | 🟢 极低 | 无需处理 | — |
