#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "month_roster_projection.h"

using namespace att;

namespace {
struct MemStore : IStorage {
  std::vector<uint8_t> bytes;
  bool sizeChecked(size_t& out) override { out = bytes.size(); return true; }
  bool read(size_t offset, uint8_t* out, size_t n) override {
    if (!out || offset > bytes.size() || n > bytes.size() - offset) return false;
    memcpy(out, bytes.data() + offset, n); return true;
  }
  bool append(const uint8_t* data, size_t n) override {
    if (!data && n) return false;
    bytes.insert(bytes.end(), data, data + n); return true;
  }
  bool truncate(size_t n) override { if (n > bytes.size()) return false; bytes.resize(n); return true; }
};
uint32_t noon(int32_t day) { return dayStartEpoch(day, 480) + 12u * 3600u; }

struct Rig {
  MemStore storage;
  LogStore log;
  RosterStore live;
  EventGroupAudit audit;
  OpenInfo opened;
  RosterLogPrefix prefix;
  RosterStore scratch;
  RosterTransitionAudit transition;
  Rig() : log(storage), live(480), opened{false, 0, 0, 0, 0, 0, -1, 0},
          prefix{0, 0, 0, 0}, scratch(480) {
    opened = log.open();
    log.attach(&live);
  }
  bool set(uint16_t id, const char* name, int32_t day) {
    Event events[5]; size_t count = 0;
    if (live.planSet(id, name, strlen(name), noon(day), events, count) != RE_OK) return false;
    return log.append(events, count);
  }
  bool deactivate(uint16_t id, int32_t day) {
    Event event = {EV_ROSTER, 0, id, noon(day), 0, RA_DEACTIVATE};
    return log.append(&event, 1);
  }
  bool activate(uint16_t id, int32_t day) {
    Event event = {EV_ROSTER, 0, id, noon(day), 0, RA_ACTIVATE};
    return log.append(&event, 1);
  }
  bool sealAndCapture() {
    return audit.attach(log) && captureRosterLogPrefix(log, audit, opened, prefix);
  }
  MonthRosterProjection projection(int32_t start, int32_t end) {
    return MonthRosterProjection(log, audit, opened, prefix, 480, scratch,
                                 transition, start, end);
  }
};
}

void setUp() {}
void tearDown() {}

void test_month_projection_preserves_multiple_intervals_for_one_month_roster_learner() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.activate(8, 115));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  TEST_ASSERT_EQUAL_UINT32(1, p.studentCount());
  TEST_ASSERT_EQUAL_UINT32(2, p.intervalCount());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(8, student.studentId);
  TEST_ASSERT_EQUAL_UINT16(2, student.overlappingIntervalCount);
  TEST_ASSERT_TRUE(student.enrolledOnLastDay);
  RosterIntervalView interval;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.intervalAt(8, 0, interval));
  TEST_ASSERT_EQUAL_INT32(100, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(110, interval.endDayExclusive);
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.intervalAt(8, 1, interval));
  TEST_ASSERT_EQUAL_INT32(115, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(INT32_MAX, interval.endDayExclusive);
  TEST_ASSERT_EQUAL_INT(MRP_END, p.next(student));
}

void test_month_projection_excludes_intervals_touching_only_exclusive_boundaries() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "Before", 90));
  TEST_ASSERT_TRUE(g.deactivate(1, 100));
  TEST_ASSERT_TRUE(g.set(2, "After", 131));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  TEST_ASSERT_EQUAL_UINT32(0, p.studentCount());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_END, p.next(student));
}

void test_month_projection_includes_learner_who_enrolled_and_exited_inside_month() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(4, "Midmonth", 104));
  TEST_ASSERT_TRUE(g.deactivate(4, 109));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  TEST_ASSERT_EQUAL_UINT32(1, p.studentCount());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(4, student.studentId);
  TEST_ASSERT_FALSE(student.enrolledOnLastDay);
}

