#include <unity.h>
#include <string.h>
#include <vector>
#include "month_set.h"
#include "log_store.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct FakeFiles : IFileSet {
  struct F { uint32_t key; std::vector<uint8_t> d; };
  std::vector<F> f;
  std::vector<std::vector<uint8_t> > q;
  size_t count() override { return f.size(); }
  uint32_t monthKey(size_t i) override { return f[i].key; }
  size_t fileSize(size_t i) override { return f[i].d.size(); }
  bool readFile(size_t i, size_t off, uint8_t* b, size_t n) override {
    if (off + n > f[i].d.size()) return false;
    memcpy(b, f[i].d.data() + off, n); return true;
  }
  bool appendFile(size_t i, const uint8_t* b, size_t n) override { f[i].d.insert(f[i].d.end(), b, b + n); return true; }
  bool truncateFile(size_t i, size_t n) override { if (n > f[i].d.size()) return false; f[i].d.resize(n); return true; }
  bool addFile(uint32_t k) override { F x; x.key = k; f.push_back(x); return true; }
  bool quarantineCopy(size_t i) override { q.push_back(f[i].d); return true; }
  bool quarantineFrom(size_t i) override {
    for (size_t j = i; j < f.size(); j++) q.push_back(f[j].d);
    f.resize(i); return true;
  }
};

static Event ev(uint16_t a) { Event e = {EV_SCAN, 0, a, (uint32_t)(1000 + a), 0, 0}; return e; }
// 3 months x 4 records through a real LogStore
static void fill(FakeFiles& ff) {
  MonthSet ms(ff);
  LogStore ls(ms);
  ls.open();
  for (int m = 0; m < 3; m++) {
    ms.setMonth(202611 + m);
    for (int i = 0; i < 4; i++) { Event e = ev((uint16_t)(m * 4 + i)); ls.append(&e, 1); }
  }
}

