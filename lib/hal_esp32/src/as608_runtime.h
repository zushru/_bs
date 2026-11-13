#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "as608_sensor_arbiter.h"
#include "as608_enroll_sensor.h"
#include "as608_attendance_client.h"
#include "stream_as608_link.h"
#include "enroll_fsm.h"

namespace att {

class As608StartupCoordinator;

// Low-level one-runtime composition for the AS608 sensor bus. It creates one
// arbiter and one Stream link, then wires enrollment and attendance to those same
// instances. The type does not enforce global singleton construction: the production
// root must instantiate one As608AttendanceOwner and must not create parallel links,
// runtimes or standalone clients for the same physical sensor. Calls are serialized
// main-loop APIs; this class adds no RTOS mutex or scheduler.
class As608Runtime {
 public:
  // Preferred owned-map Runtime form; production attendance should use As608AttendanceOwner.
  As608Runtime(Stream& stream, RosterStore& roster, uint32_t sensorPassword);
  // Low-level injected-map form for tightly controlled tests/adapters only. The
  // caller retains its own mutable reference, so production roots should not use it.
  As608Runtime(Stream& stream, RosterStore& roster, Occupancy& occupancy,
               uint32_t sensorPassword);
  As608Runtime(const As608Runtime&) = delete;
  As608Runtime& operator=(const As608Runtime&) = delete;

  // Read-only status view; runtime-managed mutation is reserved for friends.
  const As608AttendanceClient& attendance() const { return attendance_; }
  const Occupancy& occupancy() const { return *occupancy_; }
  const As608EnrollmentSession& enrollment() const { return enrollment_; }
  bool transportFaulted() const { return link_.faulted(); }

 private:
  friend class As608StartupCoordinator;
  friend class As608AttendancePipeline;
  friend class As608AttendanceOwner;
  As608AttendanceClient& attendanceClient() { return attendance_; }
  Occupancy& mutableOccupancy() { return *occupancy_; }
  As608EnrollSensor& enrollmentSensor() { return enrollmentSensor_; }
  As608OccupancyReader& occupancyReader() { return occupancyReader_; }
  EnrollFsm& enrollmentFsm() { return enrollmentFsm_; }
  As608SensorArbiter arbiter_;
  StreamAs608Link link_;
  Occupancy ownedOccupancy_;
  Occupancy* occupancy_;
  As608EnrollSensor enrollmentSensor_;
  As608OccupancyReader occupancyReader_;
  EnrollFsm enrollmentFsm_;
  As608EnrollmentSession enrollment_;
  As608AttendanceClient attendance_;
};

}  // namespace att
