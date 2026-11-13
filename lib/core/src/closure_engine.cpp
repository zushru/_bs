#include "closure_engine.h"
#include "config_store.h"
namespace att {
ClosureEngine::ClosureEngine(LogStore& log, const Params& p, const Timetable& t,
    IRoster& r, uint16_t cap, int32_t termStart, int32_t termEnd)
    : log_(log), p_(p), t_(t), roster_(r), cap_(cap > MAX_ID ? MAX_ID : cap),
      termStart_(termStart), termEnd_(termEnd), lastTick_(0), badSince_(0),
      badCause_(0), bad_(false), pending_(false), indexOk_(false) {
  indexOk_ = termStart_ >= 0 && termStart_ <= MAX_TERM_DAY &&
      termEnd_ >= termStart_ && termEnd_ <= MAX_TERM_DAY && log_.attach(&index_);
}
bool ClosureEngine::matchesConfig(const Config& config) const {
  if (termStart_ != config.termStart || termEnd_ != config.termEnd ||
      p_.utcOffsetMin != config.params.utcOffsetMin ||
      p_.lateMin != config.params.lateMin ||
      p_.scanLeadMin != config.params.scanLeadMin) return false;
  for (int day = 0; day < 5; ++day) {
    for (int session = 0; session < 2; ++session) {
      const Slot& current = t_.day[day][session];
      const Slot& expected = config.tt.day[day][session];
      if (current.start != expected.start || current.end != expected.end ||
          current.enabled != expected.enabled) return false;
    }
  }
  return true;
}
void ClosureEngine::setSchedule(const Params& p, const Timetable& t,
                                 int32_t termStart, int32_t termEnd) {
  p_ = p;
  t_ = t;
  termStart_ = termStart;
  termEnd_ = termEnd;
  if (termStart_ < 0 || termStart_ > MAX_TERM_DAY ||
      termEnd_ < termStart_ || termEnd_ > MAX_TERM_DAY) indexOk_ = false;
}
// Only the part of [from,to) that touches scan windows can change coverage. Clip to the span from the first
// overlapped window start to the last overlapped window end. Nights and weekends with no window are dropped.
bool ClosureEngine::clipToWindows(uint32_t from, uint32_t to, uint32_t& cf, uint32_t& ct) const {
  if (to <= from || termStart_ < 0 || termStart_ > MAX_TERM_DAY ||
      termEnd_ < termStart_ || termEnd_ > MAX_TERM_DAY) return false;
  int32_t d0 = dayNumber(from, p_.utcOffsetMin);
  int32_t d1 = dayNumber(to - 1, p_.utcOffsetMin);
  if (d1 < termStart_ || d0 > termEnd_) return false;
  if (d0 < termStart_) d0 = termStart_;
  if (d1 > termEnd_) d1 = termEnd_;
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
  const int32_t day = dayNumber(now, p_.utcOffsetMin);
  if (day < termStart_ || day > termEnd_ ||
      sessionAt(t_, p_, now) < 0) return false;
  if (lastTick_ != 0 && now >= lastTick_ && now - lastTick_ < TICK_PERIOD_S) return false;
  lastTick_ = now;
  return true;
}
bool ClosureEngine::closeSession(int32_t day, int s, uint32_t now) {
  if (day < termStart_ || day > termEnd_ ||
      !roster_.historyComplete() || index_.closeOverflow()) return false;            // cannot record closes safely
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
  const int32_t today = dayNumber(now, p_.utcOffsetMin);
  if (today < 0 || today < termStart_) { pending_ = false; return true; }
  const int32_t lastDay = today < termEnd_ ? today : termEnd_;
  // Anchor the rolling horizon to the last configured day, not today.
  // Otherwise a device first booted long after term end skips the term tail.
  const int32_t rollingFirst = lastDay - (MAX_TRACK_DAYS - 1);
  int32_t first = termStart_ > rollingFirst ? termStart_ : rollingFirst;
  if (first < 0) first = 0;
  if (first > lastDay) { pending_ = false; return true; }
  pending_ = false;
  for (int32_t day = first; day <= lastDay; day++) {
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
