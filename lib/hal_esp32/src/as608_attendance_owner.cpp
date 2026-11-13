#include "as608_attendance_owner.h"

namespace att {

As608AttendanceOwner::As608AttendanceOwner(Stream& stream, LogStore& log,
    RosterStore& roster, const Config& config, const IAs608PageResolver& pages,
    ApplicationAttendanceGate& gate, uint32_t sensorPassword)
    : gate_(gate),
      bindingsValid_(gate.matches(log, roster) && gate.matchesConfig(config)),
      runtime_(stream, roster, sensorPassword),
      startup_(runtime_),
      pipeline_(runtime_, log, roster, config, pages, gate) {}

bool As608AttendanceOwner::beginStartup() {
  return bindingsValid_ && gate_.readyForSensorStartup() && startup_.begin();
}

As608StartupStatus As608AttendanceOwner::stepStartup() {
  return startup_.step();
}

bool As608AttendanceOwner::beginScan(uint32_t nowEpoch) {
  return bindingsValid_ && startup_.ready() &&
      gate_.readyForAttendance() && pipeline_.begin(nowEpoch);
}

AttendancePipelineResult As608AttendanceOwner::stepScan() {
  if (!startup_.ready()) return ATT_PIPE_REJECTED;
  return pipeline_.step();
}

bool As608AttendanceOwner::canSetConfigAfterCommit(const Config& config) const {
  return bindingsValid_ && startup_.ready() && gate_.readyForAttendance() &&
      pipeline_.canSetConfigAfterCommit(config);
}

bool As608AttendanceOwner::setConfigAfterCommit(const Config& config) {
  return bindingsValid_ && startup_.ready() && gate_.readyForAttendance() &&
      pipeline_.setConfigAfterCommit(config);
}

}  // namespace att
