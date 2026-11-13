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
  FakeFiles() : failCount(false), failKey(false), failSize(false),
      partialAppendBytes(0), appendCalls(0), allowTruncate(true),
      truncateCalls(0), completeAppendThenFail(false),
      readCalls(0), failReadAt(0), failAddAfterCreate(false) {}
  std::vector<F> f;
  std::vector<std::vector<uint8_t> > q;
  bool failCount, failKey, failSize;
  size_t partialAppendBytes, appendCalls;
  bool allowTruncate;
  size_t truncateCalls;
  bool completeAppendThenFail;
  size_t readCalls, failReadAt;
  bool failAddAfterCreate;
  bool countChecked(size_t& out) override {
    if (failCount) return false;
    out = f.size(); return true;
  }
  bool monthKeyChecked(size_t i, uint32_t& out) override {
    if (failKey || i >= f.size()) return false;
    out = f[i].key; return true;
  }
  bool fileSizeChecked(size_t i, size_t& out) override {
    if (failSize || i >= f.size()) return false;
    out = f[i].d.size(); return true;
  }
  bool readFile(size_t i, size_t off, uint8_t* b, size_t n) override {
    ++readCalls;
    if (failReadAt != 0 && readCalls == failReadAt) return false;
    if (off + n > f[i].d.size()) return false;
    memcpy(b, f[i].d.data() + off, n); return true;
  }
  bool appendFile(size_t i, const uint8_t* b, size_t n) override {
    ++appendCalls;
    if (partialAppendBytes) {
      size_t take = partialAppendBytes < n ? partialAppendBytes : n;
      f[i].d.insert(f[i].d.end(), b, b + take);
      partialAppendBytes = 0;
      return false;
    }
    f[i].d.insert(f[i].d.end(), b, b + n);
    if (completeAppendThenFail) {
      completeAppendThenFail = false;
      return false;
    }
    return true;
  }
  bool truncateFile(size_t i, size_t n) override {
    ++truncateCalls;
    if (!allowTruncate || n > f[i].d.size()) return false;
    f[i].d.resize(n);
    return true;
  }
  bool addFile(uint32_t k) override {
    F x; x.key = k; f.push_back(x);
    if (failAddAfterCreate) { failAddAfterCreate = false; return false; }
    return true;
  }
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
  TEST_ASSERT_FALSE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(8, oi.tornBytes);
  TEST_ASSERT_TRUE(ls.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, ls.count());
  TEST_ASSERT_EQUAL_UINT32(72, ff.f[2].d.size());
  TEST_ASSERT_EQUAL_UINT32(0, ff.truncateCalls);
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

struct CountingObserver : LogObserver {
  size_t resets;
  size_t records;
  CountingObserver() : resets(0), records(0) {}
  void onReset() override { ++resets; records = 0; }
  void onRecord(const Event&) override { ++records; }
};

void test_replay_read_failure_resets_partial_observer_state() {
  FakeFiles ff;
  fill(ff);  // 12 valid records; three files remain untouched by this open
  ff.readCalls = 0;
  ff.failReadAt = 15;  // 12 scan reads + tail-group read + 1 replay read, then fail
  MonthSet storage(ff);
  LogStore log(storage);
  CountingObserver observer;
  TEST_ASSERT_TRUE(log.attach(&observer));

  const OpenInfo opened = log.open();
  TEST_ASSERT_FALSE(opened.ok);
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);
  TEST_ASSERT_EQUAL_UINT32(3, observer.resets);  // attach, open, rollback of partial replay
}

void test_add_file_false_after_create_faults_log_until_reopen() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);
  ff.failAddAfterCreate = true;
  Event event = ev(77);

  // addFile() reports uncertainty after creating the empty month entry.
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, ff.f.size());
  TEST_ASSERT_EQUAL_UINT32(0, ff.f[0].d.size());
  TEST_ASSERT_EQUAL_UINT32(0, ff.appendCalls);

  // A second append is refused; the event payload was never written.
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_EQUAL_UINT32(0, ff.appendCalls);

  // A fresh read-only open reconciles the visible empty file before retry.
  OpenInfo reopened = log.open();
  TEST_ASSERT_TRUE(reopened.ok);
  TEST_ASSERT_EQUAL_UINT32(0, reopened.records);
  TEST_ASSERT_EQUAL_UINT32(0, ff.f[0].d.size());
  TEST_ASSERT_TRUE(log.append(&event, 1));
  TEST_ASSERT_EQUAL_UINT32(1, ff.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(16, ff.f[0].d.size());

  reopened = log.open();
  TEST_ASSERT_TRUE(reopened.ok);
  TEST_ASSERT_EQUAL_UINT32(1, reopened.records);
}

