/**
 * LLM Traffic Light — ESP8266 串口红绿灯 Agent 接口
 * ==================================================
 * 供 AI Agent (Claude 等) 通过 USB 串口控制 ST7735 TFT 红绿灯
 *
 * UI 布局 (128×128):
 *   ┌────────────────────────────────┐
 *   │     (🔴)   (🟡)   (🟢)        │ ← 加大横向红绿灯
 *   │────────────────────────────────│
 *   │  CPU ███████░░ 73%             │ ← 一行: 标题|10格|百分比
 *   │  MEM ██████░░░ 65%            │
 *   └────────────────────────────────┘
 *
 * 串口协议 (115200 baud, 8N1):
 *   {"cmd":"light", "value":"red"}        设置灯光
 *   {"cmd":"blink", "value":"red", ...}   闪烁
 *   {"cmd":"pattern", "steps":[[...],...]} 灯光序列
 *   {"cmd":"status"}                      查询状态
 *   {"cmd":"health","cpu":73,"mem":65}    更新 CPU/MEM 显示
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// ======================== 串口配置 ========================
#define SERIAL_BAUD   115200
#define RX_BUFFER     1024

// ======================== UI 布局 (128×128) ========================

// 横向红绿灯 (撑满 128，灯最大 r=20)
#define TL_BODY_X      0
#define TL_BODY_Y      4
#define TL_BODY_W      128
#define TL_BODY_H      48
#define TL_BODY_R      6

#define TL_CY          28          // 灯中心 Y
#define TL_R           18          // 灯半径
#define TL_RED_X       22          // 红灯 X
#define TL_YELLOW_X    64          // 黄灯 X
#define TL_GREEN_X     106         // 绿灯 X

// CPU / MEM 一行布局: 标题 | 20格条形图 | 百分比
#define ROW1_Y         58          // CPU 行 Y
#define ROW2_Y         68          // MEM 行 Y
#define ROW_LABEL_X    2           // "CPU"/"MEM" 标题 X
#define ROW_BAR_X      21          // 条形图起始 X
#define ROW_BAR_W      3           // 每格宽度
#define ROW_BAR_H      7           // 每格高度
#define ROW_BAR_GAP    1           // 格间距
#define ROW_VALUE_X    102         // 百分比数值 X
#define BAR_COUNT      20          // 格子数 (每格 5%)

// 颜色
#define COL_BG        0x0000   // 黑色背景
#define COL_BODY      0x2104   // 深灰外壳
#define COL_BODY_EDGE 0x4208   // 亮灰边
#define COL_DARK      0x3186   // 未点亮时的暗色
#define COL_RED       0xF800
#define COL_RED_GLOW  0xFAA0
#define COL_YELLOW    0xFFE0
#define COL_Y_GLOW    0xFEE0
#define COL_GREEN     0x07E0
#define COL_G_GLOW    0x87E0
#define COL_HIGHLIGHT 0xFFFF
#define COL_LABEL     0xAD55   // 标签文字灰
#define COL_VALUE     0xFFFF   // 数值白色

// 灯效内部颜色
#define COL_LIGHT_EDGE   0x528A
#define COL_LIGHT_INNER  0x28A5

// ======================== 全局变量 ========================
TFT_eSPI tft = TFT_eSPI();
String currentLight = "off";
int cpuPercent = 0;
int memPercent = 0;

// 串口缓冲
char rxBuf[RX_BUFFER];
int rxIndex = 0;

// 闪烁
bool blinkingActive = false;
String blinkColor;
int blinkRemaining = 0;
unsigned long blinkOnTime = 0;
unsigned long blinkOffTime = 0;
bool blinkState = false;
unsigned long lastBlinkToggle = 0;

// 序列
bool patternActive = false;

// 三灯齐闪
bool blinkAllActive = false;
unsigned long blinkAllInterval = 0;
bool blinkAllState = false;
unsigned long blinkAllLastToggle = 0;
JsonDocument patternSteps;
size_t patternIndex = 0;
unsigned long patternStepStartTime = 0;
unsigned long patternDrift = 0;

// ======================== TFT 绘图 ========================

void drawLightOff(int cx, int cy, int r) {
    tft.fillCircle(cx, cy, r, COL_DARK);
    tft.drawCircle(cx, cy, r, COL_LIGHT_EDGE);
    tft.fillCircle(cx, cy, r - 4, COL_LIGHT_INNER);
}

void drawLightOn(int cx, int cy, int r, uint16_t color, uint16_t glowColor) {
    tft.fillCircle(cx, cy, r + 2, glowColor);
    tft.fillCircle(cx, cy, r, color);
    tft.drawCircle(cx, cy, r, 0xFFFF);
    // 高光
    tft.fillCircle(cx - 5, cy - 5, 5, COL_HIGHLIGHT);
    tft.fillCircle(cx - 7, cy - 7, 3, COL_HIGHLIGHT);
    tft.fillCircle(cx - 8, cy - 8, 1, COL_HIGHLIGHT);
}

/** 横向红绿灯外壳 (加大) */
void drawHousing() {
    tft.fillScreen(COL_BG);
    tft.fillRoundRect(TL_BODY_X, TL_BODY_Y, TL_BODY_W, TL_BODY_H, TL_BODY_R, COL_BODY);
    tft.drawRoundRect(TL_BODY_X, TL_BODY_Y, TL_BODY_W, TL_BODY_H, TL_BODY_R, COL_BODY_EDGE);
    // 底部高光线
    tft.drawLine(TL_BODY_X + TL_BODY_R, TL_BODY_Y + TL_BODY_H,
                 TL_BODY_X + TL_BODY_W - TL_BODY_R, TL_BODY_Y + TL_BODY_H,
                 COL_BODY_EDGE);
}

