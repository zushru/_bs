#include <limits.h>
#include <unity.h>
#include <string.h>
#include <vector>
#include "enrolled_roster.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct MemStore : IStorage {
  std::vector<uint8_t> d;
  size_t size() override { return d.size(); }
  bool read(size_t o, uint8_t* b, size_t n) override { if (o + n > d.size()) return false; memcpy(b, d.data() + o, n); return true; }
  bool append(const uint8_t* b, size_t n) override { d.insert(d.end(), b, b + n); return true; }
  bool truncate(size_t n) override { d.resize(n); return true; }
};
static const int32_t FRI = 2494;
static uint32_t E(int32_t day, int minute) { return dayStartEpoch(day, 0) + (uint32_t)minute * 60; }
struct Rig {
  MemStore ms; LogStore ls; RosterStore r;
  Rig() : ls(ms), r(0) { ls.open(); ls.attach(&r); }
  void add(uint16_t id, const char* nm, int32_t day = FRI - 10) {
    Event e[5]; size_t c;
    r.planSet(id, nm, strlen(nm), E(day, 600), e, c); ls.append(e, c);
  }
  void off(uint16_t id, int32_t day) {
    Event e[5]; size_t c;
    r.planDeactivate(id, E(day, 600), e, c); ls.append(e, c);
  }
};

