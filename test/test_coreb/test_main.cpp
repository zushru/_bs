#include <unity.h>
#include "fakes.h"
#include "health_fsm.h"
#include "clock_policy.h"
using namespace att;

void setUp() {}
void tearDown() {}

static const int32_t FRI = 2494;   // 2026-10-30
static uint32_t E(int32_t day, int minute) { return dayStartEpoch(day, 0) + (uint32_t)minute * 60; }
static Event mk(uint8_t type, uint8_t flags, uint16_t a, uint32_t epoch, uint16_t b) { Event e = {type, flags, a, epoch, 0, b}; return e; }
static Event scanEv(uint16_t id, uint32_t t, uint8_t st, uint8_t sess = 0) { return mk(EV_SCAN, makeFlags(sess, st, false, false, false), id, t, 150); }
static Event markEv(uint16_t id, int32_t day, uint8_t st, uint8_t sess = 0) { return mk(EV_MARK, makeFlags(sess, st, false, false, false), id, E(day, 700), (uint16_t)day); }
static Event nocEv(bool set, int32_t day, uint8_t sess, bool whole) { return mk(set ? EV_NOCLASS_SET : EV_NOCLASS_CLEAR, makeFlags(sess, 0, false, false, whole), 0, E(day, 700), (uint16_t)day); }
static bool put(LogStore& l, const Event& e) { return l.append(&e, 1); }

struct Rig {
  MemStorage ms;
  LogStore log;
  FakeRoster roster;
  Timetable tt;
  Params p;
  ClosureEngine eng;
  Rig(int32_t term = FRI) : log(ms), roster(45), tt(defaultTimetable()), p{0, 30, 15}, eng(log, p, tt, roster, 162, term) { log.open(); }
  void run(int32_t day, int from, int to, bool healthy = true) {
    for (uint32_t t = E(day, from); t <= E(day, to); t += 10) {
      eng.onHealth(t, healthy, GAP_SENSOR);
      eng.onTick(t);
      eng.step(t);
    }
  }
  int countType(uint8_t type) { Event e; int n = 0; for (size_t i = 0; i < log.count(); i++) { log.at(i, e); if (e.type == type) n++; } return n; }
  bool view(int32_t day, uint8_t s, DayView& v) { return buildDayView(log, p, day, s, v); }
};

