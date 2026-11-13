#include "esp32_pin_auth_runtime.h"

namespace att {

Esp32PinAuthRuntime::Esp32PinAuthRuntime(
    const char* counterNamespace, const ConfigBootCoordinator& configBoot)
    : configBoot_(configBoot), store_(counterNamespace, configBoot_),
      coordinator_(store_), started_(false),
      ready_(false), code_(EPAR_COLD) {}

bool Esp32PinAuthRuntime::start(uint32_t nowS) {
  if (started_) {
    code_ = EPAR_ALREADY_STARTED;
    return false;
  }
  started_ = true;

  if (!configBoot_.ready() && !configBoot_.needsSetup()) {
    code_ = EPAR_CONFIG_NOT_READY;
    return false;
  }

  if (configBoot_.needsSetup()) {
    uint16_t existing = 0;
    if (!store_.loadFailureCount(existing)) {
      // Only an explicit NVS NOT_FOUND result on a verified unprovisioned
      // config state permits first-time initialization. I/O/type errors fail.
      if (store_.lastError() != ESP_ERR_NVS_NOT_FOUND) {
        code_ = EPAR_COUNTER_READ_FAILED;
        return false;
      }
      if (!store_.initializeFailureCount()) {
        code_ = EPAR_COUNTER_INITIALIZE_FAILED;
        return false;
      }
    }
  }

  // On a provisioned config, this load must find an existing counter. The
  // coordinator never creates missing state, so a lost counter fails closed.
  if (!coordinator_.boot(nowS)) {
    code_ = EPAR_COORDINATOR_BOOT_FAILED;
    return false;
  }

  ready_ = true;
  code_ = EPAR_READY;
  return true;
}

}  // namespace att
