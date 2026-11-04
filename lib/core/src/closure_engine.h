#pragma once
#include "day_view.h"
#include "log_index.h"
namespace att {
const uint32_t TICK_PERIOD_S = 300;
const int MAX_TRACK_DAYS = 400;
class IRoster {
 public:
  virtual ~IRoster() {}
  virtual bool expected(uint16_t id, int32_t day) = 0;
};
// NOTE: ClosureEngine embeds a LogIndex (~14 KB). Allocate it statically or on the heap, never on a task stack.
class ClosureEngine {
 public:
  ClosureEngine(LogStore& log, const Params& p, const Timetable& t, IRoster& r, uint16_t cap, int32_t termStartDay);
  void onBoot(uint32_t now, uint32_t lastTickFromNvs);
  void onHealth(uint32_t now, bool healthy, uint16_t cause);
  bool onTick(uint32_t now);
  uint32_t lastTick() const { return lastTick_; }
  bool step(uint32_t now);
  bool pending() const { return pending_; }
  void invalidate() {}          // no-op: the index follows the log through the observer
  const LogIndex& index() const { return index_; }
 private:
  bool writeGap(uint32_t from, uint32_t to, uint16_t cause);
  bool clipToWindows(uint32_t from, uint32_t to, uint32_t& cf, uint32_t& ct) const;
  bool closeSession(int32_t day, int s, uint32_t now);
  LogStore& log_; Params p_; Timetable t_; IRoster& roster_;
  uint16_t cap_; int32_t term_;
  uint32_t lastTick_, badSince_; uint16_t badCause_;
  bool bad_, pending_, indexOk_;
  LogIndex index_;
};
}
