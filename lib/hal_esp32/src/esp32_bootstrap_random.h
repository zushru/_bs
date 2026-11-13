#pragma once

#include "pin_auth_session.h"

namespace att {

// One-shot pre-radio hardware RNG adapter for first-provision AP credentials.
// Each fill temporarily enables the ESP-IDF SAR-ADC entropy source, reads the
// RNG, then disables that source before ADC or Wi-Fi may be used.
//
// CRITICAL: all calls must be serialized during early boot, before Wi-Fi /
// Bluetooth / ADC / ESP32 I2S use. Espressif explicitly says this source is not
// safe while another subsystem accesses RF or ADC. The function API is void;
// this class can validate arguments but cannot report a hardware entropy fault.
class Esp32BootstrapRandom : public ISecureRandomSource {
 public:
  bool fill(uint8_t* out, size_t length) override;
};

}  // namespace att
