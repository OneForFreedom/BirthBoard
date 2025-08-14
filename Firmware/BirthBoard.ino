#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>
#include <time.h>

// ===== OLED CONFIG =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_SDA 20
#define I2C_SCL 21
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== MATRIX CONFIG =====
const uint8_t COLS = 14;
const uint8_t ROWS = 5;

uint8_t colPins[COLS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
uint8_t rowPins[ROWS] = {14, 15, 16, 17, 18}; // row4 to row0

// ===== USB HID Keyboard =====
Adafruit_USBD_HID usb_hid;
uint8_t report[6] = {0}; // HID report buffer (6-key rollover)

// ===== HID Keycodes (USAGE IDs from HID spec) =====
#include "hid_keycodes.h"  // I’ll define this shortly in code

// ===== ISO 60% Keymap =====
// Fill rows from physical layout — adjust to your exact matrix wiring
uint8_t keymap[ROWS][COLS] = {
  // Row4 (bottom) - 8 keys
  {HID_KEY_LEFT_CTRL, HID_KEY_LEFT_GUI, HID_KEY_LEFT_ALT, HID_KEY_SPACE, HID_KEY_SPACE, HID_KEY_SPACE, HID_KEY_SPACE, HID_KEY_RIGHT_ALT, HID_KEY_RIGHT_GUI, HID_KEY_APPLICATION, HID_KEY_LEFT_ARROW, HID_KEY_DOWN_ARROW, HID_KEY_UP_ARROW, HID_KEY_RIGHT_ARROW},

  // Row3 (second from bottom) - 13 keys
  {HID_KEY_LEFT_SHIFT, HID_KEY_Z, HID_KEY_X, HID_KEY_C, HID_KEY_V, HID_KEY_B, HID_KEY_N, HID_KEY_M, HID_KEY_COMMA, HID_KEY_PERIOD, HID_KEY_SLASH, HID_KEY_RIGHT_SHIFT, HID_KEY_ENTER, 0},

  // Row2 - 13 keys
  {HID_KEY_CAPS_LOCK, HID_KEY_A, HID_KEY_S, HID_KEY_D, HID_KEY_F, HID_KEY_G, HID_KEY_H, HID_KEY_J, HID_KEY_K, HID_KEY_L, HID_KEY_SEMICOLON, HID_KEY_APOSTROPHE, HID_KEY_NON_US_HASH, 0},

  // Row1 - 14 keys
  {HID_KEY_TAB, HID_KEY_Q, HID_KEY_W, HID_KEY_E, HID_KEY_R, HID_KEY_T, HID_KEY_Y, HID_KEY_U, HID_KEY_I, HID_KEY_O, HID_KEY_P, HID_KEY_LEFT_BRACKET, HID_KEY_RIGHT_BRACKET, HID_KEY_ENTER},

  // Row0 (top) - 14 keys
  {HID_KEY_ESCAPE, HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_7, HID_KEY_8, HID_KEY_9, HID_KEY_0, HID_KEY_MINUS, HID_KEY_EQUAL, HID_KEY_BACKSPACE}
};

// ===== BIRTHDAY CONFIG =====
const int BDAY_DAY = 23;
const int BDAY_MONTH = 1;
int daysUntilBday = 0;

// ===== BLINK SETTINGS =====
bool showText = true;
unsigned long lastBlink = 0;
const unsigned long BLINK_INTERVAL = 5000;

// ===== Key state =====
bool keyState[ROWS][COLS] = {false};

void setup() {
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) ;
  }
  display.clearDisplay();
  display.display();

  for (uint8_t r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }
  for (uint8_t c = 0; c < COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  usb_hid.begin();
}

void loop() {
  scanMatrix();
  updateCountdown();
  handleBlink();
}

void scanMatrix() {
  for (uint8_t r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW);
    for (uint8_t c = 0; c < COLS; c++) {
      bool pressed = (digitalRead(colPins[c]) == LOW);
      if (pressed != keyState[r][c]) {
        keyState[r][c] = pressed;
        if (pressed) {
          sendKeyPress(keymap[r][c]);
        } else {
          sendKeyRelease(keymap[r][c]);
        }
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }
}

void sendKeyPress(uint8_t keycode) {
  if (!keycode) return;
  for (int i = 0; i < 6; i++) {
    if (report[i] == 0) {
      report[i] = keycode;
      break;
    }
  }
  usb_hid.keyboardReport(0, 0, report);
}

void sendKeyRelease(uint8_t keycode) {
  for (int i = 0; i < 6; i++) {
    if (report[i] == keycode) {
      report[i] = 0;
    }
  }
  usb_hid.keyboardReport(0, 0, report);
}

void updateCountdown() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int year = t->tm_year + 1900;
  struct tm bday = {0};
  bday.tm_year = year - 1900;
  bday.tm_mon = BDAY_MONTH - 1;
  bday.tm_mday = BDAY_DAY;

  time_t bday_time = mktime(&bday);

  if (difftime(bday_time, now) < 0) {
    bday.tm_year++;
    bday_time = mktime(&bday);
  }

  daysUntilBday = (int)(difftime(bday_time, now) / 86400);
}

