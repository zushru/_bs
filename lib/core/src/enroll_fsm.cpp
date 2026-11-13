#include "enroll_fsm.h"

namespace att {

EnrollFsm::EnrollFsm(RosterStore& roster, Occupancy& occupancy, IEnrollSensor& sensor)
    : roster_(roster), occupancy_(occupancy), sensor_(sensor), queueCount_(0), queuePos_(0),
      capacity_(0), enrolledCount_(0), verifiedCount_(0), verificationFailedCount_(0),
      skippedCount_(0), matchPage_(0), matchScore_(0), uncertainStoreId_(0),
      sensorErrors_(0), modelMismatches_(0), state_(EN_IDLE), retryState_(EN_IDLE),
      afterRemovalState_(EN_IDLE), verifyAfterEnroll_(false), storedCurrent_(false), storeUncertain_(false),
      cancelRequested_(false) {
  for (uint8_t i = 0; i < ENROLL_MAX_QUEUE; i++) queue_[i] = 0;
}

bool EnrollFsm::isBusy_() const {
  return state_ != EN_IDLE && state_ != EN_DONE && state_ != EN_CANCELLED;
}

uint16_t EnrollFsm::currentId() const {
  return queuePos_ < queueCount_ ? queue_[queuePos_] : 0;
}

uint16_t EnrollFsm::duplicateId() const {
  if (state_ != EN_DUPLICATE_FOUND && state_ != EN_VERIFY_FAILED) return 0;
  return matchPage_ < capacity_ ? pageToId(matchPage_) : 0;
}

bool EnrollFsm::rebuildQueue_(uint16_t afterId) {
  if (!occupancy_.valid || !roster_.historyComplete()) return false;
  for (uint16_t id = (uint16_t)(capacity_ + 1); id <= MAX_ID; id++)
    if (roster_.active(id)) return false;
  uint16_t pending[ENROLL_MAX_QUEUE];
  uint16_t count = 0;
  const uint16_t first = afterId >= capacity_ ? (uint16_t)(capacity_ + 1) : (uint16_t)(afterId + 1);
  for (uint16_t id = first; id <= capacity_; id++) {
    if (!roster_.active(id) || occupancy_.has(id)) continue;
    if (count >= ENROLL_MAX_QUEUE) return false;
    pending[count++] = id;
  }
  for (uint8_t i = 0; i < ENROLL_MAX_QUEUE; i++) queue_[i] = i < count ? pending[i] : 0;
  queueCount_ = count;
  queuePos_ = 0;
  return true;
}

EnrollStart EnrollFsm::begin(uint16_t sensorCapacity, bool verifyAfterEnroll) {
  if (isBusy_()) return ES_BEGIN_BUSY;
  state_ = EN_IDLE;
  queueCount_ = queuePos_ = 0;
  enrolledCount_ = verifiedCount_ = verificationFailedCount_ = skippedCount_ = 0;
  matchPage_ = matchScore_ = uncertainStoreId_ = 0;
  sensorErrors_ = modelMismatches_ = 0;
  storedCurrent_ = storeUncertain_ = false;
  cancelRequested_ = false;
  if (sensorCapacity < 1 || sensorCapacity > MAX_ID) return ES_BEGIN_BAD_CAPACITY;
  if (!occupancy_.valid) return ES_BEGIN_OCCUPANCY_INVALID;
  if (roster_.overflow()) return ES_BEGIN_ROSTER_OVERFLOW;
  if (!roster_.historyComplete()) return ES_BEGIN_ROSTER_UNTRUSTED;
  capacity_ = sensorCapacity;
  for (uint16_t id = (uint16_t)(capacity_ + 1); id <= MAX_ID; id++)
    if (roster_.active(id)) return ES_BEGIN_ID_OUT_OF_RANGE;
  for (uint16_t id = 1; id <= capacity_; id++)
    if (roster_.active(id) && ++queueCount_ > ENROLL_MAX_QUEUE) {
      queueCount_ = 0;
      return ES_BEGIN_STUDENT_LIMIT;
    }
  queueCount_ = 0;
  if (!rebuildQueue_(0)) {
    if (roster_.overflow()) return ES_BEGIN_ROSTER_OVERFLOW;
    if (!roster_.historyComplete()) return ES_BEGIN_ROSTER_UNTRUSTED;
    return ES_BEGIN_STUDENT_LIMIT;
  }
  verifyAfterEnroll_ = verifyAfterEnroll;
  if (queueCount_ == 0) {
    state_ = EN_DONE;
    return ES_BEGIN_NO_WORK;
  }
  state_ = EN_WAIT_FIRST_FINGER;
  return ES_BEGIN_OK;
}

void EnrollFsm::retryOperation_(EnrollState resumeState) {
  retryState_ = resumeState;
  if (++sensorErrors_ >= ENROLL_MAX_SENSOR_ERRORS) {
    sensorErrors_ = 0;
    state_ = EN_RETRY_REQUIRED;
  }
}

void EnrollFsm::waitForRemoval_(EnrollState nextState) {
  afterRemovalState_ = nextState;
  state_ = EN_WAIT_FINGER_REMOVAL;
}

void EnrollFsm::enterSensorFault_() {
  state_ = EN_SENSOR_FAULT;
}

void EnrollFsm::handleCapture_(EnrollIo result, EnrollState next, EnrollState retryState) {
  if (result == EIO_PENDING) return;
  if (result == EIO_NO_FINGER) { sensorErrors_ = 0; return; }
  if (result == EIO_OK) {
    sensorErrors_ = 0;
    state_ = next;
  } else if (result == EIO_BAD_IMAGE) {
    waitForRemoval_(retryState);
    retryOperation_(EN_WAIT_FINGER_REMOVAL);
  } else if (result == EIO_RETRYABLE) {
    retryOperation_(retryState);
  } else {
    enterSensorFault_();
  }
}

void EnrollFsm::handleConvert_(EnrollIo result, EnrollState next, EnrollState captureState) {
  if (result == EIO_PENDING) return;
  if (result == EIO_OK) {
    sensorErrors_ = 0;
    state_ = next;
  } else if (result == EIO_BAD_IMAGE || result == EIO_RETRYABLE) {
    waitForRemoval_(captureState);
    retryOperation_(EN_WAIT_FINGER_REMOVAL);
  } else {
    enterSensorFault_();
  }
}

void EnrollFsm::handleSearch_(EnrollIo result, bool verifying) {
  if (result == EIO_PENDING) return;
  if (result == EIO_NO_MATCH) {
    sensorErrors_ = 0;
    matchPage_ = capacity_;  // no match; duplicateId() must not report page 0 / student 1
    matchScore_ = 0;
    if (verifying) {
      state_ = EN_VERIFY_FAILED;
    } else {
      waitForRemoval_(EN_WAIT_SECOND_FINGER);
    }
    return;
  }
  if (result == EIO_OK) {
    sensorErrors_ = 0;
    if (matchPage_ >= capacity_) {
      enterSensorFault_();
      return;
    }
    if (verifying) {
      if (matchPage_ == idToPage(currentId())) {
        ++verifiedCount_;
        finishCurrent_();
      } else {
        state_ = EN_VERIFY_FAILED;
      }
    } else {
      state_ = EN_DUPLICATE_FOUND;
    }
    return;
  }
  if (result == EIO_BAD_IMAGE || result == EIO_RETRYABLE) {
    waitForRemoval_(verifying ? EN_WAIT_VERIFY_FINGER : EN_WAIT_FIRST_FINGER);
    retryOperation_(EN_WAIT_FINGER_REMOVAL);
  } else {
    enterSensorFault_();
  }
}

void EnrollFsm::step() {
  if (!isBusy_()) return;
  EnrollIo io = EIO_FATAL;
  uint16_t page = 0, score = 0;
  switch (state_) {
    case EN_WAIT_FIRST_FINGER:
      io = sensor_.captureImage();
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, true); break; }
      handleCapture_(io, EN_CONVERT_FIRST, EN_WAIT_FIRST_FINGER);
      break;
    case EN_CONVERT_FIRST:
      io = sensor_.convertImage(1);
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      handleConvert_(io, EN_SEARCH_DUPLICATE, EN_WAIT_FIRST_FINGER);
      break;
    case EN_SEARCH_DUPLICATE:
      io = sensor_.searchTemplate(page, score);
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      matchPage_ = page;
      matchScore_ = score;
      handleSearch_(io, false);
      break;
    case EN_WAIT_SECOND_FINGER:
      io = sensor_.captureImage();
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, true); break; }
      handleCapture_(io, EN_CONVERT_SECOND, EN_WAIT_SECOND_FINGER);
      break;
    case EN_CONVERT_SECOND:
      io = sensor_.convertImage(2);
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      handleConvert_(io, EN_CREATE_MODEL, EN_WAIT_SECOND_FINGER);
      break;
    case EN_CREATE_MODEL:
      io = sensor_.createModel();
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      if (io == EIO_OK) {
        sensorErrors_ = 0;
        modelMismatches_ = 0;
        state_ = EN_STORE_MODEL;
      } else if (io == EIO_MISMATCH || io == EIO_BAD_IMAGE) {
        if (++modelMismatches_ >= ENROLL_MAX_SENSOR_ERRORS) {
          modelMismatches_ = 0;
          afterRemovalState_ = EN_WAIT_FIRST_FINGER;
          retryState_ = EN_WAIT_FINGER_REMOVAL;
          state_ = EN_RETRY_REQUIRED;
        } else {
          waitForRemoval_(EN_WAIT_FIRST_FINGER);
        }
      } else if (io == EIO_RETRYABLE) {
        retryOperation_(EN_CREATE_MODEL);
      } else {
        enterSensorFault_();
      }
      break;
    case EN_STORE_MODEL: {
      const uint16_t id = currentId();
      if (!sensor_.operationPending() && !occupancy_.valid) {
        state_ = EN_OCCUPANCY_REFRESH_REQUIRED;
        break;
      }
      if (!sensor_.operationPending() &&
          (!roster_.historyComplete() || roster_.overflow() || !roster_.active(id))) {
        state_ = EN_ROSTER_CHANGED;
        break;
      }
      if (!sensor_.operationPending() && occupancy_.has(id)) {
        state_ = EN_SLOT_TAKEN;
        break;
      }
      io = sensor_.storeModel(idToPage(id));
      if (io == EIO_PENDING) break;
      if (cancelRequested_) {
        if (io == EIO_OK) {
          occupancy_.setPresent(id, true);
          ++enrolledCount_;
        } else {
          occupancy_.begin();  // Store outcome is uncertain; attendance eligibility must fail closed.
        }
        finishCancelAfterPending_(io, false);
        break;
      }
      if (io != EIO_OK) {
        // A lost/error reply can mean the sensor stored the template anyway. Never retry
        // the write blindly. Invalidate occupancy and require a complete index refresh.
        uncertainStoreId_ = id;
        storeUncertain_ = true;
        occupancy_.begin();
        state_ = EN_OCCUPANCY_REFRESH_REQUIRED;
        break;
      }
      occupancy_.setPresent(id, true);
      storedCurrent_ = true;
      ++enrolledCount_;
      sensorErrors_ = 0;
      if (verifyAfterEnroll_) waitForRemoval_(EN_WAIT_VERIFY_FINGER);
      else finishCurrent_();
      break;
    }
    case EN_WAIT_FINGER_REMOVAL:
      // Do not reuse a still-held finger for the next capture stage.
      io = sensor_.captureImage();
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, true); break; }
      if (io == EIO_NO_FINGER) { sensorErrors_ = 0; state_ = afterRemovalState_; }
      else if (io == EIO_OK) { sensorErrors_ = 0; /* still held: keep waiting */ }
      else if (io == EIO_RETRYABLE || io == EIO_BAD_IMAGE) retryOperation_(EN_WAIT_FINGER_REMOVAL);
      else enterSensorFault_();
      break;
    case EN_WAIT_VERIFY_FINGER:
      io = sensor_.captureImage();
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, true); break; }
      handleCapture_(io, EN_CONVERT_VERIFY, EN_WAIT_VERIFY_FINGER);
      break;
    case EN_CANCEL_PENDING_RELEASE:
      io = sensor_.captureImage();
      if (io == EIO_PENDING) {
        // The release probe itself is asynchronous; keep the reservation while it runs.
        break;
      }
      if (io == EIO_NO_FINGER) {
        sensorErrors_ = 0;
        state_ = EN_CANCELLED;
      } else if (io == EIO_OK) {
        sensorErrors_ = 0;  // Finger remains present; continue holding sensor ownership.
      } else if (io == EIO_BAD_IMAGE || io == EIO_RETRYABLE) {
        if (++sensorErrors_ >= ENROLL_MAX_SENSOR_ERRORS) {
          sensorErrors_ = 0;
          state_ = EN_CANCEL_RELEASE_FAULT;
        }
      } else {
        sensorErrors_ = 0;
        state_ = EN_CANCEL_RELEASE_FAULT;
      }
      break;
    case EN_CANCEL_RELEASE_FAULT:
      // Fail closed: only an explicit retry can resume the finger-release fence.
      break;
    case EN_CONVERT_VERIFY:
      io = sensor_.convertImage(1);
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      handleConvert_(io, EN_SEARCH_VERIFY, EN_WAIT_VERIFY_FINGER);
      break;
    case EN_SEARCH_VERIFY:
      io = sensor_.searchTemplate(page, score);
      if (io == EIO_PENDING) break;
      if (cancelRequested_) { finishCancelAfterPending_(io, false); break; }
      matchPage_ = page;
      matchScore_ = score;
      handleSearch_(io, true);
      break;
    default:
      break;
  }
}

