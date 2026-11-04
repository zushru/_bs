#include "clock_policy.h"
namespace att {
ClockDecision decideClock(const ClockCtx& c) {
  ClockDecision d = {CA_NONE, 0, SRC_MANUAL, false, 0, 0};
  uint32_t ref = c.clockFault ? c.lastLogEpoch : c.deviceEpoch;
  bool noRef = (ref == 0);
  d.delta = noRef ? 0 : (int64_t)c.clientEpoch - (int64_t)ref;
  int64_t a = d.delta < 0 ? -d.delta : d.delta;
  if (!noRef && a > CLK_INFO_S) {
    d.gapFrom = (uint32_t)(d.delta < 0 ? c.clientEpoch : ref);
    d.gapTo = (uint32_t)(d.delta < 0 ? ref : c.clientEpoch);
  }
  if (!c.explicitSync) {
    if (noRef || a <= CLK_INFO_S) return d;
    d.needsAttention = true;
    bool due = c.lastAutoSync == 0 || (int64_t)c.deviceEpoch >= (int64_t)c.lastAutoSync + CLK_AUTO_EVERY_S;
    if (c.autoSyncEnabled && !c.clockFault && a <= CLK_AUTO_MAX_S && !c.nearWindow && due) {
      d.action = CA_APPLY; d.source = SRC_AUTO; d.needsAttention = false;
    }
    return d;
  }
  d.source = noRef ? SRC_WIZARD : SRC_MANUAL;
  if (!c.clockFault && c.inWindow) { d.action = CA_BLOCKED_WINDOW; d.gapFrom = d.gapTo = 0; return d; }
  if (a > CLK_JUMP_S && !c.confirmJump) { d.action = CA_NEED_CONFIRM_JUMP; return d; }
  if (d.delta < 0 && a > CLK_INFO_S && !c.confirmBack) { d.action = CA_NEED_CONFIRM_BACK; return d; }
  d.action = CA_APPLY;
  return d;
}
bool nearScanWindow(const Timetable& t, const Params& p, uint32_t epoch, uint32_t marginS) {
  return sessionAt(t, p, epoch) >= 0 || sessionAt(t, p, epoch + marginS) >= 0;
}
bool applyClockSet(LogStore& log, const ClockDecision& d, uint32_t oldEpoch, uint32_t newEpoch) {
  Event ev[MAX_GROUP];
  size_t n = 0;
  uint32_t cur = d.gapFrom;
  while (d.gapTo > cur) {                                // earlier groups carry the head of a very long span
    uint32_t next = cur;
    n = makeGapEvents(cur, d.gapTo, GAP_CLOCK, ev, MAX_GROUP - 2, &next);
    if (n == 0 || next <= cur) return false;
    if (next >= d.gapTo) break;                          // last chunks ride with the CLOCK_SET pair
    if (!log.append(ev, n)) return false;
    cur = next;
    n = 0;
  }
  ev[n++] = Event{EV_CLOCK_SET, 0, d.source, oldEpoch, 0, 0};
  ev[n++] = Event{EV_CLOCK_SET, 0, d.source, newEpoch, 0, 0};
  return log.append(ev, n);
}
}
