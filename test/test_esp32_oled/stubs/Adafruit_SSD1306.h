#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "Wire.h"
#include "oled_test_state.h"

#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_WHITE 1

class Adafruit_SSD1306 {
 public:
  Adafruit_SSD1306(uint8_t width, uint8_t height, TwoWire* bus,
                   int8_t resetPin, uint32_t clkDuring = 400000UL,
                   uint32_t clkAfter = 100000UL)
      : width_(width), height_(height), bus_(bus), resetPin_(resetPin),
        clkDuring_(clkDuring), clkAfter_(clkAfter) {}

  bool begin(uint8_t vcc, uint8_t address, bool reset, bool periphBegin) {
    oled_test::State& s = oled_test::state();
    ++s.beginCalls;
    s.width = width_;
    s.height = height_;
    s.vcc = vcc;
    s.address = address;
    s.reset = reset;
    s.periphBegin = periphBegin;
    s.clkDuring = clkDuring_;
    s.clkAfter = clkAfter_;
    s.bus = bus_;
    (void)resetPin_;
    return s.beginResult;
  }

  void clearDisplay() {
    oled_test::State& s = oled_test::state();
    ++s.clearCalls;
    for (size_t row = 0; row < 4; ++row) {
      s.bufferLines[row][0] = '\0';
    }
    s.cursorX = 0;
    s.cursorY = 0;
  }

  void display() {
    oled_test::State& s = oled_test::state();
    ++s.displayCalls;
    memcpy(s.visibleLines, s.bufferLines, sizeof s.visibleLines);
  }

  void setTextWrap(bool enabled) { oled_test::state().textWrap = enabled; }
  void setTextSize(uint8_t size) { oled_test::state().textSize = size; }
  void setTextColor(uint16_t color) { oled_test::state().textColor = color; }

  void setCursor(int16_t x, int16_t y) {
    oled_test::State& s = oled_test::state();
    s.cursorX = x;
    s.cursorY = y;
  }

  size_t write(uint8_t value) {
    oled_test::State& s = oled_test::state();
    ++s.writeCalls;
    const int row = s.cursorY / 8;
    const int col = s.cursorX / 6;
    if (row >= 0 && row < 4 && col >= 0 && col < 23) {
      size_t length = strlen(s.bufferLines[row]);
      while (length < static_cast<size_t>(col)) {
        s.bufferLines[row][length++] = ' ';
        s.bufferLines[row][length] = '\0';
      }
      s.bufferLines[row][col] = static_cast<char>(value);
      s.bufferLines[row][col + 1] = '\0';
    }
    s.cursorX = static_cast<int16_t>(s.cursorX + 6);
    return 1;
  }

  size_t print(const char* text) {
    if (text == nullptr) return 0;
    size_t count = 0;
    while (*text != '\0') {
      count += write(static_cast<uint8_t>(*text++));
    }
    return count;
  }

 private:
  uint8_t width_;
  uint8_t height_;
  TwoWire* bus_;
  int8_t resetPin_;
  uint32_t clkDuring_;
  uint32_t clkAfter_;
};
