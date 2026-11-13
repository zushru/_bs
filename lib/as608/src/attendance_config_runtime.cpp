#include "attendance_config_runtime.h"

namespace att {

bool AttendanceConfigRuntime::canApply(const Config& next) const {
  return pipeline_.canSetConfigAfterCommit(next);
}

bool AttendanceConfigRuntime::applyAfterCommit(const Config& next) {
  if (!pipeline_.setConfigAfterCommit(next)) return false;
  closure_.setSchedule(next.params, next.tt, next.termStart, next.termEnd);
  return true;
}

}  // namespace att
