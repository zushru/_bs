#include <unity.h>
#include <string.h>
#include <vector>
#include "roster_name_history.h"

using namespace att;

namespace {
struct MemStore : IStorage {
  MemStore() : failAfterWrite_(false) {}
  std::vector<uint8_t> bytes;
  bool sizeChecked(size_t& out) override { out = bytes.size(); return true; }
  bool read(size_t offset, uint8_t* out, size_t n) override {
    if (!out || offset > bytes.size() || n > bytes.size() - offset) return false;
    memcpy(out, bytes.data() + offset, n); return true;
  }
  bool append(const uint8_t* data, size_t n) override {
    if (!data && n) return false;
    bytes.insert(bytes.end(), data, data + n);
    if (failAfterWrite_) { failAfterWrite_ = false; return false; }
    return true;
  }
  bool truncate(size_t n) override { if (n > bytes.size()) return false; bytes.resize(n); return true; }
  void failAfterWriteOnce() { failAfterWrite_ = true; }
 private:
  bool failAfterWrite_;
};
uint32_t noon(int32_t day) { return dayStartEpoch(day, 480) + 12u * 3600u; }

RosterErr setName(LogStore& log, RosterStore& roster, uint16_t id,
                  const char* name, int32_t day) {
  Event events[5]; size_t count = 0;
  RosterErr error = roster.planSet(id, name, strlen(name), noon(day), events, count);
  if (error == RE_OK && !log.append(events, count)) return RE_FULL;
  return error;
}
RosterErr deactivate(LogStore& log, RosterStore& roster, uint16_t id, int32_t day) {
  Event events[5]; size_t count = 0;
  RosterErr error = roster.planDeactivate(id, noon(day), events, count);
  if (error == RE_OK && !log.append(events, count)) return RE_FULL;
  return error;
}
RosterErr activate(LogStore& log, RosterStore& roster, uint16_t id, int32_t day) {
  Event events[5]; size_t count = 0;
  RosterErr error = roster.planActivate(id, noon(day), events, count);
  if (error == RE_OK && !log.append(events, count)) return RE_FULL;
  return error;
}
}

void setUp() {}
void tearDown() {}

void test_interval_projection_exposes_every_half_open_enrollment_period() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  RosterStore roster(480); TEST_ASSERT_TRUE(log.attach(&roster));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 8, "Old Name", 100));
  TEST_ASSERT_EQUAL_INT(RE_OK, deactivate(log, roster, 8, 120));
  TEST_ASSERT_EQUAL_INT(RE_OK, activate(log, roster, 8, 130));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 3, "Other", 102));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_TRUE(roster.historyComplete());
  TEST_ASSERT_EQUAL_UINT32(3, roster.intervalCount());
  RosterIntervalView iv;
  TEST_ASSERT_TRUE(roster.intervalAt(0, iv));
  TEST_ASSERT_EQUAL_UINT16(8, iv.studentId);
  TEST_ASSERT_EQUAL_INT32(100, iv.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(120, iv.endDayExclusive);
  TEST_ASSERT_TRUE(roster.intervalAt(1, iv));
  TEST_ASSERT_EQUAL_UINT16(8, iv.studentId);
  TEST_ASSERT_EQUAL_INT32(130, iv.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(INT32_MAX, iv.endDayExclusive);
  TEST_ASSERT_TRUE(roster.intervalAt(2, iv));
  TEST_ASSERT_EQUAL_UINT16(3, iv.studentId);
  TEST_ASSERT_FALSE(roster.intervalAt(3, iv));
}

void test_known_id_enumeration_is_stable_and_sorted() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  RosterStore roster(480); TEST_ASSERT_TRUE(log.attach(&roster));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 8, "Eight", 100));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 3, "Three", 100));
  uint16_t id = 0;
  TEST_ASSERT_TRUE(roster.knownIdAt(0, id)); TEST_ASSERT_EQUAL_UINT16(3, id);
  TEST_ASSERT_TRUE(roster.knownIdAt(1, id)); TEST_ASSERT_EQUAL_UINT16(8, id);
  TEST_ASSERT_FALSE(roster.knownIdAt(2, id));
}

void test_name_history_reader_preserves_renames_and_original_event_sequences() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  RosterStore roster(480); TEST_ASSERT_TRUE(log.attach(&roster));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 8, "Old Name", 100));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 8, "New Name", 110));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 3, "Other", 112));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  RosterNameHistoryReader reader(log, audit, opened, 480);
  RosterNameVersion v;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(v));
  TEST_ASSERT_EQUAL_UINT16(8, v.studentId);
  TEST_ASSERT_EQUAL_INT32(100, v.effectiveDay);
  TEST_ASSERT_EQUAL_STRING("Old Name", v.name);
  TEST_ASSERT_EQUAL_UINT32(1, v.sourceSeq);
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(v));
  TEST_ASSERT_EQUAL_UINT16(8, v.studentId);
  TEST_ASSERT_EQUAL_INT32(110, v.effectiveDay);
  TEST_ASSERT_EQUAL_STRING("New Name", v.name);
  TEST_ASSERT_EQUAL_UINT32(6, v.sourceSeq);
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(v));
  TEST_ASSERT_EQUAL_UINT16(3, v.studentId);
  TEST_ASSERT_EQUAL_STRING("Other", v.name);
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_END, reader.next(v));
}

