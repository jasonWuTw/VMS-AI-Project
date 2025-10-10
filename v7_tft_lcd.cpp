#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

void drawAutoWrappedText(U8G2 &u8g2, int x, int y, int maxWidth, const char *text, int lineSpacing = 2) {
  int lineHeight = u8g2.getMaxCharHeight() + lineSpacing;
  const char *p = text;
  char buffer[128];
  int bufIdx = 0;

  y = y + 14 + lineHeight;

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 14, u8g2.getWidth(), u8g2.getHeight() - 14);
  u8g2.setDrawColor(1);

  while (*p) {

    buffer[bufIdx++] = *p;

    // 試算目前長度是否超過寬度或換行符
    buffer[bufIdx] = '\0';
    int width = u8g2.getUTF8Width(buffer);

    if (*p == '\n' || width > maxWidth) {

      if (*p == '\n') {
        bufIdx--; // 如果是寬度超出，不含這個字
      }
      
      buffer[bufIdx] = '\0';
      u8g2.setCursor(x, y);
      u8g2.print(buffer);

      y += lineHeight;
      bufIdx = 0;

      // if (*p != '\n') {
      //   p--;  // 再處理剛剛沒畫的字
      //   // buffer[bufIdx++] = *p;
      // }

    }

    p++;
  }

  // 印最後一行
  if (bufIdx > 0) {
    buffer[bufIdx] = '\0';
    u8g2.setCursor(x, y);
    u8g2.print(buffer);
  }

  u8g2.sendBuffer();

}

// 更新TopMenu;
void topMenuArea(U8G2 &u8g2, const char *title, int wifiSignalPower, int eMotionType) {

  int maxWidth = 100;
  int width = u8g2.getUTF8Width(title);
  int startX = (128 - width) / 2;

  // 先將上方清空;
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, u8g2.getWidth(), 14);
  u8g2.setDrawColor(0);
  u8g2.setCursor(startX, 12);
  u8g2.print(title);
  u8g2.sendBuffer();    // 更新Buffer;

}
