#pragma once
#include <stdint.h>
namespace att {
const uint32_t EPOCH_2020 = 1577836800u;   // 2020-01-01 00:00:00 UTC (a Wednesday)
// Latest full local day representable by uint32 Unix seconds for every
// supported UTC offset (-12:00..+14:00). Day 31446 is 2106-02-05.
const int32_t MAX_TERM_DAY = 31446;
struct Slot { uint16_t start; uint16_t end; bool enabled; };   // minutes from local midnight
struct Timetable { Slot day[5][2]; };                          // Mon..Fri x (0 = AM, 1 = PM)
struct Params { int16_t utcOffsetMin; uint16_t lateMin; uint16_t scanLeadMin; };

int32_t dayNumber(uint32_t epoch, int16_t utcOffsetMin);   // local days since 2020-01-01; -1 if before
uint8_t weekdayIndex(int32_t day);                         // 0 = Mon .. 6 = Sun; day must be >= 0
uint16_t minuteOfDay(uint32_t epoch, int16_t utcOffsetMin);
uint32_t dayStartEpoch(int32_t day, int16_t utcOffsetMin);   // UTC epoch of local midnight
bool validateTimetable(const Timetable& t, uint16_t scanLeadMin);
int sessionAt(const Timetable& t, const Params& p, uint32_t epoch);   // window [start-lead, end); -1 = none
bool isLate(const Slot& s, uint16_t lateMin, uint16_t minute);        // minute > start + lateMin
bool hasStarted(const Slot& s, uint16_t minute);                      // minute >= start
bool hasEnded(const Slot& s, uint16_t minute);                        // minute >= end
Timetable defaultTimetable();                                         // 08:00-12:30, 13:30-16:30, Mon..Fri
}
