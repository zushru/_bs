#pragma once
#include "event_codec.h"
#include "istorage.h"
namespace att {
const size_t MAX_GROUP = 16;
const size_t MAX_OBSERVERS = 16;
struct OpenInfo {
  bool ok;
  uint32_t records, lastSeq, baselineEpoch;
  uint32_t tornBytes;
  uint32_t droppedGroup;
  int32_t corruptAt;
  uint64_t generation;   // identifies the specific LogStore::open() result
};
// Derived state (indexes, caches) follows the log. onReset = log (re)opened: discard and replay.
class LogObserver {
 public:
  virtual ~LogObserver() {}
  virtual void onReset() = 0;
  virtual void onRecord(const Event& e) = 0;   // in log order, only after the record is durable
};
class LogStore {
 public:
  explicit LogStore(IStorage& s, uint32_t firstSeq = 1)
      : s_(s), count_(0), nextSeq_(firstSeq), firstSeq_(firstSeq), baseline_(0),
        openGeneration_(0), faulted_(true), nobs_(0) {
    for (size_t i = 0; i < MAX_OBSERVERS; i++) obs_[i] = nullptr;
  }
  OpenInfo open();
  // false latches faulted(); storage may contain none, partial, or all bytes.
  // Reopen/reconcile before retrying the same logical operation.
  bool append(const Event* evs, size_t n);
  bool at(size_t i, Event& e);
  bool attach(LogObserver* o);                       // replays current records into o; false = not attached
  bool hasObserver(const LogObserver* observer) const;
  bool usesStorage(const IStorage& storage) const { return &s_ == &storage; }
  size_t count() const { return count_; }
  uint32_t lastSeq() const { return nextSeq_ - 1; }
  uint32_t baselineEpoch() const { return baseline_; }
  uint64_t openGeneration() const { return openGeneration_; }
  bool faulted() const { return faulted_; }
 private:
  void track(const Event& e);
  void notifyRecord(const Event& e);
  void notifyReset();
  IStorage& s_;
  size_t count_;
  uint32_t nextSeq_, firstSeq_, baseline_;
  uint64_t openGeneration_;
  bool faulted_;
  LogObserver* obs_[MAX_OBSERVERS];
  size_t nobs_;
};
// Chunks of GAP records that fit in maxOut events. *next (optional) = where coverage stopped; == to when complete.
// A caller that ignores *next silently drops the tail of a long span: use appendGaps() or check it.
// True only when whole-minute GAP records can cover [from,to) without the rounded
// exclusive endpoint exceeding UINT32_MAX. Empty spans are representable; reversed
// spans are not.
bool gapSpanEncodable(uint32_t from, uint32_t to);
size_t makeGapEvents(uint32_t from, uint32_t to, uint16_t cause, Event* out, size_t maxOut, uint32_t* next = nullptr);
// Records GAPs covering [from,to) in as many commit groups as needed. Invalid or
// unrepresentable ranges are rejected before writing. False after preflight means an
// append was attempted and may have persisted none, part, or all of its group.
// Repeating after reopen can append duplicate GAP records: this is not exactly-once.
bool appendGaps(LogStore& log, uint32_t from, uint32_t to, uint16_t cause);
}