// ================= log_store =================
void test_log_group_append_and_reopen() {
  MemStorage ms; LogStore l(ms);
  OpenInfo o = l.open();
  TEST_ASSERT_TRUE(o.ok);
  Event g[3] = {mk(EV_ROSTER, 0, 1, 100, 0), mk(EV_ROSTER, 0, 2, 200, 0), mk(EV_ROSTER, 0, 3, 300, 0)};
  TEST_ASSERT_TRUE(l.append(g, 3));
  TEST_ASSERT_EQUAL_UINT32(3, l.count());
  Event e;
  TEST_ASSERT_TRUE(l.at(0, e)); TEST_ASSERT_TRUE(flagMore(e.flags)); TEST_ASSERT_EQUAL_UINT32(1, e.seq);
  TEST_ASSERT_TRUE(l.at(1, e)); TEST_ASSERT_TRUE(flagMore(e.flags));
  TEST_ASSERT_TRUE(l.at(2, e)); TEST_ASSERT_FALSE(flagMore(e.flags)); TEST_ASSERT_EQUAL_UINT32(3, e.seq);
  TEST_ASSERT_EQUAL_UINT32(48, ms.d.size());
  LogStore l2(ms);
  o = l2.open();
  TEST_ASSERT_TRUE(o.ok); TEST_ASSERT_EQUAL_UINT32(3, o.records); TEST_ASSERT_EQUAL_UINT32(3, o.lastSeq);
  TEST_ASSERT_TRUE(put(l2, mk(EV_BOOT, 0, 0, 400, 0)));
  TEST_ASSERT_EQUAL_UINT32(4, l2.lastSeq());
}
void test_log_append_failure_latches() {
  MemStorage ms; LogStore l(ms); l.open();
  TEST_ASSERT_TRUE(put(l, mk(EV_BOOT, 0, 0, 1, 0)));
  ms.failAppend = true;
  TEST_ASSERT_FALSE(put(l, mk(EV_BOOT, 0, 0, 2, 0)));
  TEST_ASSERT_TRUE(l.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, l.count());
  ms.failAppend = false;
  TEST_ASSERT_FALSE(put(l, mk(EV_BOOT, 0, 0, 3, 0)));       // refused until reopened
  TEST_ASSERT_TRUE(l.open().ok);
  TEST_ASSERT_TRUE(put(l, mk(EV_BOOT, 0, 0, 3, 0)));
  TEST_ASSERT_EQUAL_UINT32(2, l.count());
}
void test_log_torn_partial_record_is_preserved_and_blocks_open() {
  MemStorage ms; LogStore l(ms); l.open();
  put(l, mk(EV_BOOT, 0, 0, 1, 0)); put(l, mk(EV_BOOT, 0, 0, 2, 0));
  for (int i = 0; i < 5; i++) ms.d.push_back(0xAB);
  LogStore l2(ms);
  OpenInfo o = l2.open();
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(5, o.tornBytes);
  TEST_ASSERT_TRUE(l2.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, l2.count());
  TEST_ASSERT_EQUAL_UINT32(37, ms.d.size());
}
void test_log_torn_group_is_preserved_and_blocks_open() {
  MemStorage ms; LogStore l(ms); l.open();
  put(l, mk(EV_BOOT, 0, 0, 1, 0));
  Event g[3] = {mk(EV_ROSTER, 0, 1, 2, 0), mk(EV_ROSTER, 0, 2, 3, 0), mk(EV_ROSTER, 0, 3, 4, 0)};
  ms.tearBytes = 32;                                         // power cut after 2 of 3 records
  TEST_ASSERT_FALSE(l.append(g, 3));
  LogStore l2(ms);
  OpenInfo o = l2.open();
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(2, o.droppedGroup);
  TEST_ASSERT_TRUE(l2.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, l2.count());
  TEST_ASSERT_EQUAL_UINT32(48, ms.d.size());
  Event next = mk(EV_BOOT, 0, 0, 5, 0);
  TEST_ASSERT_FALSE(l2.append(&next, 1));
}
void test_log_mid_corruption_detected() {
  MemStorage ms; LogStore l(ms); l.open();
  for (int i = 0; i < 5; i++) put(l, mk(EV_BOOT, 0, 0, (uint32_t)i + 1, 0));
  ms.d[2 * 16 + 5] ^= 0x40;
  LogStore l2(ms);
  OpenInfo o = l2.open();
  TEST_ASSERT_FALSE(o.ok); TEST_ASSERT_EQUAL_INT(32, o.corruptAt);
  TEST_ASSERT_TRUE(l2.faulted());
  TEST_ASSERT_FALSE(put(l2, mk(EV_BOOT, 0, 0, 9, 0)));
  TEST_ASSERT_EQUAL_UINT32(80, ms.d.size());                 // nothing was truncated or rewritten
}
void test_log_bad_tail_record_is_preserved_and_blocks_open() {
  MemStorage ms; LogStore l(ms); l.open();
  for (int i = 0; i < 3; i++) put(l, mk(EV_BOOT, 0, 0, (uint32_t)i + 1, 0));
  ms.d[2 * 16 + 1] ^= 0x01;                                  // complete record corrupted at EOF
  LogStore l2(ms);
  OpenInfo o = l2.open();
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_EQUAL_INT(32, o.corruptAt);
  TEST_ASSERT_EQUAL_UINT32(16, o.tornBytes);
  TEST_ASSERT_TRUE(l2.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, l2.count());
  TEST_ASSERT_EQUAL_UINT32(48, ms.d.size());
}
void test_log_partial_tail_is_preserved_without_attempting_truncate() {
  MemStorage ms; LogStore l(ms); l.open();
  put(l, mk(EV_BOOT, 0, 0, 1, 0));
  ms.d.push_back(1);
  ms.failTruncate = true;                                   // open must not invoke truncate at all
  LogStore l2(ms);
  OpenInfo o = l2.open();
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_TRUE(l2.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, o.tornBytes);
  TEST_ASSERT_EQUAL_UINT32(17, ms.d.size());
}
void test_log_baseline_epoch() {
  MemStorage ms; LogStore l(ms); l.open();
  put(l, mk(EV_SCAN, 0, 1, 1000, 0)); put(l, mk(EV_SCAN, 0, 1, 2000, 0));
  TEST_ASSERT_EQUAL_UINT32(2000, l.baselineEpoch());
  Event cs[3] = {mk(EV_GAP, 0, GAP_CLOCK, 500, 30), mk(EV_CLOCK_SET, 0, SRC_MANUAL, 2000, 0), mk(EV_CLOCK_SET, 0, SRC_MANUAL, 500, 0)};
  TEST_ASSERT_TRUE(l.append(cs, 3));
  TEST_ASSERT_EQUAL_UINT32(500, l.baselineEpoch());
  put(l, mk(EV_SCAN, 0, 1, 600, 0));
  put(l, makePayload(0xFFFFFFFFFFFFFFFFull, 0, false));       // payload bits must not look like time
  TEST_ASSERT_EQUAL_UINT32(600, l.baselineEpoch());
  LogStore l2(ms);
  TEST_ASSERT_EQUAL_UINT32(600, l2.open().baselineEpoch);
}
void test_log_group_limits_and_unopened() {
  MemStorage ms; LogStore l(ms);
  Event e = mk(EV_BOOT, 0, 0, 1, 0);
  TEST_ASSERT_FALSE(l.append(&e, 1));                        // not opened yet
  l.open();
  Event big[17];
  for (int i = 0; i < 17; i++) big[i] = e;
  TEST_ASSERT_FALSE(l.append(big, 17));
  TEST_ASSERT_FALSE(l.append(big, 0));
  TEST_ASSERT_TRUE(l.append(big, 16));
}

