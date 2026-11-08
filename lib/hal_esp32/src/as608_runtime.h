#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "as608_sensor_arbiter.h"
#include "as608_enroll_sensor.h"
#include "as608_attendance_client.h"
#include "stream_as608_link.h"
#include "enroll_fsm.h"

namespace att {

// Single-owner composition root for the AS608 sensor bus. It creates exactly one
// arbiter and one Stream link, then wires the enrollment sensor/session and attendance
// client to those same instances. Do not construct another StreamAs608Link or arbiter
// for this physical sensor. All methods are serialized main-loop APIs; this class does
// not add an RTOS mutex or a scheduler.
class As608Runtime {
 public:
  As608Runtime(Stream& stream, RosterStore& roster, Occupancy& occupancy,
               uint32_t sensorPassword);
  As608Runtime(const As608Runtime&) = delete;
  As608Runtime& operator=(const As608Runtime&) = delete;

  As608AttendanceClient& attendance() { return attendance_; }
  As608EnrollmentSession& enrollment() { return enrollment_; }
  As608EnrollSensor& enrollmentSensor() { return enrollmentSensor_; }
  As608OccupancyReader& occupancyReader() { return occupancyReader_; }
  EnrollFsm& enrollmentFsm() { return enrollmentFsm_; }
  bool transportFaulted() const { return link_.faulted(); }

 private:
  As608SensorArbiter arbiter_;
  StreamAs608Link link_;
  As608EnrollSensor enrollmentSensor_;
  As608OccupancyReader occupancyReader_;
  EnrollFsm enrollmentFsm_;
  As608EnrollmentSession enrollment_;
  As608AttendanceClient attendance_;
};

}  // namespace att
