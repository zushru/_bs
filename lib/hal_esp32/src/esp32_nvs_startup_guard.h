#pragma once

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_err.h>

namespace att {

struct Esp32NvsStartupStatus {
  bool initObserved;
  esp_err_t firstInitResult;
  bool fullNvsEraseBlocked;
  bool mayStartApplication;
};

// Snapshot of linker-wrapper observations made during Arduino startup.
// The caller must check mayStartApplication before opening storage/services.
Esp32NvsStartupStatus esp32NvsStartupStatus();

}  // namespace att
#endif
