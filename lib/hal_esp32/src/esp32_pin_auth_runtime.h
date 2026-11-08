#pragma once

#include "nvs_pin_attempt_store.h"
#include "pin_auth.h"
#include "config_boot.h"

namespace att {

enum Esp32PinAuthRuntimeCode : uint8_t {
  EPAR_COLD,
  EPAR_READY,
  EPAR_ALREADY_STARTED,
  EPAR_CONFIG_NOT_READY,
  EPAR_COUNTER_READ_FAILED,
  EPAR_COUNTER_INITIALIZE_FAILED,
  EPAR_COORDINATOR_BOOT_FAILED
};

// Owns one NVS attempt store and the coordinator that uses that exact store.
// Start only after ConfigBootCoordinator::start() has established READY or
// NEEDS_SETUP. In NEEDS_SETUP, an absent counter is initialized; an existing
// counter is preserved so an interrupted first-time setup cannot reset lockout.
// In READY, a missing counter is a fault, never an initialization opportunity.
class Esp32PinAuthRuntime {
 public:
  explicit Esp32PinAuthRuntime(const char* counterNamespace);
  Esp32PinAuthRuntime(const Esp32PinAuthRuntime&) = delete;
  Esp32PinAuthRuntime& operator=(const Esp32PinAuthRuntime&) = delete;

  // One-shot. Call after ConfigBootCoordinator::start(), before accepting any
  // PIN/setup-code request. NVS must already be initialized by config boot.
  bool start(const ConfigBootCoordinator& configBoot, uint32_t nowS);

  bool ready() const { return ready_; }
  bool faulted() const { return started_ && !ready_; }
  Esp32PinAuthRuntimeCode code() const { return code_; }
  esp_err_t lastStoreError() const { return store_.lastError(); }

  // Returns null until start() succeeds. Routes must refuse all secret checks
  // when this returns null.
  PinAuthCoordinator* coordinator() {
    return ready_ ? &coordinator_ : nullptr;
  }

 private:
  Esp32NvsPinAttemptStore store_;
  PinAuthCoordinator coordinator_;
  bool started_;
  bool ready_;
  Esp32PinAuthRuntimeCode code_;
};

}  // namespace att