void test_partial_append_failure_latches_and_prevents_retry() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);
  CountingObserver observer;
  TEST_ASSERT_TRUE(log.attach(&observer));

  ff.partialAppendBytes = 7;
  Event event = ev(1);
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, ff.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(7, ff.f[0].d.size());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);

  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_EQUAL_UINT32(1, ff.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(7, ff.f[0].d.size());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);
}

void test_complete_write_reported_failed_is_indeterminate_after_reopen() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);
  CountingObserver observer;
  TEST_ASSERT_TRUE(log.attach(&observer));

  ff.completeAppendThenFail = true;
  Event event = ev(42);
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, ff.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, ff.f[0].d.size());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);

  MonthSet reopenedStorage(ff);
  LogStore reopened(reopenedStorage);
  const OpenInfo info = reopened.open();
  TEST_ASSERT_TRUE(info.ok);
  TEST_ASSERT_EQUAL_UINT32(1, info.records);
  TEST_ASSERT_EQUAL_UINT32(1, info.lastSeq);
  Event recovered;
  TEST_ASSERT_TRUE(reopened.at(0, recovered));
  TEST_ASSERT_EQUAL_UINT16(42, recovered.a);
}

void test_incomplete_commit_group_is_not_replayed_when_repair_fails() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);

  Event events[2] = {ev(50), ev(51)};
  ff.partialAppendBytes = REC_SIZE;
  TEST_ASSERT_FALSE(log.append(events, 2));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, ff.f[0].d.size());

  ff.allowTruncate = false;
  MonthSet reopenedStorage(ff);
  LogStore reopened(reopenedStorage);
  const OpenInfo info = reopened.open();
  TEST_ASSERT_FALSE(info.ok);
  TEST_ASSERT_TRUE(reopened.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, ff.truncateCalls);
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, ff.f[0].d.size());
}

void test_partial_append_reopen_fails_closed_without_tail_repair() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);

  ff.partialAppendBytes = 7;
  Event event = ev(2);
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_EQUAL_UINT32(7, ff.f[0].d.size());
  ff.allowTruncate = false;

  MonthSet reopenedStorage(ff);
  LogStore reopened(reopenedStorage);
  const OpenInfo info = reopened.open();
  TEST_ASSERT_FALSE(info.ok);
  TEST_ASSERT_TRUE(reopened.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, ff.truncateCalls);
  TEST_ASSERT_EQUAL_UINT32(7, ff.f[0].d.size());
}

void test_count_metadata_error_does_not_look_like_empty_log() {
  FakeFiles ff;
  ff.failCount = true;
  MonthSet ms(ff);
  LogStore log(ms);
  const OpenInfo opened = log.open();
  TEST_ASSERT_FALSE(opened.ok);
  TEST_ASSERT_TRUE(log.faulted());
}

void test_file_size_metadata_error_blocks_log_open() {
  FakeFiles ff;
  ff.addFile(202611);
  ff.failSize = true;
  MonthSet ms(ff);
  LogStore log(ms);
  TEST_ASSERT_FALSE(log.open().ok);
  TEST_ASSERT_TRUE(log.faulted());
}

void test_month_key_metadata_error_refuses_rollover() {
  FakeFiles ff;
  ff.addFile(202611);
  uint8_t d[REC_SIZE] = {0};
  MonthSet ms(ff);
  ms.setMonth(202612);
  ff.failKey = true;
  TEST_ASSERT_FALSE(ms.append(d, sizeof d));
  TEST_ASSERT_EQUAL_UINT32(1, ff.f.size());
}

void test_file_size_metadata_error_blocks_read() {
  FakeFiles ff;
  ff.addFile(202611);
  uint8_t d[REC_SIZE] = {0};
  ff.appendFile(0, d, sizeof d);
  ff.failSize = true;
  MonthSet ms(ff);
  uint8_t out[REC_SIZE];
  TEST_ASSERT_FALSE(ms.read(0, out, sizeof out));
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
  TEST_ASSERT_EQUAL_INT(32, oi.corruptAt);
  QuarantinePlan p = ms.planAt((size_t)oi.corruptAt);
  TEST_ASSERT_TRUE(p.valid);
  TEST_ASSERT_EQUAL_UINT32(0, p.fileIndex);
  TEST_ASSERT_EQUAL_UINT32(32, p.localOff);
  TEST_ASSERT_TRUE(ms.repair(p));
  oi = l2.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(2, oi.records);
}

