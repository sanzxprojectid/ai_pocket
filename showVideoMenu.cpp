void showVideoMenu() {
  display.clearDisplay();
  drawStatusBar();

  display.setTextSize(1);
  display.setCursor(30, 12);
  display.print("VIDEO PLAYER");

  int startY = 25;
  int itemHeight = 12;
  const char* videoNames[] = {"Animation 1", "Animation 2"};

  for (int i = 0; i < 2; i++) {
    int y = startY + (i * itemHeight);
    if (i == videoSelection) {
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
    display.setCursor(10, y + 2);
    display.print(videoNames[i]);
  }

  display.display();
}