void test_month_projection_reports_end_of_month_enrollment_separately_from_month_overlap() {
  Rig g;
  // The report month is [100, 131), so its final included day is 130.
  // Ending an interval on day 130 excludes that day; ending on day 131 does not.
  TEST_ASSERT_TRUE(g.set(4, "ExitedOnEnd", 100));
  TEST_ASSERT_TRUE(g.deactivate(4, 130));
  TEST_ASSERT_TRUE(g.set(5, "StillActive", 101));
  TEST_ASSERT_TRUE(g.set(6, "ExitOnExclusiveEnd", 100));
  TEST_ASSERT_TRUE(g.deactivate(6, 131));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  TEST_ASSERT_EQUAL_UINT32(3, p.studentCount());
  TEST_ASSERT_EQUAL_UINT32(2, p.enrolledOnLastDayCount());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(4, student.studentId);
  TEST_ASSERT_FALSE(student.enrolledOnLastDay);
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(5, student.studentId);
  TEST_ASSERT_TRUE(student.enrolledOnLastDay);
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(6, student.studentId);
  TEST_ASSERT_TRUE(student.enrolledOnLastDay);
}

void test_month_projection_orders_students_by_stable_device_id_not_name() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(20, "Zed", 100));
  TEST_ASSERT_TRUE(g.set(3, "Amy", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(3, student.studentId);
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.next(student));
  TEST_ASSERT_EQUAL_UINT16(20, student.studentId);
}

void test_month_projection_copies_open_and_prefix_identity_descriptors() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  // The projection must retain the captured identity even if the caller later
  // reuses or accidentally edits its descriptor objects.
  g.opened.ok = false;
  g.prefix.recordCount = 99;
  g.prefix.lastSeq = 99;
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
}

void test_month_projection_rejects_invalid_or_empty_day_ranges() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 100);
  TEST_ASSERT_EQUAL_INT(MRP_BAD_RANGE, p.begin());
}

void test_month_projection_clears_previous_replay_result_when_rebegin_sees_stale_prefix() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 1, noon(100), 0, 0};
  TEST_ASSERT_TRUE(g.log.append(&scan, 1));
  TEST_ASSERT_EQUAL_INT(MRP_LOG_UNTRUSTED, p.begin());
  TEST_ASSERT_EQUAL_INT(RPR_LOG_UNTRUSTED, p.replayResult().code);
}

void test_month_projection_rejects_untrusted_prefix_before_replaying() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_LOG_UNTRUSTED, p.begin());
}

void test_month_projection_rejects_same_day_initial_enroll_then_deactivate_under_open_e27() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_REPLAY_FAILED, p.begin());
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, p.replayResult().code);
  TEST_ASSERT_EQUAL_INT(RTA_DEACTIVATE_NOT_FORWARD, p.replayResult().transition.code);
}

void test_month_projection_rejects_same_day_deactivate_reactivate_under_open_e27() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.activate(8, 110));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_REPLAY_FAILED, p.begin());
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, p.replayResult().code);
  TEST_ASSERT_EQUAL_INT(RTA_ACTIVATE_NOT_FORWARD, p.replayResult().transition.code);
}

void test_month_projection_invalidates_reader_after_source_log_append() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 1, noon(100), 0, 0};
  TEST_ASSERT_TRUE(g.log.append(&scan, 1));
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_PREFIX_CHANGED, p.next(student));
}

void test_month_projection_detects_scratch_roster_mutation_after_begin() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  g.scratch.onReset();
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_SCRATCH_CHANGED, p.next(student));
}


