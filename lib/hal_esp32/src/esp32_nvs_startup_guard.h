#pragma once

#include <stdint.h>

namespace att {

// Snapshot of the pre-setup Arduino NVS initialization path. Call status() at
// the start of setup(), before constructing or opening any application NVS
// handles. Exactly one init call must have occurred; do not initialize NVS
// from global constructors before initArduino(). This is not an integrity test.
struct Esp32NvsStartupStatus {
  bool initObserved;
  uint32_t initCallCount;
  int32_t firstInitResult;
  bool wholePartitionEraseBlocked;
  bool safeForApplication;
};

// Link this module with --wrap=nvs_flash_init and
// --wrap=esp_partition_erase_range. Arduino-ESP32 invokes initArduino() before
// setup(); a setup-only check cannot prevent its automatic NVS erase fallback.
class Esp32NvsStartupGuard {
 public:
  static Esp32NvsStartupStatus status();
};

}  // namespace att
