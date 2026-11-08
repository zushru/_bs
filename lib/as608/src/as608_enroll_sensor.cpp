#include "as608_enroll_sensor.h"
#include <string.h>

namespace att {
namespace {
const uint8_t kPacketHeader0 = 0xEF;
const uint8_t kPacketHeader1 = 0x01;
const uint8_t kCommandPacket = 0x01;
const uint8_t kAckPacket = 0x07;
const uint32_t kSensorAddress = 0xFFFFFFFFUL;
const size_t kMaxRequest = 64;
const size_t kMaxResponse = 64;

uint16_t be16(const uint8_t* p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
}

As608EnrollSensor::As608EnrollSensor(IAs608Link& link, uint32_t sensorPassword,
                                     As608CommandOwner owner)
    : link_(link), owner_(owner), password_(sensorPassword), capacity_(0), authenticated_(false),
      commandPending_(false), activeInstruction_(0), activeParamCount_(0) {
  memset(activeParams_, 0, sizeof activeParams_);
}

As608EnrollSensor::CommandPoll As608EnrollSensor::command_(uint8_t instruction,
    const uint8_t* params, uint8_t paramCount, Ack& ack, uint32_t timeoutMs) {
  if (paramCount > sizeof activeParams_ || (paramCount && !params)) return CP_ERROR;

  if (commandPending_) {
    if (activeInstruction_ != instruction || activeParamCount_ != paramCount ||
        (paramCount && memcmp(activeParams_, params, paramCount) != 0)) return CP_ERROR;
    uint8_t response[kMaxResponse];
    size_t received = 0;
    const int polled = link_.pollTransaction(owner_, response, sizeof response, received);
    if (polled == AS608_LINK_PENDING) return CP_PENDING;
    commandPending_ = false;
    if (polled != AS608_LINK_OK) return CP_LINK_FAULT;
    if (received < 12 || received > sizeof response) return CP_ERROR;
    if (response[0] != kPacketHeader0 || response[1] != kPacketHeader1) return CP_ERROR;
    if (be16(response + 2) != 0xFFFF || be16(response + 4) != 0xFFFF) return CP_ERROR;
    if (response[6] != kAckPacket) return CP_ERROR;
    const uint16_t responseLength = be16(response + 7);
    if (responseLength < 3 || received != (size_t)responseLength + 9) return CP_ERROR;
    const size_t dataLength = responseLength - 3;
    if (dataLength > sizeof ack.data) return CP_ERROR;
    uint16_t expected = 0;
    for (size_t i = 6; i < received - 2; ++i) expected = (uint16_t)(expected + response[i]);
    const uint16_t supplied = be16(response + received - 2);
    if (expected != supplied) return CP_ERROR;
    ack.code = response[9];
    ack.dataLength = (uint8_t)dataLength;
    if (dataLength) memcpy(ack.data, response + 10, dataLength);
    return CP_ACK;
  }

  uint8_t request[kMaxRequest];
  const uint16_t packetLength = (uint16_t)paramCount + 3;
  request[0] = kPacketHeader0;
  request[1] = kPacketHeader1;
  request[2] = (uint8_t)(kSensorAddress >> 24);
  request[3] = (uint8_t)(kSensorAddress >> 16);
  request[4] = (uint8_t)(kSensorAddress >> 8);
  request[5] = (uint8_t)kSensorAddress;
  request[6] = kCommandPacket;
  request[7] = (uint8_t)(packetLength >> 8);
  request[8] = (uint8_t)packetLength;
  request[9] = instruction;
  if (paramCount) memcpy(request + 10, params, paramCount);
  uint16_t sum = (uint16_t)(kCommandPacket + request[7] + request[8] + instruction);
  for (uint8_t i = 0; i < paramCount; ++i) sum = (uint16_t)(sum + params[i]);
  request[10 + paramCount] = (uint8_t)(sum >> 8);
  request[11 + paramCount] = (uint8_t)sum;
  const int started = link_.startTransaction(owner_, request, (size_t)paramCount + 12, timeoutMs);
  if (started != AS608_LINK_OK) return CP_LINK_FAULT;
  commandPending_ = true;
  activeInstruction_ = instruction;
  activeParamCount_ = paramCount;
  if (paramCount) memcpy(activeParams_, params, paramCount);
  return CP_PENDING;
}

EnrollIo As608EnrollSensor::authenticate() {
  if (!commandPending_) { authenticated_ = false; capacity_ = 0; }
  const uint8_t password[4] = {
      (uint8_t)(password_ >> 24), (uint8_t)(password_ >> 16),
      (uint8_t)(password_ >> 8), (uint8_t)password_};
  Ack ack;
  const CommandPoll result = command_(0x13, password, sizeof password, ack, 1200);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x00) { authenticated_ = true; return EIO_OK; }
  return ack.code == 0x01 ? EIO_RETRYABLE : EIO_FATAL;
}

EnrollIo As608EnrollSensor::readCapacity(uint16_t& capacity) {
  capacity = 0;
  if (!authenticated_) return EIO_FATAL;
  if (!commandPending_) capacity_ = 0;
  Ack ack;
  const CommandPoll result = command_(0x0F, nullptr, 0, ack, 1200);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code != 0x00) return ack.code == 0x01 ? EIO_RETRYABLE : EIO_FATAL;
  // The diagnostic sketch reads capacity at ACK bytes 14..15; require its full 16-byte parameter block.
  if (ack.dataLength < 16) return EIO_FATAL;
  const uint16_t reported = be16(ack.data + 4);
  if (reported < 1 || reported > MAX_ID) return EIO_FATAL;
  capacity_ = reported;
  capacity = reported;
  return EIO_OK;
}

