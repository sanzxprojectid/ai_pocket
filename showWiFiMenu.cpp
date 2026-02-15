void showWiFiMenu() {
  display.clearDisplay();
  drawStatusBar();

  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("WIFI MANAGER");

  display.setCursor(5, 22);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Connected: ");
    display.print(WiFi.SSID());
  } else {
    display.print("Disconnected");
  }

  const char* menuItems[] = {"Scan", "Forget", "Back"};
  int startY = 35;
  int itemHeight = 10;

  for (int i = 0; i < 3; i++) {
    int y = startY + (i * itemHeight);
    if (i == menuSelection) {
      if ((millis() / 500) % 2 == 0) {
        display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.drawRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
      }
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(10, y + 1);
    display.print(menuItems[i]);
    display.setTextColor(SSD1306_WHITE);
  }

  display.display();
}
