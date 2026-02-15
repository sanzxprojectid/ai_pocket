void showMainMenu() {
  display.clearDisplay();
  drawStatusBar();

  display.setTextSize(1);
  display.setCursor(30, 12);
  display.print("MAIN MENU");

  const char* items[] = {"AI CHAT", "WIFI", "ESP-NOW", "MUSIC", "TRIVIA QUIZ", "VIDEO PLAYER", "SYSTEM"};
  int itemCount = 7;

  int startY = 20;
  int itemHeight = 8;

  for (int i = 0; i < itemCount; i++) {
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
    display.setCursor(10, y);
    display.print(items[i]);

    if (i == 3 && !musicPlayerAvailable) {
      display.setCursor(SCREEN_WIDTH - 10, y);
      display.print("X");
    }

    display.setTextColor(SSD1306_WHITE);
  }

  display.display();
}
