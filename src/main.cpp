/**
 * LLM Traffic Light — ESP8266 串口红绿灯 Agent 接口
 * ==================================================
 * 供 AI Agent (Claude 等) 通过 USB 串口控制 ST7735 TFT 红绿灯
 *
 * 串口协议 (115200 baud, 8N1):
 *   Agent 发送一行 JSON → ESP8266 执行 → 返回一行 JSON
 *
 * 支持的命令:
 *   {"cmd":"light", "value":"red"}        设置灯光
 *   {"cmd":"blink", "value":"red", ...}   闪烁
 *   {"cmd":"pattern", "steps":[[...],...]} 灯光序列
 *   {"cmd":"status"}                      查询状态
 *   {"cmd":"help"}                        显示帮助
 *
 * 同时也支持直接输入: red / yellow / green / off / status / help
 *
 * 通信方式:
 *   USB 串口 (CP2102/CH340) — 即烧录用的那个端口
 *   只需一根 USB 线连接电脑即可控制
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// ======================== 串口配置 ========================
#define SERIAL_BAUD   115200
#define RX_BUFFER     512       // 接收缓冲区大小

// ======================== 红绿灯绘图参数 (128×128) ========================
#define SCR_W   128
#define SCR_H   128

// 红绿灯外壳 — 深灰色大圆角矩形 (竖着)
#define BODY_X      29
#define BODY_Y      4
#define BODY_W      70
#define BODY_H      116
#define BODY_R      8       // 大圆角

// 三个灯的中心坐标和半径 (加大直径)
#define LIGHT_CX    64
#define LIGHT_R     14      // 半径 14 → 直径 28
#define LIGHT_R1_Y  24      // 红灯
#define LIGHT_R2_Y  62      // 黄灯
#define LIGHT_R3_Y  100     // 绿灯

// 颜色 (RGB565)
#define COL_BG      0x0000  // 黑色背景
#define COL_BODY    0x2104   // 深灰
#define COL_BODY_EDGE 0x4208 // 亮灰边
#define COL_DARK    0x3186   // 未点亮时的暗色
#define COL_RED     0xF800   // 红色
#define COL_RED_GLOW  0xFAA0 // 红色辉光
#define COL_YELLOW  0xFFE0   // 黄色
#define COL_Y_GLOW  0xFEE0   // 黄色辉光
#define COL_GREEN   0x07E0   // 绿色
#define COL_G_GLOW  0x87E0   // 绿色辉光
#define COL_HIGHLIGHT 0xFFFF // 高光白

// 灯效内部颜色
#define COL_LIGHT_EDGE   0x528A  // 未点亮灯泡边框
#define COL_LIGHT_INNER  0x28A5  // 未点亮灯泡内凹

// ======================== 全局变量 ========================
TFT_eSPI tft = TFT_eSPI();

String currentLight = "off";     // off / red / yellow / green

// 串口接收缓冲区
char rxBuf[RX_BUFFER];
int rxIndex = 0;

// 闪烁相关
bool blinkingActive = false;
String blinkColor;
int blinkRemaining = 0;
unsigned long blinkOnTime = 0;
unsigned long blinkOffTime = 0;
bool blinkState = false;
unsigned long lastBlinkToggle = 0;

// 序列(Pattern)相关
bool patternActive = false;
JsonDocument patternSteps;
size_t patternIndex = 0;
unsigned long patternStepStartTime = 0;
unsigned long patternDrift = 0;      // 上一步绘制耗时补偿

// ======================== TFT 绘图函数 ========================

void drawLightOff(int cx, int cy, int r) {
    tft.fillCircle(cx, cy, r, COL_DARK);
    tft.drawCircle(cx, cy, r, COL_LIGHT_EDGE);
    tft.fillCircle(cx, cy, r - 4, COL_LIGHT_INNER);
}

void drawLightOn(int cx, int cy, int r, uint16_t color, uint16_t glowColor) {
    // 辉光
    tft.fillCircle(cx, cy, r + 2, glowColor);
    tft.fillCircle(cx, cy, r, color);
    tft.drawCircle(cx, cy, r, 0xFFFF);
    // 高光 (左上)
    tft.fillCircle(cx - 4, cy - 5, 5, COL_HIGHLIGHT);
    tft.fillCircle(cx - 6, cy - 6, 3, COL_HIGHLIGHT);
    tft.fillCircle(cx - 7, cy - 7, 1, COL_HIGHLIGHT);
}

void drawHousing() {
    tft.fillScreen(COL_BG);
    // 外壳 — 仅纯圆角矩形，无上下装饰
    tft.fillRoundRect(BODY_X, BODY_Y, BODY_W, BODY_H, BODY_R, COL_BODY);
    tft.drawRoundRect(BODY_X, BODY_Y, BODY_W, BODY_H, BODY_R, COL_BODY_EDGE);
}

void drawTrafficLight(const String& color) {
    drawHousing();
    drawLightOff(LIGHT_CX, LIGHT_R1_Y, LIGHT_R);
    drawLightOff(LIGHT_CX, LIGHT_R2_Y, LIGHT_R);
    drawLightOff(LIGHT_CX, LIGHT_R3_Y, LIGHT_R);

    if (color == "red") {
        drawLightOn(LIGHT_CX, LIGHT_R1_Y, LIGHT_R, COL_RED, COL_RED_GLOW);
    } else if (color == "yellow") {
        drawLightOn(LIGHT_CX, LIGHT_R2_Y, LIGHT_R, COL_YELLOW, COL_Y_GLOW);
    } else if (color == "green") {
        drawLightOn(LIGHT_CX, LIGHT_R3_Y, LIGHT_R, COL_GREEN, COL_G_GLOW);
    }
}

// ======================== 灯光控制 (增量绘制) ========================
// 不重绘整个屏幕，只切换变动的灯，每次 ~5ms 而非 ~100ms

/** 根据灯色返回 Y 坐标 */
int lightCy(const String& color) {
    if (color == "red")    return LIGHT_R1_Y;
    if (color == "yellow") return LIGHT_R2_Y;
    if (color == "green")  return LIGHT_R3_Y;
    return -1;
}

