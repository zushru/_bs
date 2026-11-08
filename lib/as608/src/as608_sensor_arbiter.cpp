#include "as608_sensor_arbiter.h"

namespace att {

As608SensorArbiter::As608SensorArbiter()
    : attendanceReserved_(false), enrollmentReserved_(false), commandInFlight_(false),
      commandOwner_(AS608_OWNER_ATTENDANCE) {}

bool As608SensorArbiter::reserveAttendance() {
  if (attendanceReserved_ || enrollmentReserved_ || commandInFlight_) return false;
  attendanceReserved_ = true;
  return true;
}

bool As608SensorArbiter::releaseAttendance() {
  if (!attendanceReserved_ || commandInFlight_) return false;
  attendanceReserved_ = false;
  return true;
}

bool As608SensorArbiter::reserveEnrollment() {
  if (attendanceReserved_ || enrollmentReserved_ || commandInFlight_) return false;
  enrollmentReserved_ = true;
  return true;
}

bool As608SensorArbiter::releaseEnrollment() {
  if (!enrollmentReserved_ || commandInFlight_) return false;
  enrollmentReserved_ = false;
  return true;
}

bool As608SensorArbiter::beginCommand(As608CommandOwner owner) {
  if (commandInFlight_) return false;
  if (owner == AS608_OWNER_ENROLLMENT) {
    if (!enrollmentReserved_) return false;
  } else if (owner == AS608_OWNER_ATTENDANCE) {
    // Attendance is allowed to issue commands only while holding its scan/setup
    // lifecycle lease. Checking only enrollmentReserved_ would let raw callers
    // bypass the cross-command reservation and interleave with enrollment.
    if (!attendanceReserved_ || enrollmentReserved_) return false;
  } else {
    return false;
  }
  commandOwner_ = owner;
  commandInFlight_ = true;
  return true;
}

bool As608SensorArbiter::finishCommand(As608CommandOwner owner) {
  if (!commandInFlight_ || commandOwner_ != owner) return false;
  commandInFlight_ = false;
  return true;
}

bool As608SensorArbiter::attendanceAllowed() const {
  return !enrollmentReserved_ && !commandInFlight_;
}

As608AttendanceLease::As608AttendanceLease(As608SensorArbiter& arbiter)
    : arbiter_(arbiter), held_(false) {}

bool As608AttendanceLease::acquire() {
  if (held_ || !arbiter_.reserveAttendance()) return false;
  held_ = true;
  return true;
}

bool As608AttendanceLease::release(bool sensorCommandPending, bool linkFaulted) {
  if (!held_ || sensorCommandPending || linkFaulted || !arbiter_.releaseAttendance())
    return false;
  held_ = false;
  return true;
}

As608EnrollmentLease::As608EnrollmentLease(As608SensorArbiter& arbiter)
    : arbiter_(arbiter), held_(false) {}

bool As608EnrollmentLease::acquire() {
  if (held_ || !arbiter_.reserveEnrollment()) return false;
  held_ = true;
  return true;
}

bool As608EnrollmentLease::finish(EnrollState state, bool sensorCommandPending,
                                  bool occupancyRefreshActive) {
  if (!held_ || (state != EN_DONE && state != EN_CANCELLED) ||
      sensorCommandPending || occupancyRefreshActive || !arbiter_.releaseEnrollment())
    return false;
  held_ = false;
  return true;
}

bool As608EnrollmentLease::abortPreparation(bool sensorCommandPending,
                                            bool occupancyRefreshActive) {
  if (!held_ || sensorCommandPending || occupancyRefreshActive ||
      !arbiter_.releaseEnrollment()) return false;
  held_ = false;
  return true;
}

}  // namespace att
