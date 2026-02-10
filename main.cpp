#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_now.h>
#include <DFRobotDFPlayerMini.h>
#include "secrets.h"

// ============ OLED CONFIG ============
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============ I2C PINS ============
#define SDA_PIN 9
#define SCL_PIN 10

// ============ DFPLAYER PINS ============
#define DFPLAYER_RX 20
#define DFPLAYER_TX 21
#define DFPLAYER_BUSY 1

HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;

// ============ BATTERY MONITOR ============
#define BATTERY_PIN 0
#define BATTERY_SAMPLES 10
#define VREF 2500.0
#define ADC_MAX 4095.0
#define VOLTAGE_DIVIDER 2.0

#define BATTERY_MAX 4200
#define BATTERY_MIN 3000
#define BATTERY_CRITICAL 3300

// ============ BUTTON PINS ============
#define BTN_RIGHT  3
#define BTN_LEFT   2
#define BTN_UP     4
#define BTN_DOWN   5
#define BTN_OK     6
#define BTN_BACK   7
#define BTN_ACT    LOW

// ============ LED ============
#define LED_BUILTIN 8

// ============ APP STATE ============
enum AppState {
  STATE_BOOT,
  STATE_MAIN_MENU,
  STATE_WIFI_MENU,
  STATE_WIFI_SCAN,
  STATE_PASSWORD_INPUT,
  STATE_KEYBOARD,
  STATE_CHAT_RESPONSE,
  STATE_LOADING,
  STATE_SYSTEM_INFO,
  STATE_ESPNOW_CHAT,
  STATE_ESPNOW_MENU,
  STATE_ESPNOW_PEER_LIST,
  STATE_MUSIC_PLAYER,
  STATE_MUSIC_PLAYLIST,
  STATE_MUSIC_MENU,
  STATE_MUSIC_EQUALIZER,
  STATE_TRIVIA_MENU,
  STATE_TRIVIA_CATEGORY,
  STATE_TRIVIA_DIFFICULTY,
  STATE_TRIVIA_MODE,
  STATE_TRIVIA_LOADING,
  STATE_TRIVIA_PLAYING,
  STATE_TRIVIA_RESULT,
  STATE_TRIVIA_GAMEOVER,
  STATE_TRIVIA_LEADERBOARD
};

AppState currentState = STATE_BOOT;

// ============ TRIVIA QUIZ VARIABLES ============
struct TriviaQuestion {
  String category;
  String difficulty;
  String question;
  String correctAnswer;
  String answers[4];
  int answerCount;
  String type; // "multiple" or "boolean"
};

struct TriviaScore {
  String name;
  int score;
  int questionsAnswered;
  int correctAnswers;
};

TriviaQuestion currentQuestion;
TriviaScore currentScore;
TriviaScore leaderboard[5];

// Trivia API endpoint
const char* triviaAPI = "https://opentdb.com/api.php";

// Trivia Categories
struct TriviaCategory {
  int id;
  String name;
};

TriviaCategory triviaCategories[] = {
  {9, "General"},
  {17, "Science"},
  {18, "Computer"},
  {19, "Math"},
  {21, "Sports"},
  {22, "Geography"},
  {23, "History"},
  {24, "Politics"},
  {25, "Art"},
  {26, "Celebrity"},
  {27, "Animals"},
  {28, "Vehicles"},
  {12, "Music"},
  {11, "Film"},
  {20, "Mythology"},
  {10, "Books"},
  {13, "Television"},
  {14, "Video Games"},
  {15, "Board Games"},
  {16, "Gadgets"},
  {29, "Comics"},
  {31, "Anime & Manga"},
  {32, "Cartoon"},
  {0, "Random"}
};

int triviaCategoryCount = 24;
int selectedTriviaCategory = 0;
int selectedTriviaDifficulty = 0; // 0=Easy, 1=Medium, 2=Hard
int selectedTriviaMode = 0; // 0=Quick(5), 1=Classic(10), 2=Marathon(20), 3=Endless

String triviaDifficulties[] = {"easy", "medium", "hard"};
String triviaDifficultyNames[] = {"Easy", "Medium", "Hard"};
int triviaModeQuestions[] = {5, 10, 20, 999};
String triviaModeNames[] = {"Quick 5", "Classic 10", "Marathon 20", "Endless"};

int triviaCurrentQuestion = 0;
int triviaTotalQuestions = 5;
int triviaCorrectAnswers = 0;
int triviaWrongAnswers = 0;
int triviaScore = 0;
int triviaStreak = 0;
int triviaMaxStreak = 0;
unsigned long triviaQuestionStartTime = 0;
unsigned long triviaTimeLimit = 30000; // milliseconds
int triviaSelectedAnswer = 0;
bool triviaAnswered = false;

// Timer settings per difficulty
int triviaTimeLimits[] = {30, 20, 15}; // Easy, Medium, Hard in seconds

// ============ BATTERY VARIABLES ============
int batteryPercent = 100;
int batteryVoltage = 4200;
unsigned long lastBatteryUpdate = 0;
bool batteryCharging = false;

// ============ API ENDPOINT ============
const char* geminiEndpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent";

// ============ PREFERENCES ============
Preferences preferences;

// ============ GLOBAL VARIABLES ============
int cursorX = 0, cursorY = 0;
String userInput = "";
String passwordInput = "";
String selectedSSID = "";
String aiResponse = "";
int scrollOffset = 0;
int menuSelection = 0;
unsigned long lastDebounce = 0;
const unsigned long debounceDelay = 200;

// ============ AI MODE ============

// ============ ESP-NOW CHAT ============
#define MAX_ESPNOW_MESSAGES 30
#define MAX_ESPNOW_PEERS 5
#define ESPNOW_MESSAGE_MAX_LEN 100

struct ESPNowMessage {
  char text[ESPNOW_MESSAGE_MAX_LEN];
  uint8_t senderMAC[6];
  bool isFromMe;
};

struct ESPNowPeer {
  uint8_t mac[6];
  String nickname;
  unsigned long lastSeen;
  bool isActive;
};

ESPNowMessage espnowMessages[MAX_ESPNOW_MESSAGES];
int espnowMessageCount = 0;
int espnowScrollOffset = 0;

ESPNowPeer espnowPeers[MAX_ESPNOW_PEERS];
int espnowPeerCount = 0;
int selectedPeer = 0;

bool espnowInitialized = false;
bool espnowBroadcastMode = true;
String myNickname = "ESP32C3";

typedef struct struct_message {
  char type;
  char text[ESPNOW_MESSAGE_MAX_LEN];
  char nickname[20];
  unsigned long timestamp;
} struct_message;

struct_message outgoingMsg;
struct_message incomingMsg;

// ============ MUSIC PLAYER VARIABLES ============
bool musicPlayerAvailable = false;
bool isPlaying = false;
int currentTrack = 1;
int totalTracks = 0;
int currentVolume = 15;
unsigned long lastVolumeChange = 0;
unsigned long lastTrackChange = 0;
String trackNames[50];
int playlistSelection = 0;
bool repeatMode = false;
bool shuffleMode = false;

// Music visualizer
int spectrumBars[8] = {0};
unsigned long lastSpectrumUpdate = 0;
int currentEQ = 0; // 0=Normal, 1=Pop, 2=Rock, 3=Jazz, 4=Classic, 5=Bass

const char* eqNames[] = {"Normal", "Pop", "Rock", "Jazz", "Classic", "Bass"};