/** 全量绘制红绿灯 */
void drawTrafficLight(const String& color) {
    drawHousing();
    drawLightOff(TL_RED_X, TL_CY, TL_R);
    drawLightOff(TL_YELLOW_X, TL_CY, TL_R);
    drawLightOff(TL_GREEN_X, TL_CY, TL_R);

    if (color == "red")    drawLightOn(TL_RED_X, TL_CY, TL_R, COL_RED, COL_RED_GLOW);
    else if (color == "yellow") drawLightOn(TL_YELLOW_X, TL_CY, TL_R, COL_YELLOW, COL_Y_GLOW);
    else if (color == "green")  drawLightOn(TL_GREEN_X, TL_CY, TL_R, COL_GREEN, COL_G_GLOW);
}

/** 根据负载值返回整体颜色 (0-50%绿, 50-80%黄, 80-100%红) */
uint16_t loadColor(int value) {
    if (value <= 50) return 0x07E0;   // 绿色
    if (value <= 80) return 0xFFE0;   // 黄色
    return 0xF800;                     // 红色
}

/** 绘制 20 格条形图 (每格 5%，整体颜色随负载变化) */
void drawBars(int y, int value) {
    uint16_t color = loadColor(value);
    int perBar = 100 / BAR_COUNT;
    for (int i = 0; i < BAR_COUNT; i++) {
        int bx = ROW_BAR_X + i * (ROW_BAR_W + ROW_BAR_GAP);
        int filled = (i + 1) * perBar;
        if (value >= filled) {
            tft.fillRect(bx, y, ROW_BAR_W, ROW_BAR_H, color);
        } else if (value > i * perBar) {
            int partial = (value % perBar) * ROW_BAR_W / perBar;
            if (partial > 0) tft.fillRect(bx, y, partial, ROW_BAR_H, color);
            tft.drawRect(bx, y, ROW_BAR_W, ROW_BAR_H, color);
        } else {
            tft.drawRect(bx, y, ROW_BAR_W, ROW_BAR_H, color);
        }
    }
}

/** 更新健康面板 — 一行一条: 标题 | 20格条 | 百分比 */
void drawHealthPanel(int cpu, int mem) {
    tft.setTextSize(1);

    // ── CPU 行 ──
    tft.setCursor(ROW_LABEL_X, ROW1_Y);
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.print("CPU");

    // 先清空上一轮的条形区
    tft.fillRect(ROW_BAR_X, ROW1_Y, 128 - ROW_BAR_X, ROW_BAR_H, COL_BG);
    drawBars(ROW1_Y, cpu);

    tft.setCursor(ROW_VALUE_X, ROW1_Y);
    tft.setTextColor(COL_VALUE, COL_BG);
    tft.print(cpu);
    tft.print("%");

    // ── MEM 行 ──
    tft.setCursor(ROW_LABEL_X, ROW2_Y);
    tft.setTextColor(COL_LABEL, COL_BG);
    tft.print("MEM");

    tft.fillRect(ROW_BAR_X, ROW2_Y, 128 - ROW_BAR_X, ROW_BAR_H, COL_BG);
    drawBars(ROW2_Y, mem);

    tft.setCursor(ROW_VALUE_X, ROW2_Y);
    tft.setTextColor(COL_VALUE, COL_BG);
    tft.print(mem);
    tft.print("%");
}

