#pragma once

#include "pin_auth.h"
#include <nvs.h>

namespace att {

// Durable PIN-attempt counter for one policy-selected lockout domain.
// The caller chooses the NVS namespace so this adapter does not decide whether
// PIN and setup-code failures share a counter (E12 remains DELEGATED).
// The NVS flash driver must already be initialized by the application boot path.
class Esp32NvsPinAttemptStore : public IPinAttemptStore {
 public:
  explicit Esp32NvsPinAttemptStore(const char* nvsNamespace);
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
  char namespace_[16];
  bool configured_;
  esp_err_t lastError_;
};

}  // namespace att
