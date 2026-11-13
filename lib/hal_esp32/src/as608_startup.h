#pragma once
#include "as608_runtime.h"

namespace att {

enum As608StartupStatus : uint8_t {
  AS608_STARTUP_IDLE,
  AS608_STARTUP_PENDING,
  AS608_STARTUP_READY,
  AS608_STARTUP_FAULT
};

// Serial startup gate for one As608Runtime. It refreshes the complete sensor
// occupancy index under the enrollment lease, releases that lease, then authenticates
// the attendance client and requires its capacity to match. Attendance is not ready
// until both phases succeed. Any partial index, protocol error, transport fault, or
// capacity disagreement leaves occupancy invalid and startup faulted.
class As608StartupCoordinator {
 public:
  explicit As608StartupCoordinator(As608Runtime& runtime);
  As608StartupCoordinator(const As608StartupCoordinator&) = delete;
  As608StartupCoordinator& operator=(const As608StartupCoordinator&) = delete;

  // Call once after constructing the runtime and after storage/config boot gates.
  // False means startup could not acquire the sensor cleanly; no work was started.
  bool begin();
  // Call repeatedly from the serialized main loop. One call performs at most one
  // sensor operation / response poll. Do not service attendance until READY.
  As608StartupStatus step();

  bool ready() const { return state_ == ST_READY; }
  bool faulted() const { return state_ == ST_FAULT; }
  uint16_t capacity() const { return capacity_; }

 private:
  enum State : uint8_t { ST_IDLE, ST_AUTH, ST_CAPACITY, ST_INDEX, ST_ATTENDANCE, ST_READY, ST_FAULT };
  As608StartupStatus fail_();

  As608Runtime& runtime_;
  Occupancy& occupancy_;
  State state_;
  uint16_t capacity_;
};

}  // namespace att
