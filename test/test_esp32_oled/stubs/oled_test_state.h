#pragma once

#include <stdint.h>
#include <stddef.h>

class TwoWire;

namespace oled_test {
struct State {
  bool beginResult;
  unsigned beginCalls;
  uint8_t width;
  uint8_t height;
  uint8_t vcc;
  uint8_t address;
  bool reset;
  bool periphBegin;
  uint32_t clkDuring;
  uint32_t clkAfter;
  TwoWire* bus;
  unsigned clearCalls;
  unsigned displayCalls;
  unsigned writeCalls;
  bool textWrap;
  uint8_t textSize;
  uint16_t textColor;
  int16_t cursorX;
  int16_t cursorY;
  char bufferLines[4][24];
  char visibleLines[4][24];
};

State& state();
void reset();
}
