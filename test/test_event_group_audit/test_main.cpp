#include <unity.h>
#include <string.h>
#include <vector>
#include "event_group_audit.h"

using namespace att;

namespace {
struct MemStore : IStorage {
  std::vector<uint8_t> bytes;
  bool sizeChecked(size_t& out) override { out = bytes.size(); return true; }
  bool read(size_t offset, uint8_t* out, size_t n) override {
    if (!out || offset > bytes.size() || n > bytes.size() - offset) return false;
    memcpy(out, bytes.data() + offset, n);
    return true;
  }
  bool append(const uint8_t* data, size_t n) override {
    if (!data && n) return false;
    bytes.insert(bytes.end(), data, data + n);
    return true;
  }
  bool truncate(size_t n) override {
    if (n > bytes.size()) return false;
    bytes.resize(n); return true;
  }
};

void appendRaw(MemStore& store, const Event* events, size_t count) {
  uint8_t bytes[MAX_GROUP * REC_SIZE];
  for (size_t i = 0; i < count; ++i) {
    Event event = events[i];
    event.seq = (uint32_t)i + 1u;
    encode(event, bytes + i * REC_SIZE);
  }
  store.append(bytes, count * REC_SIZE);
}

void appendClose(LogStore& log, uint64_t words[5], uint16_t count) {
  Event events[6];
  events[0] = Event{EV_CLOSE, makeFlags(0, 0, true, false, false), count,
                    123456u, 0, 20000u};
  for (size_t i = 0; i < 5; ++i) events[i + 1] = makePayload(words[i], 0, false);
  TEST_ASSERT_TRUE(log.append(events, 6));
}

bool attachAndCheck(LogStore& log, EventGroupAudit& audit, OpenInfo& opened) {
  opened = log.open();
  if (!opened.ok) return false;
  if (!audit.attach(log)) return false;
  return audit.readyFor(log, opened);
}
}

void setUp() {}
void tearDown() {}

void test_exact_five_word_close_group_is_accepted() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  uint64_t words[5] = {0}; bitmapSet(words, 1); bitmapSet(words, 64);
  bitmapSet(words, 65); bitmapSet(words, 300);
  appendClose(log, words, 4);
  EventGroupAudit audit; OpenInfo opened;
  TEST_ASSERT_TRUE(attachAndCheck(log, audit, opened));
  TEST_ASSERT_EQUAL_UINT32(6, audit.recordsSeen());
}

void test_zero_expected_students_still_requires_five_bitmap_payloads() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  uint64_t words[5] = {0, 0, 0, 0, 0}; appendClose(log, words, 0);
  EventGroupAudit audit; OpenInfo opened;
  TEST_ASSERT_TRUE(attachAndCheck(log, audit, opened));
}

