// ============================================================
//  燈牌端 — ESP32 + ILI9488 3.5吋 TFT（最終版）
//  HC-05 D（Slave）：UART2 GPIO16(RX) / GPIO17(TX)
// ============================================================
//  接線（ESP32 30P 擴展板）：
//    TFT VCC  → 麵包板同欄（接擴展板 3V3）
//    TFT GND  → 麵包板同欄（接擴展板 GND）
//    TFT LED  → 麵包板同欄（接擴展板 3V3）
//    TFT CLK  → 擴展板右排 D14
//    TFT MOSI → 擴展板右排 D13
//    TFT MISO → 擴展板右排 D12
//    TFT CS   → 擴展板左排 D15
//    TFT DC   → 擴展板左排 D2
//    TFT RST  → 擴展板左排 D4
//    HC-05 D VCC → 擴展板下方 5V
//    HC-05 D GND → 麵包板同欄（GND）
//    HC-05 D TX  → 擴展板左排 D16（RX2，直接接）
//    HC-05 D RX  → 擴展板左排 D17（TX2，串 1kΩ 電阻）
//    行動電源    → 擴展板 Micro USB
// ============================================================
//  User_Setup.h 設定（TFT_eSPI 函式庫）：
//    #define ILI9488_DRIVER
//    #define TFT_MISO 12
//    #define TFT_MOSI 13
//    #define TFT_SCLK 14
//    #define TFT_CS   15
//    #define TFT_DC    2
//    #define TFT_RST   4
//    #define SPI_FREQUENCY 27000000
// ============================================================
//  速度段對應：
//    '0' 停止 → 三個紅色叉叉（✕✕✕）
//    '1' 慢速 → 一顆綠色圓（●灰灰）
//    '2' 中速 → 兩顆黃色圓（●●灰）
//    '3' 快速 → 三顆紅色圓（●●●）
// ============================================================

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define BT_RX 16
#define BT_TX 17

// 螢幕尺寸（ILI9488 橫向）
#define SCREEN_W 480
#define SCREEN_H 320

// 圓燈參數
#define CIRCLE_R   80
#define CIRCLE_Y  145
#define CIRCLE_X1  90
#define CIRCLE_X2 240
#define CIRCLE_X3 390
#define BORDER_W    5

// 顏色
#define COLOR_BG      TFT_BLACK
#define COLOR_WHITE   TFT_WHITE
#define COLOR_GREEN   0x07E0
#define COLOR_YELLOW  0xFFE0
#define COLOR_RED     0xF800
#define COLOR_GRAY    0x4228
#define COLOR_DARKRED 0x8000

byte currentLevel = 0;
byte lastLevel    = 255;

// ── 畫圓形符號 ──
void drawCircleSymbol(int cx, uint16_t color) {
  for (int i = 0; i <= BORDER_W; i++) {
    tft.drawCircle(cx, CIRCLE_Y, CIRCLE_R + i, COLOR_WHITE);
  }
  tft.fillCircle(cx, CIRCLE_Y, CIRCLE_R, color);
  if (color != COLOR_GRAY) {
    tft.fillCircle(cx - CIRCLE_R / 3, CIRCLE_Y - CIRCLE_R / 3, CIRCLE_R / 7, COLOR_WHITE);
  }
}

// ── 畫叉叉符號 ──
void drawCrossSymbol(int cx) {
  int s = CIRCLE_R - 10;
  int t = 14;
  tft.fillCircle(cx, CIRCLE_Y, CIRCLE_R, 0x2104);
  for (int i = 0; i <= BORDER_W; i++) {
    tft.drawCircle(cx, CIRCLE_Y, CIRCLE_R + i, COLOR_WHITE);
  }
  for (int i = -t/2; i <= t/2; i++) {
    tft.drawLine(cx - s + i, CIRCLE_Y - s, cx + s + i, CIRCLE_Y + s, COLOR_DARKRED);
    tft.drawLine(cx + s + i, CIRCLE_Y - s, cx - s + i, CIRCLE_Y + s, COLOR_DARKRED);
  }
}

// ── 更新畫面 ──
void updateDisplay(byte level) {
  tft.fillScreen(COLOR_BG);

  switch (level) {
    case 0:
      drawCrossSymbol(CIRCLE_X1);
      drawCrossSymbol(CIRCLE_X2);
      drawCrossSymbol(CIRCLE_X3);
      tft.setTextColor(COLOR_DARKRED, COLOR_BG);
      tft.setTextSize(4);
      tft.setCursor(160, SCREEN_H - 60);
      tft.print("STOP");
      break;

    case 1:
      drawCircleSymbol(CIRCLE_X1, COLOR_GREEN);
      drawCircleSymbol(CIRCLE_X2, COLOR_GRAY);
      drawCircleSymbol(CIRCLE_X3, COLOR_GRAY);
      tft.setTextColor(COLOR_GREEN, COLOR_BG);
      tft.setTextSize(4);
      tft.setCursor(120, SCREEN_H - 60);
      tft.print("  慢  速");
      break;

    case 2:
      drawCircleSymbol(CIRCLE_X1, COLOR_YELLOW);
      drawCircleSymbol(CIRCLE_X2, COLOR_YELLOW);
      drawCircleSymbol(CIRCLE_X3, COLOR_GRAY);
      tft.setTextColor(COLOR_YELLOW, COLOR_BG);
      tft.setTextSize(4);
      tft.setCursor(120, SCREEN_H - 60);
      tft.print("  中  速");
      break;

    case 3:
      drawCircleSymbol(CIRCLE_X1, COLOR_RED);
      drawCircleSymbol(CIRCLE_X2, COLOR_RED);
      drawCircleSymbol(CIRCLE_X3, COLOR_RED);
      tft.setTextColor(COLOR_RED, COLOR_BG);
      tft.setTextSize(4);
      tft.setCursor(120, SCREEN_H - 60);
      tft.print("  快  速");
      break;
  }
}

// ============================================================
void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);

  // 開機畫面
  tft.setTextColor(COLOR_WHITE, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(80, 130);
  tft.print("前臂肌群成神系統");
  tft.setCursor(170, 180);
  tft.print("連線中...");
  delay(1500);

  updateDisplay(0);

  Serial2.begin(9600, SERIAL_8N1, BT_RX, BT_TX);
}

// ============================================================
void loop() {
  if (Serial2.available()) {
    char c = Serial2.read();
    if (c >= '0' && c <= '3') {
      currentLevel = c - '0';
    }
  }

  if (currentLevel != lastLevel) {
    lastLevel = currentLevel;
    updateDisplay(currentLevel);
  }

  delay(30);
}
