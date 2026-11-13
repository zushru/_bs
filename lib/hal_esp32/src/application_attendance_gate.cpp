#include "application_attendance_gate.h"

namespace att {

ApplicationAttendanceGate::ApplicationAttendanceGate(
    const ConfigBootCoordinator& configBoot,
    LittleFsStorageOwner& storageOwner, const MonthSet& months,
    LogStore& log, RosterStore& roster, LogAttendanceAdmission& admission,
    ClosureEngine& closure, const HealthFsm& health)
    : configBoot_(configBoot), storageOwner_(storageOwner), months_(months),
      log_(log), roster_(roster), admission_(admission), closure_(closure),
      health_(health) {}

bool ApplicationAttendanceGate::storageAndReplayReady_() const {
  if (!configBoot_.ready() ||
      storageOwner_.state() != LITTLEFS_OWNER_READY) return false;
  IFileSet* files = storageOwner_.fileSet();
  if (!files || !months_.usesFileSet(*files) ||
      !log_.usesStorage(months_) || log_.faulted()) return false;
  if (!roster_.historyComplete() || !log_.hasObserver(&roster_)) return false;
  if (!admission_.attached() || !admission_.usesLog(log_) ||
      !log_.hasObserver(&admission_)) return false;
  if (!closure_.ready() || closure_.pending() ||
      !closure_.usesLog(log_) || !closure_.usesRoster(roster_)) return false;
  return true;
}

bool ApplicationAttendanceGate::readyForSensorStartup() const {
  if (!storageAndReplayReady_()) return false;
  // H_SENSOR_FAULT is allowed only so the owner can perform its bounded,
  // read-only authentication/capacity/index startup. Any other fault blocks it.
  return health_.state() == H_OK || health_.state() == H_SENSOR_FAULT;
}

bool ApplicationAttendanceGate::readyForAttendance() const {
  if (!storageAndReplayReady_() || health_.state() != H_OK) return false;
  return HealthFsm::perms(health_.state()).scan;
}

bool ApplicationAttendanceGate::matches(const LogStore& log,
                                          const RosterStore& roster) const {
  return &log_ == &log && &roster_ == &roster;
}

bool ApplicationAttendanceGate::matchesConfig(const Config& config) const {
  const Config* bootConfig = configBoot_.config();
  return bootConfig && configEqual(*bootConfig, config) &&
      roster_.utcOffsetMin() == config.params.utcOffsetMin &&
      admission_.usesParams(config.params) &&
      closure_.matchesConfig(config);
}

bool ApplicationAttendanceGate::allowScan(int32_t localDay,
                                           uint8_t session) const {
  return readyForAttendance() && admission_.allowScan(localDay, session);
}

bool ApplicationAttendanceGate::canSetParamsAfterCommit(
    const Params& params) const {
  return readyForAttendance() && admission_.canSetParamsAfterCommit(params);
}

bool ApplicationAttendanceGate::setParamsAfterCommit(const Params& params) {
  return readyForAttendance() && admission_.setParamsAfterCommit(params);
}

bool ApplicationAttendanceGate::alreadyScanned(int32_t localDay,
    uint8_t session, uint16_t studentId) const {
  return readyForAttendance() &&
      admission_.alreadyScanned(localDay, session, studentId);
}

}  // namespace att
