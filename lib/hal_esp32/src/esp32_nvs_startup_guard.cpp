#include "esp32_nvs_startup_guard.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_err.h>
#include <esp_partition.h>
#include <nvs_flash.h>

#include "nvs_startup_guard_policy.h"

namespace {

// Zero-initialized globals are available before Arduino's initArduino(). Keep
// the first result and count every call so earlier/later pre-setup init calls
// cannot silently make the startup snapshot look valid.
uint32_t gNvsInitCallCount = 0;
esp_err_t gFirstNvsInitResult = ESP_FAIL;
bool gWholePartitionEraseBlocked = false;

}  // namespace

namespace att {

Esp32NvsStartupStatus Esp32NvsStartupGuard::status() {
  Esp32NvsStartupStatus result = {};
  result.initCallCount = gNvsInitCallCount;
  result.initObserved = result.initCallCount != 0;
  result.firstInitResult = static_cast<int32_t>(gFirstNvsInitResult);
  result.wholePartitionEraseBlocked = gWholePartitionEraseBlocked;
  result.safeForApplication = mayUseNvsAfterStartup(
      result.initCallCount, result.firstInitResult == ESP_OK,
      result.wholePartitionEraseBlocked);
  return result;
}

}  // namespace att

// GNU ld --wrap preserves the framework's ordinary init path while exposing
// the first result and denying only a full DATA_NVS erase_range request.
extern "C" esp_err_t __real_nvs_flash_init(void);
extern "C" esp_err_t __wrap_nvs_flash_init(void) {
  const esp_err_t result = __real_nvs_flash_init();
  if (gNvsInitCallCount == 0) gFirstNvsInitResult = result;
  ++gNvsInitCallCount;
  return result;
}

extern "C" esp_err_t __real_esp_partition_erase_range(
    const esp_partition_t* partition, size_t offset, size_t size);
extern "C" esp_err_t __wrap_esp_partition_erase_range(
    const esp_partition_t* partition, size_t offset, size_t size) {
  const bool isNvsDataPartition = partition != nullptr &&
      partition->type == ESP_PARTITION_TYPE_DATA &&
      partition->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS;
  const size_t partitionSize = partition == nullptr ? 0 : partition->size;
  if (att::shouldBlockWholeNvsPartitionErase(isNvsDataPartition, offset, size,
                                              partitionSize)) {
    gWholePartitionEraseBlocked = true;
    return ESP_ERR_NOT_ALLOWED;
  }
  return __real_esp_partition_erase_range(partition, offset, size);
}

#endif  // defined(ARDUINO_ARCH_ESP32)
