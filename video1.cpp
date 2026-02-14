#include "videos.h"
#include <Arduino.h>

#define FRAME_WIDTH 128
#define FRAME_HEIGHT 64
#define FRAME_DELAY 70 // Bisa diubah sesuai kebutuhan

static const byte PROGMEM frames[][FRAME_WIDTH * FRAME_HEIGHT / 8] = {
  { 0 } // Dummy frame - replace with your actual frame data
};

void drawVideo1(Adafruit_SSD1306& display, int frame) {
  int frameCount = sizeof(frames) / sizeof(frames[0]);
  if (frameCount == 0) return;

  int f = frame % frameCount;
  display.clearDisplay();
  display.drawBitmap(0, 0, frames[f], FRAME_WIDTH, FRAME_HEIGHT, SSD1306_WHITE);
  display.display();
}