EnrollIo As608EnrollSensor::readIndexPage(uint8_t page, uint8_t out[32]) {
  if (!authenticated_ || !capacity_ || !out || page >= Occupancy::pagesNeeded(capacity_))
    return EIO_FATAL;
  Ack ack;
  const CommandPoll result = command_(0x1F, &page, 1, ack, 1500);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x01) return EIO_RETRYABLE;
  if (ack.code != 0x00) return EIO_FATAL;
  if (ack.dataLength != 32) return EIO_FATAL;
  memcpy(out, ack.data, 32);
  return EIO_OK;
}

bool As608EnrollSensor::imageError_(uint8_t code) {
  return code == 0x03 || code == 0x06 || code == 0x07 || code == 0x15;
}

EnrollIo As608EnrollSensor::mapImageOperation_(uint8_t code) {
  if (code == 0x00) return EIO_OK;
  if (code == 0x01) return EIO_RETRYABLE;
  if (imageError_(code)) return EIO_BAD_IMAGE;
  return EIO_FATAL;
}

EnrollIo As608EnrollSensor::captureImage() {
  if (!authenticated_) return EIO_FATAL;
  Ack ack;
  const CommandPoll result = command_(0x01, nullptr, 0, ack, 400);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x02) return EIO_NO_FINGER;
  return mapImageOperation_(ack.code);
}

EnrollIo As608EnrollSensor::convertImage(uint8_t buffer) {
  if (!authenticated_ || (buffer != 1 && buffer != 2)) return EIO_FATAL;
  Ack ack;
  const CommandPoll result = command_(0x02, &buffer, 1, ack, 1200);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  return mapImageOperation_(ack.code);
}

EnrollIo As608EnrollSensor::searchTemplate(uint16_t& page, uint16_t& score) {
  page = capacity_;
  score = 0;
  if (!authenticated_ || capacity_ == 0) return EIO_FATAL;
  // Search buffer 1 from physical page 0 for exactly the reported sensor capacity.
  const uint8_t params[5] = {1, 0, 0, (uint8_t)(capacity_ >> 8), (uint8_t)capacity_};
  Ack ack;
  const CommandPoll result = command_(0x04, params, sizeof params, ack, 2000);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x09) return EIO_NO_MATCH;
  if (ack.code == 0x01) return EIO_RETRYABLE;
  if (imageError_(ack.code)) return EIO_BAD_IMAGE;
  if (ack.code != 0x00 || ack.dataLength < 4) return EIO_FATAL;
  const uint16_t foundPage = be16(ack.data);
  if (foundPage >= capacity_) return EIO_FATAL;
  page = foundPage;
  score = be16(ack.data + 2);
  return EIO_OK;
}

