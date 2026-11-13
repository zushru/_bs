#pragma once
#include <stddef.h>
#include <stdint.h>
#include "enroll_fsm.h"
#include "as608_sensor_arbiter.h"

namespace att {

class As608Runtime;
class As608StartupCoordinator;

enum As608LinkStatus : int {
  AS608_LINK_OK = 0,
  AS608_LINK_PENDING = 1,
  AS608_LINK_TIMEOUT = -1,
  AS608_LINK_IO_ERROR = -2,
  AS608_LINK_BAD_FRAME = -3,
  AS608_LINK_OVERFLOW = -4,
  AS608_LINK_BAD_ARGUMENT = -5,
  AS608_LINK_POISONED = -6,
  AS608_LINK_BUSY = -7
};

// Single-command transport. startTransaction() sends the short request synchronously;
// pollTransaction() consumes already-available response bytes without waiting. The
// concrete Stream bridge has bounded RX work but synchronous TX writes. Every caller
// must identify itself; the transport enforces the shared arbiter reservation.
class IAs608Link {
 public:
  virtual ~IAs608Link() {}
  virtual int startTransaction(As608CommandOwner owner, const uint8_t* request,
                               size_t requestLength, uint32_t timeoutMs) = 0;
  virtual int pollTransaction(As608CommandOwner owner, uint8_t* response,
                              size_t responseCapacity, size_t& responseLength) = 0;
  virtual bool faulted() const { return false; }
};

// Raw AS608 command adapter shared by enrollment and attendance clients. This uses
// only commands exercised by the project's diagnostic sketch; no Adafruit_Fingerprint
// API is assumed. The configured owner is enforced by the shared transport gate.
class As608EnrollSensor : public IEnrollSensor {
 public:
  // owner defaults to ENROLLMENT for compatibility. The production composition
  // root constructs a separate instance with ATTENDANCE for the scan client.
  As608EnrollSensor(IAs608Link& link, uint32_t sensorPassword,
                    As608CommandOwner owner = AS608_OWNER_ENROLLMENT);

  // Verify the existing password; this sends command 0x13 only. It never changes
  // the sensor password. Must succeed before any other operation is accepted.
  EnrollIo authenticate();
  EnrollIo readCapacity(uint16_t& capacity);
  // Read one 256-slot occupancy page. On EIO_OK, out receives exactly 32 bytes.
  // The target module implementation remains subject to device verification.
  EnrollIo readIndexPage(uint8_t page, uint8_t out[32]);
  uint16_t capacity() const { return capacity_; }

  EnrollIo captureImage() override;
  EnrollIo convertImage(uint8_t buffer) override;
  EnrollIo searchTemplate(uint16_t& page, uint16_t& score) override;
  EnrollIo createModel() override;
  EnrollIo storeModel(uint16_t page) override;
  bool operationPending() const override { return commandPending_; }
  bool linkFaulted() const { return link_.faulted(); }

 private:
  struct Ack {
    uint8_t code;
    uint8_t data[32];
    uint8_t dataLength;
  };
  enum CommandPoll : uint8_t { CP_PENDING, CP_ACK, CP_ERROR, CP_LINK_FAULT };

  CommandPoll command_(uint8_t instruction, const uint8_t* params, uint8_t paramCount,
                       Ack& ack, uint32_t timeoutMs);
  static bool imageError_(uint8_t code);
  static EnrollIo mapImageOperation_(uint8_t code);

  IAs608Link& link_;
  As608CommandOwner owner_;
  uint32_t password_;
  uint16_t capacity_;
  bool authenticated_;
  bool commandPending_;
  uint8_t activeInstruction_, activeParamCount_, activeParams_[8];
};


// Asynchronous, fail-closed full occupancy refresh. begin() refuses to start while
// another command/refresh is active or capacity is invalid. step() reads one page
// at a time and marks Occupancy valid only after every required page succeeds.
// A failed page discards the partial map; callers must retry the entire refresh.
class As608OccupancyReader {
 public:
  As608OccupancyReader(As608EnrollSensor& sensor, Occupancy& occupancy);
  bool begin();
  EnrollIo step();
  bool active() const { return active_; }
 private:
  As608EnrollSensor& sensor_;
  Occupancy& occupancy_;
  uint16_t capacity_;
  uint8_t page_, pages_;
  bool active_;
};

// Binds the shared arbiter lease to the actual enrollment FSM, sensor adapter and
// occupancy reader. Acquire before authentication/capacity/index refresh; call sync()
// after each FSM step. Cancellation faults remain reserved because the FSM is not terminal.
class As608EnrollmentSession {
 public:
  As608EnrollmentSession(As608SensorArbiter& arbiter, EnrollFsm& fsm,
                         As608EnrollSensor& sensor, As608OccupancyReader& reader);
  As608EnrollmentSession(const As608EnrollmentSession&) = delete;
  As608EnrollmentSession& operator=(const As608EnrollmentSession&) = delete;
  bool acquire();
  // Call after each FSM step. True means ownership was released; false means it stays held.
  bool sync();
  // Setup-only abort: allowed only while the FSM is idle and all sensor work is drained.
  bool abortPreparation();
  bool held() const { return lease_.held(); }
  bool available() const { return !runtimeManaged_ || startupReady_; }
 private:
  friend class As608Runtime;
  friend class As608StartupCoordinator;
  As608EnrollmentSession(As608SensorArbiter& arbiter, EnrollFsm& fsm,
                         As608EnrollSensor& sensor, As608OccupancyReader& reader,
                         bool runtimeManaged);
  bool acquireForStartup();
  bool abortPreparationFromStartup();
  bool enableAfterStartup();
  bool acquireImpl_();
  bool abortPreparationImpl_();

  As608EnrollmentLease lease_;
  EnrollFsm& fsm_;
  As608EnrollSensor& sensor_;
  As608OccupancyReader& reader_;
  bool runtimeManaged_;
  bool startupReady_;
};

}  // namespace att