void test_non_default_sequence_base_replays_and_appends() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore first(storage, 41);
  TEST_ASSERT_TRUE(first.open().ok);
  storage.setMonth(202611);
  Event event = ev(41);
  TEST_ASSERT_TRUE(first.append(&event, 1));
  TEST_ASSERT_EQUAL_UINT32(41, first.lastSeq());

  LogStore reopened(storage, 41);
  const OpenInfo info = reopened.open();
  TEST_ASSERT_TRUE(info.ok);
  TEST_ASSERT_EQUAL_UINT32(1, info.records);
  TEST_ASSERT_EQUAL_UINT32(41, info.lastSeq);
  Event replayed;
  TEST_ASSERT_TRUE(reopened.at(0, replayed));
  TEST_ASSERT_EQUAL_UINT32(41, replayed.seq);
}

void test_sequence_space_exhaustion_rejects_append_without_mutation() {
  FakeFiles ff;
  MonthSet storage(ff);
  LogStore log(storage, UINT32_MAX);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);
  Event event = ev(1);
  TEST_ASSERT_FALSE(log.append(&event, 1));
  TEST_ASSERT_FALSE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, ff.f.size());
}

void test_open_preserves_partial_tail_and_fails_closed_without_replay() {
  FakeFiles ff;
  fill(ff);
  const std::vector<uint8_t> original = ff.f[2].d;
  for (int i = 0; i < 8; ++i) ff.f[2].d.push_back(0xA5);

  MonthSet storage(ff);
  LogStore log(storage);
  CountingObserver observer;
  TEST_ASSERT_TRUE(log.attach(&observer));
  const OpenInfo opened = log.open();

  TEST_ASSERT_FALSE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(8, opened.tornBytes);
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);
  TEST_ASSERT_EQUAL_UINT32(original.size() + 8, ff.f[2].d.size());
  TEST_ASSERT_EQUAL_INT(0, memcmp(original.data(), ff.f[2].d.data(), original.size()));
  TEST_ASSERT_EQUAL_UINT32(0, ff.truncateCalls);
  Event next = ev(99);
  TEST_ASSERT_FALSE(log.append(&next, 1));
}

void test_open_preserves_complete_invalid_eof_record_and_fails_closed() {
  FakeFiles ff;
  fill(ff);
  ff.f[2].d[ff.f[2].d.size() - REC_SIZE + 3] ^= 0x40;
  const std::vector<uint8_t> damaged = ff.f[2].d;

  MonthSet storage(ff);
  LogStore log(storage);
  CountingObserver observer;
  TEST_ASSERT_TRUE(log.attach(&observer));
  const OpenInfo opened = log.open();

  TEST_ASSERT_FALSE(opened.ok);
  TEST_ASSERT_EQUAL_INT(176, opened.corruptAt);
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, opened.tornBytes);
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, observer.records);
  TEST_ASSERT_EQUAL_UINT32(damaged.size(), ff.f[2].d.size());
  TEST_ASSERT_EQUAL_INT(0, memcmp(damaged.data(), ff.f[2].d.data(), damaged.size()));
  TEST_ASSERT_EQUAL_UINT32(0, ff.truncateCalls);
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
  RUN_TEST(test_add_file_false_after_create_faults_log_until_reopen);
  RUN_TEST(test_partial_append_failure_latches_and_prevents_retry);
  RUN_TEST(test_complete_write_reported_failed_is_indeterminate_after_reopen);
  RUN_TEST(test_incomplete_commit_group_is_not_replayed_when_repair_fails);
  RUN_TEST(test_partial_append_reopen_fails_closed_without_tail_repair);
  RUN_TEST(test_count_metadata_error_does_not_look_like_empty_log);
  RUN_TEST(test_file_size_metadata_error_blocks_log_open);
  RUN_TEST(test_month_key_metadata_error_refuses_rollover);
  RUN_TEST(test_file_size_metadata_error_blocks_read);
  RUN_TEST(test_odd_sized_middle_file_is_caught);
  RUN_TEST(test_non_default_sequence_base_replays_and_appends);
  RUN_TEST(test_sequence_space_exhaustion_rejects_append_without_mutation);
  RUN_TEST(test_open_preserves_partial_tail_and_fails_closed_without_replay);
  RUN_TEST(test_open_preserves_complete_invalid_eof_record_and_fails_closed);
  RUN_TEST(test_replay_read_failure_resets_partial_observer_state);
  return UNITY_END();
}
