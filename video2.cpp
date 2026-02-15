#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "videos.h"

#define FRAME_WIDTH 128
#define FRAME_HEIGHT 64
#define FRAME_DELAY 15

static const byte PROGMEM frames[][FRAME_WIDTH * FRAME_HEIGHT / 8] = {
  { 0 }
};

void drawVideo2(Adafruit_SSD1306& display, int frame) {
  int frameCount = sizeof(frames) / sizeof(frames[0]);
  if (frameCount == 0) return;

  int f = frame % frameCount;
  display.drawBitmap(0, 0, frames[f], FRAME_WIDTH, FRAME_HEIGHT, SSD1306_WHITE);
}

int getVideo2Delay() {
  return FRAME_DELAY;
}