/** 完整 UI 绘制 (红绿灯 + 健康面板) */
void drawFullUI(const String& color, int cpu, int mem) {
    drawTrafficLight(color);
    drawHealthPanel(cpu, mem);
}

// ======================== 灯光控制 (增量) ========================

int lightX(const String& color) {
    if (color == "red")    return TL_RED_X;
    if (color == "yellow") return TL_YELLOW_X;
    if (color == "green")  return TL_GREEN_X;
    return -1;
}

bool setLight(const String& color) {
    if (blinkingActive || patternActive || blinkAllActive) return false;
    if (currentLight == color) return true;

    int prevX = lightX(currentLight);
    if (prevX > 0) {
        tft.fillCircle(prevX, TL_CY, TL_R + 3, COL_BODY);
        drawLightOff(prevX, TL_CY, TL_R);
    }

    if (color == "red")    drawLightOn(TL_RED_X, TL_CY, TL_R, COL_RED, COL_RED_GLOW);
    else if (color == "yellow") drawLightOn(TL_YELLOW_X, TL_CY, TL_R, COL_YELLOW, COL_Y_GLOW);
    else if (color == "green")  drawLightOn(TL_GREEN_X, TL_CY, TL_R, COL_GREEN, COL_G_GLOW);

    currentLight = color;
    return true;
}

void setBlinkLight(const String& color, bool on) {
    int x = lightX(color);
    if (x < 0) return;
    if (on) {
        drawLightOn(x, TL_CY, TL_R,
            color == "red" ? COL_RED : (color == "yellow" ? COL_YELLOW : COL_GREEN),
            color == "red" ? COL_RED_GLOW : (color == "yellow" ? COL_Y_GLOW : COL_G_GLOW));
    } else {
        tft.fillCircle(x, TL_CY, TL_R + 2, COL_BODY);
        drawLightOff(x, TL_CY, TL_R);
    }
}

// ======================== 启动动画 ========================

void blinkAll(int times) {
    for (int t = 0; t < times; t++) {
        drawHousing();
        drawLightOff(TL_RED_X, TL_CY, TL_R);
        drawLightOff(TL_YELLOW_X, TL_CY, TL_R);
        drawLightOff(TL_GREEN_X, TL_CY, TL_R);
        drawLightOn(TL_RED_X, TL_CY, TL_R, COL_RED, COL_RED_GLOW);
        drawLightOn(TL_YELLOW_X, TL_CY, TL_R, COL_YELLOW, COL_Y_GLOW);
        drawLightOn(TL_GREEN_X, TL_CY, TL_R, COL_GREEN, COL_G_GLOW);
        delay(250);
        setLight("off");
        delay(250);
    }
}

// ======================== 串口应答 ========================

void sendResponse(const String& json) {
    Serial.flush();
    delay(1);
    Serial.println(json);
    Serial.flush();
}

void sendLog(const String& msg) {
    Serial.println("[L] " + msg);
}

void sendStatus() {
    JsonDocument doc;
    doc["status"] = "ok";
    doc["light"] = currentLight;
    doc["blinking"] = blinkingActive;
    doc["pattern_active"] = patternActive;
    doc["cpu"] = cpuPercent;
    doc["mem"] = memPercent;
    doc["uptime_ms"] = millis();
    String out;
    serializeJson(doc, out);
    sendResponse(out);
}

void sendError(const String& msg) {
    JsonDocument doc;
    doc["status"] = "error";
    doc["message"] = msg;
    String out;
    serializeJson(doc, out);
    sendResponse(out);
}

void sendOk() {
    sendResponse("{\"status\":\"ok\",\"light\":\"" + currentLight + "\"}");
}

// ======================== 串口命令解析 ========================