EnrollIo As608EnrollSensor::createModel() {
  if (!authenticated_) return EIO_FATAL;
  Ack ack;
  const CommandPoll result = command_(0x05, nullptr, 0, ack, 1500);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x0A) return EIO_MISMATCH;
  return mapImageOperation_(ack.code);
}

EnrollIo As608EnrollSensor::storeModel(uint16_t page) {
  if (!authenticated_ || capacity_ == 0 || page >= capacity_) return EIO_FATAL;
  const uint8_t params[3] = {1, (uint8_t)(page >> 8), (uint8_t)page};
  Ack ack;
  const CommandPoll result = command_(0x06, params, sizeof params, ack, 2000);
  if (result == CP_PENDING) return EIO_PENDING;
  if (result == CP_LINK_FAULT) return EIO_FATAL;
  if (result == CP_ERROR) return EIO_RETRYABLE;
  if (ack.code == 0x00) return EIO_OK;
  // The FSM treats every non-OK store result as ambiguous and forces a full index refresh.
  if (ack.code == 0x01 || ack.code == 0x18) return EIO_RETRYABLE;
  return EIO_FATAL;
}


As608OccupancyReader::As608OccupancyReader(As608EnrollSensor& sensor, Occupancy& occupancy)
    : sensor_(sensor), occupancy_(occupancy), capacity_(0), page_(0), pages_(0), active_(false) {}

bool As608OccupancyReader::begin() {
  if (active_ || sensor_.operationPending()) return false;
  const uint16_t cap = sensor_.capacity();
  if (cap < 1 || cap > MAX_ID) return false;
  capacity_ = cap;
  page_ = 0;
  pages_ = Occupancy::pagesNeeded(capacity_);
  if (pages_ == 0 || pages_ > 4) return false;
  occupancy_.begin();
  active_ = true;
  return true;
}

EnrollIo As608OccupancyReader::step() {
  if (!active_) return EIO_FATAL;
  uint8_t bitmap[32] = {0};
  const EnrollIo result = sensor_.readIndexPage(page_, bitmap);
  if (result == EIO_PENDING) return EIO_PENDING;
  if (result != EIO_OK) {
    occupancy_.begin();  // discard any earlier pages; a partial refresh is never trusted
    active_ = false;
    return result;
  }
  occupancy_.addIndexPage(page_, bitmap, capacity_);
  ++page_;
  if (page_ >= pages_) {
    occupancy_.finish();
    active_ = false;
    return EIO_OK;
  }
  return EIO_PENDING;
}

As608EnrollmentSession::As608EnrollmentSession(As608SensorArbiter& arbiter,
    EnrollFsm& fsm, As608EnrollSensor& sensor, As608OccupancyReader& reader)
    : lease_(arbiter), fsm_(fsm), sensor_(sensor), reader_(reader) {}

bool As608EnrollmentSession::acquire() {
  if (fsm_.sensorReserved() || sensor_.operationPending() || reader_.active()) return false;
  return lease_.acquire();
}

bool As608EnrollmentSession::sync() {
  if (sensor_.linkFaulted()) return false;
  return lease_.finish(fsm_.state(), sensor_.operationPending(), reader_.active());
}

bool As608EnrollmentSession::abortPreparation() {
  if (sensor_.linkFaulted() || fsm_.sensorReserved() || fsm_.state() != EN_IDLE) return false;
  return lease_.abortPreparation(sensor_.operationPending(), reader_.active());
}

}  // namespace att
