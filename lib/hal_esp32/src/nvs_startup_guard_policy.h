#pragma once

#include <stddef.h>
#include <stdint.h>

namespace att {

// Always reject an exact DATA_NVS partition erase. If Arduino's first NVS
// initialization failed, also reject every later non-empty DATA_NVS range
// erase. This prevents the framework's recovery erase from being split into
// smaller esp_partition_erase_range() calls. It does not cover raw esp_flash
// APIs, other NVS partition subtypes, or deliberate chunking after a
// successful startup.
inline bool shouldBlockNvsPartitionErase(bool isNvsDataPartition,
                                          bool firstInitFailed,
                                          size_t offset,
                                          size_t eraseSize,
                                          size_t partitionSize) {
  if (!isNvsDataPartition || eraseSize == 0) return false;
  const bool exactWholePartition = partitionSize > 0 && offset == 0 &&
                                   eraseSize == partitionSize;
  return exactWholePartition || firstInitFailed;
}

// The application may use NVS only after exactly one pre-setup init call,
// that call succeeded, and no DATA_NVS erase was intercepted.
inline bool mayUseNvsAfterStartup(uint32_t initCallCount,
                                  bool firstInitSucceeded,
                                  bool nvsEraseBlocked) {
  // Arduino-ESP32 3.3.12 calls nvs_flash_init() once in initArduino(). Any
  // missing or additional pre-setup call makes the captured startup result
  // ambiguous and therefore fails closed.
  return initCallCount == 1 && firstInitSucceeded &&
         !nvsEraseBlocked;
}

}  // namespace att
