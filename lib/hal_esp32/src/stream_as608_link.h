#pragma once
#include <Arduino.h>
#include "as608_enroll_sensor.h"

namespace att {

// Byte transport over an Arduino Stream (normally Serial2). Receive polling is
// bounded and does not wait. Stream::write() is still synchronous; its latency is
// not characterized. Any timeout, malformed frame, overflow or partial write poisons
// this instance: without a transaction ID, a late ACK cannot safely be distinguished
// from the next command's ACK. Recovery requires a full power-cycle including the
// AS608 supply; rebuilding only the ESP32 object is not proven safe.
// The supplied arbiter must be the same instance used by the enrollment session and
// every attendance client. This class acquires a per-command lease; attendance-owner
// transactions also require an active attendance lifecycle lease. It is single-thread only.
class StreamAs608Link : public IAs608Link {
 public:
  StreamAs608Link(Stream& stream, As608SensorArbiter& arbiter);
  StreamAs608Link(const StreamAs608Link&) = delete;
  StreamAs608Link& operator=(const StreamAs608Link&) = delete;
  int startTransaction(As608CommandOwner owner, const uint8_t* request,
                       size_t requestLength, uint32_t timeoutMs) override;
  int pollTransaction(As608CommandOwner owner, uint8_t* response,
                      size_t responseCapacity, size_t& responseLength) override;
  bool faulted() const override { return faulted_; }
 private:
  int consumeByte_(uint8_t value);
  void reset_();

  Stream& stream_;
  As608SensorArbiter& arbiter_;
  As608CommandOwner activeOwner_;
  bool commandLease_;
  uint8_t frame_[64];
  size_t received_, expected_;
  uint32_t startMs_, timeoutMs_;
  bool active_, prefix_, faulted_;
};

}  // namespace att
