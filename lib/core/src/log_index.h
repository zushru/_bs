#pragma once
#include "log_store.h"
namespace att {
const size_t MAX_GAPS = 512;     // ~2 years of GAPs at one per fault; overflow = coverage cannot be proven
const size_t MAX_CLOSES = 800;   // 400 school days x 2 sessions; overflow = closure refused
class LogIndex : public LogObserver {
 public:
  LogIndex() { reset_(); }
  void onReset() override { reset_(); }
  void onRecord(const Event& e) override;
  bool closed(int32_t day, uint8_t session) const;
  bool gapOverlaps(uint32_t ws, uint32_t we) const;   // [ws, we) against every indexed GAP
  bool gapOverflow() const { return gapOverflow_; }
  bool closeOverflow() const { return closeOverflow_; }
  size_t gapCount() const { return nGaps_; }
  size_t closeCount() const { return nCloses_; }
 private:
  void reset_();
  struct Span { uint32_t from, to; };
  struct CloseRef { int32_t day; uint8_t session; };
  Span gaps_[MAX_GAPS];
  CloseRef closes_[MAX_CLOSES];
  size_t nGaps_, nCloses_;
  bool gapOverflow_, closeOverflow_;
};
}
