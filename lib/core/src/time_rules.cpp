#include "time_rules.h"
namespace att {
int32_t dayNumber(uint32_t epoch, int16_t off) {
  int64_t local = (int64_t)epoch + (int64_t)off * 60 - (int64_t)EPOCH_2020;
  if (local < 0) return -1;
  return (int32_t)(local / 86400);
}
uint8_t weekdayIndex(int32_t day) { return (uint8_t)((day + 2) % 7); }
uint16_t minuteOfDay(uint32_t epoch, int16_t off) {
  int64_t s = ((int64_t)epoch + (int64_t)off * 60) % 86400;
  if (s < 0) s += 86400;
  return (uint16_t)(s / 60);
}
uint32_t dayStartEpoch(int32_t day, int16_t off) {
  return (uint32_t)((int64_t)EPOCH_2020 + (int64_t)day * 86400 - (int64_t)off * 60);
}
bool validateTimetable(const Timetable& t, uint16_t lead) {
  for (int d = 0; d < 5; d++) {
    for (int s = 0; s < 2; s++) {
      const Slot& x = t.day[d][s];
      if (!x.enabled) continue;
      if (x.end <= x.start || x.end > 1439) return false;
      if (x.start < lead) return false;
    }
    const Slot& a = t.day[d][0];
    const Slot& b = t.day[d][1];
    if (a.enabled && b.enabled && (int)b.start - (int)lead < (int)a.end) return false;
  }
  return true;
}
int sessionAt(const Timetable& t, const Params& p, uint32_t epoch) {
  int32_t day = dayNumber(epoch, p.utcOffsetMin);
  if (day < 0) return -1;
  uint8_t wd = weekdayIndex(day);
  if (wd > 4) return -1;
  int m = minuteOfDay(epoch, p.utcOffsetMin);
  for (int s = 0; s < 2; s++) {
    const Slot& x = t.day[wd][s];
    if (x.enabled && m + (int)p.scanLeadMin >= (int)x.start && m < (int)x.end) return s;
  }
  return -1;
}
bool isLate(const Slot& s, uint16_t lateMin, uint16_t minute) { return (uint32_t)minute > (uint32_t)s.start + lateMin; }
bool hasStarted(const Slot& s, uint16_t minute) { return minute >= s.start; }
bool hasEnded(const Slot& s, uint16_t minute) { return minute >= s.end; }
Timetable defaultTimetable() {
  Timetable t;
  for (int d = 0; d < 5; d++) {
    t.day[d][0] = Slot{480, 750, true};
    t.day[d][1] = Slot{810, 990, true};
  }
  return t;
}
}
