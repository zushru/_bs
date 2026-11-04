#include <unity.h>
#include <string.h>
#include <vector>
#include "roster_store.h"
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
static uint32_t noon(int32_t day) { return dayStartEpoch(day, 480) + 12 * 3600; }

struct Rig {
  MemStore ms; LogStore ls; RosterStore r;
  Rig() : ls(ms), r(480) { ls.open(); ls.attach(&r); }
  RosterErr set(uint16_t id, const char* nm, int32_t day) {
    Event e[5]; size_t c;
    RosterErr er = r.planSet(id, nm, strlen(nm), noon(day), e, c);
    if (er == RE_OK) ls.append(e, c);
    return er;
  }
  RosterErr off(uint16_t id, int32_t day) {
    Event e[5]; size_t c;
    RosterErr er = r.planDeactivate(id, noon(day), e, c);
    if (er == RE_OK) ls.append(e, c);
    return er;
  }
  RosterErr on(uint16_t id, int32_t day) {
    Event e[5]; size_t c;
    RosterErr er = r.planActivate(id, noon(day), e, c);
    if (er == RE_OK) ls.append(e, c);
    return er;
  }
};

void test_valid_names() {
  TEST_ASSERT_TRUE(RosterStore::validName("Ana", 3));
  TEST_ASSERT_TRUE(RosterStore::validName("Jos\xC3\xA9 Dela Cruz", 15));
  TEST_ASSERT_TRUE(RosterStore::validName("\xE5\xBC\xA0\xE4\xBC\x9F", 6));
  TEST_ASSERT_TRUE(RosterStore::validName("abcdefghijklmnopqrstuvwxyz012345", 32));
  TEST_ASSERT_FALSE(RosterStore::validName("abcdefghijklmnopqrstuvwxyz0123456", 33));
  TEST_ASSERT_FALSE(RosterStore::validName("", 0));
  TEST_ASSERT_FALSE(RosterStore::validName(nullptr, 3));
  TEST_ASSERT_FALSE(RosterStore::validName(" Ana", 4));
  TEST_ASSERT_FALSE(RosterStore::validName("Ana ", 4));
  TEST_ASSERT_FALSE(RosterStore::validName("A\nB", 3));
  TEST_ASSERT_FALSE(RosterStore::validName("A\x7F", 2));
  TEST_ASSERT_FALSE(RosterStore::validName("\xC3", 1));                 // truncated sequence
  TEST_ASSERT_FALSE(RosterStore::validName("\xC0\x80", 2));             // overlong
  TEST_ASSERT_FALSE(RosterStore::validName("\xED\xA0\x80", 3));         // surrogate
  TEST_ASSERT_FALSE(RosterStore::validName("\xF4\x90\x80\x80", 4));     // > U+10FFFF
  TEST_ASSERT_FALSE(RosterStore::validName("\xFF", 1));
}
void test_set_and_expected() {
  Rig g;
  TEST_ASSERT_EQUAL_INT(RE_OK, g.set(5, "Ana Reyes", 100));
  char nm[ROSTER_NAME_MAX + 1];
  TEST_ASSERT_EQUAL_UINT32(9, g.r.nameOf(5, nm));
  TEST_ASSERT_TRUE(strcmp(nm, "Ana Reyes") == 0);
  TEST_ASSERT_TRUE(g.r.known(5));
  TEST_ASSERT_TRUE(g.r.active(5));
  TEST_ASSERT_FALSE(g.r.expected(5, 99));
  TEST_ASSERT_TRUE(g.r.expected(5, 100));
  TEST_ASSERT_TRUE(g.r.expected(5, 250));
  TEST_ASSERT_FALSE(g.r.expected(6, 100));
  TEST_ASSERT_EQUAL_UINT32(1, g.r.knownCount());
  TEST_ASSERT_EQUAL_UINT32(1, g.r.activeCount());
}
void test_rename_and_noop() {
  Rig g;
  g.set(5, "Ana", 100);
  TEST_ASSERT_EQUAL_INT(RE_NOOP, g.set(5, "Ana", 101));
  TEST_ASSERT_EQUAL_INT(RE_OK, g.set(5, "Ana Reyes", 101));
  char nm[ROSTER_NAME_MAX + 1];
  g.r.nameOf(5, nm);
  TEST_ASSERT_TRUE(strcmp(nm, "Ana Reyes") == 0);
  TEST_ASSERT_TRUE(g.r.expected(5, 100));              // rename does not move the start day
  TEST_ASSERT_EQUAL_UINT32(1, g.r.knownCount());
}
void test_deactivate_and_reactivate() {
  Rig g;
  g.set(5, "Ana", 100);
  TEST_ASSERT_EQUAL_INT(RE_NOOP, g.on(5, 101));
  TEST_ASSERT_EQUAL_INT(RE_OK, g.off(5, 120));
  TEST_ASSERT_EQUAL_INT(RE_NOOP, g.off(5, 121));
  TEST_ASSERT_TRUE(g.r.expected(5, 119));
  TEST_ASSERT_FALSE(g.r.expected(5, 120));             // forward-only: not expected from the deactivation day
  TEST_ASSERT_FALSE(g.r.active(5));
  TEST_ASSERT_TRUE(g.r.known(5));                      // never deleted
  TEST_ASSERT_EQUAL_INT(RE_OK, g.on(5, 130));
  TEST_ASSERT_FALSE(g.r.expected(5, 125));
  TEST_ASSERT_TRUE(g.r.expected(5, 130));
  TEST_ASSERT_TRUE(g.r.expected(5, 119));              // history preserved
}
void test_plan_errors() {
  Rig g;
  Event e[5]; size_t c = 9;
  TEST_ASSERT_EQUAL_INT(RE_BAD_ID, g.r.planSet(0, "Ana", 3, 1, e, c));
  TEST_ASSERT_EQUAL_INT(RE_BAD_ID, g.r.planSet(MAX_ID + 1, "Ana", 3, 1, e, c));
  TEST_ASSERT_EQUAL_UINT32(0, c);
  TEST_ASSERT_EQUAL_INT(RE_BAD_NAME, g.r.planSet(1, "", 0, 1, e, c));
  TEST_ASSERT_EQUAL_INT(RE_UNKNOWN, g.r.planDeactivate(7, 1, e, c));
  TEST_ASSERT_EQUAL_INT(RE_UNKNOWN, g.r.planActivate(7, 1, e, c));
  TEST_ASSERT_EQUAL_INT(RE_BAD_ID, g.r.planDeactivate(0, 1, e, c));
}
void test_name_packing() {
  Rig g;
  Event e[5]; size_t c;
  TEST_ASSERT_EQUAL_INT(RE_OK, g.r.planSet(3, "ABCDEFGHIJ", 10, 5000, e, c));
  TEST_ASSERT_EQUAL_UINT32(5, c);
  TEST_ASSERT_EQUAL_INT(EV_ROSTER, e[0].type);
  TEST_ASSERT_EQUAL_UINT16(3, e[0].a);
  TEST_ASSERT_EQUAL_UINT16(RA_SET, e[0].b);
  TEST_ASSERT_TRUE(payloadBits(e[1]) == 0x4847464544434241ull);   // "ABCDEFGH" little-endian
  TEST_ASSERT_TRUE(payloadBits(e[2]) == 0x0000000000004A49ull);   // "IJ" then zeros
  TEST_ASSERT_TRUE(payloadBits(e[3]) == 0 && payloadBits(e[4]) == 0);
}
void test_replay_matches_live() {
  Rig g;
  g.set(1, "Ana", 100); g.set(2, "Ben", 100); g.set(300, "Zed", 101);
  g.off(2, 110); g.on(2, 115); g.off(1, 120);
  RosterStore fresh(480);
  TEST_ASSERT_TRUE(g.ls.attach(&fresh));                // replays the whole log
  for (uint16_t id = 1; id <= MAX_ID; id++)
    for (int32_t d = 95; d < 130; d += 3) TEST_ASSERT_EQUAL_INT(g.r.expected(id, d), fresh.expected(id, d));
  TEST_ASSERT_EQUAL_UINT32(g.r.knownCount(), fresh.knownCount());
  TEST_ASSERT_EQUAL_UINT32(g.r.activeCount(), fresh.activeCount());
  TEST_ASSERT_TRUE(fresh.known(300));
}
void test_incomplete_group_is_ignored() {
  RosterStore r(480);
  r.onRecord(Event{EV_ROSTER, 0, 4, noon(100), 1, RA_SET});
  r.onRecord(makePayload(0x434241ull, 2, false));
  r.onRecord(Event{EV_SCAN, 0, 4, noon(100), 3, 0});      // group cut short
  r.onRecord(makePayload(0x434241ull, 4, false));         // stray payloads
  r.onRecord(makePayload(0, 5, false));
  r.onRecord(makePayload(0, 6, false));
  r.onRecord(makePayload(0, 7, false));
  TEST_ASSERT_FALSE(r.known(4));
  r.onRecord(Event{EV_ROSTER, 0, 0, noon(100), 8, RA_SET});     // bad id
  r.onRecord(Event{EV_ROSTER, 0, 4, noon(100), 9, 99});         // unknown action
  r.onRecord(Event{EV_ROSTER, 0, 4, noon(100), 10, RA_DEACTIVATE});
  TEST_ASSERT_EQUAL_UINT32(0, r.knownCount());
}
void test_interval_overflow_fails_closed() {
  Rig g;
  g.set(1, "Ana", 100);
  g.set(2, "Ben", 100);
  int32_t d = 101;
  while (true) {
    TEST_ASSERT_EQUAL_INT(RE_OK, g.off(1, d++));
    RosterErr er = g.on(1, d++);
    if (er == RE_FULL) break;
    TEST_ASSERT_EQUAL_INT(RE_OK, er);
  }
  TEST_ASSERT_FALSE(g.r.overflow());                     // planning refuses before the table overflows
  TEST_ASSERT_TRUE(g.r.expected(2, 100));
  Event e = Event{EV_ROSTER, 0, 1, noon(d), 0, RA_ACTIVATE};
  g.r.onRecord(e);                                       // a record that bypassed planning
  TEST_ASSERT_TRUE(g.r.overflow());
  TEST_ASSERT_FALSE(g.r.expected(2, 100));               // nobody expected: never a false Absent
  Event ne[5]; size_t c;
  TEST_ASSERT_EQUAL_INT(RE_FULL, g.r.planSet(9, "Cy", 2, 1, ne, c));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_names);
  RUN_TEST(test_set_and_expected);
  RUN_TEST(test_rename_and_noop);
  RUN_TEST(test_deactivate_and_reactivate);
  RUN_TEST(test_plan_errors);
  RUN_TEST(test_name_packing);
  RUN_TEST(test_replay_matches_live);
  RUN_TEST(test_incomplete_group_is_ignored);
  RUN_TEST(test_interval_overflow_fails_closed);
  return UNITY_END();
}
