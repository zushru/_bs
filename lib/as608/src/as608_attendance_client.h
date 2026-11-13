#pragma once
#include <stdint.h>
#include "as608_enroll_sensor.h"
#include "as608_sensor_arbiter.h"

namespace att {

class As608Runtime;
class As608StartupCoordinator;
class As608AttendancePipeline;

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

  // Direct control is supported only for standalone three-argument clients.
  // Runtime-managed instances are serviced only by startup/pipeline friends.
  bool beginSetup();
  As608AttendanceResult stepSetup();
  bool ready() const { return ready_; }
  // Runtime-managed raw scans are intentionally disabled; use the event pipeline.
  bool beginScan();
  As608AttendanceResult stepScan(uint16_t& physicalPage, uint16_t& score);

  bool active() const;
  bool held() const { return lease_.held(); }
  bool faulted() const { return state_ == ST_FAULT; }
  uint16_t capacity() const { return sensor_.capacity(); }

 private:
  friend class As608Runtime;
  friend class As608StartupCoordinator;
  friend class As608AttendancePipeline;

  // Runtime-managed clients are serviced only by As608StartupCoordinator and
  // As608AttendancePipeline. The public three-argument constructor remains for
  // isolated low-level use and intentionally does not provide those guarantees.
  As608AttendanceClient(As608SensorArbiter& arbiter, IAs608Link& link,
                        uint32_t sensorPassword, bool runtimeManaged);
  bool invalidateReady();
  bool beginSetupFromStartup();
  bool beginSetupImpl_();
  As608AttendanceResult stepSetupFromStartup();
  As608AttendanceResult stepSetupImpl_();
  bool beginScanFromPipeline();
  bool beginScanImpl_();
  As608AttendanceResult stepScanFromPipeline(uint16_t& physicalPage,
                                               uint16_t& score);
  As608AttendanceResult stepScanImpl_(uint16_t& physicalPage, uint16_t& score);

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
  bool runtimeManaged_;
};

}  // namespace att