// ============ KEYBOARD LAYOUTS ============
const char* keyboardLower[3][10] = {
  {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
  {"a", "s", "d", "f", "g", "h", "j", "k", "l", "<"},
  {"#", "z", "x", "c", "v", "b", "n", "m", " ", "OK"}
};

const char* keyboardUpper[3][10] = {
  {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
  {"A", "S", "D", "F", "G", "H", "J", "K", "L", "<"},
  {"#", "Z", "X", "C", "V", "B", "N", "M", ".", "OK"}
};

const char* keyboardNumbers[3][10] = {
  {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
  {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"},
  {"#", "-", "_", "=", "+", "[", "]", "?", ".", "OK"}
};

enum KeyboardMode { MODE_LOWER, MODE_UPPER, MODE_NUMBERS };
KeyboardMode currentKeyboardMode = MODE_LOWER;

enum KeyboardContext { CONTEXT_CHAT, CONTEXT_WIFI_PASSWORD, CONTEXT_ESPNOW_CHAT, CONTEXT_ESPNOW_NICKNAME };
KeyboardContext keyboardContext = CONTEXT_CHAT;

// ============ WIFI SCANNER ============
struct WiFiNetwork {
  String ssid;
  int rssi;
  bool encrypted;
};
WiFiNetwork networks[15];
int networkCount = 0;
int selectedNetwork = 0;
int wifiPage = 0;
const int wifiPerPage = 4;

// ============ SYSTEM VARIABLES ============
int loadingFrame = 0;
String chatHistory = "";
int chatMessageCount = 0;

// ============ AI PROMPTS ============
const char* AI_SYSTEM_PROMPT_STANDARD = 
  "Kamu adalah AI assistant yang helpful, informative, dan profesional. "
  "Berikan informasi yang akurat dan penjelasan yang jelas.\n\n"
  "TONE: Profesional, informatif, dan membantu.";

// ============ FORWARD DECLARATIONS ============
void changeState(AppState newState);
void drawStatusBar();
void showStatus(String message, int delayMs);
void scanWiFiNetworks();
void sendToGemini();
void handleKeyPress();
void handlePasswordKeyPress();
void refreshCurrentScreen();
bool initESPNow();
void sendESPNowMessage(String message);
String buildPrompt(String currentMessage);
bool initMusicPlayer();
void drawMusicPlayer();
void drawMusicPlaylist();
void drawMusicMenu();
void drawMusicEqualizer();
void playTrack(int track);
void nextTrack();
void previousTrack();
void togglePlayPause();
void toggleShuffle();
void toggleRepeat();
void applyEqualizer();
int readBatteryVoltage();
void updateBatteryStatus();

// Trivia functions
void drawTriviaMenu();
void drawTriviaCategory();
void drawTriviaDifficulty();
void drawTriviaMode();
void drawTriviaPlaying();
void drawTriviaResult();
void drawTriviaGameOver();
void drawTriviaLeaderboard();
void fetchTriviaQuestion();
void checkTriviaAnswer();
void calculateTriviaScore(unsigned long timeElapsed);
String urlDecode(String str);
void saveTriviaScore();
void loadLeaderboard();
void playSoundEffect(int trackNum);

// ============ BATTERY FUNCTIONS ============
int readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(5);
  }
  int avgRaw = sum / BATTERY_SAMPLES;
  float voltage = (avgRaw * VREF / ADC_MAX) * VOLTAGE_DIVIDER;
  return (int)voltage;
}

void updateBatteryStatus() {
  if (millis() - lastBatteryUpdate < 5000) return;
  
  lastBatteryUpdate = millis();
  batteryVoltage = readBatteryVoltage();
  
  if (batteryVoltage >= BATTERY_MAX) {
    batteryPercent = 100;
  } else if (batteryVoltage <= BATTERY_MIN) {
    batteryPercent = 0;
  } else {
    batteryPercent = map(batteryVoltage, BATTERY_MIN, BATTERY_MAX, 0, 100);
  }
  
  batteryPercent = constrain(batteryPercent, 0, 100);
}

void drawBatteryIcon(int x, int y) {
  display.drawRect(x, y, 18, 8, SSD1306_WHITE);
  display.fillRect(x + 18, y + 2, 2, 4, SSD1306_WHITE);
  
  int fillWidth = map(batteryPercent, 0, 100, 0, 16);
  if (fillWidth > 0) {
    display.fillRect(x + 1, y + 1, fillWidth, 6, SSD1306_WHITE);
  }
  
  if (batteryVoltage < BATTERY_CRITICAL && (millis() / 500) % 2 == 0) {
    display.fillRect(x, y, 18, 8, SSD1306_WHITE);
  }
}

// ============ TRIVIA QUIZ FUNCTIONS ============
String urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = str.length();
  
  for (unsigned int i = 0; i < len; i++) {
    char decodedChar;
    if (str[i] == '+') {
      decoded += ' ';
    } else if (str[i] == '%') {
      temp[2] = str[++i];
      temp[3] = str[++i];
      decodedChar = strtol(temp, NULL, 16);
      decoded += decodedChar;
    } else {
      decoded += str[i];
    }
  }
  
  // Decode HTML entities
  decoded.replace("&quot;", "\"");
  decoded.replace("&#039;", "'");
  decoded.replace("&amp;", "&");
  decoded.replace("&lt;", "<");
  decoded.replace("&gt;", ">");
  
  return decoded;
}

void fetchTriviaQuestion() {
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("WiFi Error!", 1500);
    changeState(STATE_TRIVIA_MENU);
    return;
  }
  
  changeState(STATE_TRIVIA_LOADING);
  
  HTTPClient http;
  String url = String(triviaAPI) + "?amount=1";
  
  // Add category if not random
  if (triviaCategories[selectedTriviaCategory].id != 0) {
    url += "&category=" + String(triviaCategories[selectedTriviaCategory].id);
  }
  
  // Add difficulty
  url += "&difficulty=" + triviaDifficulties[selectedTriviaDifficulty];
  
  Serial.println("Fetching: " + url);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonObject result = doc["results"][0];
      
      currentQuestion.category = result["category"].as<String>();
      currentQuestion.difficulty = result["difficulty"].as<String>();
      currentQuestion.type = result["type"].as<String>();
      currentQuestion.question = urlDecode(result["question"].as<String>());
      currentQuestion.correctAnswer = urlDecode(result["correct_answer"].as<String>());
      
      if (currentQuestion.type == "boolean") {
        currentQuestion.answerCount = 2;
        currentQuestion.answers[0] = "True";
        currentQuestion.answers[1] = "False";
      } else {
        JsonArray incorrect = result["incorrect_answers"];
        currentQuestion.answerCount = incorrect.size() + 1;
        
        // Add incorrect answers
        for (int i = 0; i < incorrect.size(); i++) {
          currentQuestion.answers[i] = urlDecode(incorrect[i].as<String>());
        }
        
        // Add correct answer at random position
        int correctPos = random(0, currentQuestion.answerCount);
        for (int i = currentQuestion.answerCount - 1; i > correctPos; i--) {
          currentQuestion.answers[i] = currentQuestion.answers[i - 1];
        }
        currentQuestion.answers[correctPos] = currentQuestion.correctAnswer;
      }
      
      triviaSelectedAnswer = 0;
      triviaAnswered = false;
      triviaQuestionStartTime = millis();
      triviaTimeLimit = triviaTimeLimits[selectedTriviaDifficulty] * 1000;
      
      changeState(STATE_TRIVIA_PLAYING);
      
    } else {
      showStatus("Parse Error!", 1500);
      changeState(STATE_TRIVIA_MENU);
    }
  } else {
    showStatus("API Error!", 1500);
    changeState(STATE_TRIVIA_MENU);
  }
  
  http.end();
}

void checkTriviaAnswer() {
  triviaAnswered = true;
  unsigned long timeElapsed = millis() - triviaQuestionStartTime;
  
  String selectedAnswerText = currentQuestion.answers[triviaSelectedAnswer];
  bool isCorrect = (selectedAnswerText == currentQuestion.correctAnswer);
  
  if (isCorrect) {
    triviaCorrectAnswers++;
    triviaStreak++;
    if (triviaStreak > triviaMaxStreak) {
      triviaMaxStreak = triviaStreak;
    }
    calculateTriviaScore(timeElapsed);
  } else {
    triviaWrongAnswers++;
    triviaStreak = 0;
  }
  
  triviaCurrentQuestion++;
  changeState(STATE_TRIVIA_RESULT);
}

void calculateTriviaScore(unsigned long timeElapsed) {
  // Base score by difficulty
  int baseScore = 0;
  if (selectedTriviaDifficulty == 0) baseScore = 10; // Easy
  else if (selectedTriviaDifficulty == 1) baseScore = 20; // Medium
  else baseScore = 30; // Hard
  
  // Time bonus
  float timePercent = (float)(triviaTimeLimit - timeElapsed) / triviaTimeLimit;
  float timeBonus = 1.0;
  
  if (timePercent > 0.7) timeBonus = 1.5;
  else if (timePercent > 0.5) timeBonus = 1.25;
  else if (timePercent > 0.3) timeBonus = 1.1;
  
  // Streak bonus
  float streakBonus = 1.0;
  if (triviaStreak >= 10) streakBonus = 3.0;
  else if (triviaStreak >= 5) streakBonus = 2.0;
  else if (triviaStreak >= 3) streakBonus = 1.5;
  
  int questionScore = baseScore * timeBonus * streakBonus;
  triviaScore += questionScore;
}

void playSoundEffect(int trackNum) {
  if (musicPlayerAvailable) {
    // Use playFolder(1, trackNum) to isolate SFX from music files.
    // Create folder "01" on SD card: 001.mp3 (Correct), 002.mp3 (Wrong)
    dfPlayer.playFolder(1, trackNum);
    delay(50);
  }
}

void saveTriviaScore() {
  preferences.begin("trivia", false);
  
  // Load existing scores
  for (int i = 0; i < 5; i++) {
    String key = "score" + String(i);
    leaderboard[i].score = preferences.getInt(key.c_str(), 0);
    key = "name" + String(i);
    leaderboard[i].name = preferences.getString(key.c_str(), "---");
    key = "questions" + String(i);
    leaderboard[i].questionsAnswered = preferences.getInt(key.c_str(), 0);
    key = "correct" + String(i);
    leaderboard[i].correctAnswers = preferences.getInt(key.c_str(), 0);
  }
  
  // Check if current score makes it to leaderboard
  for (int i = 0; i < 5; i++) {
    if (triviaScore > leaderboard[i].score) {
      // Shift scores down
      for (int j = 4; j > i; j--) {
        leaderboard[j] = leaderboard[j - 1];
      }
      
      // Insert new score
      leaderboard[i].score = triviaScore;
      leaderboard[i].name = myNickname;
      leaderboard[i].questionsAnswered = triviaCurrentQuestion;
      leaderboard[i].correctAnswers = triviaCorrectAnswers;
      break;
    }
  }
  
  // Save leaderboard
  for (int i = 0; i < 5; i++) {
    String key = "score" + String(i);
    preferences.putInt(key.c_str(), leaderboard[i].score);
    key = "name" + String(i);
    preferences.putString(key.c_str(), leaderboard[i].name);
    key = "questions" + String(i);
    preferences.putInt(key.c_str(), leaderboard[i].questionsAnswered);
    key = "correct" + String(i);
    preferences.putInt(key.c_str(), leaderboard[i].correctAnswers);
  }
  
  preferences.end();
}

