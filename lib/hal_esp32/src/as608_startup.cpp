#include "as608_startup.h"

namespace att {

As608StartupCoordinator::As608StartupCoordinator(As608Runtime& runtime)
    : runtime_(runtime), occupancy_(runtime.mutableOccupancy()), state_(ST_IDLE), capacity_(0) {}

bool As608StartupCoordinator::begin() {
  if (state_ != ST_IDLE || runtime_.transportFaulted() ||
      runtime_.enrollmentFsm().state() != EN_IDLE ||
      runtime_.enrollment_.held() || runtime_.occupancyReader().active() ||
      runtime_.attendanceClient().ready() || runtime_.attendanceClient().active()) return false;
  if (!runtime_.enrollment_.acquireForStartup()) return false;
  occupancy_.begin();
  capacity_ = 0;
  state_ = ST_AUTH;
  return true;
}

As608StartupStatus As608StartupCoordinator::fail_() {
  // Partial occupancy must never survive a failed startup. Keep the enrollment
  // reservation if a command is pending, the link is poisoned, or the reader is active.
  occupancy_.begin();
  if (runtime_.enrollment_.held() &&
      !runtime_.enrollmentSensor().linkFaulted() &&
      !runtime_.enrollmentSensor().operationPending() &&
      !runtime_.occupancyReader().active() &&
      runtime_.enrollmentFsm().state() == EN_IDLE) {
    (void)runtime_.enrollment_.abortPreparationFromStartup();
  }
  // A completed attendance setup can be revoked on capacity disagreement. If setup
  // itself faulted, the client already holds its poisoned lease and is not ready.
  (void)runtime_.attendanceClient().invalidateReady();
  state_ = ST_FAULT;
  return AS608_STARTUP_FAULT;
}

As608StartupStatus As608StartupCoordinator::step() {
  if (state_ == ST_IDLE) return AS608_STARTUP_IDLE;
  if (state_ == ST_READY) return AS608_STARTUP_READY;
  if (state_ == ST_FAULT) return AS608_STARTUP_FAULT;

  if (state_ == ST_AUTH) {
    const EnrollIo result = runtime_.enrollmentSensor().authenticate();
    if (result == EIO_PENDING) return AS608_STARTUP_PENDING;
    if (result != EIO_OK) return fail_();
    state_ = ST_CAPACITY;
    return AS608_STARTUP_PENDING;
  }

  if (state_ == ST_CAPACITY) {
    uint16_t reported = 0;
    const EnrollIo result = runtime_.enrollmentSensor().readCapacity(reported);
    if (result == EIO_PENDING) return AS608_STARTUP_PENDING;
    if (result != EIO_OK || reported < 1 || reported > MAX_ID ||
        reported != runtime_.enrollmentSensor().capacity()) return fail_();
    capacity_ = reported;
    if (!runtime_.occupancyReader().begin()) return fail_();
    state_ = ST_INDEX;
    return AS608_STARTUP_PENDING;
  }

  if (state_ == ST_INDEX) {
    const EnrollIo result = runtime_.occupancyReader().step();
    if (result == EIO_PENDING) return AS608_STARTUP_PENDING;
    if (result != EIO_OK || !occupancy_.valid ||
        runtime_.enrollmentSensor().capacity() != capacity_) return fail_();
    if (!runtime_.enrollment_.abortPreparationFromStartup()) return fail_();
    if (!runtime_.attendanceClient().beginSetupFromStartup()) return fail_();
    state_ = ST_ATTENDANCE;
    return AS608_STARTUP_PENDING;
  }

  if (state_ == ST_ATTENDANCE) {
    const As608AttendanceResult result = runtime_.attendanceClient().stepSetupFromStartup();
    if (result == AS608_ATT_PENDING) return AS608_STARTUP_PENDING;
    if (result != AS608_ATT_READY || !runtime_.attendanceClient().ready() ||
        runtime_.attendanceClient().capacity() != capacity_) return fail_();
    if (!runtime_.enrollment_.enableAfterStartup()) return fail_();
    state_ = ST_READY;
    return AS608_STARTUP_READY;
  }

  return fail_();
}

}  // namespace att
