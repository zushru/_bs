#pragma once
#include "enrolled_roster.h"

namespace att {

// The product limit is 45 active students even though the sensor can hold 300 templates.
const uint8_t ENROLL_MAX_QUEUE = 45;
const uint8_t ENROLL_MAX_SENSOR_ERRORS = 3;

enum EnrollIo : uint8_t {
  EIO_OK,
  EIO_NO_FINGER,       // normal while waiting for a finger to be placed/removed
  EIO_NO_MATCH,        // normal result from a whole-library search
  EIO_BAD_IMAGE,       // recoverable image/capture failure
  EIO_MISMATCH,        // the two enrollment samples did not form one model
  EIO_RETRYABLE,       // transient sensor/transport failure
  EIO_FATAL,           // adapter reports a non-recoverable sensor fault
  EIO_PENDING          // asynchronous sensor response is still in flight
};

enum EnrollStart : uint8_t {
  ES_BEGIN_OK,
  ES_BEGIN_NO_WORK,
  ES_BEGIN_BUSY,
  ES_BEGIN_BAD_CAPACITY,
  ES_BEGIN_OCCUPANCY_INVALID,
  ES_BEGIN_ROSTER_OVERFLOW,
  ES_BEGIN_STUDENT_LIMIT,
  ES_BEGIN_ID_OUT_OF_RANGE,
  ES_BEGIN_ROSTER_UNTRUSTED
};

enum EnrollState : uint8_t {
  EN_IDLE,
  EN_WAIT_FIRST_FINGER,
  EN_CONVERT_FIRST,
  EN_SEARCH_DUPLICATE,
  EN_DUPLICATE_FOUND,
  EN_WAIT_SECOND_FINGER,
  EN_CONVERT_SECOND,
  EN_CREATE_MODEL,
  EN_STORE_MODEL,
  EN_WAIT_FINGER_REMOVAL,
  EN_WAIT_VERIFY_FINGER,
  EN_CONVERT_VERIFY,
  EN_SEARCH_VERIFY,
  EN_VERIFY_FAILED,
  EN_RETRY_REQUIRED,
  EN_SLOT_TAKEN,
  EN_ROSTER_CHANGED,
  EN_SENSOR_FAULT,
  EN_OCCUPANCY_REFRESH_REQUIRED,
  EN_DONE,
  EN_CANCELLED,
  EN_CANCEL_PENDING_RELEASE,
  EN_CANCEL_RELEASE_FAULT
};

// Hardware adapter contract. Each method performs one sensor operation and maps its
// module/library return values to EnrollIo. searchTemplate() MUST search the complete
// occupied sensor library and return a physical sensor page (not a student ID).
// No specific Adafruit_Fingerprint API is assumed or verified here.
class IEnrollSensor {
 public:
  virtual ~IEnrollSensor() {}
  virtual EnrollIo captureImage() = 0;
  virtual EnrollIo convertImage(uint8_t buffer) = 0;       // buffer is 1 or 2
  virtual EnrollIo searchTemplate(uint16_t& page, uint16_t& score) = 0;
  virtual EnrollIo createModel() = 0;
  virtual EnrollIo storeModel(uint16_t page) = 0;
  // True while one command is in flight. cancel() uses this to drain the current
  // command before starting the finger-release fence.
  virtual bool operationPending() const = 0;
};

// Queue-driven enrollment for students already present and active in RosterStore.
// One call to step() performs at most one sensor operation. The caller schedules step()
// from its service loop; no web-server or UI work belongs in this class. The adapter must
// exclusively own the sensor for the whole batch; attendance scan commands must not interleave.
class EnrollFsm {
 public:
  EnrollFsm(RosterStore& roster, Occupancy& occupancy, IEnrollSensor& sensor);

  // verifyAfterEnroll is selected by the admin for this batch. It is deliberately not
  // persisted in Config: no config-schema migration is implied by the ON/OFF control.
  EnrollStart begin(uint16_t sensorCapacity, bool verifyAfterEnroll);
  void step();
  bool retry();
  bool tryDifferentFinger();
  bool skipCurrent();
  bool retryVerification();
  bool continueAfterVerificationFailure();
  bool resumeAfterOccupancyRefresh();
  void cancel();
  bool retryCancelRelease();

  EnrollState state() const { return state_; }
  // True until EN_DONE or EN_CANCELLED. The scheduler must not issue attendance
  // commands while this is true, including during a cancellation-release fault.
  bool sensorReserved() const { return isBusy_(); }
  uint16_t currentId() const;
  uint16_t duplicateId() const;
  uint16_t duplicateScore() const { return duplicateId() ? matchScore_ : 0; }
  uint16_t queueSize() const { return queueCount_; }
  uint16_t queuePosition() const { return queuePos_ < queueCount_ ? (uint16_t)(queuePos_ + 1) : queueCount_; }
  uint16_t enrolledCount() const { return enrolledCount_; }
  uint16_t verifiedCount() const { return verifiedCount_; }
  uint16_t verificationFailedCount() const { return verificationFailedCount_; }
  uint16_t skippedCount() const { return skippedCount_; }
  bool verificationEnabled() const { return verifyAfterEnroll_; }

 private:
  bool isBusy_() const;
  bool rebuildQueue_(uint16_t afterId);
  bool canSkip_() const;
  void retryOperation_(EnrollState resumeState);
  void waitForRemoval_(EnrollState nextState);
  void finishCancelAfterPending_(EnrollIo result, bool wasCapture);
  void finishCurrent_();
  void enterSensorFault_();
  void handleCapture_(EnrollIo result, EnrollState next, EnrollState retryState);
  void handleConvert_(EnrollIo result, EnrollState next, EnrollState captureState);
  void handleSearch_(EnrollIo result, bool verifying);

  RosterStore& roster_;
  Occupancy& occupancy_;
  IEnrollSensor& sensor_;
  uint16_t queue_[ENROLL_MAX_QUEUE];
  uint16_t queueCount_, queuePos_;
  uint16_t capacity_;
  uint16_t enrolledCount_, verifiedCount_, verificationFailedCount_, skippedCount_;
  uint16_t matchPage_, matchScore_, uncertainStoreId_;
  uint8_t sensorErrors_, modelMismatches_;
  EnrollState state_, retryState_, afterRemovalState_;
  bool verifyAfterEnroll_, storedCurrent_, storeUncertain_, cancelRequested_;
};

}  // namespace att