void processCommand(const String& line) {
    if (line.startsWith("{")) {
        JsonDocument cmd;
        DeserializationError err = deserializeJson(cmd, line);
        if (err) {
            sendError("Invalid JSON: " + String(err.c_str()));
            return;
        }

        const char* c = cmd["cmd"];
        if (!c) { sendError("Missing 'cmd' field"); return; }
        String command = String(c);

        if (command == "light") {
            const char* val = cmd["value"];
            if (!val) { sendError("Missing 'value'"); return; }
            String v = String(val);
            if (v != "red" && v != "yellow" && v != "green" && v != "off") {
                sendError("Invalid value: red/yellow/green/off");
                return;
            }
            blinkingActive = false;
            blinkAllActive = false;
            patternActive = false;
            setLight(v);
            sendLog("light → " + v);
            sendOk();

        } else if (command == "blink") {
            const char* val = cmd["value"];
            if (!val) { sendError("Missing 'value'"); return; }
            String v = String(val);
            if (v != "red" && v != "yellow" && v != "green") {
                sendError("Invalid value: red/yellow/green");
                return;
            }
            int times = cmd["times"] | 3;
            int interval = cmd["interval"] | 500;
            patternActive = false;
            blinkColor = v;
            blinkRemaining = times * 2;
            blinkOnTime = interval;
            blinkOffTime = interval;
            blinkState = false;
            lastBlinkToggle = 0;
            setLight("off");
            currentLight = "off";
            blinkingActive = true;  // 先关灯再设标志，避免 setLight 被跳过
            sendLog("blink " + v + " x" + String(times) + " @" + String(interval) + "ms");
            JsonDocument res;
            res["status"] = "ok";
            res["action"] = "blink";
            res["light"] = v;
            res["times"] = times;
            res["interval_ms"] = interval;
            String out;
            serializeJson(res, out);
            sendResponse(out);

        } else if (command == "pattern") {
            JsonArray steps = cmd["steps"].as<JsonArray>();
            if (steps.isNull() || steps.size() == 0) {
                sendError("Missing or empty 'steps'");
                return;
            }
            if (steps.size() > 50) {
                sendError("Too many steps (max 50)");
                return;
            }
            blinkingActive = false;
            blinkAllActive = false;
            patternSteps.clear();
            patternSteps["steps"] = steps;
            patternIndex = 0;
            patternStepStartTime = 0;
            setLight("off");
            currentLight = "off";
            patternActive = true;  // 先关灯再设标志
            sendLog("pattern " + String(steps.size()) + " steps");
            JsonDocument res;
            res["status"] = "ok";
            res["action"] = "pattern";
            res["steps"] = steps.size();
            String out;
            serializeJson(res, out);
            sendResponse(out);

        } else if (command == "blink_all") {
            int times = cmd["times"] | 3;
            int interval = cmd["interval"] | 500;
            blinkingActive = false;
            blinkAllActive = false;
            patternActive = false;
            blinkAllActive = true;
            blinkAllInterval = interval;
            blinkAllState = false;
            blinkAllLastToggle = 0;
            drawTrafficLight("off");
            currentLight = "off";
            sendLog("blink_all x" + String(times) + " @" + String(interval) + "ms");
            JsonDocument res;
            res["status"] = "ok";
            res["action"] = "blink_all";
            res["times"] = times;
            String out;
            serializeJson(res, out);
            sendResponse(out);

        } else if (command == "status") {
            sendStatus();

        } else if (command == "health") {
            // 更新 CPU/MEM 显示
            int cpu = cmd["cpu"] | cpuPercent;
            int mem = cmd["mem"] | memPercent;
            if (cpu > 100) cpu = 100;
            if (mem > 100) mem = 100;
            if (cpu < 0) cpu = 0;
            if (mem < 0) mem = 0;
            cpuPercent = cpu;
            memPercent = mem;
            // 只重绘健康面板，不碰红绿灯
            drawHealthPanel(cpuPercent, memPercent);
            sendLog("health cpu=" + String(cpu) + "% mem=" + String(mem) + "%");
            sendResponse("{\"status\":\"ok\",\"action\":\"health\",\"cpu\":" + String(cpu) + ",\"mem\":" + String(mem) + "}");

        } else if (command == "help") {
            sendResponse("{\"status\":\"ok\",\"help\":\"light|blink|pattern|status|health\"}");

        } else {
            sendError("Unknown command: " + command);
        }
        return;
    }

    // 文本快捷命令
    String t = line; t.trim(); t.toLowerCase();
    if (t == "red" || t == "yellow" || t == "green" || t == "off") {
        blinkingActive = false;
            blinkAllActive = false; patternActive = false;
        setLight(t); sendLog("light → " + t); sendOk();
    } else if (t == "status") {
        sendStatus();
    } else if (t == "help") {
        Serial.println();
        Serial.println("╔══════════════════════════════╗");
        Serial.println("║  🚦 LLM Traffic Light       ║");
        Serial.println("║  light/blink/pattern/status ║");
        Serial.println("║  health/help                 ║");
        Serial.println("╚══════════════════════════════╝");
        Serial.println();
        sendResponse("{\"status\":\"ok\",\"message\":\"Help printed\"}");
    } else {
        sendError("Unknown command. Type 'help'.");
    }
}

