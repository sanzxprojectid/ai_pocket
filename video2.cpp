#include "videos.h"
#include <Arduino.h>

void drawVideo2(Adafruit_SSD1306& display, int frame) {
  display.clearDisplay();

  // Title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 0);
  display.print("CUTE CHIBI");

  // Animation: Bouncing chibi head
  int y = 35 + sin(frame * 0.2) * 10;

  // Head
  display.drawCircle(64, y, 15, SSD1306_WHITE);

  // Eyes
  int eyeOffset = (frame % 20 < 10) ? 2 : 0; // Blink effect
  if (eyeOffset == 0) {
    display.fillCircle(58, y - 5, 2, SSD1306_WHITE);
    display.fillCircle(70, y - 5, 2, SSD1306_WHITE);
  } else {
    display.drawLine(56, y - 5, 60, y - 5, SSD1306_WHITE);
    display.drawLine(68, y - 5, 72, y - 5, SSD1306_WHITE);
  }

  // Mouth
  display.drawCircle(64, y + 5, 3, SSD1306_WHITE);

  display.setCursor(45, 55);
  display.print("So Kawaii!");

  display.display();
}
