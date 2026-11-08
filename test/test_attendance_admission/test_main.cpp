#include <unity.h>
#include <string.h>
#include "as608_attendance_pipeline.h"

using namespace att;

namespace {
class MemStorage : public IStorage {
 public:
  MemStorage() : length_(0), failAppend_(false) { memset(bytes_, 0, sizeof bytes_); }
  size_t size() override { return length_; }
  bool read(size_t off, uint8_t* out, size_t n) override {
    if (!out || off > length_ || n > length_ - off) return false;
    memcpy(out, bytes_ + off, n);
    return true;
  }
  bool append(const uint8_t* in, size_t n) override {
    if (failAppend_ || !in || n > sizeof bytes_ - length_) return false;
    memcpy(bytes_ + length_, in, n);
    length_ += n;
    return true;
  }
  bool truncate(size_t n) override {
    if (n > length_) return false;
    length_ = n;
    return true;
  }
  void failAppend(bool value) { failAppend_ = value; }
 private:
  uint8_t bytes_[REC_SIZE * 64];
  size_t length_;
  bool failAppend_;
};

const int32_t TEST_DAY = 1;  // Thursday 2020-01-02
const uint32_t TEST_EPOCH = EPOCH_2020 + 86400u + 8u * 3600u + 5u * 60u;
const Params TEST_PARAMS = {0, 30, 15};

Event noClass(uint8_t session, bool wholeDay, bool clear = false) {
  return Event{(uint8_t)(clear ? EV_NOCLASS_CLEAR : EV_NOCLASS_SET),
      makeFlags(session, ST_CLEAR, false, false, wholeDay), 0, TEST_EPOCH, 0,
      (uint16_t)TEST_DAY};
}

Event closeSession(uint8_t session) {
  return Event{EV_CLOSE, makeFlags(session, ST_CLEAR, true, false, false),
      0, TEST_EPOCH, 0, (uint16_t)TEST_DAY};
}

Event scan(uint8_t session, uint16_t id) {
  return Event{EV_SCAN, makeFlags(session, ST_PRESENT, false, false, false),
      id, TEST_EPOCH, 0, 0};
}

bool openLog(LogStore& log) { return log.open().ok; }

class EmptyObserver : public LogObserver {
 public:
  void onReset() override {}
  void onRecord(const Event&) override {}
};
}

void setUp() {}
void tearDown() {}

void test_open_log_allows_session_without_no_class_or_close() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  TEST_ASSERT_TRUE(admission.attached());
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
}

void test_unopened_or_faulted_log_blocks_admission() {
  MemStorage storage;
  LogStore log(storage);
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  TEST_ASSERT_TRUE(admission.attached());
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_TRUE(openLog(log));
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
}

void test_whole_day_no_class_blocks_both_sessions() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event e = noClass(0, true);
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 1));
}

void test_session_no_class_blocks_only_that_session() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event e = noClass(1, false);
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 1));
}

void test_no_class_clear_reopens_session() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event set = noClass(0, true);
  Event clear = noClass(0, true, true);
  TEST_ASSERT_TRUE(log.append(&set, 1));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_TRUE(log.append(&clear, 1));
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
}

void test_closed_session_blocks_scan() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event e = closeSession(0);
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 1));
}

void test_existing_scan_is_detected_and_updated_incrementally() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_FALSE(admission.alreadyScanned(TEST_DAY, 0, 7));
  Event e = scan(0, 7);
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_TRUE(admission.alreadyScanned(TEST_DAY, 0, 7));
  TEST_ASSERT_FALSE(admission.alreadyScanned(TEST_DAY, 0, 8));
  TEST_ASSERT_FALSE(admission.alreadyScanned(TEST_DAY, 1, 7));
}

void test_parameter_update_replays_with_new_utc_offset() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event e = scan(0, 9);
  e.epoch += 12u * 3600u;  // 20:05 UTC becomes the next local day at UTC+8
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_TRUE(admission.alreadyScanned(TEST_DAY, 0, 9));
  Params invalid = TEST_PARAMS;
  invalid.utcOffsetMin = 900;
  TEST_ASSERT_FALSE(admission.setParamsAfterCommit(invalid));
  TEST_ASSERT_TRUE(admission.alreadyScanned(TEST_DAY, 0, 9));
  Params next = TEST_PARAMS;
  next.utcOffsetMin = 480;
  TEST_ASSERT_TRUE(admission.setParamsAfterCommit(next));
  TEST_ASSERT_FALSE(admission.alreadyScanned(TEST_DAY, 0, 9));
  TEST_ASSERT_TRUE(admission.alreadyScanned(TEST_DAY + 1, 0, 9));
}

void test_log_append_failure_closes_admission() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
  storage.failAppend(true);
  Event e = noClass(0, true);
  TEST_ASSERT_FALSE(log.append(&e, 1));
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
}

void test_log_reset_invalidates_cached_day_view() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  Event e = noClass(0, true);
  TEST_ASSERT_TRUE(log.append(&e, 1));
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
  TEST_ASSERT_TRUE(storage.truncate(0));
  TEST_ASSERT_TRUE(openLog(log));
  TEST_ASSERT_TRUE(admission.allowScan(TEST_DAY, 0));
}

void test_observer_capacity_failure_closes_admission() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  EmptyObserver observers[MAX_OBSERVERS];
  for (size_t i = 0; i < MAX_OBSERVERS; ++i)
    TEST_ASSERT_TRUE(log.attach(&observers[i]));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  TEST_ASSERT_FALSE(admission.attached());
  TEST_ASSERT_FALSE(admission.allowScan(TEST_DAY, 0));
}

void test_config_params_cannot_change_after_log_fault() {
  MemStorage storage;
  LogStore log(storage);
  TEST_ASSERT_TRUE(openLog(log));
  LogAttendanceAdmission admission(log, TEST_PARAMS);
  storage.failAppend(true);
  Event e = noClass(0, true);
  TEST_ASSERT_FALSE(log.append(&e, 1));
  Params next = TEST_PARAMS;
  next.utcOffsetMin = 480;
  TEST_ASSERT_FALSE(admission.canSetParamsAfterCommit(next));
  TEST_ASSERT_FALSE(admission.setParamsAfterCommit(next));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_open_log_allows_session_without_no_class_or_close);
  RUN_TEST(test_unopened_or_faulted_log_blocks_admission);
  RUN_TEST(test_whole_day_no_class_blocks_both_sessions);
  RUN_TEST(test_session_no_class_blocks_only_that_session);
  RUN_TEST(test_no_class_clear_reopens_session);
  RUN_TEST(test_closed_session_blocks_scan);
  RUN_TEST(test_existing_scan_is_detected_and_updated_incrementally);
  RUN_TEST(test_parameter_update_replays_with_new_utc_offset);
  RUN_TEST(test_log_append_failure_closes_admission);
  RUN_TEST(test_log_reset_invalidates_cached_day_view);
  RUN_TEST(test_observer_capacity_failure_closes_admission);
  RUN_TEST(test_config_params_cannot_change_after_log_fault);
  return UNITY_END();
}
