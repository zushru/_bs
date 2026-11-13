#include "as608_owner_config_runtime.h"

namespace att {

bool As608OwnerConfigRuntime::canApply(const Config& next) const {
  return bindingsValid_ && owner_.canSetConfigAfterCommit(next);
}

bool As608OwnerConfigRuntime::applyAfterCommit(const Config& next) {
  if (!bindingsValid_) return false;
  // If this fails after the config-slot commit, ConfigApplyCoordinator latches its
  // runtime fault and the application must stop until reboot/reload. Do not reorder.
  if (!owner_.setConfigAfterCommit(next)) return false;
  closure_.setSchedule(next.params, next.tt, next.termStart);
  return true;
}

}  // namespace att
