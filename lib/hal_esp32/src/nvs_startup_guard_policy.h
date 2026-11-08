#pragma once

#include <stddef.h>
#include <stdint.h>

namespace att {

// Pure startup policy so its decisions can be tested without ESP-IDF.
struct NvsStartupGuardPolicy {
  bool initObserved;
  int32_t firstInitResult;
  bool fullNvsEraseBlocked;

  void observeInitResult(int32_t result) {
    if (initObserved) return;
    firstInitResult = result;
    initObserved = true;
  }

  bool blockWholeNvsPartitionErase(bool isNvsDataPartition,
                                    size_t offset,
                                    size_t eraseSize,
                                    size_t partitionSize) {
    const bool block = isNvsDataPartition && partitionSize > 0 &&
        offset == 0 && eraseSize > 0 && eraseSize == partitionSize;
    if (block) fullNvsEraseBlocked = true;
    return block;
  }

  bool mayStartApplication(int32_t espOk) const {
    return initObserved && firstInitResult == espOk &&
        !fullNvsEraseBlocked;
  }
};

}  // namespace att