void test_name_history_reader_refuses_unattached_or_untrusted_audit() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit;
  RosterNameHistoryReader reader(log, audit, opened, 480);
  RosterNameVersion v;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_LOG_UNTRUSTED, reader.next(v));
}

void test_name_history_reader_detects_log_revision_change_mid_stream() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  RosterStore roster(480); TEST_ASSERT_TRUE(log.attach(&roster));
  TEST_ASSERT_EQUAL_INT(RE_OK, setName(log, roster, 8, "Old Name", 100));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  RosterNameHistoryReader reader(log, audit, opened, 480);
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 8,
                noon(100), 0, 0};
  TEST_ASSERT_TRUE(log.append(&scan, 1));
  RosterNameVersion v;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_SNAPSHOT_CHANGED, reader.next(v));
}

void test_name_history_reader_rejects_noncanonical_or_invalid_name_payload() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  uint8_t bytes[ROSTER_NAME_MAX] = {0};
  bytes[0] = 'B'; bytes[1] = '\n'; bytes[2] = 'd';
  Event events[5];
  events[0] = Event{EV_ROSTER, 0, 8, noon(100), 0, RA_SET};
  for (size_t w = 0; w < 4; ++w) {
    uint64_t bits = 0;
    for (size_t b = 0; b < 8; ++b) bits |= (uint64_t)bytes[w * 8 + b] << (8 * b);
    events[w + 1] = makePayload(bits, 0, false);
  }
  TEST_ASSERT_TRUE(log.append(events, 5));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  RosterNameHistoryReader reader(log, audit, opened, 480);
  RosterNameVersion v;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_BAD_SET, reader.next(v));
}

void test_complete_roster_write_failure_replay_retry_keeps_one_name_history_version() {
  MemStore storage;
  LogStore first(storage);
  RosterStore live(480);
  TEST_ASSERT_TRUE(first.open().ok);
  TEST_ASSERT_TRUE(first.attach(&live));

  Event events[5]; size_t count = 0;
  TEST_ASSERT_EQUAL_INT(RE_OK,
      live.planSet(8, "Old Name", 8, noon(100), events, count));
  TEST_ASSERT_EQUAL_UINT32(5, count);
  storage.failAfterWriteOnce();
  TEST_ASSERT_FALSE(first.append(events, count));
  TEST_ASSERT_TRUE(first.faulted());
  TEST_ASSERT_FALSE(live.known(8));

  LogStore recovered(storage);
  OpenInfo opened = recovered.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(5, opened.records);
  RosterStore replayed(480);
  TEST_ASSERT_TRUE(recovered.attach(&replayed));
  TEST_ASSERT_TRUE(replayed.known(8));

  const uint32_t beforeRetry = recovered.count();
  TEST_ASSERT_EQUAL_INT(RE_NOOP, setName(recovered, replayed, 8, "Old Name", 101));
  TEST_ASSERT_EQUAL_UINT32(beforeRetry, recovered.count());
  TEST_ASSERT_FALSE(recovered.faulted());

  EventGroupAudit audit;
  TEST_ASSERT_TRUE(audit.attach(recovered));
  TEST_ASSERT_TRUE(audit.readyFor(recovered, opened));
  RosterNameHistoryReader reader(recovered, audit, opened, 480);
  RosterNameVersion version;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(version));
  TEST_ASSERT_EQUAL_UINT16(8, version.studentId);
  TEST_ASSERT_EQUAL_INT32(100, version.effectiveDay);
  TEST_ASSERT_EQUAL_STRING("Old Name", version.name);
  TEST_ASSERT_EQUAL_UINT32(1, version.sourceSeq);
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_END, reader.next(version));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_interval_projection_exposes_every_half_open_enrollment_period);
  RUN_TEST(test_known_id_enumeration_is_stable_and_sorted);
  RUN_TEST(test_name_history_reader_preserves_renames_and_original_event_sequences);
  RUN_TEST(test_name_history_reader_refuses_unattached_or_untrusted_audit);
  RUN_TEST(test_name_history_reader_detects_log_revision_change_mid_stream);
  RUN_TEST(test_name_history_reader_rejects_noncanonical_or_invalid_name_payload);
  RUN_TEST(test_complete_roster_write_failure_replay_retry_keeps_one_name_history_version);
  return UNITY_END();
}
