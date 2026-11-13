#pragma once

#include "as608_attendance_pipeline.h"
#include "config_boot.h"
#include "health_fsm.h"
#include "closure_engine.h"
#include "month_set.h"
#include "littlefs_mount_owner.h"

namespace att {

// Mandatory admission gate for the preferred As608AttendanceOwner. It binds the
// one validated config boot, no-format storage owner, exact file-set -> MonthSet ->
// LogStore chain, roster/admission/closure observers, and current application health.
// All referenced objects are non-owned and must outlive this gate. All calls belong
// to the same serialized application loop; no cross-task synchronization is added.
class ApplicationAttendanceGate final : public IAttendanceAdmission {
 public:
  ApplicationAttendanceGate(const ConfigBootCoordinator& configBoot,
      LittleFsStorageOwner& storageOwner, const MonthSet& months,
      LogStore& log, RosterStore& roster, LogAttendanceAdmission& admission,
      ClosureEngine& closure, const HealthFsm& health);
  ApplicationAttendanceGate(const ApplicationAttendanceGate&) = delete;
  ApplicationAttendanceGate& operator=(const ApplicationAttendanceGate&) = delete;

  // Sensor auth/capacity/index startup may proceed when sensor is the only
  // health fault. Config, storage, log, roster, closure and clock must be sound.
  bool readyForSensorStartup() const;
  // Attendance and config application require the full H_OK permission state.
  bool readyForAttendance() const;
  bool matches(const LogStore& log, const RosterStore& roster) const;
  bool matchesConfig(const Config& config) const;
  bool matchesClosure(const ClosureEngine& closure) const {
    return &closure_ == &closure;
  }

  bool allowScan(int32_t localDay, uint8_t session) const override;
  bool canSetParamsAfterCommit(const Params& params) const override;
  bool setParamsAfterCommit(const Params& params) override;
  bool alreadyScanned(int32_t localDay, uint8_t session,
                      uint16_t studentId) const override;

 private:
  bool storageAndReplayReady_() const;

  const ConfigBootCoordinator& configBoot_;
  LittleFsStorageOwner& storageOwner_;
  const MonthSet& months_;
  LogStore& log_;
  RosterStore& roster_;
  LogAttendanceAdmission& admission_;
  ClosureEngine& closure_;
  const HealthFsm& health_;
};

}  // namespace att