void EnrollFsm::finishCurrent_() {
  storedCurrent_ = false;
  sensorErrors_ = modelMismatches_ = 0;
  ++queuePos_;
  if (queuePos_ < queueCount_) {
    // Never let the previous student's still-held finger become the next student's sample.
    waitForRemoval_(EN_WAIT_FIRST_FINGER);
  } else {
    // Do not release the sensor to attendance scanning while an enrollment finger is still down.
    waitForRemoval_(EN_DONE);
  }
}

bool EnrollFsm::retry() {
  if (state_ != EN_RETRY_REQUIRED) return false;
  sensorErrors_ = modelMismatches_ = 0;
  state_ = retryState_;
  return true;
}

bool EnrollFsm::tryDifferentFinger() {
  if (state_ != EN_DUPLICATE_FOUND) return false;
  matchPage_ = capacity_;
  matchScore_ = 0;
  sensorErrors_ = modelMismatches_ = 0;
  waitForRemoval_(EN_WAIT_FIRST_FINGER);
  return true;
}

bool EnrollFsm::canSkip_() const {
  if (storedCurrent_ || currentId() == 0) return false;
  switch (state_) {
    case EN_WAIT_FIRST_FINGER:
    case EN_WAIT_FINGER_REMOVAL:
    case EN_CONVERT_FIRST:
    case EN_SEARCH_DUPLICATE:
    case EN_DUPLICATE_FOUND:
    case EN_WAIT_SECOND_FINGER:
    case EN_CONVERT_SECOND:
    case EN_CREATE_MODEL:
    case EN_RETRY_REQUIRED:
    case EN_SLOT_TAKEN:
    case EN_ROSTER_CHANGED:
      return true;
    default:
      return false;
  }
}

