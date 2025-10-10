#include <Arduino.h>
#include <U8g2lib.h>

// #ifdef U8X8_HAVE_HW_SPI
// #include <SPI.h>
// #endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define statusTextY 14
#define voiceTextY 62
#define emotionTextY 25

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
extern char statusText[];
extern char voiceText[];

extern int16_t statusTextX;       // 起始 x 座標
extern int16_t voiceTextX;
extern uint16_t statusTextWidth;  // 文字寬度（用於循環）
extern uint16_t voiceTextWidth;  // 文字寬度（用於循環）

void drawAutoWrappedText(U8G2 &u8g2, int x, int y, int maxWidth, const char *text, int lineSpacing = 2);

void topMenuArea(U8G2 &u8g2, const char *title, int wifiSignalPower, int eMotionType);