// ================= day_view =================
void test_dayview_scan_mark_precedence() {
  Rig r;
  put(r.log, scanEv(5, E(FRI, 490), ST_PRESENT));
  put(r.log, scanEv(5, E(FRI, 495), ST_LATE));               // second scan ignored
  put(r.log, scanEv(6, E(FRI, 500), ST_LATE));
  put(r.log, markEv(6, FRI, ST_EXCUSED));
  put(r.log, markEv(7, FRI, ST_LATE));
  put(r.log, markEv(7, FRI, ST_CLEAR));                      // clear -> falls back to scan (none)
  put(r.log, markEv(8, FRI, ST_PRESENT));
  put(r.log, markEv(8, FRI, ST_LATE));                       // latest mark wins
  DayView v;
  TEST_ASSERT_TRUE(r.view(FRI, 0, v));
  TEST_ASSERT_EQUAL_INT(D_PRESENT, statusFor(v, 5, true, true));
  TEST_ASSERT_EQUAL_INT(D_EXCUSED, statusFor(v, 6, true, true));
  TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 7, true, true));
  TEST_ASSERT_EQUAL_INT(D_LATE, statusFor(v, 8, true, true));
  TEST_ASSERT_EQUAL_INT(D_UPCOMING, statusFor(v, 9, true, false));
  TEST_ASSERT_EQUAL_INT(D_NA, statusFor(v, 9, false, true));
  TEST_ASSERT_EQUAL_INT(D_NA, statusFor(v, 0, true, true));
  TEST_ASSERT_EQUAL_INT(D_NA, statusFor(v, 301, true, true));
}
void test_dayview_noclass_toggle() {
  Rig r;
  put(r.log, nocEv(true, FRI, 0, true));                     // whole day
  DayView v;
  r.view(FRI, 1, v);
  TEST_ASSERT_EQUAL_INT(D_NOCLASS, statusFor(v, 1, true, true));
  put(r.log, nocEv(false, FRI, 0, true));                    // cleared again
  r.view(FRI, 1, v);
  TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 1, true, true));
  put(r.log, nocEv(true, FRI, 1, false));                    // PM only
  r.view(FRI, 0, v); TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 1, true, true));
  r.view(FRI, 1, v); TEST_ASSERT_EQUAL_INT(D_NOCLASS, statusFor(v, 1, true, true));
}
void test_dayview_close_bitmap_300() {
  Rig r;
  uint64_t w[5] = {0, 0, 0, 0, 0};
  bitmapSet(w, 1); bitmapSet(w, 64); bitmapSet(w, 65); bitmapSet(w, 300);
  Event g[6];
  g[0] = mk(EV_CLOSE, makeFlags(0, 0, true, false, false), 4, E(FRI, 751), (uint16_t)FRI);
  for (int i = 0; i < 5; i++) g[i + 1] = makePayload(w[i], 0, false);
  TEST_ASSERT_TRUE(r.log.append(g, 6));
  DayView v;
  TEST_ASSERT_TRUE(r.view(FRI, 0, v));
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_TRUE(v.coverage); TEST_ASSERT_EQUAL_UINT32(4, v.expectedCount);
  TEST_ASSERT_EQUAL_INT(D_ABSENT, statusFor(v, 300, false, true));
  TEST_ASSERT_EQUAL_INT(D_ABSENT, statusFor(v, 65, false, true));
  TEST_ASSERT_EQUAL_INT(D_NA, statusFor(v, 66, true, true));  // snapshot wins over live roster
}
void test_dayview_ignores_other_day_and_session() {
  Rig r;
  put(r.log, scanEv(5, E(FRI - 1, 490), ST_PRESENT));        // yesterday
  put(r.log, scanEv(6, E(FRI, 830), ST_PRESENT, 1));         // PM scan
  put(r.log, markEv(7, FRI - 1, ST_PRESENT));
  DayView v;
  r.view(FRI, 0, v);
  TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 5, true, true));
  TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 6, true, true));
  TEST_ASSERT_EQUAL_INT(D_PENDING, statusFor(v, 7, true, true));
  r.view(FRI, 1, v);
  TEST_ASSERT_EQUAL_INT(D_PRESENT, statusFor(v, 6, true, true));
}

