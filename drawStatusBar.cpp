void drawStatusBar() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (musicPlayerAvailable && isPlaying) {
    display.setCursor(0, 0);
    display.print("♪");
  } else {
    // Pulse animation for "life"
    if ((millis() / 1000) % 2 == 0) {
      display.drawPixel(2, 3, SSD1306_WHITE);
      display.drawPixel(3, 2, SSD1306_WHITE);
      display.drawPixel(3, 4, SSD1306_WHITE);
      display.drawPixel(4, 3, SSD1306_WHITE);
    }
  }

  drawBatteryIcon(SCREEN_WIDTH - 22, 0);

  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int bars = map(rssi, -100, -50, 1, 4);
    bars = constrain(bars, 1, 4);
    for (int i = 0; i < 4; i++) {
      int h = (i + 1) * 2;
      if (i < bars) {
        display.fillRect(85 + (i * 3), 8 - h, 2, h, SSD1306_WHITE);
      } else {
        display.drawRect(85 + (i * 3), 8 - h, 2, h, SSD1306_WHITE);
      }
    }
  }

  if (espnowInitialized) {
    display.setCursor(65, 0);
    display.print("E:");
    display.print(espnowPeerCount);
  }

  if (chatMessageCount > 0) {
    display.setCursor(15, 0);
    display.print("M:");
    display.print(chatMessageCount);
  }

  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
}