void handleBlink() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlink >= BLINK_INTERVAL) {
    showText = !showText;
    lastBlink = currentMillis;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (isBirthday()) {
    if (showText) {
      display.setTextSize(2);
      display.setCursor(0, 20);
      display.println(F("HAPPY"));
      display.setCursor(0, 40);
      display.println(F("BIRTHDAY!!"));
    }
  } else {
    if (showText) {
      display.setTextSize(3);
      display.setCursor(0, 20);
      display.printf("%d", daysUntilBday);
    }
  }
  display.display();
}

bool isBirthday() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  return (t->tm_mday == BDAY_DAY && t->tm_mon == (BDAY_MONTH - 1));
}

// ===== Minimal HID Keycode Definitions =====
#define HID_KEY_A              0x04
#define HID_KEY_B              0x05
#define HID_KEY_C              0x06
#define HID_KEY_D              0x07
#define HID_KEY_E              0x08
#define HID_KEY_F              0x09
#define HID_KEY_G              0x0A
#define HID_KEY_H              0x0B
#define HID_KEY_I              0x0C
#define HID_KEY_J              0x0D
#define HID_KEY_K              0x0E
#define HID_KEY_L              0x0F
#define HID_KEY_M              0x10
#define HID_KEY_N              0x11
#define HID_KEY_O              0x12
#define HID_KEY_P              0x13
#define HID_KEY_Q              0x14
#define HID_KEY_R              0x15
#define HID_KEY_S              0x16
#define HID_KEY_T              0x17
#define HID_KEY_U              0x18
#define HID_KEY_V              0x19
#define HID_KEY_W              0x1A
#define HID_KEY_X              0x1B
#define HID_KEY_Y              0x1C
#define HID_KEY_Z              0x1D
#define HID_KEY_1              0x1E
#define HID_KEY_2              0x1F
#define HID_KEY_3              0x20
#define HID_KEY_4              0x21
#define HID_KEY_5              0x22
#define HID_KEY_6              0x23
#define HID_KEY_7              0x24
#define HID_KEY_8              0x25
#define HID_KEY_9              0x26
#define HID_KEY_0              0x27
#define HID_KEY_ENTER          0x28
#define HID_KEY_ESCAPE         0x29
#define HID_KEY_BACKSPACE      0x2A
#define HID_KEY_TAB            0x2B
#define HID_KEY_SPACE          0x2C
#define HID_KEY_MINUS          0x2D
#define HID_KEY_EQUAL          0x2E
#define HID_KEY_LEFT_BRACKET   0x2F
#define HID_KEY_RIGHT_BRACKET  0x30
#define HID_KEY_BACKSLASH      0x31
#define HID_KEY_NON_US_HASH    0x32
#define HID_KEY_SEMICOLON      0x33
#define HID_KEY_APOSTROPHE     0x34
#define HID_KEY_GRAVE          0x35
#define HID_KEY_COMMA          0x36
#define HID_KEY_PERIOD         0x37
#define HID_KEY_SLASH          0x38
#define HID_KEY_CAPS_LOCK      0x39
#define HID_KEY_LEFT_CTRL      0xE0
#define HID_KEY_LEFT_SHIFT     0xE1
#define HID_KEY_LEFT_ALT       0xE2
#define HID_KEY_LEFT_GUI       0xE3
#define HID_KEY_RIGHT_CTRL     0xE4
#define HID_KEY_RIGHT_SHIFT    0xE5
#define HID_KEY_RIGHT_ALT      0xE6
#define HID_KEY_RIGHT_GUI      0xE7
#define HID_KEY_APPLICATION    0x65
#define HID_KEY_LEFT_ARROW     0x50
#define HID_KEY_RIGHT_ARROW    0x4F
#define HID_KEY_UP_ARROW       0x52
#define HID_KEY_DOWN_ARROW     0x51
