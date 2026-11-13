#include "config_runtime_guard.h"
#include "api_logic.h"

namespace att {

HState updateHealthFromConfigBoot(HealthFsm& health,
                                  const ConfigBootCoordinator& boot,
                                  HealthInputs inputs,
                                  bool clockReverified) {
  switch (boot.state()) {
    case CFG_BOOT_READY:
      inputs.provisioned = true;
      inputs.cfgOk = true;
      break;
    case CFG_BOOT_NEEDS_SETUP:
      inputs.provisioned = false;
      inputs.cfgOk = true;
      break;
    case CFG_BOOT_COLD:
    case CFG_BOOT_FAULT:
    default:
      // A cold/failed boot is not proof that NVS is blank. Suppress the
      // secondary UNPROVISIONED bit and report the configuration fault.
      inputs.provisioned = true;
      inputs.cfgOk = false;
      break;
  }
  return health.update(inputs, clockReverified);
}

ConfigWindowCode deriveConfigWindowOpen(const Config& current,
                                         uint32_t nowEpoch,
                                         bool clockTrusted,
                                         bool& windowOpen) {
  windowOpen = true;
  if (!validateConfig(current)) return CFG_WINDOW_CONFIG_INVALID;
  if (!clockTrusted || dayNumber(nowEpoch, current.params.utcOffsetMin) < 0)
    return CFG_WINDOW_CLOCK_UNTRUSTED;
  windowOpen = windowOpenNow(current.tt, current.params, nowEpoch);
  return CFG_WINDOW_OK;
}

}  // namespace att