// ================= closure_engine =================
void test_close_normal_day_coverage_yes() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 400, 464);
  put(r.log, scanEv(5, E(FRI, 490), ST_PRESENT));
  r.run(FRI, 465, 760);
  DayView v;
  TEST_ASSERT_TRUE(r.view(FRI, 0, v));
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_TRUE(v.coverage); TEST_ASSERT_EQUAL_UINT32(45, v.expectedCount);
  TEST_ASSERT_EQUAL_INT(D_PRESENT, statusFor(v, 5, true, true));
  TEST_ASSERT_EQUAL_INT(D_ABSENT, statusFor(v, 6, true, true));
  TEST_ASSERT_EQUAL_INT(D_NA, statusFor(v, 46, true, true));
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_GAP));
  r.view(FRI, 1, v);
  TEST_ASSERT_FALSE(v.closed);                               // PM not over yet
}
void test_close_waits_for_session_end() {
  Rig r;
  r.eng.onBoot(E(FRI, 460), 0);
  r.run(FRI, 465, 749);
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  r.eng.step(E(FRI, 750) - 1);
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  r.eng.step(E(FRI, 750));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));
  r.eng.step(E(FRI, 751));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));           // idempotent
}
void test_boot_midwindow_makes_session_unverified() {
  Rig r;
  r.eng.onBoot(E(FRI, 500), E(FRI - 1, 985));                // last tick yesterday, power back at 08:20
  put(r.log, scanEv(5, E(FRI, 505), ST_PRESENT));
  r.run(FRI, 500, 760);
  DayView v;
  r.view(FRI, 0, v);
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
  TEST_ASSERT_EQUAL_INT(D_UNVERIFIED, statusFor(v, 6, true, true));
  TEST_ASSERT_EQUAL_INT(D_PRESENT, statusFor(v, 5, true, true));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_GAP));
}
void test_boot_before_window_keeps_coverage() {
  Rig r;
  r.eng.onBoot(E(FRI, 420), E(FRI - 1, 985));                // gap ends 07:00, window opens 07:45
  r.run(FRI, 420, 760);
  DayView v; r.view(FRI, 0, v);
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_TRUE(v.coverage);
}
void test_short_unhealthy_span_in_window() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 465, 599);
  r.run(FRI, 600, 602, false);                               // 3 minutes of sensor fault, between two ticks
  r.run(FRI, 603, 760);
  DayView v; r.view(FRI, 0, v);
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
  TEST_ASSERT_EQUAL_INT(D_UNVERIFIED, statusFor(v, 6, true, true));
}
void test_outage_between_sessions_keeps_coverage() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 465, 755);
  r.run(FRI, 760, 769, false);
  r.run(FRI, 770, 1000);
  DayView v;
  r.view(FRI, 0, v); TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_TRUE(v.coverage);
  r.view(FRI, 1, v); TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_TRUE(v.coverage);
}
void test_overdue_closure_after_downtime() {
  Rig r;
  r.eng.onBoot(E(FRI, 1000), E(FRI, 480));                   // died 08:00, back at 16:40
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 1000)));
  TEST_ASSERT_EQUAL_INT(2, r.countType(EV_CLOSE));
  DayView v;
  r.view(FRI, 0, v); TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
  r.view(FRI, 1, v); TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
  Event e; int firstClose = -1, secondClose = -1;
  for (size_t i = 0; i < r.log.count(); i++) {
    r.log.at(i, e);
    if (e.type == EV_CLOSE) { if (firstClose < 0) firstClose = flagSession(e.flags); else secondClose = flagSession(e.flags); }
  }
  TEST_ASSERT_EQUAL_INT(0, firstClose); TEST_ASSERT_EQUAL_INT(1, secondClose);   // oldest first
  size_t before = r.log.count();
  r.eng.step(E(FRI, 1001));
  TEST_ASSERT_EQUAL_UINT32(before, r.log.count());
}
void test_closure_waits_while_unhealthy_then_recovers() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 465, 739);
  r.run(FRI, 740, 751, false);                               // fault spans the session end
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  TEST_ASSERT_TRUE(r.eng.pending());
  r.eng.onHealth(E(FRI, 752), true, GAP_SENSOR);
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 752)));
  TEST_ASSERT_FALSE(r.eng.pending());
  DayView v; r.view(FRI, 0, v);
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
}
void test_storage_failure_during_closure() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 465, 749);
  r.ms.failAppend = true;
  TEST_ASSERT_FALSE(r.eng.step(E(FRI, 750)));
  TEST_ASSERT_TRUE(r.eng.pending()); TEST_ASSERT_TRUE(r.log.faulted());
  r.ms.failAppend = false;
  TEST_ASSERT_FALSE(r.eng.step(E(FRI, 751)));                // log still latched
  TEST_ASSERT_TRUE(r.log.open().ok);
  r.eng.invalidate();
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 752)));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));
}
void test_failed_boot_gap_is_retried() {
  Rig r;
  r.ms.failAppend = true;
  r.eng.onBoot(E(FRI, 500), E(FRI, 480));                    // GAP could not be written
  r.ms.failAppend = false;
  r.log.open();
  r.eng.onHealth(E(FRI, 501), true, GAP_SENSOR);             // retried on recovery
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_GAP));
}
void test_engine_set_schedule_updates_window_and_session_end() {
  Rig r;
  Params nextP = {0, 30, 30};
  Timetable nextT = defaultTimetable();
  nextT.day[4][0].start = 500;  // Friday 08:20
  nextT.day[4][0].end = 765;    // Friday 12:45
  r.eng.setSchedule(nextP, nextT, FRI);

  TEST_ASSERT_FALSE(r.eng.onTick(E(FRI, 469)));  // New window opens at 07:50, not old 07:45.
  TEST_ASSERT_TRUE(r.eng.onTick(E(FRI, 470)));
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 764)));
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 765)));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));
  Event e;
  bool found = false;
  for (size_t i = 0; i < r.log.count(); i++) {
    TEST_ASSERT_TRUE(r.log.at(i, e));
    if (e.type == EV_CLOSE) {
      found = true;
      TEST_ASSERT_EQUAL_INT(FRI, e.b);
      TEST_ASSERT_EQUAL_INT(0, flagSession(e.flags));
    }
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 766)));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));  // Existing LogIndex still observes the close.
}
void test_engine_set_schedule_updates_utc_offset() {
  Rig r;
  Params nextP = {480, 30, 15};
  Timetable nextT = defaultTimetable();
  r.eng.setSchedule(nextP, nextT, FRI);

  // 04:29 UTC is 12:29 PHT; the AM session closes at 12:30 PHT.
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 269)));
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 270)));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));
}
void test_engine_set_schedule_updates_term_start() {
  Rig r(FRI - 10);
  Params nextP = {0, 30, 15};
  Timetable nextT = defaultTimetable();
  r.eng.setSchedule(nextP, nextT, FRI);

  // Monday is after the old term start but before the newly configured term start.
  TEST_ASSERT_TRUE(r.eng.step(E(FRI - 4, 1000)));
  TEST_ASSERT_EQUAL_INT(0, r.countType(EV_CLOSE));
  TEST_ASSERT_TRUE(r.eng.step(E(FRI, 750)));
  TEST_ASSERT_EQUAL_INT(1, r.countType(EV_CLOSE));
}

