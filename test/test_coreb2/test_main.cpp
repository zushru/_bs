#include <unity.h>
#include <string.h>
#include "fakes.h"
#include "log_index.h"
#include "day_view.h"
using namespace att;

void setUp() {}
void tearDown() {}

static uint32_t rs = 12345;
static uint32_t rnd() { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static uint64_t rnd64() { return ((uint64_t)rnd() << 40) ^ ((uint64_t)rnd() << 20) ^ (uint64_t)rnd(); }
static const int32_t FRI = 2494;
static uint32_t E(int32_t day, int minute) { return dayStartEpoch(day, 0) + (uint32_t)minute * 60; }
static Params P() { Params p = {0, 30, 15}; return p; }

// One random durable append: scans, marks, no-class, closes (with payload), gaps, and junk ids.
static void randomAppend(LogStore& l, int32_t d0) {
  int32_t day = d0 + (int32_t)(rnd() % 4);
  uint8_t sess = (uint8_t)(rnd() % 2);
  uint32_t t = E(day, (int)(rnd() % 1440));
  switch (rnd() % 6) {
    case 0: { Event e = {EV_SCAN, makeFlags(sess, 1 + rnd() % 2, false, false, false), (uint16_t)(rnd() % 62), t, 0, 100}; l.append(&e, 1); break; }
    case 1: { Event e = {EV_MARK, makeFlags(sess, rnd() % 4, false, false, false), (uint16_t)(rnd() % 62), t, (uint16_t)day}; l.append(&e, 1); break; }
    case 2: { Event e = {rnd() % 2 ? EV_NOCLASS_SET : EV_NOCLASS_CLEAR, makeFlags(sess, 0, false, false, rnd() % 2), 0, t, (uint16_t)day}; l.append(&e, 1); break; }
    case 3: {
      Event g[6];
      g[0] = Event{EV_CLOSE, makeFlags(sess, 0, rnd() % 2, true, false), (uint16_t)(rnd() % 61), t, 0, (uint16_t)day};
      for (int i = 0; i < 5; i++) g[i + 1] = makePayload(rnd64(), 0, i < 4);
      l.append(g, 6);
      break;
    }
    case 4: { Event e = {EV_GAP, 0, GAP_POWER, t, 0, (uint16_t)(1 + rnd() % 120)}; l.append(&e, 1); break; }
    default: { Event e = {EV_SCAN, makeFlags(sess, 1, false, false, false), (uint16_t)(rnd() % 2 ? 0 : 301), t, 0, 1}; l.append(&e, 1); break; }
  }
}
static bool sameView(const DayView& a, const DayView& b) {
  if (a.day != b.day || a.session != b.session || a.noClassDay != b.noClassDay ||
      a.noClassSession != b.noClassSession || a.closed != b.closed || a.coverage != b.coverage ||
      a.expectedCount != b.expectedCount) return false;
  if (memcmp(a.scan, b.scan, sizeof a.scan) || memcmp(a.mark, b.mark, sizeof a.mark)) return false;
  return memcmp(a.expected, b.expected, sizeof a.expected) == 0;
}
static bool bruteClosed(LogStore& l, int32_t day, uint8_t s) {
  Event e;
  for (size_t i = 0; i < l.count(); i++) {
    l.at(i, e);
    if (e.type == EV_CLOSE && (int32_t)e.b == day && flagSession(e.flags) == s) return true;
  }
  return false;
}
static bool bruteOverlap(LogStore& l, uint32_t ws, uint32_t we) {
  Event e;
  for (size_t i = 0; i < l.count(); i++) {
    l.at(i, e);
    if (e.type != EV_GAP) continue;
    uint32_t gs = e.epoch, ge = e.epoch + (uint32_t)e.b * 60u;
    if (gs < we && ge > ws) return true;
  }
  return false;
}
// Checks every cache and the index against a full replay / brute scan. Returns false on first mismatch.
static bool checkAll(LogStore& l, LogIndex& idx, DayCache* caches[8], const Params& p, int32_t d0) {
  for (int di = 0; di < 4; di++)
    for (int s = 0; s < 2; s++) {
      int32_t day = d0 + di;
      caches[di * 2 + s]->target(day, (uint8_t)s);
      const DayView* v = nullptr;
      if (!caches[di * 2 + s]->view(v)) return false;
      DayView ref;
      if (!buildDayView(l, p, day, (uint8_t)s, ref)) return false;
      if (!sameView(*v, ref)) return false;
      if (idx.closed(day, (uint8_t)s) != bruteClosed(l, day, (uint8_t)s)) return false;
    }
  for (int k = 0; k < 6; k++) {
    uint32_t ws = E(d0 + (int32_t)(rnd() % 4), (int)(rnd() % 1440));
    uint32_t we = ws + 60u * (1 + rnd() % 600);
    if (!idx.gapOverflow() && idx.gapOverlaps(ws, we) != bruteOverlap(l, ws, we)) return false;
  }
  return true;
}

void test_index_and_cache_match_replay_random() {
  MemStorage ms; LogStore l(ms); l.open();
  Params p = P();
  LogIndex idx;
  TEST_ASSERT_TRUE(l.attach(&idx));
  DayCache* caches[8];
  for (int i = 0; i < 8; i++) { caches[i] = new DayCache(l, p); TEST_ASSERT_TRUE(l.attach(caches[i])); }
  bool ok = true;
  for (int step = 0; step < 400 && ok; step++) {
    randomAppend(l, FRI - 2);
    ok = checkAll(l, idx, caches, p, FRI - 2);
  }
  for (int i = 0; i < 8; i++) delete caches[i];
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(idx.closeCount() > 0);
  TEST_ASSERT_TRUE(idx.gapCount() > 0);
}
void test_reopen_after_tail_loss_rebuilds_everything() {
  MemStorage ms; LogStore l(ms); l.open();
  Params p = P();
  LogIndex idx;
  l.attach(&idx);
  DayCache* caches[8];
  for (int i = 0; i < 8; i++) { caches[i] = new DayCache(l, p); l.attach(caches[i]); }
  for (int step = 0; step < 200; step++) randomAppend(l, FRI - 2);
  TEST_ASSERT_TRUE(checkAll(l, idx, caches, p, FRI - 2));
  size_t cut = ms.d.size() / 2;                       // lose the back half of the log (e.g. a bad restore)
  cut -= cut % REC_SIZE;
  ms.d.resize(cut);
  OpenInfo o = l.open();
  TEST_ASSERT_TRUE(o.ok);
  bool ok = checkAll(l, idx, caches, p, FRI - 2);     // stale caches would fail here
  for (int i = 0; i < 8; i++) delete caches[i];
  TEST_ASSERT_TRUE(ok);
}
void test_attach_replays_existing_records() {
  MemStorage ms; LogStore l(ms); l.open();
  Event g[3];
  g[0] = Event{EV_GAP, 0, GAP_POWER, E(FRI, 400), 0, 60};
  g[1] = Event{EV_CLOSE, makeFlags(0, 0, true, true, false), 2, E(FRI, 751), 0, (uint16_t)FRI};
  g[2] = makePayload(0x3ull, 0, false);
  TEST_ASSERT_TRUE(l.append(g, 3));
  LogIndex idx;
  TEST_ASSERT_TRUE(l.attach(&idx));                   // attached after the records already exist
  TEST_ASSERT_TRUE(idx.closed(FRI, 0));
  TEST_ASSERT_FALSE(idx.closed(FRI, 1));
  TEST_ASSERT_TRUE(idx.gapOverlaps(E(FRI, 400), E(FRI, 401)));
  TEST_ASSERT_FALSE(idx.gapOverlaps(E(FRI, 460), E(FRI, 470)));
}
void test_observer_limit() {
  MemStorage ms; LogStore l(ms); l.open();
  LogIndex idx[MAX_OBSERVERS], extra;
  for (size_t i = 0; i < MAX_OBSERVERS; i++) TEST_ASSERT_TRUE(l.attach(&idx[i]));
  TEST_ASSERT_FALSE(l.attach(&extra));          // slots full
  TEST_ASSERT_FALSE(l.attach(&idx[0]));         // already registered
}
void test_nightly_boots_write_no_gaps() {
  // Device powered off every night, back on before the morning window: nothing touches a window, so no GAP.
  MemStorage ms; LogStore l(ms); l.open();
  Timetable tt = defaultTimetable();
  Params p = P();
  FakeRoster ro(45);
  ClosureEngine eng(l, p, tt, ro, 45, FRI - 60);
  for (int d = 0; d < 50; d++) {
    int32_t day = FRI - 50 + d;
    if (weekdayIndex(day) > 4) continue;
    eng.onBoot(E(day, 420), E(day - 1, 1000));          // last tick 16:40 the day before, boot 07:00
  }
  Event e; int gaps = 0;
  for (size_t i = 0; i < l.count(); i++) { l.at(i, e); if (e.type == EV_GAP) gaps++; }
  TEST_ASSERT_EQUAL_INT(0, gaps);
  TEST_ASSERT_EQUAL_UINT32(0, eng.index().gapCount());
}
void test_boot_inside_window_clips_gap() {
  MemStorage ms; LogStore l(ms); l.open();
  Timetable tt = defaultTimetable();
  Params p = P();
  FakeRoster ro(45);
  ClosureEngine eng(l, p, tt, ro, 45, FRI);
  eng.onBoot(E(FRI, 480), E(FRI, 460));                 // down 07:40 -> 08:00; window opens 07:45
  Event e;
  TEST_ASSERT_TRUE(l.at(0, e));
  TEST_ASSERT_EQUAL_INT(EV_GAP, e.type);
  TEST_ASSERT_EQUAL_UINT32(E(FRI, 465), e.epoch);       // clipped to the window start, not 07:40
  TEST_ASSERT_EQUAL_UINT16(15, e.b);
}
void test_engine_refuses_without_index() {
  MemStorage ms; LogStore l(ms); l.open();
  LogIndex filler[MAX_OBSERVERS];
  for (size_t i = 0; i < MAX_OBSERVERS; i++) l.attach(&filler[i]);     // fill every slot
  Timetable tt = defaultTimetable();
  Params p = P();
  FakeRoster ro(45);
  ClosureEngine eng(l, p, tt, ro, 45, FRI);                          // its attach must fail
  size_t before = l.count();
  TEST_ASSERT_FALSE(eng.step(E(FRI, 800)));
  TEST_ASSERT_TRUE(eng.pending());
  TEST_ASSERT_EQUAL_UINT32(before, l.count());                       // nothing written
}
void test_gap_overflow_flag() {
  LogIndex idx;
  Event e = Event{EV_GAP, 0, GAP_POWER, E(FRI, 0), 0, 1};
  for (size_t i = 0; i < MAX_GAPS; i++) idx.onRecord(e);
  TEST_ASSERT_FALSE(idx.gapOverflow());
  idx.onRecord(e);
  TEST_ASSERT_TRUE(idx.gapOverflow());
  TEST_ASSERT_EQUAL_UINT32(MAX_GAPS, idx.gapCount());
}
void test_engine_gap_overflow_is_unverified() {
  MemStorage ms; LogStore l(ms); l.open();
  for (size_t i = 0; i <= MAX_GAPS; i++) {               // one more GAP than the index can hold; all well before the session
    Event g = Event{EV_GAP, 0, GAP_POWER, E(FRI - 3, (int)(i % 1400)), 0, 1};
    TEST_ASSERT_TRUE(l.append(&g, 1));
  }
  Timetable tt = defaultTimetable();
  Params p = P();
  FakeRoster ro(45);
  ClosureEngine eng(l, p, tt, ro, 45, FRI);
  TEST_ASSERT_TRUE(eng.index().gapOverflow());
  TEST_ASSERT_TRUE(eng.step(E(FRI, 800)));              // session 0 ends 12:30 -> closed
  Event e; bool found = false, cov = true;
  for (size_t i = 0; i < l.count(); i++) {
    l.at(i, e);
    if (e.type == EV_CLOSE && e.b == (uint16_t)FRI && flagSession(e.flags) == 0) { found = true; cov = flagCoverage(e.flags); }
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_FALSE(cov);                               // overflow must yield Unverified, never Absent
}
void test_close_overflow_refuses_more_closes() {
  LogIndex idx;
  for (size_t i = 0; i < MAX_CLOSES; i++) {
    Event c = Event{EV_CLOSE, makeFlags(0, 0, true, false, false), 1, E(FRI, 800), 0, (uint16_t)(1000 + i)};
    idx.onRecord(c);
  }
  TEST_ASSERT_FALSE(idx.closeOverflow());
  TEST_ASSERT_TRUE(idx.closed(1000, 0));
  Event extra = Event{EV_CLOSE, makeFlags(0, 0, true, false, false), 1, E(FRI, 800), 0, 5000};
  idx.onRecord(extra);
  TEST_ASSERT_TRUE(idx.closeOverflow());
  TEST_ASSERT_FALSE(idx.closed(5000, 0));
  TEST_ASSERT_EQUAL_UINT32(MAX_CLOSES, idx.closeCount());
}
void test_first_close_wins_in_index_and_cache() {
  MemStorage ms; LogStore l(ms); l.open();
  LogIndex idx; l.attach(&idx);
  Params p = P();
  DayCache c(l, p); l.attach(&c);
  c.target(FRI, 0);
  const DayView* v = nullptr;
  c.view(v);
  Event a[2];
  a[0] = Event{EV_CLOSE, makeFlags(0, 0, true, true, false), 1, E(FRI, 751), 0, (uint16_t)FRI};
  a[1] = makePayload(0x1ull, 0, false);
  l.append(a, 2);
  Event b[2];
  b[0] = Event{EV_CLOSE, makeFlags(0, 0, false, true, false), 9, E(FRI, 752), 0, (uint16_t)FRI};
  b[1] = makePayload(0x2ull, 0, false);
  l.append(b, 2);
  TEST_ASSERT_TRUE(c.view(v));
  TEST_ASSERT_TRUE(v->closed); TEST_ASSERT_TRUE(v->coverage); TEST_ASSERT_EQUAL_UINT32(1, v->expectedCount);
  TEST_ASSERT_TRUE(idx.closed(FRI, 0));
  TEST_ASSERT_EQUAL_UINT32(1, idx.closeCount());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_index_and_cache_match_replay_random);
  RUN_TEST(test_reopen_after_tail_loss_rebuilds_everything);
  RUN_TEST(test_attach_replays_existing_records);
  RUN_TEST(test_observer_limit);
  RUN_TEST(test_engine_refuses_without_index);
  RUN_TEST(test_nightly_boots_write_no_gaps);
  RUN_TEST(test_boot_inside_window_clips_gap);
  RUN_TEST(test_gap_overflow_flag);
  RUN_TEST(test_engine_gap_overflow_is_unverified);
  RUN_TEST(test_close_overflow_refuses_more_closes);
  RUN_TEST(test_first_close_wins_in_index_and_cache);
  return UNITY_END();
}