/** 运行时切换灯光 — 只绘制变动的灯，外壳和其他灯不变 */
void setLight(const String& color) {
    if (blinkingActive || patternActive) return;
    if (currentLight == color) return;

    int prevCy = lightCy(currentLight);
    int newCy  = lightCy(color);

    // 熄灭前一个灯 (清除辉光 + 画暗灯)
    if (prevCy > 0) {
        tft.fillCircle(LIGHT_CX, prevCy, LIGHT_R + 3, COL_BODY);
        drawLightOff(LIGHT_CX, prevCy, LIGHT_R);
    }

    // 点亮新灯
    if (color == "red")
        drawLightOn(LIGHT_CX, LIGHT_R1_Y, LIGHT_R, COL_RED, COL_RED_GLOW);
    else if (color == "yellow")
        drawLightOn(LIGHT_CX, LIGHT_R2_Y, LIGHT_R, COL_YELLOW, COL_Y_GLOW);
    else if (color == "green")
        drawLightOn(LIGHT_CX, LIGHT_R3_Y, LIGHT_R, COL_GREEN, COL_G_GLOW);
    // "off": 不需要点亮

    currentLight = color;
}

/** 闪烁切换 — 只控制指定灯，不清除其他灯 */
void setBlinkLight(const String& color, bool on) {
    int cy = lightCy(color);
    if (cy < 0) return;

    if (on) {
        drawLightOn(LIGHT_CX, cy, LIGHT_R,
            color == "red" ? COL_RED : (color == "yellow" ? COL_YELLOW : COL_GREEN),
            color == "red" ? COL_RED_GLOW : (color == "yellow" ? COL_Y_GLOW : COL_G_GLOW));
    } else {
        tft.fillCircle(LIGHT_CX, cy, LIGHT_R + 2, COL_BODY);
        drawLightOff(LIGHT_CX, cy, LIGHT_R);
    }
}

// ======================== 串口应答 ========================
void sendResponse(const String& json) {
    // 确保所有调试输出已发送完毕，避免和 JSON 响应混在一起
    Serial.flush();
    delay(1);
    Serial.println(json);
    Serial.flush();
}

void sendLog(const String& msg) {
    // 调试日志也通过 Serial 输出，但 Agent 的 Python CLI 会自动过滤掉非 JSON 行
    Serial.println("[L] " + msg);
}

