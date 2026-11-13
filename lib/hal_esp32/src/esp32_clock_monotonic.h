#pragma once

#include "clock_confirmation.h"

namespace att {

// ESP-IDF high-resolution timer adapter. The source is measured in
// microseconds since ESP Timer initialization, not UTC or local time.
class Esp32ClockMonotonicSource final : public IClockMonotonicSource {
 public:
  bool nowMicros(uint64_t& out) override;
};

}  // namespace att