void loadLeaderboard() {
  preferences.begin("trivia", true);
  
  for (int i = 0; i < 5; i++) {
    String key = "score" + String(i);
    leaderboard[i].score = preferences.getInt(key.c_str(), 0);
    key = "name" + String(i);
    leaderboard[i].name = preferences.getString(key.c_str(), "---");
    key = "questions" + String(i);
    leaderboard[i].questionsAnswered = preferences.getInt(key.c_str(), 0);
    key = "correct" + String(i);
    leaderboard[i].correctAnswers = preferences.getInt(key.c_str(), 0);
  }
  
  preferences.end();
}

void drawTriviaMenu() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("TRIVIA QUIZ");
  
  const char* menuItems[] = {"Play Game", "Leaderboard", "Back"};
  int startY = 28;
  int itemHeight = 12;
  
  for (int i = 0; i < 3; i++) {
    int y = startY + (i * itemHeight);
    if (i == menuSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(20, y + 2);
    display.print(menuItems[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawTriviaCategory() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("SELECT CATEGORY");
  
  int startY = 22;
  int itemHeight = 8;
  int maxVisible = 5;
  int startIdx = max(0, selectedTriviaCategory - maxVisible + 1);
  
  for (int i = startIdx; i < triviaCategoryCount && i < startIdx + maxVisible; i++) {
    int y = startY + ((i - startIdx) * itemHeight);
    
    if (i == selectedTriviaCategory) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(2, y);
    String catName = triviaCategories[i].name;
    if (catName.length() > 20) catName = catName.substring(0, 20);
    display.print(catName);
    
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawTriviaDifficulty() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(15, 12);
  display.print("SELECT DIFFICULTY");
  
  const char* difficulties[] = {"Easy - 30s", "Medium - 20s", "Hard - 15s"};
  int startY = 28;
  int itemHeight = 12;
  
  for (int i = 0; i < 3; i++) {
    int y = startY + (i * itemHeight);
    if (i == selectedTriviaDifficulty) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(10, y + 2);
    display.print(difficulties[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawTriviaMode() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("SELECT MODE");
  
  int startY = 26;
  int itemHeight = 10;
  
  for (int i = 0; i < 4; i++) {
    int y = startY + (i * itemHeight);
    if (i == selectedTriviaMode) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(15, y + 1);
    display.print(triviaModeNames[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawTriviaPlaying() {
  display.clearDisplay();
  
  // 1. Header (0-7)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Q:"); display.print(triviaCurrentQuestion + 1);
  display.print("/"); display.print(triviaTotalQuestions);
  display.setCursor(50, 0);
  display.print("Sc:"); display.print(triviaScore);
  if (triviaStreak >= 3) {
    display.setCursor(105, 0);
    display.print("x"); display.print(triviaStreak);
  }

  // 2. Timer Bar (8-10)
  unsigned long timeElapsed = millis() - triviaQuestionStartTime;
  unsigned long timeRemaining = (timeElapsed >= triviaTimeLimit) ? 0 : (triviaTimeLimit - timeElapsed);
  
  if (timeRemaining == 0 && !triviaAnswered) {
    triviaAnswered = true;
    triviaWrongAnswers++;
    triviaStreak = 0;
    triviaCurrentQuestion++;
    changeState(STATE_TRIVIA_RESULT);
    return;
  }
  
  int timerW = map(timeRemaining, 0, triviaTimeLimit, 0, SCREEN_WIDTH);
  display.drawRect(0, 8, SCREEN_WIDTH, 3, SSD1306_WHITE);
  display.fillRect(0, 8, timerW, 3, SSD1306_WHITE);
  
  // 3. Question with Horizontal Scroll (12-20)
  display.setTextWrap(false);
  String question = currentQuestion.question;
  int textWidth = question.length() * 6;
  int scrollGap = 40;

  if (textWidth > SCREEN_WIDTH) {
    int scrollOffset = (millis() / 40) % (textWidth + scrollGap);
    display.setCursor(-scrollOffset, 12);
    display.print(question);
    display.setCursor(textWidth + scrollGap - scrollOffset, 12);
    display.print(question);
  } else {
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 12);
    display.print(question);
  }
  display.setTextWrap(true);

  display.drawFastHLine(0, 22, SCREEN_WIDTH, SSD1306_WHITE);

  // 4. Answers - All 4 visible (24-63)
  int answerStartY = 24;
  int answerHeight = 10;
  for (int i = 0; i < currentQuestion.answerCount && i < 4; i++) {
    int ay = answerStartY + (i * answerHeight);
    if (i == triviaSelectedAnswer) {
      display.fillRect(0, ay, SCREEN_WIDTH, answerHeight - 1, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(2, ay + 1);
    display.print(i == 0 ? "A:" : i == 1 ? "B:" : i == 2 ? "C:" : "D:");

    String ans = currentQuestion.answers[i];
    if (ans.length() > 17) ans = ans.substring(0, 17);
    display.setCursor(18, ay + 1);
    display.print(ans);
  }
  
  display.display();
}

void drawTriviaResult() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  
  bool lastAnswerCorrect = (triviaCorrectAnswers > 0 && 
                            triviaCurrentQuestion == triviaCorrectAnswers + triviaWrongAnswers);
  
  if (lastAnswerCorrect) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(40, 15);
    display.print(" CORRECT ");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.setCursor(45, 15);
    display.print("WRONG");
    display.setCursor(5, 28);
    display.print("Correct:");
    String correct = currentQuestion.correctAnswer;
    if (correct.length() > 20) correct = correct.substring(0, 17) + "...";
    display.setCursor(5, 38);
    display.print(correct);
  }
  
  display.setCursor(10, 45);
  display.print("Score: ");
  display.print(triviaScore);
  
  display.setCursor(10, 55);
  display.print("Streak: ");
  display.print(triviaStreak);
  
  display.setCursor(85, 55);
  display.print("Next >");
  
  display.display();
}

void drawTriviaGameOver() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(30, 12);
  display.print("GAME OVER");
  
  display.setCursor(5, 25);
  display.print("Questions: ");
  display.print(triviaCurrentQuestion);
  
  display.setCursor(5, 33);
  display.print("Correct: ");
  display.print(triviaCorrectAnswers);
  display.setCursor(75, 33);
  display.print("Wrong: ");
  display.print(triviaWrongAnswers);
  
  display.setCursor(5, 41);
  display.print("Max Streak: ");
  display.print(triviaMaxStreak);
  
  display.setCursor(5, 49);
  display.setTextSize(1);
  display.print("FINAL SCORE:");
  
  display.setTextSize(2);
  String scoreStr = String(triviaScore);
  int scoreW = scoreStr.length() * 12;
  display.setCursor((SCREEN_WIDTH - scoreW) / 2, 56);
  display.print(triviaScore);
  
  display.display();
}

void drawTriviaLeaderboard() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("LEADERBOARD");
  
  int startY = 22;
  int itemHeight = 8;
  
  for (int i = 0; i < 5; i++) {
    int y = startY + (i * itemHeight);
    
    display.setCursor(2, y);
    display.print(i + 1);
    display.print(".");
    
    String name = leaderboard[i].name;
    if (name.length() > 8) name = name.substring(0, 8);
    display.setCursor(12, y);
    display.print(name);
    
    display.setCursor(SCREEN_WIDTH - 30, y);
    display.print(leaderboard[i].score);
  }
  
  display.display();
}

// ============ MUSIC PLAYER FUNCTIONS ============
bool initMusicPlayer() {
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(1500);
  
  Serial.println("Initializing DFPlayer Mini...");
  
  if (!dfPlayer.begin(dfPlayerSerial, true, true)) {
    Serial.println("✗ DFPlayer Failed!");
    return false;
  }
  
  Serial.println("✓ DFPlayer Detected!");
  
  dfPlayer.setTimeOut(500);
  delay(200);
  
  totalTracks = dfPlayer.readFileCounts();
  Serial.print("  Total Tracks: ");
  Serial.println(totalTracks);
  
  if (totalTracks == 0 || totalTracks == -1 || totalTracks > 255) {
    Serial.println("  ⚠ No MP3 files found!");
    totalTracks = 0;
  }
  
  dfPlayer.volume(currentVolume);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  
  pinMode(DFPLAYER_BUSY, INPUT);
  
  if (totalTracks > 0) {
    preferences.begin("music", true);
    for (int i = 0; i < totalTracks && i < 50; i++) {
      String key = "track" + String(i + 1);
      trackNames[i] = preferences.getString(key.c_str(), "Track " + String(i + 1));
    }
    preferences.end();
  }
  
  Serial.println("✓ DFPlayer Init Complete");
  return true;
}

void playTrack(int track) {
  if (track < 1) track = 1;
  if (track > totalTracks) track = totalTracks;
  
  currentTrack = track;
  dfPlayer.play(currentTrack);
  isPlaying = true;
  lastTrackChange = millis();
  
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
}

void nextTrack() {
  if (millis() - lastTrackChange < 500) return;
  
  if (shuffleMode) {
    currentTrack = random(1, totalTracks + 1);
  } else {
    currentTrack++;
    if (currentTrack > totalTracks) {
      currentTrack = repeatMode ? 1 : totalTracks;
    }
  }
  playTrack(currentTrack);
}

void previousTrack() {
  if (millis() - lastTrackChange < 500) return;
  
  currentTrack--;
  if (currentTrack < 1) {
    currentTrack = repeatMode ? totalTracks : 1;
  }
  playTrack(currentTrack);
}

void togglePlayPause() {
  if (isPlaying) {
    dfPlayer.pause();
    isPlaying = false;
  } else {
    dfPlayer.start();
    isPlaying = true;
  }
}

void toggleShuffle() {
  shuffleMode = !shuffleMode;
  showStatus(shuffleMode ? "Shuffle ON" : "Shuffle OFF", 800);
}

void toggleRepeat() {
  repeatMode = !repeatMode;
  showStatus(repeatMode ? "Repeat ON" : "Repeat OFF", 800);
}

void changeVolume(int delta) {
  if (millis() - lastVolumeChange < 200) return;
  
  currentVolume += delta;
  currentVolume = constrain(currentVolume, 0, 30);
  dfPlayer.volume(currentVolume);
  lastVolumeChange = millis();
  
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
}

void applyEqualizer() {
  switch(currentEQ) {
    case 0:
      dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
      break;
    case 1:
      dfPlayer.EQ(DFPLAYER_EQ_POP);
      break;
    case 2:
      dfPlayer.EQ(DFPLAYER_EQ_ROCK);
      break;
    case 3:
      dfPlayer.EQ(DFPLAYER_EQ_JAZZ);
      break;
    case 4:
      dfPlayer.EQ(DFPLAYER_EQ_CLASSIC);
      break;
    case 5:
      dfPlayer.EQ(DFPLAYER_EQ_BASS);
      break;
  }
  
  showStatus("EQ: " + String(eqNames[currentEQ]), 800);
}

void updateSpectrum() {
  if (millis() - lastSpectrumUpdate < 50) return;
  lastSpectrumUpdate = millis();
  
  if (isPlaying) {
    for (int i = 0; i < 8; i++) {
      int target = random(2, 20);
      if (spectrumBars[i] < target) {
        spectrumBars[i] += 3;
      } else {
        spectrumBars[i] -= 2;
      }
      spectrumBars[i] = constrain(spectrumBars[i], 0, 20);
    }
  } else {
    for (int i = 0; i < 8; i++) {
      if (spectrumBars[i] > 0) spectrumBars[i]--;
    }
  }
}

void drawMusicPlayer() {
  display.clearDisplay();
  drawStatusBar();
  
  if (!musicPlayerAvailable) {
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.print("DFPlayer Mini");
    display.setCursor(15, 35);
    display.print("Not Found!");
    display.display();
    return;
  }
  
  if (totalTracks == 0) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.print("No MP3 Files");
    display.display();
    return;
  }
  
  isPlaying = (digitalRead(DFPLAYER_BUSY) == LOW);
  
  updateSpectrum();
  
  display.setTextSize(1);
  if (isPlaying && (millis() / 500) % 2 == 0) {
    display.setCursor(2, 12);
    display.print("♪");
  }
  display.setCursor(12, 12);
  display.print("NOW PLAYING");
  
  String trackName = trackNames[currentTrack - 1];
  if (trackName.length() > 21) {
    int scrollPos = (millis() / 300) % (trackName.length() + 5);
    trackName = trackName + "     " + trackName;
    trackName = trackName.substring(scrollPos, scrollPos + 21);
  }
  display.setCursor(2, 22);
  display.print(trackName);
  
  int spectrumY = 33;
  int barWidth = 14;
  int barSpacing = 2;
  
  for (int i = 0; i < 8; i++) {
    int x = 2 + (i * (barWidth + barSpacing));
    int h = spectrumBars[i];
    
    for (int j = 0; j < h; j += 3) {
      display.fillRect(x, spectrumY + 20 - j, barWidth, 2, SSD1306_WHITE);
    }
    
    display.drawRect(x, spectrumY, barWidth, 20, SSD1306_WHITE);
  }
  
  display.setCursor(2, 55);
  display.print(currentTrack);
  display.print("/");
  display.print(totalTracks);
  
  if (isPlaying) {
    display.fillTriangle(28, 55, 28, 62, 35, 58, SSD1306_WHITE);
  } else {
    display.fillRect(28, 55, 3, 7, SSD1306_WHITE);
    display.fillRect(32, 55, 3, 7, SSD1306_WHITE);
  }
  
  if (repeatMode) {
    display.setCursor(42, 55);
    display.print("R");
  }
  
  if (shuffleMode) {
    display.setCursor(52, 55);
    display.print("S");
  }
  
  int volW = map(currentVolume, 0, 30, 0, 45);
  display.drawRect(78, 55, 47, 7, SSD1306_WHITE);
  if (volW > 0) {
    display.fillRect(79, 56, volW, 5, SSD1306_WHITE);
  }
  
  display.display();
}

void drawMusicPlaylist() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(30, 12);
  display.print("PLAYLIST");
  
  int startY = 22;
  int itemHeight = 10;
  int maxVisible = 4;
  int startIdx = max(0, playlistSelection - maxVisible + 1);
  
  for (int i = startIdx; i < totalTracks && i < startIdx + maxVisible; i++) {
    int y = startY + ((i - startIdx) * itemHeight);
    
    if (i == playlistSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(2, y + 1);
    display.print(i + 1);
    display.print(".");
    
    String name = trackNames[i];
    if (name.length() > 18) name = name.substring(0, 18);
    display.setCursor(15, y + 1);
    display.print(name);
    
    if (i + 1 == currentTrack) {
      display.setCursor(SCREEN_WIDTH - 10, y + 1);
      display.print(">");
    }
    
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawMusicMenu() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("MUSIC MENU");
  
  const char* menuItems[] = {"Playlist", "Equalizer", "Shuffle", "Repeat", "Back"};
  int startY = 22;
  int itemHeight = 8;
  
  for (int i = 0; i < 5; i++) {
    int y = startY + (i * itemHeight);
    
    if (i == menuSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(5, y);
    display.print(menuItems[i]);
    
    if (i == 2 && shuffleMode) {
      display.setCursor(SCREEN_WIDTH - 15, y);
      display.print("ON");
    }
    if (i == 3 && repeatMode) {
      display.setCursor(SCREEN_WIDTH - 15, y);
      display.print("ON");
    }
    
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

void drawMusicEqualizer() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("EQUALIZER");
  
  int startY = 24;
  int itemHeight = 8;
  
  for (int i = 0; i < 6; i++) {
    int y = startY + (i * itemHeight);
    
    if (i == currentEQ) {
      display.fillRect(5, y, SCREEN_WIDTH - 10, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(15, y);
    display.print(eqNames[i]);
    
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

// ============ ESP-NOW FUNCTIONS ============
void onESPNowDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
  const uint8_t *mac = recv_info->src_addr;
  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  
  String nickname = String(incomingMsg.nickname);
  if (nickname.length() == 0) nickname = "Unknown";
  
  bool peerExists = false;
  for (int i = 0; i < espnowPeerCount; i++) {
    if (memcmp(espnowPeers[i].mac, mac, 6) == 0) {
      espnowPeers[i].lastSeen = millis();
      espnowPeers[i].nickname = nickname;
      espnowPeers[i].isActive = true;
      peerExists = true;
      break;
    }
  }
  
  if (!peerExists && espnowPeerCount < MAX_ESPNOW_PEERS) {
    memcpy(espnowPeers[espnowPeerCount].mac, mac, 6);
    espnowPeers[espnowPeerCount].nickname = nickname;
    espnowPeers[espnowPeerCount].lastSeen = millis();
    espnowPeers[espnowPeerCount].isActive = true;
    espnowPeerCount++;
  }
  
  if (incomingMsg.type == 'M') {
    if (espnowMessageCount < MAX_ESPNOW_MESSAGES) {
      strncpy(espnowMessages[espnowMessageCount].text, incomingMsg.text, ESPNOW_MESSAGE_MAX_LEN - 1);
      memcpy(espnowMessages[espnowMessageCount].senderMAC, mac, 6);
      espnowMessages[espnowMessageCount].isFromMe = false;
      espnowMessageCount++;
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  
  if (currentState == STATE_ESPNOW_CHAT) {
    espnowScrollOffset = espnowMessageCount * 10;
  }
}

void onESPNowDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

bool initESPNow() {
  if (espnowInitialized) return true;
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != ESP_OK) return false;
  
  esp_now_register_recv_cb(onESPNowDataRecv);
  esp_now_register_send_cb(onESPNowDataSent);
  
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
  
  espnowInitialized = true;
  
  outgoingMsg.type = 'H';
  strncpy(outgoingMsg.nickname, myNickname.c_str(), 19);
  outgoingMsg.timestamp = millis();
  esp_now_send((uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));
  
  return true;
}

void sendESPNowMessage(String message) {
  if (!espnowInitialized) {
    if (!initESPNow()) {
      showStatus("ESP-NOW Failed!", 1500);
      return;
    }
  }
  
  outgoingMsg.type = 'M';
  strncpy(outgoingMsg.text, message.c_str(), ESPNOW_MESSAGE_MAX_LEN - 1);
  strncpy(outgoingMsg.nickname, myNickname.c_str(), 19);
  outgoingMsg.timestamp = millis();
  
  esp_err_t result;
  
  if (espnowBroadcastMode) {
    result = esp_now_send((uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));
  } else {
    if (selectedPeer < espnowPeerCount) {
      result = esp_now_send(espnowPeers[selectedPeer].mac, (uint8_t *)&outgoingMsg, sizeof(outgoingMsg));
    } else {
      showStatus("No peer!", 1000);
      return;
    }
  }
  
  if (result == ESP_OK) {
    if (espnowMessageCount < MAX_ESPNOW_MESSAGES) {
      strncpy(espnowMessages[espnowMessageCount].text, message.c_str(), ESPNOW_MESSAGE_MAX_LEN - 1);
      memset(espnowMessages[espnowMessageCount].senderMAC, 0, 6);
      espnowMessages[espnowMessageCount].isFromMe = true;
      espnowMessageCount++;
      espnowScrollOffset = espnowMessageCount * 10;
    }
  }
}

// ============ DISPLAY FUNCTIONS ============
void drawStatusBar() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (musicPlayerAvailable && isPlaying) {
    display.setCursor(0, 0);
    display.print("♪");
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

void showStatus(String message, int delayMs) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  int y = 20;
  int start = 0;
  while (start < message.length()) {
    int end = message.indexOf('\n', start);
    if (end == -1) end = message.length();
    
    String line = message.substring(start, end);
    int x = (SCREEN_WIDTH - (line.length() * 6)) / 2;
    display.setCursor(x, y);
    display.print(line);
    y += 10;
    start = end + 1;
  }
  
  display.display();
  if (delayMs > 0) delay(delayMs);
}

void showProgressBar(String title, int percent) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  int titleW = title.length() * 6;
  display.setCursor((SCREEN_WIDTH - titleW) / 2, 20);
  display.print(title);
  
  int barX = 10;
  int barY = 35;
  int barW = SCREEN_WIDTH - 20;
  int barH = 8;
  
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  
  int fillW = map(percent, 0, 100, 0, barW - 2);
  if (fillW > 0) {
    display.fillRect(barX + 1, barY + 1, fillW, barH - 2, SSD1306_WHITE);
  }
  
  display.setCursor(SCREEN_WIDTH / 2 - 9, barY + 12);
  display.print(percent);
  display.print("%");
  
  display.display();
}

// ============ MAIN MENU ============
void showMainMenu() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(30, 12);
  display.print("MAIN MENU");
  
  const char* items[] = {"AI CHAT", "WIFI", "ESP-NOW", "MUSIC", "TRIVIA QUIZ", "SYSTEM"};
  int itemCount = 6;
  
  int startY = 20;
  int itemHeight = 8;
  
  for (int i = 0; i < itemCount; i++) {
    int y = startY + (i * itemHeight);
    if (i == menuSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
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

// ============ AI MODE SELECTION ============
// ============ WIFI MENU ============
void showWiFiMenu() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(25, 12);
  display.print("WIFI MANAGER");
  
  display.setCursor(5, 22);
  if (WiFi.status() == WL_CONNECTED) {
    String ssid = WiFi.SSID();
    if (ssid.length() > 20) ssid = ssid.substring(0, 20);
    display.print(ssid);
  } else {
    display.print("Not Connected");
  }
  
  const char* menuItems[] = {"Scan", "Forget", "Back"};
  int startY = 35;
  int itemHeight = 10;
  
  for (int i = 0; i < 3; i++) {
    int y = startY + (i * itemHeight);
    if (i == menuSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(10, y + 1);
    display.print(menuItems[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

// ============ WIFI SCAN ============
void displayWiFiNetworks() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("NETWORKS (");
  display.print(networkCount);
  display.print(")");
  
  if (networkCount == 0) {
    display.setCursor(20, 35);
    display.print("No networks");
  } else {
    int startIdx = wifiPage * wifiPerPage;
    int endIdx = min(networkCount, startIdx + wifiPerPage);
    int itemHeight = 12;
    int startY = 22;
    
    for (int i = startIdx; i < endIdx; i++) {
      int y = startY + ((i - startIdx) * itemHeight);
      
      if (i == selectedNetwork) {
        display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      
      display.setCursor(2, y + 2);
      String displaySSID = networks[i].ssid;
      if (displaySSID.length() > 16) displaySSID = displaySSID.substring(0, 16);
      display.print(displaySSID);
      
      if (networks[i].encrypted) {
        display.setCursor(SCREEN_WIDTH - 10, y + 2);
        display.print("L");
      }
      
      display.setTextColor(SSD1306_WHITE);
    }
  }
  
  display.display();
}

// ============ KEYBOARD ============
const char* getCurrentKey() {
  if (currentKeyboardMode == MODE_LOWER) {
    return keyboardLower[cursorY][cursorX];
  } else if (currentKeyboardMode == MODE_UPPER) {
    return keyboardUpper[cursorY][cursorX];
  } else {
    return keyboardNumbers[cursorY][cursorX];
  }
}

void toggleKeyboardMode() {
  if (currentKeyboardMode == MODE_LOWER) {
    currentKeyboardMode = MODE_UPPER;
  } else if (currentKeyboardMode == MODE_UPPER) {
    currentKeyboardMode = MODE_NUMBERS;
  } else {
    currentKeyboardMode = MODE_LOWER;
  }
}

void drawKeyboard() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(0, 12);
  
  String displayText = "";
  if (keyboardContext == CONTEXT_WIFI_PASSWORD) {
    for(unsigned int i=0; i<passwordInput.length(); i++) displayText += "*";
  } else {
    displayText = userInput;
  }
  
  if (displayText.length() > 21) {
    displayText = displayText.substring(displayText.length() - 21);
  }
  display.print(displayText);
  
  int startY = 24;
  int keyW = 12;
  int keyH = 12;
  
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 10; c++) {
      int x = 1 + c * keyW;
      int y = startY + r * keyH;
      
      const char* keyLabel;
      if (currentKeyboardMode == MODE_LOWER) {
        keyLabel = keyboardLower[r][c];
      } else if (currentKeyboardMode == MODE_UPPER) {
        keyLabel = keyboardUpper[r][c];
      } else {
        keyLabel = keyboardNumbers[r][c];
      }
      
      if (r == cursorY && c == cursorX) {
        display.fillRect(x, y, keyW, keyH, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.drawRect(x, y, keyW, keyH, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
      }
      
      int tX = x + 3;
      if(strlen(keyLabel) > 1) tX = x + 1;
      
      display.setCursor(tX, y + 2);
      display.print(keyLabel);
      display.setTextColor(SSD1306_WHITE);
    }
  }
  
  display.display();
}

// ============ CHAT RESPONSE ============
void displayResponse() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(10, 12);
  display.print("AI RESPONSE");
  
  int y = 24 - scrollOffset;
  int lineHeight = 10;
  String word = "";
  int x = 2;
  
  for (unsigned int i = 0; i < aiResponse.length(); i++) {
    char c = aiResponse.charAt(i);
    if (c == ' ' || c == '\n' || i == aiResponse.length() - 1) {
      if (i == aiResponse.length() - 1 && c != ' ' && c != '\n') {
        word += c;
      }
      int wordWidth = word.length() * 6;
      if (x + wordWidth > SCREEN_WIDTH - 2) {
        y += lineHeight;
        x = 2;
      }
      if (y >= 20 && y < SCREEN_HEIGHT) {
        display.setCursor(x, y);
        display.print(word);
      }
      x += wordWidth + 6;
      word = "";
      if (c == '\n') {
        y += lineHeight;
        x = 2;
      }
    } else {
      word += c;
    }
  }
  
  display.display();
}

// ============ LOADING ANIMATION ============
void showLoadingAnimation() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(30, 25);
  display.print("Thinking");
  
  int cx = SCREEN_WIDTH / 2;
  int cy = 45;
  int r = 12;
  
  for (int i = 0; i < 8; i++) {
    float angle = (loadingFrame + i) * (2 * PI / 8);
    int x = cx + cos(angle) * r;
    int y = cy + sin(angle) * r;
    if (i == 0) {
      display.fillCircle(x, y, 2, SSD1306_WHITE);
    } else {
      display.drawCircle(x, y, 1, SSD1306_WHITE);
    }
  }
  
  display.display();
}

// ============ SYSTEM INFO ============
void showSystemInfo() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("SYSTEM INFO");
  
  display.setCursor(0, 22);
  display.print("Temp: ");
  display.print(temperatureRead(), 1);
  display.print("C");
  
  display.setCursor(0, 32);
  display.print("RAM: ");
  display.print(ESP.getFreeHeap() / 1024);
  display.print("KB");
  
  display.setCursor(0, 42);
  display.print("Battery: ");
  display.print(batteryVoltage);
  display.print("mV ");
  display.print(batteryPercent);
  display.print("%");
  
  display.setCursor(0, 52);
  display.print("Chat: ");
  display.print(chatMessageCount);
  display.print(" msgs");
  
  display.display();
}

// ============ ESP-NOW CHAT ============
void drawESPNowChat() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("ESP-NOW CHAT");
  
  int startY = 22;
  int msgHeight = 10;
  int visibleMsgs = (SCREEN_HEIGHT - startY) / msgHeight;
  int startIdx = max(0, espnowMessageCount - visibleMsgs);
  
  int y = startY;
  
  for (int i = startIdx; i < espnowMessageCount; i++) {
    ESPNowMessage &msg = espnowMessages[i];
    
    display.setCursor(2, y);
    if (msg.isFromMe) {
      display.print(">");
    } else {
      display.print("<");
    }
    display.print(msg.text);
    
    y += msgHeight;
  }
  
  display.display();
}

// ============ ESP-NOW MENU ============
void drawESPNowMenu() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("ESP-NOW MENU");
  
  display.setCursor(5, 22);
  display.print("Status: ");
  display.print(espnowInitialized ? "ON" : "OFF");
  display.print(" P:");
  display.print(espnowPeerCount);
  
  const char* menuItems[] = {"Chat", "View Peers", "Nickname", "Back"};
  int startY = 32;
  int itemHeight = 8;
  
  for (int i = 0; i < 4; i++) {
    int y = startY + (i * itemHeight);
    if (i == menuSelection) {
      display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(5, y);
    display.print(menuItems[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  
  display.display();
}

// ============ ESP-NOW PEER LIST ============
void drawESPNowPeerList() {
  display.clearDisplay();
  drawStatusBar();
  
  display.setTextSize(1);
  display.setCursor(20, 12);
  display.print("PEERS (");
  display.print(espnowPeerCount);
  display.print(")");
  
  if (espnowPeerCount == 0) {
    display.setCursor(15, 35);
    display.print("No peers found");
    display.setCursor(5, 50);
    display.print("Wait for messages");
  } else {
    int startY = 24;
    int itemHeight = 10;
    
    for (int i = 0; i < espnowPeerCount && i < 4; i++) {
      int y = startY + (i * itemHeight);
      
      if (i == selectedPeer) {
        display.fillRect(0, y, SCREEN_WIDTH, itemHeight, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      
      display.setCursor(2, y + 1);
      String nickname = espnowPeers[i].nickname;
      if (nickname.length() > 15) nickname = nickname.substring(0, 15);
      display.print(nickname);
      
      display.setTextColor(SSD1306_WHITE);
    }
  }
  
  display.display();
}

// ============ WIFI FUNCTIONS ============
void scanWiFiNetworks() {
  showProgressBar("Scanning", 0);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  showProgressBar("Scanning", 30);
  int n = WiFi.scanNetworks();
  networkCount = min(n, 15);
  
  showProgressBar("Processing", 60);
  for (int i = 0; i < networkCount; i++) {
    networks[i].ssid = WiFi.SSID(i);
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  
  for (int i = 0; i < networkCount - 1; i++) {
    for (int j = i + 1; j < networkCount; j++) {
      if (networks[j].rssi > networks[i].rssi) {
        WiFiNetwork temp = networks[i];
        networks[i] = networks[j];
        networks[j] = temp;
      }
    }
  }
  
  showProgressBar("Complete", 100);
  delay(500);
  selectedNetwork = 0;
  wifiPage = 0;
  menuSelection = 0;
  changeState(STATE_WIFI_SCAN);
}

void connectToWiFi(String ssid, String password) {
  showProgressBar("Connecting", 0);
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    showProgressBar("Connecting", attempts * 5);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    preferences.begin("app-config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    showStatus("Connected!", 1500);
    changeState(STATE_MAIN_MENU);
  } else {
    showStatus("Failed!", 1500);
    changeState(STATE_WIFI_MENU);
  }
}

void forgetNetwork() {
  WiFi.disconnect(true, true);
  preferences.begin("app-config", false);
  preferences.putString("ssid", "");
  preferences.putString("password", "");
  preferences.end();
  showStatus("Forgotten", 1500);
  changeState(STATE_WIFI_MENU);
}

// ============ AI CHAT ============
String buildPrompt(String currentMessage) {
  String prompt = String(AI_SYSTEM_PROMPT_STANDARD);
  prompt += "\n\n";
  
  if (chatHistory.length() > 0) {
    prompt += "HISTORY:\n" + chatHistory + "\n\n";
  }
  
  prompt += "USER: " + currentMessage + "\n\n";
  prompt += "Jawab dengan singkat (maks 200 karakter):";
  
  return prompt;
}

void sendToGemini() {
  currentState = STATE_LOADING;
  loadingFrame = 0;
  
  for (int i = 0; i < 5; i++) {
    showLoadingAnimation();
    delay(100);
    loadingFrame++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    aiResponse = "WiFi not connected";
    currentState = STATE_CHAT_RESPONSE;
    scrollOffset = 0;
    return;
  }
  
  HTTPClient http;
  String url = String(geminiEndpoint) + "?key=" + geminiApiKey1;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);
  
  String enhancedPrompt = buildPrompt(userInput);
  
  String escapedInput = enhancedPrompt;
  escapedInput.replace("\\", "\\\\");
  escapedInput.replace("\"", "\\\"");
  escapedInput.replace("\n", "\\n");
  escapedInput.replace("\r", "");
  
  String jsonPayload = "{\"contents\":[{\"parts\":[{\"text\":\"" + escapedInput + "\"}]}],";
  jsonPayload += "\"generationConfig\":{";
  jsonPayload += "\"temperature\":0.8,";
  jsonPayload += "\"maxOutputTokens\":200";
  jsonPayload += "}}";
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error && !responseDoc["candidates"].isNull()) {
      JsonArray candidates = responseDoc["candidates"];
      if (candidates.size() > 0) {
        JsonObject content = candidates[0]["content"];
        JsonArray parts = content["parts"];
        if (parts.size() > 0) {
          aiResponse = parts[0]["text"].as<String>();
          aiResponse.trim();
          
          String historyEntry = "U:" + userInput + "\nA:" + aiResponse + "\n";
          if (chatHistory.length() + historyEntry.length() > 1000) {
            int cutPos = chatHistory.indexOf("\n", 200);
            if (cutPos != -1) {
              chatHistory = chatHistory.substring(cutPos + 1);
            }
          }
          chatHistory += historyEntry;
          chatMessageCount++;
        } else {
          aiResponse = "No response";
        }
      } else {
        aiResponse = "Error response";
      }
    } else {
      aiResponse = "Parse error";
    }
  } else {
    aiResponse = "HTTP Error: " + String(httpResponseCode);
  }
  
  http.end();
  currentState = STATE_CHAT_RESPONSE;
  scrollOffset = 0;
}

// ============ STATE CHANGE ============
void changeState(AppState newState) {
  currentState = newState;
}

// ============ MENU HANDLERS ============
void handleMainMenuSelect() {
  if (menuSelection == 0) {
    // AI CHAT
    if (WiFi.status() == WL_CONNECTED) {
      userInput = "";
      keyboardContext = CONTEXT_CHAT;
      cursorX = 0;
      cursorY = 0;
      currentKeyboardMode = MODE_LOWER;
      changeState(STATE_KEYBOARD);
    } else {
      showStatus("WiFi OFF!", 1500);
    }
  } else if (menuSelection == 1) {
    // WIFI
    menuSelection = 0;
    changeState(STATE_WIFI_MENU);
  } else if (menuSelection == 2) {
    // ESP-NOW
    menuSelection = 0;
    if (!espnowInitialized) {
      if (initESPNow()) {
        showStatus("ESP-NOW ON!", 1000);
      } else {
        showStatus("ESP-NOW FAIL!", 1500);
        return;
      }
    }
    changeState(STATE_ESPNOW_MENU);
  } else if (menuSelection == 3) {
    // MUSIC
    changeState(STATE_MUSIC_PLAYER);
  } else if (menuSelection == 4) {
    // TRIVIA QUIZ
    if (WiFi.status() == WL_CONNECTED) {
      menuSelection = 0;
      loadLeaderboard();
      changeState(STATE_TRIVIA_MENU);
    } else {
      showStatus("Need WiFi!", 1500);
    }
  } else if (menuSelection == 5) {
    // SYSTEM
    changeState(STATE_SYSTEM_INFO);
  }
}

void handleWiFiMenuSelect() {
  switch(menuSelection) {
    case 0:
      menuSelection = 0;
      scanWiFiNetworks();
      break;
    case 1:
      forgetNetwork();
      menuSelection = 0;
      break;
    case 2:
      menuSelection = 0;
      changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleESPNowMenuSelect() {
  switch(menuSelection) {
    case 0:
      changeState(STATE_ESPNOW_CHAT);
      break;
    case 1:
      selectedPeer = 0;
      changeState(STATE_ESPNOW_PEER_LIST);
      break;
    case 2:
      userInput = myNickname;
      keyboardContext = CONTEXT_ESPNOW_NICKNAME;
      cursorX = 0;
      cursorY = 0;
      currentKeyboardMode = MODE_LOWER;
      changeState(STATE_KEYBOARD);
      break;
    case 3:
      menuSelection = 0;
      changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleMusicMenuSelect() {
  switch(menuSelection) {
    case 0: // Playlist
      playlistSelection = currentTrack - 1;
      changeState(STATE_MUSIC_PLAYLIST);
      break;
    case 1: // Equalizer
      changeState(STATE_MUSIC_EQUALIZER);
      break;
    case 2: // Shuffle
      toggleShuffle();
      break;
    case 3: // Repeat
      toggleRepeat();
      break;
    case 4: // Back
      menuSelection = 0;
      changeState(STATE_MUSIC_PLAYER);
      break;
  }
}

void handleTriviaMenuSelect() {
  switch(menuSelection) {
    case 0: // Play Game
      selectedTriviaCategory = 0;
      changeState(STATE_TRIVIA_CATEGORY);
      break;
    case 1: // Leaderboard
      changeState(STATE_TRIVIA_LEADERBOARD);
      break;
    case 2: // Back
      menuSelection = 0;
      changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleKeyPress() {
  const char* key = getCurrentKey();
  if (strcmp(key, "OK") == 0) {
    if (keyboardContext == CONTEXT_CHAT) {
      if (userInput.length() > 0) {
        sendToGemini();
      }
    } else if (keyboardContext == CONTEXT_ESPNOW_CHAT) {
      if (userInput.length() > 0) {
        sendESPNowMessage(userInput);
        userInput = "";
        changeState(STATE_ESPNOW_CHAT);
      }
    } else if (keyboardContext == CONTEXT_ESPNOW_NICKNAME) {
      if (userInput.length() > 0) {
        myNickname = userInput;
        preferences.begin("app-config", false);
        preferences.putString("espnow_nick", myNickname);
        preferences.end();
        showStatus("Saved!", 1000);
        changeState(STATE_ESPNOW_MENU);
      }
    }
  } else if (strcmp(key, "<") == 0) {
    if (userInput.length() > 0) {
      userInput.remove(userInput.length() - 1);
    }
  } else if (strcmp(key, "#") == 0) {
    toggleKeyboardMode();
  } else {
    if (userInput.length() < 100) {
      userInput += key;
    }
  }
}

void handlePasswordKeyPress() {
  const char* key = getCurrentKey();
  if (strcmp(key, "OK") == 0) {
    connectToWiFi(selectedSSID, passwordInput);
  } else if (strcmp(key, "<") == 0) {
    if (passwordInput.length() > 0) {
      passwordInput.remove(passwordInput.length() - 1);
    }
  } else if (strcmp(key, "#") == 0) {
    toggleKeyboardMode();
  } else {
    passwordInput += key;
  }
}

// ============ REFRESH SCREEN ============
void refreshCurrentScreen() {
  switch(currentState) {
    case STATE_MAIN_MENU:
      showMainMenu();
      break;
    case STATE_WIFI_MENU:
      showWiFiMenu();
      break;
    case STATE_WIFI_SCAN:
      displayWiFiNetworks();
      break;
    case STATE_KEYBOARD:
      drawKeyboard();
      break;
    case STATE_PASSWORD_INPUT:
      drawKeyboard();
      break;
    case STATE_CHAT_RESPONSE:
      displayResponse();
      break;
    case STATE_LOADING:
      showLoadingAnimation();
      break;
    case STATE_SYSTEM_INFO:
      showSystemInfo();
      break;
    case STATE_ESPNOW_CHAT:
      drawESPNowChat();
      break;
    case STATE_ESPNOW_MENU:
      drawESPNowMenu();
      break;
    case STATE_ESPNOW_PEER_LIST:
      drawESPNowPeerList();
      break;
    case STATE_MUSIC_PLAYER:
      drawMusicPlayer();
      break;
    case STATE_MUSIC_PLAYLIST:
      drawMusicPlaylist();
      break;
    case STATE_MUSIC_MENU:
      drawMusicMenu();
      break;
    case STATE_MUSIC_EQUALIZER:
      drawMusicEqualizer();
      break;
    case STATE_TRIVIA_MENU:
      drawTriviaMenu();
      break;
    case STATE_TRIVIA_CATEGORY:
      drawTriviaCategory();
      break;
    case STATE_TRIVIA_DIFFICULTY:
      drawTriviaDifficulty();
      break;
    case STATE_TRIVIA_MODE:
      drawTriviaMode();
      break;
    case STATE_TRIVIA_PLAYING:
      drawTriviaPlaying();
      break;
    case STATE_TRIVIA_RESULT:
      drawTriviaResult();
      break;
    case STATE_TRIVIA_GAMEOVER:
      drawTriviaGameOver();
      break;
    case STATE_TRIVIA_LEADERBOARD:
      drawTriviaLeaderboard();
      break;
    default:
      showMainMenu();
      break;
  }
}

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32-C3 AI POCKET ===");
  Serial.println("With Trivia Quiz!");
  Serial.println("==========================\n");
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  Serial.println("✓ Buttons Init");
  
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);
  updateBatteryStatus();
  Serial.println("✓ Battery Monitor Init");
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("✓ I2C Init (SDA:9, SCL:10)");
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("✗ OLED FAILED!");
    for(;;);
  }
  Serial.println("✓ OLED Init");
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("AI POCKET");
  display.setTextSize(1);
  display.setCursor(15, 30);
  display.println("ESP32-C3");
  display.setCursor(10, 45);
  display.println("Trivia Ready!");
  display.display();
  delay(2000);
  
  Serial.println("\n--- Initializing DFPlayer ---");
  musicPlayerAvailable = initMusicPlayer();
  if (musicPlayerAvailable) {
    showStatus("Music Ready!\n" + String(totalTracks) + " tracks", 1500);
  } else {
    showStatus("DFPlayer Failed\nContinuing...", 1000);
  }
  
  preferences.begin("app-config", true);
  myNickname = preferences.getString("espnow_nick", "ESP32C3");
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();
  Serial.println("✓ Preferences loaded");
  
  if (savedSSID.length() > 0) {
    Serial.print("Connecting to: ");
    Serial.println(savedSSID);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✓ WiFi Connected");
    } else
{
      Serial.println("\n✗ WiFi Failed");
    }
  }
  
  currentState = STATE_MAIN_MENU;
  menuSelection = 0;
  showMainMenu();
  
  Serial.println("\n=== SETUP COMPLETE ===");
  Serial.print("Battery: ");
  Serial.print(batteryVoltage);
  Serial.print("mV (");
  Serial.print(batteryPercent);
  Serial.println("%)");
  Serial.print("DFPlayer: ");
  Serial.println(musicPlayerAvailable ? "OK" : "FAIL");
  Serial.println("Trivia Quiz Ready!");
  Serial.println("======================\n");
}

// ============ LOOP ============
void loop() {
  unsigned long currentMillis = millis();
  
  updateBatteryStatus();

  // Unified Screen Refresh logic for all states
  static unsigned long lastScreenUpdate = 0;
  int refreshInterval = 100; // Default: 10 FPS
  
  if (currentState == STATE_MUSIC_PLAYER || currentState == STATE_TRIVIA_PLAYING) {
    refreshInterval = 50; // 20 FPS for visualizer/scrolling
  } else if (currentState == STATE_LOADING) {
    refreshInterval = 150; // Slower for loading animation
  }
  
  if (currentMillis - lastScreenUpdate > refreshInterval) {
    lastScreenUpdate = currentMillis;
    if (currentState == STATE_LOADING) {
      loadingFrame = (loadingFrame + 1) % 8;
    }
    refreshCurrentScreen();
  }
  
  if (currentMillis - lastDebounce > debounceDelay) {
    bool buttonPressed = false;
    
    if (digitalRead(BTN_UP) == BTN_ACT) {
      switch(currentState) {
        case STATE_MAIN_MENU:
          if (menuSelection > 0) menuSelection--;
          break;
        case STATE_WIFI_MENU:
          if (menuSelection > 0) menuSelection--;
          break;
        case STATE_ESPNOW_MENU:
          if (menuSelection > 0) menuSelection--;
          break;
        case STATE_ESPNOW_PEER_LIST:
          if (selectedPeer > 0) selectedPeer--;
          break;
        case STATE_WIFI_SCAN:
          if (selectedNetwork > 0) {
            selectedNetwork--;
            if (selectedNetwork < wifiPage * wifiPerPage) wifiPage--;
          }
          break;
        case STATE_KEYBOARD:
        case STATE_PASSWORD_INPUT:
          cursorY--;
          if (cursorY < 0) cursorY = 2;
          break;
        case STATE_CHAT_RESPONSE:
          if (scrollOffset > 0) scrollOffset -= 10;
          break;
        case STATE_ESPNOW_CHAT:
          if (espnowScrollOffset > 0) espnowScrollOffset -= 10;
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable) {
            changeVolume(1);
          }
          break;
        case STATE_MUSIC_PLAYLIST:
          if (playlistSelection > 0) playlistSelection--;
          break;
        case STATE_MUSIC_MENU:
          if (menuSelection > 0) menuSelection--;
          break;
        case STATE_MUSIC_EQUALIZER:
          if (currentEQ > 0) {
            currentEQ--;
            applyEqualizer();
          }
          break;
        case STATE_TRIVIA_MENU:
          if (menuSelection > 0) menuSelection--;
          break;
        case STATE_TRIVIA_CATEGORY:
          if (selectedTriviaCategory > 0) selectedTriviaCategory--;
          break;
        case STATE_TRIVIA_DIFFICULTY:
          if (selectedTriviaDifficulty > 0) selectedTriviaDifficulty--;
          break;
        case STATE_TRIVIA_MODE:
          if (selectedTriviaMode > 0) selectedTriviaMode--;
          break;
        case STATE_TRIVIA_PLAYING:
          if (triviaSelectedAnswer > 0) triviaSelectedAnswer--;
          break;
        default: break;
      }
      buttonPressed = true;
    }
    
    if (digitalRead(BTN_DOWN) == BTN_ACT) {
      switch(currentState) {
        case STATE_MAIN_MENU:
          if (menuSelection < 5) menuSelection++;
          break;
        case STATE_WIFI_MENU:
          if (menuSelection < 2) menuSelection++;
          break;
        case STATE_ESPNOW_MENU:
          if (menuSelection < 3) menuSelection++;
          break;
        case STATE_ESPNOW_PEER_LIST:
          if (selectedPeer < espnowPeerCount - 1) selectedPeer++;
          break;
        case STATE_WIFI_SCAN:
          if (selectedNetwork < networkCount - 1) {
            selectedNetwork++;
            if (selectedNetwork >= (wifiPage + 1) * wifiPerPage) wifiPage++;
          }
          break;
        case STATE_KEYBOARD:
        case STATE_PASSWORD_INPUT:
          cursorY++;
          if (cursorY > 2) cursorY = 0;
          break;
        case STATE_CHAT_RESPONSE:
          scrollOffset += 10;
          break;
        case STATE_ESPNOW_CHAT:
          espnowScrollOffset += 10;
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable) {
            changeVolume(-1);
          }
          break;
        case STATE_MUSIC_PLAYLIST:
          if (playlistSelection < totalTracks - 1) playlistSelection++;
          break;
        case STATE_MUSIC_MENU:
          if (menuSelection < 4) menuSelection++;
          break;
        case STATE_MUSIC_EQUALIZER:
          if (currentEQ < 5) {
            currentEQ++;
            applyEqualizer();
          }
          break;
        case STATE_TRIVIA_MENU:
          if (menuSelection < 2) menuSelection++;
          break;
        case STATE_TRIVIA_CATEGORY:
          if (selectedTriviaCategory < triviaCategoryCount - 1) selectedTriviaCategory++;
          break;
        case STATE_TRIVIA_DIFFICULTY:
          if (selectedTriviaDifficulty < 2) selectedTriviaDifficulty++;
          break;
        case STATE_TRIVIA_MODE:
          if (selectedTriviaMode < 3) selectedTriviaMode++;
          break;
        case STATE_TRIVIA_PLAYING:
          if (triviaSelectedAnswer < currentQuestion.answerCount - 1) triviaSelectedAnswer++;
          break;
        default: break;
      }
      buttonPressed = true;
    }
    
    if (digitalRead(BTN_LEFT) == BTN_ACT) {
      switch(currentState) {
        case STATE_KEYBOARD:
        case STATE_PASSWORD_INPUT:
          cursorX--;
          if (cursorX < 0) cursorX = 9;
          break;
        case STATE_ESPNOW_CHAT:
          espnowBroadcastMode = !espnowBroadcastMode;
          showStatus(espnowBroadcastMode ? "Broadcast" : "Direct", 500);
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable && totalTracks > 0) {
            previousTrack();
          }
          break;
        default: break;
      }
      buttonPressed = true;
    }
    
    if (digitalRead(BTN_RIGHT) == BTN_ACT) {
      switch(currentState) {
        case STATE_KEYBOARD:
        case STATE_PASSWORD_INPUT:
          cursorX++;
          if (cursorX > 9) cursorX = 0;
          break;
        case STATE_ESPNOW_CHAT:
          espnowBroadcastMode = !espnowBroadcastMode;
          showStatus(espnowBroadcastMode ? "Broadcast" : "Direct", 500);
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable && totalTracks > 0) {
            nextTrack();
          }
          break;
        case STATE_TRIVIA_RESULT:
          if (triviaCurrentQuestion >= triviaTotalQuestions) {
            saveTriviaScore();
            changeState(STATE_TRIVIA_GAMEOVER);
          } else if (selectedTriviaMode == 3 && triviaStreak == 0 && triviaWrongAnswers > 0) {
            saveTriviaScore();
            changeState(STATE_TRIVIA_GAMEOVER);
          } else {
            fetchTriviaQuestion();
          }
          break;
        default: break;
      }
      buttonPressed = true;
    }
    
    if (digitalRead(BTN_OK) == BTN_ACT) {
      switch(currentState) {
        case STATE_MAIN_MENU:
          handleMainMenuSelect();
          break;
        case STATE_WIFI_MENU:
          handleWiFiMenuSelect();
          break;
        case STATE_ESPNOW_MENU:
          handleESPNowMenuSelect();
          break;
        case STATE_WIFI_SCAN:
          if (networkCount > 0) {
            selectedSSID = networks[selectedNetwork].ssid;
            if (networks[selectedNetwork].encrypted) {
              passwordInput = "";
              keyboardContext = CONTEXT_WIFI_PASSWORD;
              cursorX = 0;
              cursorY = 0;
              changeState(STATE_PASSWORD_INPUT);
            } else {
              connectToWiFi(selectedSSID, "");
            }
          }
          break;
        case STATE_ESPNOW_CHAT:
          userInput = "";
          keyboardContext = CONTEXT_ESPNOW_CHAT;
          cursorX = 0;
          cursorY = 0;
          currentKeyboardMode = MODE_LOWER;
          changeState(STATE_KEYBOARD);
          break;
        case STATE_ESPNOW_PEER_LIST:
          if (espnowPeerCount > 0) {
            espnowBroadcastMode = false;
            showStatus("Direct mode!", 800);
            changeState(STATE_ESPNOW_CHAT);
          }
          break;
        case STATE_KEYBOARD:
          handleKeyPress();
          break;
        case STATE_PASSWORD_INPUT:
          handlePasswordKeyPress();
          break;
        case STATE_SYSTEM_INFO:
          chatHistory = "";
          chatMessageCount = 0;
          showStatus("Chat cleared!", 1000);
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable && totalTracks > 0) {
            menuSelection = 0;
            changeState(STATE_MUSIC_MENU);
          }
          break;
        case STATE_MUSIC_PLAYLIST:
          playTrack(playlistSelection + 1);
          changeState(STATE_MUSIC_PLAYER);
          break;
        case STATE_MUSIC_MENU:
          handleMusicMenuSelect();
          break;
        case STATE_MUSIC_EQUALIZER:
          changeState(STATE_MUSIC_MENU);
          break;
        case STATE_TRIVIA_MENU:
          handleTriviaMenuSelect();
          break;
        case STATE_TRIVIA_CATEGORY:
          selectedTriviaDifficulty = 0;
          changeState(STATE_TRIVIA_DIFFICULTY);
          break;
        case STATE_TRIVIA_DIFFICULTY:
          selectedTriviaMode = 0;
          changeState(STATE_TRIVIA_MODE);
          break;
        case STATE_TRIVIA_MODE:
          // Stop any background music before starting quiz
          if (musicPlayerAvailable) {
            dfPlayer.stop();
            isPlaying = false;
          }
          // Initialize game
          triviaCurrentQuestion = 0;
          triviaTotalQuestions = triviaModeQuestions[selectedTriviaMode];
          triviaCorrectAnswers = 0;
          triviaWrongAnswers = 0;
          triviaScore = 0;
          triviaStreak = 0;
          triviaMaxStreak = 0;
          fetchTriviaQuestion();
          break;
        case STATE_TRIVIA_PLAYING:
          if (!triviaAnswered) {
            checkTriviaAnswer();
          }
          break;
        case STATE_TRIVIA_GAMEOVER:
          menuSelection = 0;
          changeState(STATE_TRIVIA_MENU);
          break;
        case STATE_TRIVIA_LEADERBOARD:
          menuSelection = 0;
          changeState(STATE_TRIVIA_MENU);
          break;
        default: break;
      }
      buttonPressed = true;
    }
    
    if (digitalRead(BTN_BACK) == BTN_ACT) {
      switch(currentState) {
        case STATE_PASSWORD_INPUT:
          changeState(STATE_WIFI_SCAN);
          break;
        case STATE_WIFI_SCAN:
          menuSelection = 0;
          changeState(STATE_WIFI_MENU);
          break;
        case STATE_WIFI_MENU:
        case STATE_SYSTEM_INFO:
          menuSelection = 0;
          changeState(STATE_MAIN_MENU);
          break;
        case STATE_ESPNOW_MENU:
          menuSelection = 0;
          changeState(STATE_MAIN_MENU);
          break;
        case STATE_ESPNOW_PEER_LIST:
          changeState(STATE_ESPNOW_MENU);
          break;
        case STATE_ESPNOW_CHAT:
          changeState(STATE_ESPNOW_MENU);
          break;
        case STATE_CHAT_RESPONSE:
          changeState(STATE_KEYBOARD);
          break;
        case STATE_KEYBOARD:
          if (keyboardContext == CONTEXT_CHAT) {
            menuSelection = 0;
            changeState(STATE_MAIN_MENU);
          } else if (keyboardContext == CONTEXT_ESPNOW_CHAT) {
            changeState(STATE_ESPNOW_CHAT);
          } else if (keyboardContext == CONTEXT_ESPNOW_NICKNAME) {
            changeState(STATE_ESPNOW_MENU);
          } else {
            changeState(STATE_WIFI_SCAN);
          }
          break;
        case STATE_MUSIC_PLAYER:
          if (musicPlayerAvailable && totalTracks > 0) {
            togglePlayPause();
          } else {
            menuSelection = 0;
            changeState(STATE_MAIN_MENU);
          }
          break;
        case STATE_MUSIC_PLAYLIST:
          changeState(STATE_MUSIC_MENU);
          break;
        case STATE_MUSIC_MENU:
          changeState(STATE_MUSIC_PLAYER);
          break;
        case STATE_MUSIC_EQUALIZER:
          changeState(STATE_MUSIC_MENU);
          break;
        case STATE_TRIVIA_MENU:
          menuSelection = 0;
          changeState(STATE_MAIN_MENU);
          break;
        case STATE_TRIVIA_CATEGORY:
          menuSelection = 0;
          changeState(STATE_TRIVIA_MENU);
          break;
        case STATE_TRIVIA_DIFFICULTY:
          changeState(STATE_TRIVIA_CATEGORY);
          break;
        case STATE_TRIVIA_MODE:
          changeState(STATE_TRIVIA_DIFFICULTY);
          break;
        case STATE_TRIVIA_PLAYING:
          // Skip question on back button
          triviaWrongAnswers++;
          triviaStreak = 0;
          triviaCurrentQuestion++;
          if (triviaCurrentQuestion >= triviaTotalQuestions) {
            saveTriviaScore();
            changeState(STATE_TRIVIA_GAMEOVER);
          } else {
            fetchTriviaQuestion();
          }
          break;
        case STATE_TRIVIA_GAMEOVER:
        case STATE_TRIVIA_LEADERBOARD:
          menuSelection = 0;
          changeState(STATE_TRIVIA_MENU);
          break;
        default:
          menuSelection = 0;
          changeState(STATE_MAIN_MENU);
          break;
      }
      buttonPressed = true;
    }
    
    if (buttonPressed) {
      lastDebounce = currentMillis;
      digitalWrite(LED_BUILTIN, HIGH);
      delay(30);
      digitalWrite(LED_BUILTIN, LOW);
      refreshCurrentScreen();
      lastScreenUpdate = millis();
    }
  }
}
