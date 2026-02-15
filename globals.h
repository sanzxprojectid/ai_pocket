#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <Wire.h>

// Screen dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Battery constants
#define BATTERY_CRITICAL 3300

// Extern global variables
extern Adafruit_SSD1306 display;
extern bool musicPlayerAvailable;
extern bool isPlaying;
extern bool espnowInitialized;
extern int espnowPeerCount;
extern int chatMessageCount;
extern int batteryPercent;
extern int batteryVoltage;
extern int menuSelection;

// Function prototypes
void drawStatusBar();
void showMainMenu();
void drawBatteryIcon(int x, int y);

#endif
