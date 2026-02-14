#include "videos.h"
#include <Arduino.h>

void drawVideo1(Adafruit_SSD1306& display, int frame) {
  display.clearDisplay();

  // Title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 0);
  display.print("BEST FRIEND");

  // Animation: Two circles moving together
  int x1 = 40 + (frame % 20);
  int x2 = 80 - (frame % 20);

  display.drawCircle(x1, 30, 10, SSD1306_WHITE);
  display.drawCircle(x2, 30, 10, SSD1306_WHITE);

  // Heart in the middle when they meet
  if (frame % 40 > 15 && frame % 40 < 25) {
    display.fillCircle(64, 30, 5, SSD1306_WHITE);
    display.setCursor(45, 45);
    display.print("BFF Mode!");
  } else {
    display.setCursor(40, 45);
    display.print("Searching...");
  }

  display.display();
}
