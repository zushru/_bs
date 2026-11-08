#if defined(ARDUINO_ARCH_ESP32)
#include "esp32_nvs_startup_guard.h"

#include <esp_partition.h>
#include <nvs_flash.h>

#include "nvs_startup_guard_policy.h"

namespace {

// Constant-initialized POD: wrappers may run before sketch setup().
att::NvsStartupGuardPolicy gNvsStartupPolicy = {
    false, ESP_FAIL, false};

}  // namespace

namespace att {

Esp32NvsStartupStatus esp32NvsStartupStatus() {
  Esp32NvsStartupStatus status;
  status.initObserved = gNvsStartupPolicy.initObserved;
  status.firstInitResult =
      static_cast<esp_err_t>(gNvsStartupPolicy.firstInitResult);
  status.fullNvsEraseBlocked = gNvsStartupPolicy.fullNvsEraseBlocked;
  status.mayStartApplication =
      gNvsStartupPolicy.mayStartApplication(ESP_OK);
  return status;
}

}  // namespace att

// Redirect Arduino-ESP32 startup and direct calls through these wrappers.
extern "C" esp_err_t __real_nvs_flash_init(void);

extern "C" esp_err_t __wrap_nvs_flash_init(void) {
  const esp_err_t result = __real_nvs_flash_init();
  gNvsStartupPolicy.observeInitResult(static_cast<int32_t>(result));
  return result;
}

extern "C" esp_err_t __real_esp_partition_erase_range(
    const esp_partition_t* partition, size_t offset, size_t size);

extern "C" esp_err_t __wrap_esp_partition_erase_range(
    const esp_partition_t* partition, size_t offset, size_t size) {
  const bool isNvsDataPartition = partition != nullptr &&
      partition->type == ESP_PARTITION_TYPE_DATA &&
      partition->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS;
  if (gNvsStartupPolicy.blockWholeNvsPartitionErase(
          isNvsDataPartition, offset, size,
          partition == nullptr ? 0 : partition->size)) {
    return ESP_ERR_NOT_ALLOWED;
  }
  return __real_esp_partition_erase_range(partition, offset, size);
}
#endif
