#pragma once
#include <stdint.h>
#include "config_boot.h"
#include "config_store.h"
#include "health_fsm.h"

namespace att {

// Merge the config boot state into health inputs without overwriting RTC,
// storage, or sensor facts owned by their respective adapters.
// COLD and FAULT are CONFIG_FAULT, never a fallback/unprovisioned runtime.
HState updateHealthFromConfigBoot(HealthFsm& health,
                                  const ConfigBootCoordinator& boot,
                                  HealthInputs inputs,
                                  bool clockReverified = false);

enum ConfigWindowCode : uint8_t {
  CFG_WINDOW_OK,
  CFG_WINDOW_CONFIG_INVALID,
  CFG_WINDOW_CLOCK_UNTRUSTED
};

// Derive only the current timetable-window fact (not term eligibility). The
// caller must not infer unclosedPast from today's timetable: historical schedule
// boundaries are not persisted in v0.2.15. Errors set windowOpen=true.
ConfigWindowCode deriveConfigWindowOpen(const Config& current,
                                         uint32_t nowEpoch,
                                         bool clockTrusted,
                                         bool& windowOpen);

}  // namespace att
