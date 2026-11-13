#include "esp32_clock_monotonic.h"

#include <esp_timer.h>

namespace att {

bool Esp32ClockMonotonicSource::nowMicros(uint64_t& out) {
  const int64_t elapsedUs = esp_timer_get_time();
  if (elapsedUs < 0) return false;

  out = static_cast<uint64_t>(elapsedUs);
  return true;
}

}  // namespace att
