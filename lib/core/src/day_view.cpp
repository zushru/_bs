#include "day_view.h"
#include <string.h>
namespace att {
void applyEvent(DayView& v, const Event& e, const Params& p) {
  if (e.type == EV_PAYLOAD) {
    if (v.collecting) {
      if (v.pi < 5) v.expected[v.pi++] = payloadBits(e);
      if (!flagMore(e.flags)) v.collecting = false;
    }
    return;
  }
  v.collecting = false;
  bool sess = flagSession(e.flags) == v.session;
  bool idOk = e.a >= 1 && e.a <= MAX_ID;
  switch (e.type) {
    case EV_SCAN:
      if (sess && idOk && v.scan[e.a] == 0 && dayNumber(e.epoch, p.utcOffsetMin) == v.day) {
        uint8_t st = flagStatus(e.flags);
        if (st == ST_PRESENT || st == ST_LATE) v.scan[e.a] = st;
      }
      break;
    case EV_MARK:
      if (sess && idOk && e.b == v.day) v.mark[e.a] = flagStatus(e.flags);
      break;
    case EV_NOCLASS_SET:
    case EV_NOCLASS_CLEAR:
      if (e.b == v.day) {
        bool on = e.type == EV_NOCLASS_SET;
        if (flagWholeDay(e.flags)) v.noClassDay = on; else if (sess) v.noClassSession = on;
      }
      break;
    case EV_CLOSE:
      if (sess && e.b == v.day && !v.closed) {
        v.closed = true; v.coverage = flagCoverage(e.flags); v.expectedCount = e.a;
        v.collecting = flagMore(e.flags); v.pi = 0;
      }
      break;
    default: break;
  }
}
bool buildDayView(LogStore& log, const Params& p, int32_t day, uint8_t session, DayView& v) {
  memset(&v, 0, sizeof v);
  v.day = day; v.session = session;
  Event e;
  for (size_t i = 0; i < log.count(); i++) {
    if (!log.at(i, e)) return false;
    applyEvent(v, e, p);
  }
  return true;
}
Derived statusFor(const DayView& v, uint16_t id, bool liveExpected, bool started) {
  if (id < 1 || id > MAX_ID) return D_NA;
  Facts f;
  f.noClass = v.noClassDay || v.noClassSession;
  f.expected = v.closed ? bitmapTest(v.expected, id) : liveExpected;
  f.started = started; f.closed = v.closed; f.coverage = v.coverage;
  f.markStatus = v.mark[id]; f.scanStatus = v.scan[id];
  return derive(f);
}
void DayCache::target(int32_t day, uint8_t s) {
  if (day != day_ || s != s_) { day_ = day; s_ = s; dirty_ = true; }
}
bool DayCache::view(const DayView*& out) {
  if (dirty_) {
    if (!buildDayView(log_, p_, day_, s_, v_)) return false;
    dirty_ = false;
  }
  out = &v_;
  return true;
}
}