bool EnrollFsm::skipCurrent() {
  // Never replace the FSM state while an asynchronous command still owns the UART.
  if (sensor_.operationPending() || !canSkip_()) return false;
  ++skippedCount_;
  finishCurrent_();
  return true;
}

bool EnrollFsm::retryVerification() {
  if (state_ != EN_VERIFY_FAILED || !storedCurrent_) return false;
  matchPage_ = capacity_;
  matchScore_ = 0;
  sensorErrors_ = 0;
  waitForRemoval_(EN_WAIT_VERIFY_FINGER);
  return true;
}

bool EnrollFsm::continueAfterVerificationFailure() {
  if (state_ != EN_VERIFY_FAILED || !storedCurrent_) return false;
  ++verificationFailedCount_;
  finishCurrent_();
  return true;
}

bool EnrollFsm::resumeAfterOccupancyRefresh() {
  if (state_ != EN_OCCUPANCY_REFRESH_REQUIRED || !occupancy_.valid ||
      !roster_.historyComplete()) return false;
  uint16_t afterId = currentId() ? (uint16_t)(currentId() - 1) : 0;
  if (storeUncertain_) {
    // If the full index now contains the target, the previous store took effect despite
    // its missing/error reply. Count it once and do not send a second store command.
    if (occupancy_.has(uncertainStoreId_)) {
      ++enrolledCount_;
      afterId = uncertainStoreId_;
    } else {
      afterId = (uint16_t)(uncertainStoreId_ - 1);
    }
    storeUncertain_ = false;
    uncertainStoreId_ = 0;
  }
  if (!rebuildQueue_(afterId)) return false;
  storedCurrent_ = false;
  sensorErrors_ = modelMismatches_ = 0;
  // Refresh may have followed an ambiguous store while a finger was still placed.
  // Keep sensor ownership until its removal is positively observed.
  waitForRemoval_(queueCount_ ? EN_WAIT_FIRST_FINGER : EN_DONE);
  return true;
}

void EnrollFsm::finishCancelAfterPending_(EnrollIo result, bool wasCapture) {
  cancelRequested_ = false;
  storedCurrent_ = false;
  sensorErrors_ = 0;
  state_ = (wasCapture && result == EIO_NO_FINGER) ? EN_CANCELLED : EN_CANCEL_PENDING_RELEASE;
}

void EnrollFsm::cancel() {
  if (state_ == EN_IDLE || state_ == EN_DONE || state_ == EN_CANCELLED ||
      state_ == EN_CANCEL_PENDING_RELEASE || state_ == EN_CANCEL_RELEASE_FAULT) return;
  storedCurrent_ = false;
  sensorErrors_ = 0;
  if (sensor_.operationPending()) cancelRequested_ = true;
  else state_ = EN_CANCEL_PENDING_RELEASE;
}

bool EnrollFsm::retryCancelRelease() {
  if (state_ != EN_CANCEL_RELEASE_FAULT) return false;
  sensorErrors_ = 0;
  state_ = EN_CANCEL_PENDING_RELEASE;
  return true;
}

}  // namespace att
