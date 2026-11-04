#pragma once
#include "log_store.h"
#include "status_derive.h"
#include "time_rules.h"
namespace att {
const uint16_t MAX_ID = 300;
struct DayView {
  int32_t day; uint8_t session;
  bool noClassDay, noClassSession, closed, coverage;
  uint16_t expectedCount;
  uint8_t scan[MAX_ID + 1];
  uint8_t mark[MAX_ID + 1];
  uint64_t expected[5];
  bool collecting; uint16_t pi;     // CLOSE payload collection state (persists across incremental applies)
};
void applyEvent(DayView& v, const Event& e, const Params& p);
bool buildDayView(LogStore& log, const Params& p, int32_t day, uint8_t session, DayView& v);   // full replay
Derived statusFor(const DayView& v, uint16_t id, bool liveExpected, bool started);
// Incremental view for one (day, session). Rebuilt by replay only when the target changes or the log resets.
class DayCache : public LogObserver {
 public:
  DayCache(LogStore& log, const Params& p) : log_(log), p_(p), day_(0), s_(0), dirty_(true) {}
  void target(int32_t day, uint8_t session);
  bool view(const DayView*& out);
  void onReset() override { dirty_ = true; }
  void onRecord(const Event& e) override { if (!dirty_) applyEvent(v_, e, p_); }
 private:
  LogStore& log_; Params p_; DayView v_; int32_t day_; uint8_t s_; bool dirty_;
};
}
