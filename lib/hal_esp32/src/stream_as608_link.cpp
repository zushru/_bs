#include "stream_as608_link.h"
#include <string.h>

namespace att {

StreamAs608Link::StreamAs608Link(Stream& stream, As608SensorArbiter& arbiter)
    : stream_(stream), arbiter_(arbiter), activeOwner_(AS608_OWNER_ATTENDANCE),
      commandLease_(false), received_(0), expected_(0), startMs_(0), timeoutMs_(0),
      active_(false), prefix_(false), faulted_(false) {}

void StreamAs608Link::reset_() {
  received_ = expected_ = 0;
  startMs_ = timeoutMs_ = 0;
  active_ = prefix_ = false;
}

int StreamAs608Link::startTransaction(As608CommandOwner owner, const uint8_t* request,
                                      size_t requestLength, uint32_t timeoutMs) {
  if (!request || requestLength < 12 || requestLength > sizeof frame_ || timeoutMs == 0)
    return AS608_LINK_BAD_ARGUMENT;
  if (faulted_) return AS608_LINK_POISONED;
  if (active_ || commandLease_) return AS608_LINK_IO_ERROR;
  if (!arbiter_.beginCommand(owner)) return AS608_LINK_BUSY;
  commandLease_ = true;
  activeOwner_ = owner;
  // Do not blindly flush RX: it cannot prove that a late ACK will not arrive after
  // the flush. Normal framing hunts for EF01; an earlier transaction timeout poisons
  // the link rather than risking misattribution of a late response.
  if (stream_.write(request, requestLength) != requestLength) {
    faulted_ = true;
    arbiter_.finishCommand(owner);
    commandLease_ = false;
    return AS608_LINK_IO_ERROR;
  }
  received_ = expected_ = 0;
  prefix_ = false;
  startMs_ = millis();
  timeoutMs_ = timeoutMs;
  active_ = true;
  return AS608_LINK_OK;
}

int StreamAs608Link::consumeByte_(uint8_t value) {
  if (received_ == 0) {
    if (!prefix_) {
      if (value == 0xEF) { frame_[0] = value; prefix_ = true; }
    } else if (value == 0x01) {
      frame_[1] = value;
      received_ = 2;
      prefix_ = false;
    } else if (value == 0xEF) {
      frame_[0] = value;
    } else {
      prefix_ = false;
    }
    return 0;
  }

  if (received_ >= sizeof frame_) return AS608_LINK_OVERFLOW;
  frame_[received_++] = value;
  if (received_ == 9) {
    const uint16_t packetLength = (uint16_t)(((uint16_t)frame_[7] << 8) | frame_[8]);
    if (packetLength < 3) return AS608_LINK_BAD_FRAME;
    expected_ = (size_t)packetLength + 9;
    if (expected_ > sizeof frame_) return AS608_LINK_OVERFLOW;
  }
  return expected_ && received_ == expected_ ? 1 : 0;
}

int StreamAs608Link::pollTransaction(As608CommandOwner owner, uint8_t* response,
                                     size_t responseCapacity, size_t& responseLength) {
  responseLength = 0;
  if (faulted_) return AS608_LINK_POISONED;
  if (!active_ || !response || responseCapacity < 12) return AS608_LINK_BAD_ARGUMENT;
  if (!commandLease_ || owner != activeOwner_) return AS608_LINK_BUSY;
  if ((uint32_t)(millis() - startMs_) >= timeoutMs_) {
    faulted_ = true;
    reset_();
    arbiter_.finishCommand(owner);
    commandLease_ = false;
    return AS608_LINK_TIMEOUT;
  }
  // Bound RX work per loop iteration so noisy input cannot monopolize the main loop.
  for (uint8_t processed = 0; processed < 32 && stream_.available() > 0; ++processed) {
    const int value = stream_.read();
    if (value < 0) break;
    const int state = consumeByte_((uint8_t)value);
    if (state < 0) {
      faulted_ = true; reset_();
      arbiter_.finishCommand(owner); commandLease_ = false;
      return state;
    }
    if (state == 1) {
      if (received_ > responseCapacity) {
        faulted_ = true; reset_();
        arbiter_.finishCommand(owner); commandLease_ = false;
        return AS608_LINK_OVERFLOW;
      }
      memcpy(response, frame_, received_);
      responseLength = received_;
      reset_();
      arbiter_.finishCommand(owner); commandLease_ = false;
      return AS608_LINK_OK;
    }
  }
  if ((uint32_t)(millis() - startMs_) >= timeoutMs_) {
    faulted_ = true;
    reset_();
    arbiter_.finishCommand(owner);
    commandLease_ = false;
    return AS608_LINK_TIMEOUT;
  }
  return AS608_LINK_PENDING;
}

}  // namespace att
