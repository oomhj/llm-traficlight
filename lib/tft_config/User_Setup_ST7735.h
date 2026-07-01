/**
 * Custom TFT_eSPI setup for ST7735 1.8" 128x160 TFT on ESP8266
 * =============================================================
 * 供 Agent 红绿灯项目使用的 ST7735 屏幕配置
 *
 * 引脚接线:
 *   ST7735     ESP8266 NodeMCU
 *   ───────    ────────────────
 *   CS   →     D2  (GPIO4)
 *   DC   →     D1  (GPIO5)
 *   RST  →     D4  (GPIO2)
 *   MOSI →     D7  (GPIO13)  HW SPI MOSI
 *   SCLK →     D5  (GPIO14)  HW SPI SCLK
 *   BL   →     3.3V (或 D0/GPIO16 可通过 PWM 调亮度)
 *   VCC  →     3.3V
 *   GND  →     GND
 *
 * 注意:
 *   - 使用 ESP8266 硬件 SPI (不需要额外定义 SPI 引脚)
 *   - 如果 RST 接 GPIO2 (D4), 确保上电时该引脚被内部上拉拉高
 *   - 若要软件 SPI(软串), 取消注释 TFT_SOFT_SPI 相关行
 */

// ======================== 驱动选择 ========================
#define ST7735_DRIVER        // 使用 ST7735 驱动 (1.8" 128x160 TFT)

// ======================== 屏幕尺寸 ========================
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// ======================== SPI 引脚 (ESP8266 硬件 SPI) ========================
// ESP8266 硬件 SPI 固定引脚:
//   MOSI = GPIO13 (D7)
//   MISO = GPIO12 (D6)   — ST7735 不需要 MISO
//   SCLK = GPIO14 (D5)
// 无需额外定义，下面只定义 CS/DC/RST

#define TFT_CS   D2   // GPIO4  — 片选
#define TFT_DC   D1   // GPIO5  — 数据/命令选择
#define TFT_RST  D4   // GPIO2  — 复位 (可设为 -1 以绑定 ESP RST)

// ======================== 背光引脚 (可选) ========================
// 如果背光接在 GPIO 上可以 PWM 调亮度
// #define TFT_BL  D0   // GPIO16 — 背光控制

// ======================== SPI 频率 ========================
#define SPI_FREQUENCY  27000000  // 27MHz — ST7735 典型最大频率
#define SPI_READ_FREQUENCY  8000000
#define SPI_TOUCH_FREQUENCY  2500000

// ======================== ST7735 型号选择 ========================
// 不同品牌/批次的 ST7735 可能需要不同的初始化序列
// 如果显示异常(花屏/偏移/颜色颠倒)，尝试取消注释下面其中一行:
#define ST7735_GREENTAB   // 绿标签 (最常见)
// #define ST7735_REDTAB   // 红标签
// #define ST7735_BLACKTAB // 黑标签 (ST7735S)

// ======================== 颜色格式 ========================
// 如果颜色反了(红色变蓝色)，取消注释下面这行:
#define TFT_RGB_ORDER TFT_BGR  // BGR 色彩顺序 (大多数 1.8" ST7735 使用 BGR)

// ======================== 字体 ========================
#define LOAD_GLCD   // 加载基础字体
#define LOAD_FONT2  // 加载扩展字体
#define LOAD_FONT4  // 加载大号字体

// ======================== 功能选项 ========================
#define SPI_TRANSACTION  // 支持 SPI 事务
#define SUPPORT_TRANSACTIONS

// ======================== 内存优化 ========================
// ESP8266 内存有限，关闭不需要的功能
// #define USE_HSPI_PORT  // ESP8266 只有一个 SPI，不需要这个
