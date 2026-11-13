#include <limits.h>
#include <unity.h>
#include <string.h>
#include <vector>
#include "backup_format.h"
#include "clock_policy.h"
#include "month_set.h"
#include "roster_store.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct MemStore : IStorage {
  std::vector<uint8_t> d; bool failRead = false;
  bool sizeChecked(size_t& out) override { out = d.size(); return true; }
  bool read(size_t o, uint8_t* b, size_t n) override { if (failRead || o + n > d.size()) return false; memcpy(b, d.data() + o, n); return true; }
  bool append(const uint8_t* b, size_t n) override { d.insert(d.end(), b, b + n); return true; }
  bool truncate(size_t n) override { d.resize(n); return true; }
};
struct CountObs : LogObserver {
  int resets = 0, recs = 0;
  void onReset() override { resets++; recs = 0; }
  void onRecord(const Event&) override { recs++; }
};
static uint32_t noon(int32_t day) { return dayStartEpoch(day, 480) + 12 * 3600; }

// ---- GAP truncation ----
static void checkCoverage(LogStore& ls, uint32_t from, uint32_t to, uint16_t cause, size_t* gaps) {
  uint32_t end = from; size_t n = 0;
  for (size_t i = 0; i < ls.count(); i++) {
    Event e;
    if (!ls.at(i, e) || e.type != EV_GAP) continue;
    TEST_ASSERT_EQUAL_UINT16(cause, e.a);
    TEST_ASSERT_TRUE(e.epoch <= end);                  // no hole between chunks
    uint32_t stop = e.epoch + (uint32_t)e.b * 60;
    if (stop > end) end = stop;
    n++;
  }
  TEST_ASSERT_TRUE(end >= to);                         // the whole span is covered
  if (gaps) *gaps = n;
}
void test_gap_chunks_report_progress() {
  Event ev[40];
  uint32_t next = 0;
  uint32_t to = 20u * 65535u * 60u;
  size_t n = makeGapEvents(0, to, GAP_CLOCK, ev, 16, &next);
  TEST_ASSERT_EQUAL_UINT32(16, n);
  TEST_ASSERT_TRUE(next < to);                         // caller can see coverage stopped short
  n = makeGapEvents(0, to, GAP_CLOCK, ev, 32, &next);
  TEST_ASSERT_EQUAL_UINT32(20, n);
  TEST_ASSERT_TRUE(next >= to);
}
void test_append_gaps_covers_long_outage() {
  MemStore ms; LogStore ls(ms); ls.open();
  uint32_t from = 1000, to = 1000 + 3u * 365u * 86400u;        // ~3 years
  TEST_ASSERT_TRUE(appendGaps(ls, from, to, GAP_STORAGE));
  size_t gaps = 0;
  checkCoverage(ls, from, to, GAP_STORAGE, &gaps);
  TEST_ASSERT_TRUE(gaps > 16);                                  // needed more than one commit group
  MemStore copy; copy.d = ms.d; LogStore l2(copy);
  OpenInfo oi = l2.open();
  TEST_ASSERT_TRUE(oi.ok);
  TEST_ASSERT_EQUAL_UINT32(gaps, oi.records);
}
void test_append_gaps_empty_span_is_ok() {
  MemStore ms; LogStore ls(ms); ls.open();
  TEST_ASSERT_TRUE(appendGaps(ls, 500, 500, GAP_STORAGE));
  TEST_ASSERT_EQUAL_UINT32(0, ls.count());
}
void test_clock_set_with_very_long_gap() {
  MemStore ms; LogStore ls(ms); ls.open();
  ClockDecision d;
  d.action = CA_APPLY; d.delta = 0; d.source = 1; d.needsAttention = false;
  d.gapFrom = 5000; d.gapTo = 5000 + 4u * 365u * 86400u;       // 4 years: more than one group of GAPs
  TEST_ASSERT_EQUAL_INT(CLOCK_APPLY_COMMITTED, applyClockSet(ls, d, 111, 222));
  size_t gaps = 0;
  checkCoverage(ls, d.gapFrom, d.gapTo, GAP_CLOCK, &gaps);
  TEST_ASSERT_TRUE(gaps > 14);
  Event a, b;
  TEST_ASSERT_TRUE(ls.at(ls.count() - 2, a));
  TEST_ASSERT_TRUE(ls.at(ls.count() - 1, b));
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, a.type);
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, b.type);
  TEST_ASSERT_EQUAL_UINT32(111, a.epoch);
  TEST_ASSERT_EQUAL_UINT32(222, b.epoch);
}
// ---- attach() rollback ----
void test_failed_attach_deregisters() {
  MemStore ms; LogStore ls(ms); ls.open();
  for (int i = 0; i < 5; i++) { Event e = Event{EV_SCAN, 0, (uint16_t)(i + 1), (uint32_t)(100 + i), 0, 0}; ls.append(&e, 1); }
  CountObs o;
  ms.failRead = true;
  TEST_ASSERT_FALSE(ls.attach(&o));
  ms.failRead = false;
  int resets = o.resets;
  Event e = Event{EV_SCAN, 0, 9, 200, 0, 0};
  ls.append(&e, 1);
  TEST_ASSERT_EQUAL_INT(0, o.recs);                    // no longer registered
  TEST_ASSERT_EQUAL_INT(resets, o.resets);
  CountObs many[MAX_OBSERVERS];                        // the slot was given back
  for (size_t i = 0; i < MAX_OBSERVERS; i++) TEST_ASSERT_TRUE(ls.attach(&many[i]));
  CountObs extra;
  TEST_ASSERT_FALSE(ls.attach(&extra));
}
void test_duplicate_attach_is_refused() {
  MemStore ms; LogStore ls(ms); ls.open();
  CountObs o;
  TEST_ASSERT_TRUE(ls.attach(&o));
  TEST_ASSERT_FALSE(ls.attach(&o));
  Event e = Event{EV_SCAN, 0, 1, 100, 0, 0};
  ls.append(&e, 1);
  TEST_ASSERT_EQUAL_INT(1, o.recs);                    // seen once, not twice
}
// ---- MonthSet ----
struct FakeFiles : IFileSet {
  struct F { uint32_t key; std::vector<uint8_t> d; };
  std::vector<F> f;
  bool countChecked(size_t& out) override { out = f.size(); return true; }
  bool monthKeyChecked(size_t i, uint32_t& out) override {
    if (i >= f.size()) return false;
    out = f[i].key; return true;
  }
  bool fileSizeChecked(size_t i, size_t& out) override {
    if (i >= f.size()) return false;
    out = f[i].d.size(); return true;
  }
  bool readFile(size_t i, size_t o, uint8_t* b, size_t n) override { if (o + n > f[i].d.size()) return false; memcpy(b, f[i].d.data() + o, n); return true; }
  bool appendFile(size_t i, const uint8_t* b, size_t n) override { f[i].d.insert(f[i].d.end(), b, b + n); return true; }
  bool truncateFile(size_t i, size_t n) override { f[i].d.resize(n); return true; }
  bool addFile(uint32_t k) override { F x; x.key = k; f.push_back(x); return true; }
  bool quarantineCopy(size_t) override { return true; }
  bool quarantineFrom(size_t i) override { f.resize(i); return true; }
};
void test_unset_month_never_appends() {
  FakeFiles ff;
  ff.addFile(202611);
  uint8_t d[16] = {0};
  ff.appendFile(0, d, 16);
  MonthSet ms(ff);                                     // month never set (fresh boot)
  TEST_ASSERT_FALSE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(16, ff.f[0].d.size());
  ms.setMonth(202611);
  TEST_ASSERT_TRUE(ms.append(d, 16));
  TEST_ASSERT_EQUAL_UINT32(32, ff.f[0].d.size());
}
void test_refused_truncate_does_not_go_stale() {
  FakeFiles ff;
  ff.addFile(1); ff.addFile(2);
  uint8_t d[64] = {0};
  ff.appendFile(0, d, 32); ff.appendFile(1, d, 32);
  MonthSet ms(ff);
  TEST_ASSERT_FALSE(ms.truncate(16));
  TEST_ASSERT_EQUAL_UINT32(16, ms.refusedTruncate());
  TEST_ASSERT_TRUE(ms.truncate(48));
  TEST_ASSERT_TRUE(ms.refusedTruncate() == (size_t)-1);
}
// ---- Roster replay strictness ----
static Event more(Event e) { e.flags |= FL_MORE; return e; }
static void feedSet(RosterStore& r, uint16_t id, const uint8_t name[32], bool firstMore, bool lastMore, int32_t day) {
  Event h = Event{EV_ROSTER, 0, id, noon(day), 1, RA_SET};
  if (firstMore) h = more(h);
  r.onRecord(h);
  for (int w = 0; w < 4; w++) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)name[w * 8 + i] << (8 * i);
    Event p = makePayload(v, (uint32_t)(2 + w), false);
    if (w < 3 || lastMore) p = more(p);
    r.onRecord(p);
  }
}
void test_roster_replay_is_strict() {
  uint8_t ana[32] = {'A', 'n', 'a'};
  RosterStore r(480);
  feedSet(r, 1, ana, true, false, 100);                 // well-formed
  TEST_ASSERT_TRUE(r.known(1));
  TEST_ASSERT_EQUAL_UINT32(0, r.malformed());
  feedSet(r, 2, ana, false, false, 100);                // header without MORE
  TEST_ASSERT_FALSE(r.known(2));
  feedSet(r, 3, ana, true, true, 100);                  // last payload claims a continuation
  TEST_ASSERT_FALSE(r.known(3));
  uint8_t bad8[32] = {0xC0, 0x80};                      // overlong UTF-8
  feedSet(r, 4, bad8, true, false, 100);
  TEST_ASSERT_FALSE(r.known(4));
  uint8_t junk[32] = {'A', 'n', 'a', 0, 'x'};           // data after the NUL
  feedSet(r, 5, junk, true, false, 100);
  TEST_ASSERT_FALSE(r.known(5));
  uint8_t blank[32] = {0};                              // empty name
  feedSet(r, 6, blank, true, false, 100);
  TEST_ASSERT_FALSE(r.known(6));
  r.onRecord(more(Event{EV_ROSTER, 0, 1, noon(110), 20, RA_DEACTIVATE}));   // part of a bigger group
  TEST_ASSERT_TRUE(r.active(1));
  r.onRecord(Event{EV_ROSTER, 0, 1, noon(110), 21, RA_DEACTIVATE});
  TEST_ASSERT_FALSE(r.active(1));
  TEST_ASSERT_TRUE(r.malformed() >= 6);
  TEST_ASSERT_EQUAL_UINT32(1, r.knownCount());
}
void test_roster_cut_short_group_and_foreign_payloads() {
  RosterStore r(480);
  uint8_t ana[32] = {'A', 'n', 'a'};
  r.onRecord(more(Event{EV_ROSTER, 0, 7, noon(100), 1, RA_SET}));
  r.onRecord(more(makePayload(0x616E41ull, 2, true)));
  r.onRecord(Event{EV_SCAN, 0, 7, noon(100), 3, 0});                 // group cut short
  TEST_ASSERT_FALSE(r.known(7));
  TEST_ASSERT_TRUE(r.malformed() >= 1);
  r.onRecord(makePayload(0x616E41ull, 4, false));                    // stray payload (e.g. a CLOSE bitmap)
  TEST_ASSERT_FALSE(r.known(7));
  feedSet(r, 8, ana, true, false, 100);                              // roster still works afterwards
  TEST_ASSERT_TRUE(r.known(8));
  static_assert(ROSTER_NAME_MAX == 32, "name length");
}
// ---- Backup ----
struct NullCipher : ICipher {
  bool deriveKey(const uint8_t*, size_t, const uint8_t*, uint32_t, uint8_t k[32]) override { memset(k, 7, 32); return true; }
  bool seal(const uint8_t*, const uint8_t*, const uint8_t*, size_t, const uint8_t* in, size_t n, uint8_t* out, uint8_t t[16]) override { memcpy(out, in, n); memset(t, 1, 16); return true; }
  bool open(const uint8_t*, const uint8_t*, const uint8_t*, size_t, const uint8_t* in, size_t n, const uint8_t* t, uint8_t* out) override {
    for (int i = 0; i < 16; i++) if (t[i] != 1) return false;
    memcpy(out, in, n); return true;
  }
};
struct Buf : IByteSink { std::vector<uint8_t> v; bool fail = false; bool write(const uint8_t* d, size_t n) override { if (fail) return false; v.insert(v.end(), d, d + n); return true; } };
static const uint8_t SALT[16] = {1}, NONCE[12] = {2}, PASS[4] = {'a', 'b', 'c', 'd'};

