#pragma once
#include <stdint.h>
#include "enroll_fsm.h"

namespace att {

class StreamAs608Link;

enum As608CommandOwner : uint8_t {
  AS608_OWNER_ATTENDANCE = 0,
  AS608_OWNER_ENROLLMENT = 1
};

// Main-loop, single-thread arbiter. Every transport transaction must acquire one
// command lease and retain it until its response is consumed or the link faults.
class As608SensorArbiter {
 public:
  As608SensorArbiter();
  As608SensorArbiter(const As608SensorArbiter&) = delete;
  As608SensorArbiter& operator=(const As608SensorArbiter&) = delete;
  // True when attendance is not blocked by enrollment or an in-flight command.
  // This is not authorization: an attendance lifecycle lease is still mandatory.
  bool attendanceAllowed() const;
  bool attendanceReserved() const { return attendanceReserved_; }
  bool enrollmentReserved() const { return enrollmentReserved_; }
  bool commandInFlight() const { return commandInFlight_; }
 private:
  friend class As608EnrollmentLease;
  friend class As608AttendanceLease;
  friend class StreamAs608Link;
  bool reserveAttendance();
  bool releaseAttendance();
  bool reserveEnrollment();
  bool beginCommand(As608CommandOwner owner);
  bool finishCommand(As608CommandOwner owner);
  bool releaseEnrollment();
  bool attendanceReserved_;
  bool enrollmentReserved_;
  bool commandInFlight_;
  As608CommandOwner commandOwner_;
};

// Attendance owns the sensor across the capture -> convert -> search sequence. This
// prevents enrollment from acquiring the bus between two separately acknowledged
// commands. A faulted transport keeps this lease held until the runtime is rebuilt
// after a physical sensor/ESP32 power-cycle.
class As608AttendanceLease {
 public:
  explicit As608AttendanceLease(As608SensorArbiter& arbiter);
  As608AttendanceLease(const As608AttendanceLease&) = delete;
  As608AttendanceLease& operator=(const As608AttendanceLease&) = delete;
  bool acquire();
  bool release(bool sensorCommandPending, bool linkFaulted);
  bool held() const { return held_; }
 private:
  As608SensorArbiter& arbiter_;
  bool held_;
};

// Lifecycle lease held across authentication, capacity/index refresh and the full
// enrollment FSM. The caller acquires before the first setup command. It may release
// only after EN_DONE/EN_CANCELLED, or after a setup failure, and only when no command
// or occupancy refresh remains active. Destruction does not auto-release: loss of the
// scheduler must fail closed rather than resume attendance with an unknown finger state.
class As608EnrollmentLease {
 public:
  explicit As608EnrollmentLease(As608SensorArbiter& arbiter);
  As608EnrollmentLease(const As608EnrollmentLease&) = delete;
  As608EnrollmentLease& operator=(const As608EnrollmentLease&) = delete;
  bool acquire();
  bool finish(EnrollState state, bool sensorCommandPending, bool occupancyRefreshActive);
  bool abortPreparation(bool sensorCommandPending, bool occupancyRefreshActive);
  bool held() const { return held_; }
 private:
  As608SensorArbiter& arbiter_;
  bool held_;
};

}  // namespace att