void test_short_close_group_is_rejected() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event events[2] = {
    Event{EV_CLOSE, makeFlags(0, 0, true, false, false), 0, 123456u, 0, 20000u},
    makePayload(0, 0, false)
  };
  TEST_ASSERT_TRUE(log.append(events, 2));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_close_without_bitmap_payload_is_rejected_even_at_zero_count() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event close = {EV_CLOSE, makeFlags(0, 0, true, false, false), 0, 123456u, 0, 20000u};
  TEST_ASSERT_TRUE(log.append(&close, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_close_count_must_match_bitmap_popcount() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  uint64_t words[5] = {1, 0, 0, 0, 0}; appendClose(log, words, 0);
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.malformed());
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_close_padding_bits_above_id_300_must_be_zero() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  uint64_t words[5] = {0, 0, 0, 0, (uint64_t)1 << 44};
  appendClose(log, words, 1);
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_group_member_sequences_must_be_contiguous() {
  EventGroupAudit audit; audit.onReset();
  Event events[2] = {
    Event{EV_CLOCK_SET, FL_MORE, SRC_MANUAL, 1000, 1, 0},
    Event{EV_CLOCK_SET, 0, SRC_MANUAL, 1060, 3, 0}
  };
  audit.onRecord(events[0]); audit.onRecord(events[1]);
  TEST_ASSERT_TRUE(audit.sequenceError());
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_orphan_payload_is_rejected() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event payload = makePayload(0, 0, false);
  TEST_ASSERT_TRUE(log.append(&payload, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_incomplete_more_group_is_not_ready() {
  EventGroupAudit audit; audit.onReset();
  audit.onRecord(Event{EV_CLOCK_SET, FL_MORE, SRC_MANUAL, 1000, 1, 0});
  TEST_ASSERT_TRUE(audit.pending());
  TEST_ASSERT_FALSE(audit.malformed());
}

void test_log_open_rejects_and_preserves_incomplete_group() {
  MemStore storage;
  Event unfinished = {EV_CLOSE, FL_MORE | FL_COVERAGE, 0, 123456u, 1, 20000u};
  appendRaw(storage, &unfinished, 1);
  const std::vector<uint8_t> original = storage.bytes;
  LogStore log(storage); OpenInfo opened = log.open();
  TEST_ASSERT_FALSE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(1, opened.droppedGroup);
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, log.count());
  TEST_ASSERT_EQUAL_UINT32(original.size(), storage.bytes.size());
  TEST_ASSERT_EQUAL_INT(0, memcmp(original.data(), storage.bytes.data(), original.size()));
  EventGroupAudit audit;
  TEST_ASSERT_FALSE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.attached());
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_live_audit_requires_healthy_open_and_complete_replay() {
  MemStore storage; LogStore log(storage); OpenInfo opened = log.open();
  EventGroupAudit audit;
  TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  TEST_ASSERT_EQUAL_UINT32(0, audit.recordsSeen());
}

void test_scan_records_require_valid_id_and_present_or_late_status() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event invalidId = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false),
                     0, 123456u, 0, 900};
  TEST_ASSERT_TRUE(log.append(&invalidId, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_scan_clear_or_excused_status_is_not_a_valid_scan_event() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event invalidStatus = {EV_SCAN, makeFlags(0, ST_CLEAR, false, false, false),
                         1, 123456u, 0, 900};
  TEST_ASSERT_TRUE(log.append(&invalidStatus, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_mark_records_reject_reserved_flag_bits() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event invalid = {EV_MARK, (uint8_t)(makeFlags(0, ST_PRESENT, false, false, false) | 0x40u),
                   1, 123456u, 0, 20000};
  TEST_ASSERT_TRUE(log.append(&invalid, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_no_class_records_reject_student_ids_and_status_bits() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event invalid = {EV_NOCLASS_SET, makeFlags(0, ST_PRESENT, false, false, false),
                   1, 123456u, 0, 20000};
  TEST_ASSERT_TRUE(log.append(&invalid, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
}

void test_unimplemented_event_types_fail_closed() {
  MemStore storage; LogStore log(storage); TEST_ASSERT_TRUE(log.open().ok);
  Event setting = {EV_SETTING, 0, 1, 123456u, 0, 1};
  TEST_ASSERT_TRUE(log.append(&setting, 1));
  OpenInfo opened = log.open(); TEST_ASSERT_TRUE(opened.ok);
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_FALSE(audit.readyFor(log, opened));
  TEST_ASSERT_TRUE(audit.malformed());
}

void test_live_audit_remains_ready_after_valid_append_since_open() {
  MemStore storage; LogStore log(storage); OpenInfo opened = log.open();
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false),
                1, 123456u, 0, 900};
  TEST_ASSERT_TRUE(log.append(&scan, 1));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  TEST_ASSERT_EQUAL_UINT32(1, audit.recordsSeen());
  TEST_ASSERT_EQUAL_UINT32(1, audit.lastSeq());
}

void test_live_audit_rejects_stale_open_info_after_reopen() {
  MemStore storage; LogStore log(storage); OpenInfo firstOpen = log.open();
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, firstOpen));
  OpenInfo secondOpen = log.open();
  TEST_ASSERT_TRUE(secondOpen.generation > firstOpen.generation);
  TEST_ASSERT_FALSE(audit.readyFor(log, firstOpen));
  TEST_ASSERT_TRUE(audit.readyFor(log, secondOpen));
}

void test_live_audit_rejects_stale_open_info_after_preserved_tail_reopen() {
  MemStore storage; LogStore log(storage); OpenInfo firstOpen = log.open();
  EventGroupAudit audit; TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, firstOpen));
  storage.bytes.push_back(0xAA);  // a new open must preserve and reject the torn trailing byte
  const std::vector<uint8_t> damaged = storage.bytes;
  OpenInfo failedOpen = log.open();
  TEST_ASSERT_FALSE(failedOpen.ok);
  TEST_ASSERT_EQUAL_UINT32(1, failedOpen.tornBytes);
  TEST_ASSERT_TRUE(failedOpen.generation > firstOpen.generation);
  TEST_ASSERT_FALSE(audit.readyFor(log, firstOpen));
  TEST_ASSERT_FALSE(audit.readyFor(log, failedOpen));
  TEST_ASSERT_EQUAL_UINT32(damaged.size(), storage.bytes.size());
  TEST_ASSERT_EQUAL_INT(0, memcmp(damaged.data(), storage.bytes.data(), damaged.size()));
}

void test_nondefault_sequence_base_supports_empty_and_replayed_month_logs() {
  MemStore storage;
  LogStore log(storage, 7);
  OpenInfo opened = log.open();
  EventGroupAudit audit(7);
  TEST_ASSERT_TRUE(audit.attach(log));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  TEST_ASSERT_EQUAL_UINT32(6, audit.lastSeq());

  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false),
                1, 123456u, 0, 900};
  TEST_ASSERT_TRUE(log.append(&scan, 1));
  TEST_ASSERT_TRUE(audit.readyFor(log, opened));
  TEST_ASSERT_EQUAL_UINT32(7, audit.lastSeq());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_exact_five_word_close_group_is_accepted);
  RUN_TEST(test_zero_expected_students_still_requires_five_bitmap_payloads);
  RUN_TEST(test_short_close_group_is_rejected);
  RUN_TEST(test_close_without_bitmap_payload_is_rejected_even_at_zero_count);
  RUN_TEST(test_close_count_must_match_bitmap_popcount);
  RUN_TEST(test_close_padding_bits_above_id_300_must_be_zero);
  RUN_TEST(test_group_member_sequences_must_be_contiguous);
  RUN_TEST(test_orphan_payload_is_rejected);
  RUN_TEST(test_incomplete_more_group_is_not_ready);
  RUN_TEST(test_log_open_rejects_and_preserves_incomplete_group);
  RUN_TEST(test_live_audit_requires_healthy_open_and_complete_replay);
  RUN_TEST(test_scan_records_require_valid_id_and_present_or_late_status);
  RUN_TEST(test_scan_clear_or_excused_status_is_not_a_valid_scan_event);
  RUN_TEST(test_mark_records_reject_reserved_flag_bits);
  RUN_TEST(test_no_class_records_reject_student_ids_and_status_bits);
  RUN_TEST(test_unimplemented_event_types_fail_closed);
  RUN_TEST(test_live_audit_remains_ready_after_valid_append_since_open);
  RUN_TEST(test_live_audit_rejects_stale_open_info_after_reopen);
  RUN_TEST(test_live_audit_rejects_stale_open_info_after_preserved_tail_reopen);
  RUN_TEST(test_nondefault_sequence_base_supports_empty_and_replayed_month_logs);
  return UNITY_END();
}
