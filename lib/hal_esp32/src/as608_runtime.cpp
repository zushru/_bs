#include "as608_runtime.h"

namespace att {

As608Runtime::As608Runtime(Stream& stream, RosterStore& roster,
    Occupancy& occupancy, uint32_t sensorPassword)
    : arbiter_(),
      link_(stream, arbiter_),
      enrollmentSensor_(link_, sensorPassword, AS608_OWNER_ENROLLMENT),
      occupancyReader_(enrollmentSensor_, occupancy),
      enrollmentFsm_(roster, occupancy, enrollmentSensor_),
      enrollment_(arbiter_, enrollmentFsm_, enrollmentSensor_, occupancyReader_),
      attendance_(arbiter_, link_, sensorPassword) {}

}  // namespace att