void test_id_page_mapping_boundaries() {
  TEST_ASSERT_EQUAL_UINT16(0, idToPage(1));
  TEST_ASSERT_EQUAL_UINT16(299, idToPage(300));        // the last valid page of a 300-template sensor
  for (uint16_t id = 1; id <= 300; id++) TEST_ASSERT_EQUAL_UINT16(id, pageToId(idToPage(id)));
}
void test_occupancy_basics_and_bounds() {
  Occupancy o;
  TEST_ASSERT_FALSE(o.valid);
  o.setPresent(1, true); o.setPresent(300, true); o.setPresent(45, true);
  o.setPresent(0, true); o.setPresent(301, true); o.setPresent(65535, true);
  TEST_ASSERT_EQUAL_UINT32(3, o.count());
  TEST_ASSERT_TRUE(o.has(1) && o.has(300) && o.has(45));
  TEST_ASSERT_FALSE(o.has(0)); TEST_ASSERT_FALSE(o.has(301)); TEST_ASSERT_FALSE(o.has(2));
  o.setPresent(45, false);
  TEST_ASSERT_FALSE(o.has(45));
  o.begin();
  TEST_ASSERT_EQUAL_UINT32(0, o.count());
  TEST_ASSERT_FALSE(o.valid);
}
void test_index_page_decoding() {
  Occupancy o; uint8_t p0[32] = {0}, p1[32] = {0};
  p0[0] = 0x01;            // slot 0   -> ID 1
  p0[1] = 0x80;            // slot 15  -> ID 16
  p0[31] = 0x80;           // slot 255 -> ID 256
  p1[0] = 0x01;            // slot 256 -> ID 257
  p1[5] = 0x08;            // slot 256+43 = 299 -> ID 300
  p1[6] = 0x01;            // slot 304: beyond capacity 300, ignored
  o.begin();
  o.addIndexPage(0, p0, 300);
  TEST_ASSERT_FALSE(o.valid);                           // pages added but read not finished
  o.addIndexPage(1, p1, 300);
  o.finish();
  TEST_ASSERT_TRUE(o.valid);
  TEST_ASSERT_EQUAL_UINT32(5, o.count());
  TEST_ASSERT_TRUE(o.has(1) && o.has(16) && o.has(256) && o.has(257) && o.has(300));
  Occupancy small; small.begin(); small.addIndexPage(0, p0, 100); small.finish();   // capacity 100
  TEST_ASSERT_EQUAL_UINT32(2, small.count());
  uint8_t junk[32]; memset(junk, 0xFF, 32);
  Occupancy x; x.addIndexPage(4, junk, 300);            // no such page
  TEST_ASSERT_EQUAL_UINT32(0, x.count());
  TEST_ASSERT_EQUAL_UINT8(2, Occupancy::pagesNeeded(300));
  TEST_ASSERT_EQUAL_UINT8(1, Occupancy::pagesNeeded(256));
  TEST_ASSERT_EQUAL_UINT8(2, Occupancy::pagesNeeded(257));
}
void test_expected_requires_template_and_valid_read() {
  Rig g; g.add(1, "Ana"); g.add(2, "Ben"); g.add(3, "Cy");
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(3, true);
  EnrolledRoster er(g.r, o);
  TEST_ASSERT_FALSE(er.expected(1, FRI));               // occupancy not finished: nobody
  o.finish();
  TEST_ASSERT_TRUE(er.expected(1, FRI));
  TEST_ASSERT_FALSE(er.expected(2, FRI));               // no template
  TEST_ASSERT_TRUE(er.expected(3, FRI));
  TEST_ASSERT_FALSE(er.expected(1, FRI - 20));          // before the student's start day
  g.off(3, FRI - 2);
  TEST_ASSERT_FALSE(er.expected(3, FRI));               // deactivated: template alone is not enough
  o.setPresent(9, true);
  TEST_ASSERT_FALSE(er.expected(9, FRI));               // a stray template is not a student
  o.begin();                                            // refresh restarted: back to nobody
  TEST_ASSERT_FALSE(er.expected(1, FRI));
}
void test_reconcile_counts() {
  Rig g; g.add(1, "Ana"); g.add(2, "Ben"); g.add(3, "Cy"); g.add(4, "Di"); g.off(4, FRI - 5);
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(3, true); o.setPresent(4, true); o.setPresent(7, true);
  Reconcile bad = reconcile(g.r, o);
  TEST_ASSERT_FALSE(bad.valid);
  TEST_ASSERT_EQUAL_UINT16(0, bad.matched);
  o.finish();
  Reconcile x = reconcile(g.r, o);
  TEST_ASSERT_TRUE(x.valid);
  TEST_ASSERT_EQUAL_UINT16(2, x.matched);               // 1, 3
  TEST_ASSERT_EQUAL_UINT16(1, x.orphans);               // 2
  TEST_ASSERT_EQUAL_UINT16(2, x.firstOrphan);
  TEST_ASSERT_EQUAL_UINT16(1, x.strays);                // 7 (4 is a known, inactive student: not a stray)
  TEST_ASSERT_EQUAL_UINT16(7, x.firstStray);
}
void test_queue_and_slot_allocation() {
  Rig g; g.add(1, "A1"); g.add(2, "B2"); g.add(3, "C3"); g.add(5, "E5"); g.off(3, FRI - 5);
  Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(4, true); o.finish();
  TEST_ASSERT_EQUAL_UINT16(2, nextToEnroll(g.r, o, 0));
  TEST_ASSERT_EQUAL_UINT16(5, nextToEnroll(g.r, o, 2));  // 3 is inactive, 4 is not a student
  TEST_ASSERT_EQUAL_UINT16(0, nextToEnroll(g.r, o, 5));
  TEST_ASSERT_EQUAL_UINT16(0, nextToEnroll(g.r, o, 65535));   // no wraparound
  TEST_ASSERT_EQUAL_UINT16(6, firstFreeSlot(g.r, o, 300));    // 1,2,3(inactive),5 known; 4 occupied
  TEST_ASSERT_EQUAL_UINT16(0, firstFreeSlot(g.r, o, 5));      // capacity 5: all taken
  Rig empty; Occupancy e; e.begin(); e.finish();
  TEST_ASSERT_EQUAL_UINT16(1, firstFreeSlot(empty.r, e, 300));
  TEST_ASSERT_EQUAL_UINT16(0, firstFreeSlot(empty.r, e, 0));
}
// ---- end to end: a restore onto a blank sensor must not produce false Absent marks ----
static int closedExpected(Occupancy& o, Rig& g, int* closes) {
  Params p = {0, 30, 15};
  Timetable tt = defaultTimetable();
  EnrolledRoster er(g.r, o);
  ClosureEngine eng(g.ls, p, tt, er, 300, FRI);
  eng.onBoot(E(FRI, 400), 0);
  for (uint32_t t = E(FRI, 465); t <= E(FRI, 760); t += 10) { eng.onHealth(t, true, GAP_SENSOR); eng.onTick(t); eng.step(t); }
  Event e; int exp = -1; *closes = 0;
  for (size_t i = 0; i < g.ls.count(); i++) {
    g.ls.at(i, e);
    if (e.type == EV_CLOSE && flagSession(e.flags) == 0 && e.b == FRI) { exp = e.a; (*closes)++; }
  }
  return exp;
}
void test_close_snapshot_excludes_unenrolled() {
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");
    Occupancy o; o.begin(); for (uint16_t id = 1; id <= 5; id++) o.setPresent(id, true); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(5, closedExpected(o, g, &closes)); TEST_ASSERT_EQUAL_INT(1, closes); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // restored roster, sensor has 3 templates
    Occupancy o; o.begin(); o.setPresent(1, true); o.setPresent(2, true); o.setPresent(5, true); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(3, closedExpected(o, g, &closes)); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // blank replacement sensor
    Occupancy o; o.begin(); o.finish();
    int closes; TEST_ASSERT_EQUAL_INT(0, closedExpected(o, g, &closes)); }
  { Rig g; for (uint16_t id = 1; id <= 5; id++) g.add(id, "Stu");            // index read never finished
    Occupancy o; o.begin(); for (uint16_t id = 1; id <= 5; id++) o.setPresent(id, true);
    int closes; TEST_ASSERT_EQUAL_INT(0, closedExpected(o, g, &closes)); }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_id_page_mapping_boundaries);
  RUN_TEST(test_occupancy_basics_and_bounds);
  RUN_TEST(test_index_page_decoding);
  RUN_TEST(test_expected_requires_template_and_valid_read);
  RUN_TEST(test_reconcile_counts);
  RUN_TEST(test_queue_and_slot_allocation);
  RUN_TEST(test_close_snapshot_excludes_unenrolled);
  return UNITY_END();
}