void test_backup_iteration_limits_agree() {
  NullCipher c;
  { Buf o; BackupWriter w(c, o); TEST_ASSERT_FALSE(w.begin(PASS, 4, SALT, NONCE, BK_ITER_MIN - 1)); }
  { Buf o; BackupWriter w(c, o); TEST_ASSERT_FALSE(w.begin(PASS, 4, SALT, NONCE, BK_ITER_MAX + 1)); }
  { Buf o; BackupWriter w(c, o); TEST_ASSERT_FALSE(w.begin(PASS, 4, SALT, NONCE, 0)); }
  uint32_t its[2] = {BK_ITER_MIN, BK_ITER_MAX};
  for (int k = 0; k < 2; k++) {                        // anything the writer accepts, the reader accepts
    Buf o; BackupWriter w(c, o);
    TEST_ASSERT_TRUE(w.begin(PASS, 4, SALT, NONCE, its[k]));
    uint8_t p[10] = {1, 2, 3};
    TEST_ASSERT_TRUE(w.write(p, 10)); TEST_ASSERT_TRUE(w.finish());
    Buf got; BackupReader r(c, got, PASS, 4);
    TEST_ASSERT_TRUE(r.feed(o.v.data(), o.v.size()));
    TEST_ASSERT_TRUE(r.done());
    TEST_ASSERT_EQUAL_UINT32(10, got.v.size());
  }
}
void test_ack_needs_finished_stream() {
  NullCipher c;
  Buf o; BackupWriter w(c, o);
  w.begin(PASS, 4, SALT, NONCE, 1000);
  uint8_t p[100] = {0};
  w.write(p, 100);
  TEST_ASSERT_FALSE(w.finished());
  TEST_ASSERT_FALSE(ackMatches(w, w.crc(), w.length()));            // matches what was emitted so far, still refused
  TEST_ASSERT_TRUE(w.finish());
  TEST_ASSERT_TRUE(ackMatches(w, w.crc(), w.length()));
  Buf bad; BackupWriter w2(c, bad);
  w2.begin(PASS, 4, SALT, NONCE, 1000);
  w2.write(p, 100);
  bad.fail = true;
  TEST_ASSERT_FALSE(w2.finish());
  TEST_ASSERT_FALSE(w2.finished());
  TEST_ASSERT_FALSE(ackMatches(w2, w2.crc(), w2.length()));
}
void test_inner_builder_refuses_inconsistent_streams() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 1, 32); strcpy(c.wifiPass, "classroom-ap-77");
  uint8_t rec[16] = {0};
  { Buf b; InnerBuilder ib(b); ib.begin(c, 1, 3, 2); ib.records(rec, 16); TEST_ASSERT_FALSE(ib.finish()); }   // 1 of 2 records
  { Buf b; InnerBuilder ib(b); ib.begin(c, 1, 3, 1); TEST_ASSERT_FALSE(ib.finish()); }                          // none of 1
  { Buf b; InnerBuilder ib(b); ib.begin(c, 1, 3, 0); TEST_ASSERT_FALSE(ib.records(rec, 16)); }                  // more than declared
  { Buf b; InnerBuilder ib(b); ib.begin(c, 1, 3, 0); TEST_ASSERT_TRUE(ib.finish()); }
  { Buf b; InnerBuilder ib(b); ib.begin(c, 1, 3, 0); TEST_ASSERT_FALSE(ib.records(rec, 15)); }                  // not whole records
}

