#pragma once
#include <stdint.h>
#include "as608_enroll_sensor.h"
#include "as608_sensor_arbiter.h"

namespace att {

enum As608AttendanceResult : uint8_t {
  AS608_ATT_PENDING,
  AS608_ATT_READY,
  AS608_ATT_NO_FINGER,
  AS608_ATT_MATCH,
  AS608_ATT_NO_MATCH,
  AS608_ATT_BAD_IMAGE,
  AS608_ATT_RETRYABLE,
  AS608_ATT_FATAL
};

// Non-blocking attendance scan client. Setup authenticates and reads capacity once;
// each scan then performs capture -> convert -> full-library search. The returned page
// is the physical sensor page; this class deliberately does not apply the DELEGATED
// student-ID/page mapping (E18).
//
// An attendance lease covers the entire multi-command scan, not merely each UART
// transaction. Enrollment therefore cannot start between capture, convert and search.
// A poisoned transport leaves the lease held; rebuild the runtime only after a full
// sensor/ESP32 power-cycle. Calls belong on one serialized main loop, not concurrent tasks.
class As608AttendanceClient {
 public:
  As608AttendanceClient(As608SensorArbiter& arbiter, IAs608Link& link,
                        uint32_t sensorPassword);
  As608AttendanceClient(const As608AttendanceClient&) = delete;
  As608AttendanceClient& operator=(const As608AttendanceClient&) = delete;

  bool beginSetup();
  As608AttendanceResult stepSetup();
  bool ready() const { return ready_; }

  bool beginScan();
  As608AttendanceResult stepScan(uint16_t& physicalPage, uint16_t& score);

  bool active() const;
  bool held() const { return lease_.held(); }
  bool faulted() const { return state_ == ST_FAULT; }
  uint16_t capacity() const { return sensor_.capacity(); }

 private:
  enum State : uint8_t {
    ST_IDLE,
    ST_SETUP_AUTH,
    ST_SETUP_CAPACITY,
    ST_SCAN_CAPTURE,
    ST_SCAN_CONVERT,
    ST_SCAN_SEARCH,
    ST_FAULT
  };

  static As608AttendanceResult mapError_(EnrollIo result);
  As608AttendanceResult finishSetup_(As608AttendanceResult result, bool ready);
  As608AttendanceResult finishScan_(As608AttendanceResult result);

  As608AttendanceLease lease_;
  As608EnrollSensor sensor_;
  State state_;
  bool ready_;
};

}  // namespace att
