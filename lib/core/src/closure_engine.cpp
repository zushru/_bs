#include "closure_engine.h"
namespace att {
ClosureEngine::ClosureEngine(LogStore& log, const Params& p, const Timetable& t, IRoster& r, uint16_t cap, int32_t term)
    : log_(log), p_(p), t_(t), roster_(r), cap_(cap > MAX_ID ? MAX_ID : cap), term_(term),
      lastTick_(0), badSince_(0), badCause_(0), bad_(false), pending_(false), indexOk_(false) {
  indexOk_ = log_.attach(&index_);      // false = no observer slot: this engine must refuse to close
}
// Only the part of [from,to) that touches scan windows can change coverage. Clip to the span from the first
// overlapped window start to the last overlapped window end. Nights and weekends with no window are dropped.
bool ClosureEngine::clipToWindows(uint32_t from, uint32_t to, uint32_t& cf, uint32_t& ct) const {
  if (to <= from) return false;
  int32_t d0 = dayNumber(from, p_.utcOffsetMin);
  int32_t d1 = dayNumber(to - 1, p_.utcOffsetMin);
  if (d0 < 0) d0 = 0;
  if (d1 < d0) return false;
  bool any = false;
  uint32_t first = 0, last = 0;
  for (int32_t day = d0; day <= d1; day++) {
    if (weekdayIndex(day) > 4) continue;
    uint32_t ds = dayStartEpoch(day, p_.utcOffsetMin);
    for (int s = 0; s < 2; s++) {
      const Slot& sl = t_.day[weekdayIndex(day)][s];
      if (!sl.enabled) continue;
      uint32_t ws = ds + (uint32_t)(sl.start - p_.scanLeadMin) * 60u;
      uint32_t we = ds + (uint32_t)sl.end * 60u;
      if (ws < to && we > from) {
        uint32_t a = ws > from ? ws : from;
        uint32_t b = we < to ? we : to;
        if (!any) { first = a; any = true; }
        last = b;
      }
    }
  }
  if (any) { cf = first; ct = last; }
  return any;
}
bool ClosureEngine::writeGap(uint32_t from, uint32_t to, uint16_t cause) {
  uint32_t cf = 0, ct = 0;
  if (!clipToWindows(from, to, cf, ct)) return true;     // touches no scan window: nothing to record
  return appendGaps(log_, cf, ct, cause);                // multi-group: never silently drops a long outage
}
void ClosureEngine::onBoot(uint32_t now, uint32_t lastTick) {
  if (lastTick != 0 && now > lastTick && !writeGap(lastTick, now, GAP_POWER)) {
    bad_ = true; badSince_ = lastTick; badCause_ = GAP_POWER;
  }
  lastTick_ = now;
}
void ClosureEngine::onHealth(uint32_t now, bool healthy, uint16_t cause) {
  if (!healthy) {
    if (!bad_) { bad_ = true; badSince_ = now; badCause_ = cause; }
    return;
  }
  if (bad_ && writeGap(badSince_, now, badCause_)) bad_ = false;
}
bool ClosureEngine::onTick(uint32_t now) {
  if (sessionAt(t_, p_, now) < 0) return false;
  if (lastTick_ != 0 && now >= lastTick_ && now - lastTick_ < TICK_PERIOD_S) return false;
  lastTick_ = now;
  return true;
}
bool ClosureEngine::closeSession(int32_t day, int s, uint32_t now) {
  if (index_.closeOverflow()) return false;            // cannot record closes safely
  const Slot& sl = t_.day[weekdayIndex(day)][s];
  uint32_t ds = dayStartEpoch(day, p_.utcOffsetMin);
  uint32_t ws = ds + (uint32_t)(sl.start - p_.scanLeadMin) * 60;
  uint32_t we = ds + (uint32_t)sl.end * 60;
  bool cov = !index_.gapOverflow() && !index_.gapOverlaps(ws, we);   // overflow = cannot prove coverage
  uint64_t words[5] = {0, 0, 0, 0, 0};
  uint16_t count = 0;
  for (uint16_t id = 1; id <= cap_; id++)
    if (roster_.expected(id, day)) { bitmapSet(words, id); count++; }
  Event ev[MAX_GROUP];
  size_t n = 0;
  uint16_t pc = payloadCount(cap_);
  ev[n++] = Event{EV_CLOSE, makeFlags((uint8_t)s, 0, cov, false, false), count, now, 0, (uint16_t)day};
  for (uint16_t i = 0; i < pc; i++) ev[n++] = makePayload(words[i], 0, false);
  return log_.append(ev, n);
}
bool ClosureEngine::step(uint32_t now) {
  if (!indexOk_ || bad_) { pending_ = true; return false; }
  int32_t today = dayNumber(now, p_.utcOffsetMin);
  if (today < 0) return true;
  int32_t first = term_ > today - (MAX_TRACK_DAYS - 1) ? term_ : today - (MAX_TRACK_DAYS - 1);
  if (first < 0) first = 0;
  pending_ = false;
  for (int32_t day = first; day <= today; day++) {
    if (weekdayIndex(day) > 4) continue;
    for (int s = 0; s < 2; s++) {
      const Slot& sl = t_.day[weekdayIndex(day)][s];
      if (!sl.enabled || index_.closed(day, (uint8_t)s)) continue;
      if (now < dayStartEpoch(day, p_.utcOffsetMin) + (uint32_t)sl.end * 60) continue;
      if (!closeSession(day, s, now)) { pending_ = true; return false; }
    }
  }
  return true;
}
}
