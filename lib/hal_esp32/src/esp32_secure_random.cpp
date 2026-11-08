#include "esp32_secure_random.h"
#include <esp_random.h>

namespace att {

bool Esp32SecureRandom::fill(uint8_t* out, size_t length) {
  if (!rfStarted_ || out == nullptr || length == 0) return false;
  // ESP-IDF declares esp_fill_random(void*, size_t) as a void-returning API.
  // Entropy readiness is therefore enforced by the explicit RF lifecycle gate.
  esp_fill_random(out, length);
  return true;
}

}  // namespace att