// ======================== 串口读取 ========================

void readSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (rxIndex > 0) {
                rxBuf[rxIndex] = '\0';
                processCommand(String(rxBuf));
                rxIndex = 0;
            }
        } else if (rxIndex < RX_BUFFER - 1) {
            rxBuf[rxIndex++] = c;
        }
        if (rxIndex >= RX_BUFFER - 1) {
            rxIndex = 0;
            sendError("Command too long");
        }
    }
}

// ======================== 非阻塞更新 ========================

void updateBlinkAll() {
    if (!blinkAllActive) return;
    unsigned long now = millis();
    if (now - blinkAllLastToggle >= blinkAllInterval) {
        blinkAllLastToggle = now;
        blinkAllState = !blinkAllState;
        if (blinkAllState) {
            drawHousing();
            drawLightOff(TL_RED_X, TL_CY, TL_R);
            drawLightOff(TL_YELLOW_X, TL_CY, TL_R);
            drawLightOff(TL_GREEN_X, TL_CY, TL_R);
            drawLightOn(TL_RED_X, TL_CY, TL_R, COL_RED, COL_RED_GLOW);
            drawLightOn(TL_YELLOW_X, TL_CY, TL_R, COL_YELLOW, COL_Y_GLOW);
            drawLightOn(TL_GREEN_X, TL_CY, TL_R, COL_GREEN, COL_G_GLOW);
        } else {
            drawTrafficLight("off");
        }
        // 恢复健康面板显示（drawHousing 清空了屏幕）
        drawHealthPanel(cpuPercent, memPercent);
        // blinkAll 持续闪烁直到被下一个命令停止
    }
}

void updateBlink() {
    if (!blinkingActive) return;
    unsigned long now = millis();
    unsigned long currentInterval = blinkState ? blinkOnTime : blinkOffTime;
    if (now - lastBlinkToggle >= currentInterval) {
        lastBlinkToggle = now;
        blinkState = !blinkState;
        blinkRemaining--;
        setBlinkLight(blinkColor, blinkState);
        if (blinkRemaining <= 0) {
            blinkingActive = false;
            blinkAllActive = false;
            setLight("off");
            currentLight = "off";
            Serial.println("[BLINK] Finished");
        }
    }
}

void updatePattern() {
    if (!patternActive) return;
    unsigned long now = millis();
    JsonArray steps = patternSteps["steps"].as<JsonArray>();
    if (patternIndex >= steps.size()) {
        patternActive = false;
        setLight("off");
            currentLight = "off";
        Serial.println("[PATTERN] Finished");
        return;
    }
    JsonArray step = steps[patternIndex].as<JsonArray>();
    if (step.size() < 2) { patternIndex++; return; }
    const char* targetLight = step[0];
    unsigned long duration = step[1].as<unsigned long>();
    if (patternStepStartTime == 0) {
        unsigned long transStart = millis();
        String tl = String(targetLight);
        patternActive = false;     // 放行 setLight
        setLight(tl);
        patternActive = true;      // 恢复
        patternStepStartTime = millis();
        patternDrift = patternStepStartTime - transStart;
        if (patternDrift > duration) patternDrift = duration;
        Serial.printf("[PATTERN] Step %zu: %s for %lums (drift %lums)\n", patternIndex, targetLight, duration, patternDrift);
    }
    if (now - patternStepStartTime >= duration - patternDrift) {
        patternIndex++;
        patternStepStartTime = 0;
    }
}

// ======================== 初始化 ========================

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.setRxBufferSize(256);
    delay(200);

    Serial.println();
    Serial.println("╔══════════════════════════════╗");
    Serial.println("║  🚦 LLM Traffic Light v4.0  ║");
    Serial.println("║  横向红绿灯 + 健康面板       ║");
    Serial.println("╚══════════════════════════════╝");
    Serial.println();

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COL_BG);
    tft.setTextColor(0xFFFF, COL_BG);
    tft.setTextSize(1);

    // 启动动画
    drawFullUI("off", 0, 0);
    delay(500);
    blinkAll(3);
    drawFullUI("off", 0, 0);

    Serial.println("✅ Ready.");
    Serial.print("> ");
}

// ======================== 主循环 ========================

void loop() {
    readSerial();
    updateBlinkAll();
    updateBlink();
    updatePattern();
}
