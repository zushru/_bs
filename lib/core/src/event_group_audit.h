#pragma once

#include "log_store.h"

namespace att {

// Validates current event commit-group framing and key attendance/coverage fields.
// Call attach() after a successful LogStore::open(); it replays the current log.
// The observer must outlive the LogStore registration. This is not a cryptographic
// integrity check or a complete semantic validator for future event types.
class EventGroupAudit : public LogObserver {
 public:
  explicit EventGroupAudit(uint32_t firstSeq = 1);
  EventGroupAudit(const EventGroupAudit&) = delete;
  EventGroupAudit& operator=(const EventGroupAudit&) = delete;
  bool attach(LogStore& log);  // attaches once; when log is open, replays the full log
  // `opened` supplies the original open/repair outcome; current sequence/count
  // are compared against the live LogStore so valid later appends stay auditable.
  bool readyFor(const LogStore& log, const OpenInfo& opened) const;
  void onReset() override;
  void onRecord(const Event& event) override;
  bool malformed() const { return malformed_; }
  bool pending() const { return count_ != 0; }
  bool sequenceError() const { return sequenceError_; }
  bool attached() const { return attached_; }
  uint32_t recordsSeen() const { return recordsSeen_; }
  uint32_t lastSeq() const { return lastSeq_; }
  uint32_t generation() const { return generation_; }

 private:
  bool validGroup_() const;
  uint32_t firstSeq_;
  Event group_[MAX_GROUP];
  size_t count_;
  bool malformed_;
  bool sequenceError_;
  bool attached_;
  uint32_t recordsSeen_;
  uint32_t lastSeq_;
  uint32_t generation_;
};

}  // namespace att