// ---- restore-time semantic replay ----
static Config restoreCfg() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 1, 32); strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}
static std::vector<uint8_t> innerOf(const std::vector<uint8_t>& log) {
  Buf b; InnerBuilder ib(b);
  ib.begin(restoreCfg(), 1, 3, (uint32_t)(log.size() / REC_SIZE));
  ib.records(log.data(), log.size());
  ib.finish();
  return b.v;
}
void test_restore_replays_roster_into_observer() {
  MemStore ms; LogStore ls(ms); ls.open();
  RosterStore live(480); ls.attach(&live);
  Event e[5]; size_t c;
  live.planSet(1, "Ana", 3, noon(100), e, c); ls.append(e, c);
  live.planSet(2, "Ben", 3, noon(100), e, c); ls.append(e, c);
  live.planSet(3, "Cy", 2, noon(101), e, c); ls.append(e, c);
  live.planDeactivate(2, noon(110), e, c); ls.append(e, c);
  std::vector<uint8_t> in = innerOf(ms.d);
  MemStore stg; RestoreValidator v(&stg, 3);
  RosterStore restored(480);
  TEST_ASSERT_TRUE(v.addObserver(&restored));
  TEST_ASSERT_TRUE(v.write(in.data(), in.size()));
  TEST_ASSERT_TRUE(v.ok());
  TEST_ASSERT_EQUAL_UINT32(0, restored.malformed());
  TEST_ASSERT_EQUAL_UINT32(live.knownCount(), restored.knownCount());
  TEST_ASSERT_EQUAL_UINT32(live.activeCount(), restored.activeCount());
  for (uint16_t id = 1; id <= 4; id++)
    for (int32_t d = 99; d < 120; d++) TEST_ASSERT_EQUAL_INT(live.expected(id, d), restored.expected(id, d));
  RosterStore late(480);
  TEST_ASSERT_FALSE(RestoreValidator(&stg, 3).addObserver(nullptr));
  MemStore s2; RestoreValidator v2(&s2, 3);
  TEST_ASSERT_TRUE(v2.write(in.data(), 1));
  TEST_ASSERT_FALSE(v2.addObserver(&late));                          // too late: bytes already consumed
}
void test_restore_semantic_check_catches_malformed_roster() {
  Event bad = Event{EV_ROSTER, 0, 5, noon(100), 1, RA_SET};          // SET with no group: structurally valid record
  uint8_t rec[REC_SIZE]; encode(bad, rec);
  std::vector<uint8_t> log(rec, rec + REC_SIZE);
  std::vector<uint8_t> in = innerOf(log);
  MemStore stg; RestoreValidator v(&stg, 3);
  RosterStore r(480);
  v.addObserver(&r);
  TEST_ASSERT_TRUE(v.write(in.data(), in.size()));
  TEST_ASSERT_TRUE(v.ok());                                          // structure alone would accept it
  TEST_ASSERT_TRUE(r.malformed() >= 1);                              // the adapter must refuse the restore here
  TEST_ASSERT_FALSE(r.known(5));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_gap_chunks_report_progress);
  RUN_TEST(test_append_gaps_covers_long_outage);
  RUN_TEST(test_append_gaps_empty_span_is_ok);
  RUN_TEST(test_clock_set_with_very_long_gap);
  RUN_TEST(test_failed_attach_deregisters);
  RUN_TEST(test_duplicate_attach_is_refused);
  RUN_TEST(test_unset_month_never_appends);
  RUN_TEST(test_refused_truncate_does_not_go_stale);
  RUN_TEST(test_roster_replay_is_strict);
  RUN_TEST(test_roster_cut_short_group_and_foreign_payloads);
  RUN_TEST(test_backup_iteration_limits_agree);
  RUN_TEST(test_ack_needs_finished_stream);
  RUN_TEST(test_inner_builder_refuses_inconsistent_streams);
  RUN_TEST(test_restore_replays_roster_into_observer);
  RUN_TEST(test_restore_semantic_check_catches_malformed_roster);
  return UNITY_END();
}
