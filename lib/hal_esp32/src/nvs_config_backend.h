#pragma once

#include "config_boot.h"
#include <nvs.h>

namespace att {

// ESP-IDF NVS implementation of the two-slot config backend. The default NVS
// partition is used; this class never erases or reformats it. All calls are
// synchronous and must be made by the serialized application loop.
class Esp32NvsConfigBackend : public IConfigBootBackend {
 public:
  Esp32NvsConfigBackend() : ready_(false), lastError_(ESP_OK) {}

  // Calls nvs_flash_init(). Any error, including NO_FREE_PAGES or
  // NEW_VERSION_FOUND, is returned as failure. No automatic erase/retry occurs.
  bool begin() override;
  bool load(Config& out, CfgPick& pick) override;
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override;
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override;
  bool readProvisionMarker(bool& present) override;

  bool ready() const { return ready_; }
  esp_err_t lastError() const { return lastError_; }

 private:
  bool ready_;
  esp_err_t lastError_;
};

}  // namespace att