void test_no_close_for_weekend_or_before_term() {
  Rig r(FRI);
  r.eng.onBoot(E(FRI, 400), 0);
  r.eng.step(E(FRI + 2, 1200));                              // Sunday evening
  TEST_ASSERT_EQUAL_INT(2, r.countType(EV_CLOSE));
  Event e;
  for (size_t i = 0; i < r.log.count(); i++) {
    r.log.at(i, e);
    if (e.type == EV_CLOSE) TEST_ASSERT_EQUAL_INT(FRI, e.b);
  }
}
void test_long_gap_is_chunked_and_marks_old_sessions() {
  Rig r(FRI - 95);                                           // Monday 2399
  r.eng.onBoot(E(FRI, 500), E(FRI, 500) - 100u * 86400u);
  TEST_ASSERT_EQUAL_INT(3, r.countType(EV_GAP));
  Event e; uint32_t total = 0;
  for (size_t i = 0; i < r.log.count(); i++) { r.log.at(i, e); if (e.type == EV_GAP) total += e.b; }
  TEST_ASSERT_EQUAL_UINT32(100u * 1440u, total);
  r.eng.step(E(FRI, 500));
  DayView v;
  r.view(FRI - 95, 0, v);
  TEST_ASSERT_TRUE(v.closed); TEST_ASSERT_FALSE(v.coverage);
}
void test_tick_only_in_window_and_every_300s() {
  Rig r;
  TEST_ASSERT_FALSE(r.eng.onTick(E(FRI, 400)));
  TEST_ASSERT_TRUE(r.eng.onTick(E(FRI, 465)));
  TEST_ASSERT_EQUAL_UINT32(E(FRI, 465), r.eng.lastTick());
  TEST_ASSERT_FALSE(r.eng.onTick(E(FRI, 465) + 299));
  TEST_ASSERT_TRUE(r.eng.onTick(E(FRI, 465) + 300));
  TEST_ASSERT_FALSE(r.eng.onTick(E(FRI, 750)));
}
void test_close_snapshots_roster() {
  Rig r;
  r.eng.onBoot(E(FRI, 400), 0);
  r.run(FRI, 465, 760);
  r.roster.n = 10;                                           // roster shrinks afterwards
  DayView v; r.view(FRI, 0, v);
  TEST_ASSERT_EQUAL_INT(D_ABSENT, statusFor(v, 20, false, true));
}

// ================= health_fsm =================
static HealthInputs good() { HealthInputs i = {true, true, false, false, false, true, true, true, false, true}; return i; }
void test_health_priority_and_perms() {
  HealthFsm h;
  TEST_ASSERT_EQUAL_INT(H_OK, h.update(good()));
  Perms p = HealthFsm::perms(H_OK);
  TEST_ASSERT_TRUE(p.scan && p.manual && p.enroll && p.readHistory && p.exportOk);
  HealthInputs i = good(); i.sensorOk = false;
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT, h.update(i));
  p = HealthFsm::perms(H_SENSOR_FAULT);
  TEST_ASSERT_FALSE(p.scan); TEST_ASSERT_TRUE(p.manual); TEST_ASSERT_FALSE(p.enroll);
  i = good(); i.rtcLostPower = true;
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, h.update(i));
  p = HealthFsm::perms(H_CLOCK_FAULT);
  TEST_ASSERT_FALSE(p.scan); TEST_ASSERT_FALSE(p.manual); TEST_ASSERT_TRUE(p.readHistory); TEST_ASSERT_FALSE(p.exportOk);
  i.cfgOk = false;
  TEST_ASSERT_EQUAL_INT(H_CONFIG_FAULT, h.update(i));
  i.storageOk = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i));
  TEST_ASSERT_EQUAL_INT(HB_STORAGE | HB_CONFIG | HB_CLOCK, h.mask());
  p = HealthFsm::perms(H_STORAGE_FAULT);
  TEST_ASSERT_FALSE(p.scan || p.manual || p.enroll || p.readHistory || p.exportOk);
}
void test_health_faults_latch_until_reverified() {
  HealthFsm h;
  HealthInputs i = good(); i.storageOk = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i));
  i = good();                                                // cause looks gone
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i));       // still latched
  i.reverified = true;
  TEST_ASSERT_EQUAL_INT(H_OK, h.update(i));
  i = good(); i.epochBelowLog = true;
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, h.update(i));
  i.epochBelowLog = false; i.reverified = true; i.storageOk = true;
  TEST_ASSERT_EQUAL_INT(H_OK, h.update(i));
  i = good(); i.rtcLostPower = true; h.update(i);
  i.reverified = true;                                       // verified but the cause is still active
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, h.update(i));
}
void test_health_clock_reverification_clears_only_clock_latch() {
  HealthFsm h;
  HealthInputs i = good();
  i.clockTrusted = false;
  i.storageOk = false;
  i.cfgOk = false;
  i.sensorOk = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i));
  TEST_ASSERT_EQUAL_UINT8(HB_STORAGE | HB_CONFIG | HB_CLOCK | HB_SENSOR,
                          h.mask());

  // Clock-only verification leaves storage/config latches and sensor fault.
  i = good();
  i.sensorOk = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i, true));
  TEST_ASSERT_EQUAL_UINT8(HB_STORAGE | HB_CONFIG | HB_SENSOR, h.mask());

  // It cannot clear a clock cause that remains active.
  i.clockTrusted = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, h.update(i, true));
  TEST_ASSERT_EQUAL_UINT8(HB_STORAGE | HB_CONFIG | HB_CLOCK | HB_SENSOR,
                          h.mask());
}

