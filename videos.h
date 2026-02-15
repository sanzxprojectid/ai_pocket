#ifndef VIDEOS_H
#define VIDEOS_H

#include <Adafruit_SSD1306.h>

void drawVideo1(Adafruit_SSD1306& display, int frame);
void drawVideo2(Adafruit_SSD1306& display, int frame);
int getVideo1Delay();
int getVideo2Delay();

#endif
