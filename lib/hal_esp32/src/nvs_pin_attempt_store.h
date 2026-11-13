#pragma once

#include "pin_auth.h"
#include "config_boot.h"
#include <nvs.h>

namespace att {

// Durable PIN-attempt counter for one policy-selected lockout domain.
// The caller chooses the NVS namespace so this adapter does not decide whether
// PIN and setup-code failures share a counter (E12 remains DELEGATED). Each
// operation checks the pre-setup startup snapshot and the bound, non-owned
// ConfigBootCoordinator; only READY/NEEDS_SETUP permits NVS access. The boot
// coordinator must outlive this store.
class Esp32NvsPinAttemptStore : public IPinAttemptStore {
 public:
  Esp32NvsPinAttemptStore(const char* nvsNamespace,
                          const ConfigBootCoordinator& configBoot);
  Esp32NvsPinAttemptStore(const Esp32NvsPinAttemptStore&) = delete;
  Esp32NvsPinAttemptStore& operator=(const Esp32NvsPinAttemptStore&) = delete;

  // Explicit first-provisioning operation only. It refuses to overwrite an
  // existing key. Do not call this as a recovery path after a read failure.
  bool initializeFailureCount();

  bool loadFailureCount(uint16_t& out) override;
  bool persistFailureCount(uint16_t failures) override;

  bool configured() const { return configured_; }
  esp_err_t lastError() const { return lastError_; }

 private:
  const ConfigBootCoordinator& configBoot_;
  char namespace_[16];
  bool configured_;
  esp_err_t lastError_;
};

}  // namespace att