void test_health_unprovisioned_and_sensor_do_not_latch() {
  HealthFsm h;
  HealthInputs i = good(); i.provisioned = false;
  TEST_ASSERT_EQUAL_INT(H_UNPROVISIONED, h.update(i));
  TEST_ASSERT_FALSE(HealthFsm::perms(H_UNPROVISIONED).manual);
  i.provisioned = true; i.sensorOk = false;
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT, h.update(i));
  i.sensorOk = true;
  TEST_ASSERT_EQUAL_INT(H_OK, h.update(i));
}

// ================= clock_policy =================
static ClockCtx ctx() {
  ClockCtx c;
  memset(&c, 0, sizeof c);
  c.deviceEpoch = E(FRI, 300);
  c.clientEpoch = c.deviceEpoch;
  return c;
}
void test_clock_small_delta_is_info_only() {
  ClockCtx c = ctx(); c.clientEpoch += 100; c.autoSyncEnabled = true;
  ClockDecision d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_NONE, d.action); TEST_ASSERT_FALSE(d.needsAttention);
  TEST_ASSERT_EQUAL_INT(100, (int)d.delta);
  TEST_ASSERT_EQUAL_UINT32(0, d.gapTo);
}
void test_clock_auto_sync_rules() {
  ClockCtx c = ctx(); c.autoSyncEnabled = true; c.clientEpoch = c.deviceEpoch + 200;
  ClockDecision d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_INT(SRC_AUTO, d.source);
  TEST_ASSERT_EQUAL_UINT32(c.deviceEpoch, d.gapFrom); TEST_ASSERT_EQUAL_UINT32(c.clientEpoch, d.gapTo);
  c.clientEpoch = c.deviceEpoch - 200;                       // backwards works too, gap = re-lived span
  d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_UINT32(c.clientEpoch, d.gapFrom);
  c.clientEpoch = c.deviceEpoch + 200;
  ClockCtx x = c; x.autoSyncEnabled = false;
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_NONE, d.action); TEST_ASSERT_TRUE(d.needsAttention);
  x = c; x.clientEpoch = c.deviceEpoch + 301;
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_NONE, d.action); TEST_ASSERT_TRUE(d.needsAttention);
  x = c; x.nearWindow = true;
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_NONE, d.action); TEST_ASSERT_TRUE(d.needsAttention);
  x = c; x.clockFault = true; x.lastLogEpoch = c.deviceEpoch;
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_NONE, d.action);
  x = c; x.lastAutoSync = c.deviceEpoch - 3600;              // synced an hour ago: not due
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_NONE, d.action); TEST_ASSERT_TRUE(d.needsAttention);
  x = c; x.lastAutoSync = c.deviceEpoch - 86400;
  d = decideClock(x); TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action);
}
void test_clock_explicit_rules() {
  ClockCtx c = ctx(); c.explicitSync = true;
  c.clientEpoch = c.deviceEpoch + 400;
  ClockDecision d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_INT(SRC_MANUAL, d.source);
  ClockCtx w = c; w.inWindow = true;
  TEST_ASSERT_EQUAL_INT(CA_BLOCKED_WINDOW, decideClock(w).action);
  ClockCtx b = c; b.clientEpoch = c.deviceEpoch - 500;
  d = decideClock(b); TEST_ASSERT_EQUAL_INT(CA_NEED_CONFIRM_BACK, d.action);
  b.confirmBack = true;
  d = decideClock(b); TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_UINT32(b.clientEpoch, d.gapFrom);
  ClockCtx j = c; j.clientEpoch = c.deviceEpoch + 2 * 86400;
  TEST_ASSERT_EQUAL_INT(CA_NEED_CONFIRM_JUMP, decideClock(j).action);
  j.confirmJump = true;
  TEST_ASSERT_EQUAL_INT(CA_APPLY, decideClock(j).action);
  ClockCtx s = c; s.clientEpoch = c.deviceEpoch - 60;
  d = decideClock(s); TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_UINT32(0, d.gapTo);
  ClockCtx back2 = c; back2.clientEpoch = c.deviceEpoch - 2 * 86400;
  TEST_ASSERT_EQUAL_INT(CA_NEED_CONFIRM_JUMP, decideClock(back2).action);   // jump check first, then back
}
void test_clock_fault_uses_log_reference() {
  ClockCtx c = ctx(); c.explicitSync = true; c.clockFault = true; c.inWindow = true;
  c.deviceEpoch = 946684800u;                                // garbage RTC (year 2000)
  c.clientEpoch = E(FRI, 300);
  ClockDecision d = decideClock(c);                          // empty log: nothing to protect
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_INT(SRC_WIZARD, d.source); TEST_ASSERT_EQUAL_UINT32(0, d.gapTo);
  c.lastLogEpoch = E(FRI - 1, 900);
  d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action); TEST_ASSERT_EQUAL_INT(SRC_MANUAL, d.source);
  TEST_ASSERT_EQUAL_UINT32(c.lastLogEpoch, d.gapFrom); TEST_ASSERT_EQUAL_UINT32(c.clientEpoch, d.gapTo);
  c.clientEpoch = c.lastLogEpoch - 1000;
  TEST_ASSERT_EQUAL_INT(CA_NEED_CONFIRM_BACK, decideClock(c).action);
}
void test_clock_apply_writes_one_group() {
  Rig r;
  put(r.log, scanEv(1, E(FRI, 490), ST_PRESENT));
  ClockCtx c = ctx(); c.explicitSync = true; c.confirmBack = true;
  c.deviceEpoch = E(FRI, 600); c.clientEpoch = E(FRI, 590); c.lastLogEpoch = r.log.baselineEpoch();
  ClockDecision d = decideClock(c);
  TEST_ASSERT_EQUAL_INT(CA_APPLY, d.action);
  size_t before = r.log.count();
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMITTED, applyClockSet(r.log, d, c.deviceEpoch, c.clientEpoch));
  TEST_ASSERT_EQUAL_UINT32(before + 3, r.log.count());       // GAP + CLOCK_SET old + CLOCK_SET new
  Event e;
  r.log.at(before, e);     TEST_ASSERT_EQUAL_INT(EV_GAP, e.type); TEST_ASSERT_TRUE(flagMore(e.flags));
  r.log.at(before + 1, e); TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, e.type); TEST_ASSERT_TRUE(flagMore(e.flags));
  r.log.at(before + 2, e); TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, e.type); TEST_ASSERT_FALSE(flagMore(e.flags));
  TEST_ASSERT_EQUAL_UINT32(c.clientEpoch, r.log.baselineEpoch());
  Rig r2;                                                    // a torn clock-set group leaves the old time authoritative
  put(r2.log, scanEv(1, E(FRI, 490), ST_PRESENT));
  r2.ms.tearBytes = 32;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMIT_UNCERTAIN, applyClockSet(r2.log, d, c.deviceEpoch, c.clientEpoch));
  const std::vector<uint8_t> damaged = r2.ms.d;
  LogStore again(r2.ms);
  OpenInfo o = again.open();
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(2, o.droppedGroup);
  TEST_ASSERT_TRUE(again.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, again.count());
  TEST_ASSERT_EQUAL_UINT32(damaged.size(), r2.ms.d.size());
  TEST_ASSERT_EQUAL_INT(0, memcmp(damaged.data(), r2.ms.d.data(), damaged.size()));
}
void test_clock_apply_rejects_unapproved_or_invalid_decisions_without_writing() {
  MemStorage ms; LogStore log(ms);
  TEST_ASSERT_TRUE(log.open().ok);
  ClockDecision d = {CA_NONE, 0, SRC_MANUAL, false, 0, 0};
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  d.action = CA_NEED_CONFIRM_BACK;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  d.action = CA_NEED_CONFIRM_JUMP;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  d.action = CA_BLOCKED_WINDOW;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  d.action = CA_APPLY;
  d.gapFrom = 300; d.gapTo = 299;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  d.gapFrom = d.gapTo = 0;
  d.source = (uint16_t)(SRC_WIZARD + 1);
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, ms.d.size());
  TEST_ASSERT_FALSE(log.faulted());
}
void test_clock_apply_rejects_unrepresentable_epoch_tail_before_any_write() {
  MemStorage bytes; LogStore log(bytes);
  TEST_ASSERT_TRUE(log.open().ok);
  ClockDecision d = {CA_APPLY, 0, SRC_MANUAL, false,
                     UINT32_MAX - 100u, UINT32_MAX};
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_REJECTED, applyClockSet(log, d, 111, 222));
  TEST_ASSERT_FALSE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, bytes.d.size());
}
void test_clock_apply_distinguishes_faulted_log_from_attempted_commit() {
  MemStorage unopenedBytes;
  LogStore unopened(unopenedBytes);
  ClockDecision d = {CA_APPLY, 0, SRC_MANUAL, false, 0, 0};
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_LOG_FAULTED, applyClockSet(unopened, d, 111, 222));
  TEST_ASSERT_EQUAL_UINT32(0, unopenedBytes.d.size());

  MemStorage bytes; LogStore log(bytes);
  TEST_ASSERT_TRUE(log.open().ok);
  bytes.failAppend = true;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMIT_UNCERTAIN, applyClockSet(log, d, 111, 222));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, bytes.d.size());
}
void test_clock_apply_complete_write_then_false_is_uncertain_and_replayed() {
  MemStorage bytes; LogStore log(bytes);
  TEST_ASSERT_TRUE(log.open().ok);
  ClockDecision d = {CA_APPLY, 0, SRC_MANUAL, false, 0, 0};
  bytes.completeAppendThenFail = true;
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMIT_UNCERTAIN, applyClockSet(log, d, 111, 222));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());

  LogStore recovered(bytes);
  OpenInfo opened = recovered.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(2, opened.records);
  TEST_ASSERT_EQUAL_UINT32(222, opened.baselineEpoch);
  Event oldSet, newSet;
  TEST_ASSERT_TRUE(recovered.at(0, oldSet));
  TEST_ASSERT_TRUE(recovered.at(1, newSet));
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, oldSet.type);
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, newSet.type);
  TEST_ASSERT_EQUAL_UINT32(111, oldSet.epoch);
  TEST_ASSERT_EQUAL_UINT32(222, newSet.epoch);
}
void test_clock_apply_late_group_failure_leaves_gap_prefix_but_no_clock_pair() {
  MemStorage bytes; LogStore log(bytes);
  TEST_ASSERT_TRUE(log.open().ok);
  const uint32_t from = 1000;
  const uint32_t to = from + 20u * 65535u * 60u;
  const uint32_t oldEpoch = 3000000000u, newEpoch = 3000000001u;
  ClockDecision d = {CA_APPLY, 0, SRC_MANUAL, false, from, to};
  bytes.failAppendAt = 2; // first 14 GAPs persist; the final GAP+CLOCK_SET group is refused
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMIT_UNCERTAIN, applyClockSet(log, d, oldEpoch, newEpoch));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(14, log.count());              // successful prefix append is tracked in this instance
  TEST_ASSERT_EQUAL_UINT32(14u * REC_SIZE, bytes.d.size());

  LogStore recovered(bytes);
  OpenInfo opened = recovered.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(14, opened.records);
  TEST_ASSERT_TRUE(opened.baselineEpoch < oldEpoch);       // no CLOCK_SET pair was appended
  Event e;
  for (size_t i = 0; i < recovered.count(); ++i) {
    TEST_ASSERT_TRUE(recovered.at(i, e));
    TEST_ASSERT_EQUAL_INT(EV_GAP, e.type);
  }
}