void test_read_spans_files() {
  FakeFiles ff;
  const size_t sz[3] = {48, 32, 64};
  std::vector<uint8_t> all;
  for (int k = 0; k < 3; k++) {
    FakeFiles::F x; x.key = 1 + k;
    for (size_t i = 0; i < sz[k]; i++) { x.d.push_back((uint8_t)((all.size() * 7 + 3) % 251)); all.push_back(x.d.back()); }
    ff.f.push_back(x);
  }
  MonthSet ms(ff);
  uint8_t b[200];
  for (size_t off = 0; off <= all.size(); off++)
    for (size_t n = 0; off + n <= all.size(); n++) {
      TEST_ASSERT_TRUE(ms.read(off, b, n));
      TEST_ASSERT_TRUE(n == 0 || memcmp(b, all.data() + off, n) == 0);
    }
}
void test_size_and_out_of_range() {
  FakeFiles ff;
  ff.addFile(1); ff.addFile(2);
  uint8_t d[32] = {0};
  ff.appendFile(0, d, 32); ff.appendFile(1, d, 16);
  MonthSet ms(ff);
  uint8_t b[64];
  TEST_ASSERT_EQUAL_UINT32(48, ms.size());
  TEST_ASSERT_TRUE(ms.read(40, b, 8));
  TEST_ASSERT_FALSE(ms.read(40, b, 9));
  TEST_ASSERT_FALSE(ms.read(49, b, 1));
}
void test_append_routing() {
  FakeFiles ff;
  MonthSet ms(ff);
  uint8_t d[16] = {0};
  TEST_ASSERT_FALSE(ms.append(d, 16));                 // month never set
  ms.setMonth(202611);
  TEST_ASSERT_TRUE(ms.append(d, 16));
  TEST_ASSERT_TRUE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(1, ff.f.size());
  ms.setMonth(202612);
  TEST_ASSERT_TRUE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(2, ff.f.size());
  ms.setMonth(202610);                                  // clock went backwards: newest file
  TEST_ASSERT_TRUE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(2, ff.f.size());
  TEST_ASSERT_EQUAL_UINT32(32, ff.f[1].d.size());
  ff.f[1].d.push_back(0);                               // misaligned last file
  ms.setMonth(202701);
  TEST_ASSERT_FALSE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(2, ff.f.size());
}
void test_truncate_rules() {
  FakeFiles ff;
  ff.addFile(1); ff.addFile(2); ff.addFile(3);
  uint8_t d[64] = {0};
  ff.appendFile(0, d, 32); ff.appendFile(1, d, 32);
  MonthSet ms(ff);
  TEST_ASSERT_FALSE(ms.truncate(16));                   // earlier file, later one non-empty
  TEST_ASSERT_EQUAL_UINT32(16, ms.refusedTruncate());
  TEST_ASSERT_EQUAL_UINT32(32, ff.f[0].d.size());
  TEST_ASSERT_TRUE(ms.truncate(48));                    // last non-empty file; file 3 is empty
  TEST_ASSERT_EQUAL_UINT32(16, ff.f[1].d.size());
  TEST_ASSERT_FALSE(ms.truncate(500));
}
void test_logstore_end_to_end() {
  FakeFiles ff;
  fill(ff);
  TEST_ASSERT_EQUAL_UINT32(3, ff.f.size());
  TEST_ASSERT_EQUAL_UINT32(64, ff.f[0].d.size());
  MonthSet ms(ff);
  LogStore ls(ms);
  OpenInfo oi = ls.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(12, oi.records);
  TEST_ASSERT_EQUAL_UINT32(12, oi.lastSeq);
  Event e;
  TEST_ASSERT_TRUE(ls.at(7, e));
  TEST_ASSERT_EQUAL_UINT16(7, e.a);
}
void test_torn_tail_in_last_file() {
  FakeFiles ff;
  fill(ff);
  for (int i = 0; i < 8; i++) ff.f[2].d.push_back(0x5A);
  MonthSet ms(ff);
  LogStore ls(ms);
  OpenInfo oi = ls.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(8, oi.tornBytes);
  TEST_ASSERT_EQUAL_UINT32(12, oi.records);
  TEST_ASSERT_EQUAL_UINT32(64, ff.f[2].d.size());
}
void test_mid_corruption_quarantine() {
  FakeFiles ff;
  fill(ff);
  ff.f[1].d[2 * 16 + 3] ^= 0x40;                        // global record 6
  MonthSet ms(ff);
  LogStore ls(ms);
  OpenInfo oi = ls.open();
  TEST_ASSERT_FALSE(oi.ok);
  TEST_ASSERT_EQUAL_INT(96, oi.corruptAt);
  QuarantinePlan p = ms.planAt((size_t)oi.corruptAt);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT32(1, p.fileIndex);
  TEST_ASSERT_EQUAL_UINT32(32, p.localOff);
  TEST_ASSERT_TRUE(ms.repair(p));
  TEST_ASSERT_EQUAL_UINT32(2, ff.q.size());
  TEST_ASSERT_EQUAL_UINT32(64, ff.q[0].size());
  TEST_ASSERT_EQUAL_UINT32(64, ff.q[1].size());
  TEST_ASSERT_EQUAL_UINT32(2, ff.f.size());
  TEST_ASSERT_EQUAL_UINT32(32, ff.f[1].d.size());
  oi = ls.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(6, oi.records);
  TEST_ASSERT_EQUAL_UINT32(1005, oi.baselineEpoch);     // last good record's epoch: GAP starts here
}
void test_partial_repair_is_repeatable() {
  FakeFiles ff;
  fill(ff);
  ff.f[1].d[2 * 16 + 3] ^= 0x40;
  MonthSet ms(ff);
  ff.quarantineCopy(1); ff.quarantineFrom(2);            // crash before the truncate step
  LogStore ls(ms);
  OpenInfo oi = ls.open();
  TEST_ASSERT_FALSE(oi.ok);
  TEST_ASSERT_EQUAL_INT(96, oi.corruptAt);
  QuarantinePlan p = ms.planAt((size_t)oi.corruptAt);
  TEST_ASSERT_TRUE(ms.repair(p));
  oi = ls.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(6, oi.records);
}
void test_plan_invalid_beyond_end() {
  FakeFiles ff;
  fill(ff);
  MonthSet ms(ff);
  TEST_ASSERT_FALSE(ms.planAt(192).valid);
  TEST_ASSERT_FALSE(ms.planAt(100000).valid);
  QuarantinePlan none = {false, 0, 0};
  TEST_ASSERT_FALSE(ms.repair(none));
}
void test_odd_sized_middle_file_is_caught() {
  FakeFiles ff;
  ff.addFile(1);
  MonthSet ms(ff);
  LogStore ls(ms);
  ls.open();
  ms.setMonth(1);
  for (int i = 0; i < 2; i++) { Event e = ev((uint16_t)i); ls.append(&e, 1); }
  for (int i = 0; i < 8; i++) ff.f[0].d.push_back(0x77);   // 40-byte file
  FakeFiles::F x; x.key = 2;
  for (int i = 2; i < 4; i++) {
    Event e = ev((uint16_t)i); e.seq = (uint32_t)i + 1;
    uint8_t r[REC_SIZE]; encode(e, r);
    x.d.insert(x.d.end(), r, r + REC_SIZE);
  }
  ff.f.push_back(x);
  LogStore l2(ms);
  OpenInfo oi = l2.open();
  TEST_ASSERT_FALSE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(32, ms.refusedTruncate());
  QuarantinePlan p = ms.planAt(ms.refusedTruncate());
  TEST_ASSERT_EQUAL_UINT32(0, p.fileIndex);
  TEST_ASSERT_EQUAL_UINT32(32, p.localOff);
  TEST_ASSERT_TRUE(ms.repair(p));
  oi = l2.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(2, oi.records);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_read_spans_files);
  RUN_TEST(test_size_and_out_of_range);
  RUN_TEST(test_append_routing);
  RUN_TEST(test_truncate_rules);
  RUN_TEST(test_logstore_end_to_end);
  RUN_TEST(test_torn_tail_in_last_file);
  RUN_TEST(test_mid_corruption_quarantine);
  RUN_TEST(test_partial_repair_is_repeatable);
  RUN_TEST(test_plan_invalid_beyond_end);
  RUN_TEST(test_odd_sized_middle_file_is_caught);
  return UNITY_END();
}
