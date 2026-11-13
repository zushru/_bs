#pragma once

#include <stddef.h>
#include <stdint.h>

namespace att {

// This policy intentionally matches only Arduino-ESP32's whole-default-NVS
// recovery erase. It is not a general NVS erase firewall.
inline bool shouldBlockWholeNvsPartitionErase(bool isNvsDataPartition,
                                               size_t offset,
                                               size_t eraseSize,
                                               size_t partitionSize) {
  return isNvsDataPartition && partitionSize > 0 && offset == 0 &&
         eraseSize == partitionSize;
}

// The application may use NVS only after exactly one pre-setup init call,
// that call succeeded, and no full DATA_NVS erase was intercepted.
inline bool mayUseNvsAfterStartup(uint32_t initCallCount,
                                  bool firstInitSucceeded,
                                  bool wholePartitionEraseBlocked) {
  // Arduino-ESP32 3.3.12 calls nvs_flash_init() once in initArduino(). Any
  // missing or additional pre-setup call makes the captured startup result
  // ambiguous and therefore fails closed.
  return initCallCount == 1 && firstInitSucceeded &&
         !wholePartitionEraseBlocked;
}

}  // namespace att