void test_near_scan_window() {
  Timetable t = defaultTimetable(); Params p = {0, 30, 15};
  TEST_ASSERT_TRUE(nearScanWindow(t, p, E(FRI, 465), 300));
  TEST_ASSERT_TRUE(nearScanWindow(t, p, E(FRI, 460), 300));
  TEST_ASSERT_FALSE(nearScanWindow(t, p, E(FRI, 459), 300));
  TEST_ASSERT_TRUE(nearScanWindow(t, p, E(FRI, 749), 300));
  TEST_ASSERT_FALSE(nearScanWindow(t, p, E(FRI, 750), 300));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_log_group_append_and_reopen);
  RUN_TEST(test_log_append_failure_latches);
  RUN_TEST(test_log_torn_partial_record_is_preserved_and_blocks_open);
  RUN_TEST(test_log_torn_group_is_preserved_and_blocks_open);
  RUN_TEST(test_log_mid_corruption_detected);
  RUN_TEST(test_log_bad_tail_record_is_preserved_and_blocks_open);
  RUN_TEST(test_log_partial_tail_is_preserved_without_attempting_truncate);
  RUN_TEST(test_log_baseline_epoch);
  RUN_TEST(test_log_group_limits_and_unopened);
  RUN_TEST(test_dayview_scan_mark_precedence);
  RUN_TEST(test_dayview_noclass_toggle);
  RUN_TEST(test_dayview_close_bitmap_300);
  RUN_TEST(test_dayview_ignores_other_day_and_session);
  RUN_TEST(test_close_normal_day_coverage_yes);
  RUN_TEST(test_close_waits_for_session_end);
  RUN_TEST(test_boot_midwindow_makes_session_unverified);
  RUN_TEST(test_boot_before_window_keeps_coverage);
  RUN_TEST(test_short_unhealthy_span_in_window);
  RUN_TEST(test_outage_between_sessions_keeps_coverage);
  RUN_TEST(test_overdue_closure_after_downtime);
  RUN_TEST(test_closure_waits_while_unhealthy_then_recovers);
  RUN_TEST(test_storage_failure_during_closure);
  RUN_TEST(test_failed_boot_gap_is_retried);
  RUN_TEST(test_engine_set_schedule_updates_window_and_session_end);
  RUN_TEST(test_engine_set_schedule_updates_utc_offset);
  RUN_TEST(test_engine_set_schedule_updates_term_start);
  RUN_TEST(test_no_close_for_weekend_or_before_term);
  RUN_TEST(test_long_gap_is_chunked_and_marks_old_sessions);
  RUN_TEST(test_tick_only_in_window_and_every_300s);
  RUN_TEST(test_close_snapshots_roster);
  RUN_TEST(test_health_priority_and_perms);
  RUN_TEST(test_health_faults_latch_until_reverified);
  RUN_TEST(test_health_clock_reverification_clears_only_clock_latch);
  RUN_TEST(test_health_unprovisioned_and_sensor_do_not_latch);
  RUN_TEST(test_clock_small_delta_is_info_only);
  RUN_TEST(test_clock_auto_sync_rules);
  RUN_TEST(test_clock_explicit_rules);
  RUN_TEST(test_clock_fault_uses_log_reference);
  RUN_TEST(test_clock_apply_writes_one_group);
  RUN_TEST(test_clock_apply_rejects_unapproved_or_invalid_decisions_without_writing);
  RUN_TEST(test_clock_apply_rejects_unrepresentable_epoch_tail_before_any_write);
  RUN_TEST(test_clock_apply_distinguishes_faulted_log_from_attempted_commit);
  RUN_TEST(test_clock_apply_complete_write_then_false_is_uncertain_and_replayed);
  RUN_TEST(test_clock_apply_late_group_failure_leaves_gap_prefix_but_no_clock_pair);
  RUN_TEST(test_near_scan_window);
  return UNITY_END();
}
