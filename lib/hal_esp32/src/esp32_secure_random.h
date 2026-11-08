#pragma once

#include "pin_auth_session.h"

namespace att {

// ESP32 hardware-RNG adapter. The caller MUST invoke notifyRfStarted() only
// after esp_wifi_start() has succeeded (or another continuous entropy source
// has been enabled). Invoke notifyRfStopped() when that source is stopped.
// esp_fill_random() alone is not a true-random guarantee before that point.
// This flag is not synchronized: notifications and fill() must run on one
// serialized task unless the target integration adds an appropriate lock.
class Esp32SecureRandom : public ISecureRandomSource {
 public:
  Esp32SecureRandom() : rfStarted_(false) {}

  void notifyRfStarted() { rfStarted_ = true; }
  void notifyRfStopped() { rfStarted_ = false; }
  bool rfStarted() const { return rfStarted_; }

  bool fill(uint8_t* out, size_t length) override;

 private:
  bool rfStarted_;
};

}  // namespace att
