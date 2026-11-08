#include "as608_attendance_client.h"

namespace att {

As608AttendanceClient::As608AttendanceClient(As608SensorArbiter& arbiter,
    IAs608Link& link, uint32_t sensorPassword)
    : lease_(arbiter), sensor_(link, sensorPassword, AS608_OWNER_ATTENDANCE),
      state_(ST_IDLE), ready_(false) {}

bool As608AttendanceClient::beginSetup() {
  if (ready_ || state_ != ST_IDLE || sensor_.operationPending()) return false;
  if (!lease_.acquire()) return false;
  state_ = ST_SETUP_AUTH;
  return true;
}

As608AttendanceResult As608AttendanceClient::mapError_(EnrollIo result) {
  if (result == EIO_BAD_IMAGE) return AS608_ATT_BAD_IMAGE;
  if (result == EIO_RETRYABLE) return AS608_ATT_RETRYABLE;
  return AS608_ATT_FATAL;
}

As608AttendanceResult As608AttendanceClient::finishSetup_(As608AttendanceResult result,
                                                            bool ready) {
  if (sensor_.linkFaulted() ||
      !lease_.release(sensor_.operationPending(), sensor_.linkFaulted())) {
    ready_ = false;
    state_ = ST_FAULT;
    return AS608_ATT_FATAL;
  }
  ready_ = ready;
  state_ = ST_IDLE;
  return result;
}

As608AttendanceResult As608AttendanceClient::stepSetup() {
  if (state_ == ST_SETUP_AUTH) {
    const EnrollIo result = sensor_.authenticate();
    if (result == EIO_PENDING) return AS608_ATT_PENDING;
    if (result == EIO_OK) {
      state_ = ST_SETUP_CAPACITY;
      return AS608_ATT_PENDING;
    }
    return finishSetup_(mapError_(result), false);
  }

  if (state_ == ST_SETUP_CAPACITY) {
    uint16_t capacity = 0;
    const EnrollIo result = sensor_.readCapacity(capacity);
    if (result == EIO_PENDING) return AS608_ATT_PENDING;
    if (result == EIO_OK) return finishSetup_(AS608_ATT_READY, true);
    return finishSetup_(mapError_(result), false);
  }
  return AS608_ATT_FATAL;
}

bool As608AttendanceClient::beginScan() {
  if (!ready_ || state_ != ST_IDLE || sensor_.operationPending()) return false;
  if (!lease_.acquire()) return false;
  state_ = ST_SCAN_CAPTURE;
  return true;
}

As608AttendanceResult As608AttendanceClient::finishScan_(As608AttendanceResult result) {
  if (sensor_.linkFaulted() ||
      !lease_.release(sensor_.operationPending(), sensor_.linkFaulted())) {
    ready_ = false;
    state_ = ST_FAULT;
    return AS608_ATT_FATAL;
  }
  state_ = ST_IDLE;
  return result;
}

As608AttendanceResult As608AttendanceClient::stepScan(uint16_t& physicalPage,
                                                       uint16_t& score) {
  physicalPage = 0xFFFF;
  score = 0;

  if (state_ == ST_SCAN_CAPTURE) {
    const EnrollIo result = sensor_.captureImage();
    if (result == EIO_PENDING) return AS608_ATT_PENDING;
    if (result == EIO_NO_FINGER) return finishScan_(AS608_ATT_NO_FINGER);
    if (result == EIO_OK) {
      state_ = ST_SCAN_CONVERT;
      return AS608_ATT_PENDING;
    }
    return finishScan_(mapError_(result));
  }

  if (state_ == ST_SCAN_CONVERT) {
    const EnrollIo result = sensor_.convertImage(1);
    if (result == EIO_PENDING) return AS608_ATT_PENDING;
    if (result == EIO_OK) {
      state_ = ST_SCAN_SEARCH;
      return AS608_ATT_PENDING;
    }
    return finishScan_(mapError_(result));
  }

  if (state_ == ST_SCAN_SEARCH) {
    uint16_t foundPage = 0, foundScore = 0;
    const EnrollIo result = sensor_.searchTemplate(foundPage, foundScore);
    if (result == EIO_PENDING) return AS608_ATT_PENDING;
    if (result == EIO_OK) {
      physicalPage = foundPage;
      score = foundScore;
      return finishScan_(AS608_ATT_MATCH);
    }
    if (result == EIO_NO_MATCH) return finishScan_(AS608_ATT_NO_MATCH);
    return finishScan_(mapError_(result));
  }

  return AS608_ATT_FATAL;
}

bool As608AttendanceClient::active() const {
  return state_ != ST_IDLE;
}

}  // namespace att
