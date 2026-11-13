#pragma once
#include <stddef.h>
#include <stdint.h>
extern uint32_t gTestMillis;
class Stream {
 public:
  virtual ~Stream() {}
  virtual size_t write(const uint8_t* data, size_t length) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
};
inline uint32_t millis() { return gTestMillis; }