void sendStatus() {
    JsonDocument doc;
    doc["status"] = "ok";
    doc["light"] = currentLight;
    doc["blinking"] = blinkingActive;
    doc["pattern_active"] = patternActive;
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
    // 尝试 JSON 解析
    if (line.startsWith("{")) {
        JsonDocument cmd;
        DeserializationError err = deserializeJson(cmd, line);
        if (err) {
            sendError("Invalid JSON: " + String(err.c_str()));
            return;
        }

        const char* c = cmd["cmd"];
        if (!c) {
            sendError("Missing 'cmd' field");
            return;
        }
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
            blinkingActive = true;
            blinkColor = v;
            blinkRemaining = times * 2;
            blinkOnTime = interval;
            blinkOffTime = interval;
            blinkState = false;
            lastBlinkToggle = 0;

            drawTrafficLight("off");
            currentLight = "off";

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
            patternSteps.clear();
            patternSteps["steps"] = steps;

            patternActive = true;
            patternIndex = 0;
            patternStepStartTime = 0;

            drawTrafficLight("off");
            currentLight = "off";

            sendLog("pattern " + String(steps.size()) + " steps");

            JsonDocument res;
            res["status"] = "ok";
            res["action"] = "pattern";
            res["steps"] = steps.size();
            String out;
            serializeJson(res, out);
            sendResponse(out);

        } else if (command == "status") {
            sendStatus();

        } else if (command == "help") {
            sendResponse("{\"status\":\"ok\",\"help\":\"Commands: "
                "{\\\"cmd\\\":\\\"light\\\",\\\"value\\\":\\\"red|yellow|green|off\\\"} | "
                "{\\\"cmd\\\":\\\"blink\\\",\\\"value\\\":\\\"red\\\",\\\"times\\\":3,\\\"interval\\\":500} | "
                "{\\\"cmd\\\":\\\"pattern\\\",\\\"steps\\\":[[\\\"red\\\",2000],[\\\"green\\\",3000]]} | "
                "{\\\"cmd\\\":\\\"status\\\"} | "
                "Shortcuts: red/yellow/green/off/status/help\"}");

        } else {
            sendError("Unknown command: " + command);
        }
        return;
    }

    // 非 JSON → 尝试简单文本命令
    String t = line;
    t.trim();
    t.toLowerCase();

    if (t == "red" || t == "yellow" || t == "green" || t == "off") {
        blinkingActive = false;
        patternActive = false;
        setLight(t);
        sendLog("light → " + t);
        sendOk();

    } else if (t == "status") {
        sendStatus();

    } else if (t == "help") {
        Serial.println();
        Serial.println("╔══════════════════════════════════════════════╗");
        Serial.println("║  🚦 LLM Traffic Light — 串口命令帮助        ║");
        Serial.println("╠══════════════════════════════════════════════╣");
        Serial.println("║  文本快捷命令:                              ║");
        Serial.println("║    red         亮红灯                       ║");
        Serial.println("║    yellow      亮黄灯                       ║");
        Serial.println("║    green       亮绿灯                       ║");
        Serial.println("║    off         关闭所有灯                   ║");
        Serial.println("║    status      查询当前状态                 ║");
        Serial.println("║    help        显示本帮助                   ║");
        Serial.println("╠══════════════════════════════════════════════╣");
        Serial.println("║  JSON 命令 (Agent 推荐使用):                ║");
        Serial.println("║  {\"cmd\":\"light\",\"value\":\"red\"}            ║");
        Serial.println("║  {\"cmd\":\"blink\",\"value\":\"red\",\"times\":3, ║");
        Serial.println("║     \"interval\":500}                         ║");
        Serial.println("║  {\"cmd\":\"pattern\",\"steps\":[              ║");
        Serial.println("║     [\"red\",2000],[\"green\",3000]]}          ║");
        Serial.println("║  {\"cmd\":\"status\"}                          ║");
        Serial.println("╚══════════════════════════════════════════════╝");
        Serial.println();
        sendResponse("{\"status\":\"ok\",\"message\":\"Help printed to serial\"}");

    } else {
        sendError("Unknown command. Type 'help' for available commands.");
    }
}

// ======================== 串口读取 ========================
void readSerial() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (rxIndex > 0) {
                rxBuf[rxIndex] = '\0';       // 字符串结束
                processCommand(String(rxBuf));
                rxIndex = 0;
            }
        } else if (rxIndex < RX_BUFFER - 1) {
            rxBuf[rxIndex++] = c;
        }
        // 缓冲区满 → 丢弃并重置 (防溢出)
        if (rxIndex >= RX_BUFFER - 1) {
            rxIndex = 0;
            sendError("Command too long");
        }
    }
}

// ======================== 非阻塞更新 ========================
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
            drawTrafficLight("off");
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
        drawTrafficLight("off");
        currentLight = "off";
        Serial.println("[PATTERN] Finished");
        return;
    }

    JsonArray step = steps[patternIndex].as<JsonArray>();
    if (step.size() < 2) {
        patternIndex++;
        return;
    }

    const char* targetLight = step[0];
    unsigned long duration = step[1].as<unsigned long>();

    if (patternStepStartTime == 0) {
        // 增量切换，不重绘整个屏幕
        String tl = String(targetLight);
        setLight(tl);
        patternStepStartTime = now;
        Serial.printf("[PATTERN] Step %zu: %s for %lums\n", patternIndex, targetLight, duration);
    }

    if (now - patternStepStartTime >= duration - patternDrift) {
        // 记录实际过渡耗时，补偿到下一步
        patternDrift = millis() - now;
        if (patternDrift > duration) patternDrift = 0;  // 防溢出
        patternIndex++;
        patternStepStartTime = 0;
    }
}

// ======================== 初始化 ========================
void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.setRxBufferSize(256);  // 增大串口缓冲区，降低溢出概率
    delay(200);

    Serial.println();
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║   🚦 LLM Traffic Light v3.0          ║");
    Serial.println("║   串口模式 · TFT 显示                 ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.println();

    // 初始化 TFT
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COL_BG);
    tft.setTextColor(0xFFFF, COL_BG);
    tft.setTextSize(1);

    // 启动画面
    tft.setCursor(10, 68);
    tft.print("Traffic Light");
    tft.setCursor(14, 82);
    tft.setTextColor(0xAAAA, COL_BG);
    tft.print("Serial Mode");
    tft.setTextColor(0xFFFF, COL_BG);

    Serial.println("Self-test: Red → Yellow → Green");
    delay(1000);

    drawTrafficLight("red");
    delay(500);
    drawTrafficLight("yellow");
    delay(500);
    drawTrafficLight("green");
    delay(500);
    drawTrafficLight("off");

    Serial.println("✅ Ready. Send a command (type 'help' for options):");
    Serial.print("> ");
}

// ======================== 主循环 ========================
void loop() {
    readSerial();
    updateBlink();
    updatePattern();
}