void test_month_projection_detects_name_mutation_even_when_roster_counts_stay_the_same() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "Original", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  Event rename[5]; size_t count = 0;
  TEST_ASSERT_EQUAL_INT(RE_OK, g.scratch.planSet(1, "Mutated", 7, noon(100), rename, count));
  TEST_ASSERT_EQUAL_UINT32(5, count);
  for (size_t i = 0; i < count; ++i) g.scratch.onRecord(rename[i]);
  TEST_ASSERT_EQUAL_UINT32(1, g.scratch.knownCount());
  TEST_ASSERT_EQUAL_UINT32(1, g.scratch.intervalCount());
  MonthRosterStudent student;
  TEST_ASSERT_EQUAL_INT(MRP_SCRATCH_CHANGED, p.next(student));
}

void test_month_projection_interval_query_rejects_unknown_ids_and_missing_ordinals() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(1, "One", 100));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  RosterIntervalView interval;
  TEST_ASSERT_EQUAL_INT(MRP_NOT_FOUND, p.intervalAt(2, 0, interval));
  TEST_ASSERT_EQUAL_INT(MRP_NOT_FOUND, p.intervalAt(1, 1, interval));
}

void test_month_projection_rejects_backward_effective_day_history() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Newer", 110));
  // A SET on an already-known ID is a name revision; this is backdated.
  Event events[5]; size_t count = 0;
  TEST_ASSERT_EQUAL_INT(RE_OK, g.live.planSet(8, "Older", 5, noon(105), events, count));
  TEST_ASSERT_TRUE(g.log.append(events, count));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_REPLAY_FAILED, p.begin());
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, p.replayResult().code);
}


void test_month_projection_accepts_lifecycle_transition_on_later_rename_day() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 120));
  TEST_ASSERT_TRUE(g.activate(8, 120));
  TEST_ASSERT_TRUE(g.sealAndCapture());
  MonthRosterProjection p = g.projection(100, 131);
  TEST_ASSERT_EQUAL_INT(MRP_READY, p.begin());
  TEST_ASSERT_EQUAL_UINT32(1, p.studentCount());
  TEST_ASSERT_EQUAL_UINT32(2, p.intervalCount());
  RosterIntervalView interval;
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.intervalAt(8, 0, interval));
  TEST_ASSERT_EQUAL_INT32(100, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(110, interval.endDayExclusive);
  TEST_ASSERT_EQUAL_INT(MRP_VALUE, p.intervalAt(8, 1, interval));
  TEST_ASSERT_EQUAL_INT32(120, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(INT32_MAX, interval.endDayExclusive);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_month_projection_preserves_multiple_intervals_for_one_month_roster_learner);
  RUN_TEST(test_month_projection_excludes_intervals_touching_only_exclusive_boundaries);
  RUN_TEST(test_month_projection_includes_learner_who_enrolled_and_exited_inside_month);
  RUN_TEST(test_month_projection_reports_end_of_month_enrollment_separately_from_month_overlap);
  RUN_TEST(test_month_projection_orders_students_by_stable_device_id_not_name);
  RUN_TEST(test_month_projection_copies_open_and_prefix_identity_descriptors);
  RUN_TEST(test_month_projection_rejects_invalid_or_empty_day_ranges);
  RUN_TEST(test_month_projection_clears_previous_replay_result_when_rebegin_sees_stale_prefix);
  RUN_TEST(test_month_projection_rejects_untrusted_prefix_before_replaying);
  RUN_TEST(test_month_projection_rejects_same_day_initial_enroll_then_deactivate_under_open_e27);
  RUN_TEST(test_month_projection_rejects_same_day_deactivate_reactivate_under_open_e27);
  RUN_TEST(test_month_projection_invalidates_reader_after_source_log_append);
  RUN_TEST(test_month_projection_detects_scratch_roster_mutation_after_begin);
  RUN_TEST(test_month_projection_detects_name_mutation_even_when_roster_counts_stay_the_same);
  RUN_TEST(test_month_projection_interval_query_rejects_unknown_ids_and_missing_ordinals);
  RUN_TEST(test_month_projection_rejects_backward_effective_day_history);
  RUN_TEST(test_month_projection_accepts_lifecycle_transition_on_later_rename_day);
  return UNITY_END();
}
