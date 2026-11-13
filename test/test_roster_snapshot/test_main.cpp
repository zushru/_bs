#include <unity.h>
#include <string.h>
#include <vector>
#include "roster_snapshot.h"
#include "roster_name_history.h"

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
  Rig() : log(storage), live(480), opened{false, 0, 0, 0, 0, 0, -1, 0},
          prefix{0, 0, 0, 0} {
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
  bool seal() { return audit.attach(log); }
  bool capture() { return captureRosterLogPrefix(log, audit, opened, prefix); }
};
}

void setUp() {}
void tearDown() {}

void test_capture_prefix_pins_open_count_last_sequence_and_audit_generation() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(7, "Seven", 100));
  TEST_ASSERT_TRUE(g.seal());
  TEST_ASSERT_TRUE(g.capture());
  TEST_ASSERT_TRUE(g.log.openGeneration() == g.prefix.openGeneration);
  TEST_ASSERT_EQUAL_UINT32(g.log.count(), g.prefix.recordCount);
  TEST_ASSERT_EQUAL_UINT32(g.log.lastSeq(), g.prefix.lastSeq);
  TEST_ASSERT_EQUAL_UINT32(g.audit.generation(), g.prefix.auditGeneration);
  TEST_ASSERT_TRUE(rosterLogPrefixCurrent(g.log, g.audit, g.opened, g.prefix));
}

void test_capture_refuses_missing_or_untrusted_audit() {
  Rig g;
  TEST_ASSERT_FALSE(g.capture());
  TEST_ASSERT_FALSE(g.audit.attached());
}

void test_prefix_replay_reconstructs_half_open_intervals_without_mutating_live_roster() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Old Name", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 120));
  TEST_ASSERT_TRUE(g.activate(8, 130));
  TEST_ASSERT_TRUE(g.set(3, "Other", 102));
  TEST_ASSERT_TRUE(g.seal());
  TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480);
  RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_OK, result.code);
  TEST_ASSERT_EQUAL_UINT32(3, result.intervals);
  TEST_ASSERT_EQUAL_UINT32(2, result.knownStudents);
  TEST_ASSERT_FALSE(scratch.expected(8, 125));
  TEST_ASSERT_TRUE(scratch.expected(8, 130));
  TEST_ASSERT_TRUE(scratch.expected(8, 119));
  TEST_ASSERT_TRUE(g.live.expected(8, 130));
  TEST_ASSERT_EQUAL_UINT32(g.live.intervalCount(), scratch.intervalCount());
}

void test_replay_rejects_scratch_roster_with_different_timezone() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(0); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_OFFSET_MISMATCH, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, scratch.intervalCount());
}

void test_replay_rejects_deactivation_before_initial_enrollment_day() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 99));
  TEST_ASSERT_TRUE(g.seal());
  TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_DEACTIVATE_NOT_FORWARD, result.transition.code);
  TEST_ASSERT_EQUAL_UINT16(8, result.transition.studentId);
}

void test_replay_rejects_duplicate_deactivation_transition() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.deactivate(8, 120));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_DEACTIVATE_INACTIVE, result.transition.code);
}

void test_replay_rejects_activation_of_an_already_active_student() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.activate(8, 110));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_ACTIVATE_ACTIVE, result.transition.code);
}

void test_replay_rejects_activation_before_first_set() {
  Rig g;
  TEST_ASSERT_TRUE(g.activate(8, 110));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_ACTIVATE_UNKNOWN, result.transition.code);
}

void test_replay_rejects_backdated_name_revision_after_a_later_name() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Newer", 110));
  TEST_ASSERT_TRUE(g.set(8, "Older", 105));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_SET_BACKWARD_DAY, result.transition.code);
}

void test_replay_rejects_same_day_deactivate_and_reactivate() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.activate(8, 110));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_ACTIVATE_NOT_FORWARD, result.transition.code);
}

void test_replay_allows_same_day_name_revisions_in_source_sequence_order() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_OK, result.code);
  RosterNameHistoryReader reader(g.log, g.audit, g.opened, g.prefix, 480);
  RosterNameVersion version;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(version));
  TEST_ASSERT_EQUAL_STRING("Ana", version.name);
  uint32_t firstSeq = version.sourceSeq;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(version));
  TEST_ASSERT_EQUAL_STRING("Ana Reyes", version.name);
  TEST_ASSERT_TRUE(version.sourceSeq > firstSeq);
  TEST_ASSERT_EQUAL_INT32(100, version.effectiveDay);
}

void test_existing_reader_constructor_still_detects_any_append() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal());
  RosterNameHistoryReader reader(g.log, g.audit, g.opened, 480);
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 8, noon(100), 0, 0};
  TEST_ASSERT_TRUE(g.log.append(&scan, 1));
  RosterNameVersion version;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_SNAPSHOT_CHANGED, reader.next(version));
}

void test_explicit_prefix_name_reader_streams_only_its_captured_prefix() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterNameHistoryReader reader(g.log, g.audit, g.opened, g.prefix, 480);
  RosterNameVersion version;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_VALUE, reader.next(version));
  TEST_ASSERT_EQUAL_UINT16(8, version.studentId);
  TEST_ASSERT_EQUAL_STRING("Ana", version.name);
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_END, reader.next(version));
}

void test_explicit_prefix_reader_rejects_prefix_if_log_has_appended() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterNameHistoryReader reader(g.log, g.audit, g.opened, g.prefix, 480);
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 8, noon(100), 0, 0};
  TEST_ASSERT_TRUE(g.log.append(&scan, 1));
  RosterNameVersion version;
  TEST_ASSERT_EQUAL_INT(ROSTER_NAME_SNAPSHOT_CHANGED, reader.next(version));
}

void test_prefix_replay_rejects_log_revision_change_after_capture() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false), 8, noon(100), 0, 0};
  TEST_ASSERT_TRUE(g.log.append(&scan, 1));
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_SNAPSHOT_CHANGED, result.code);
}

void test_old_open_info_cannot_be_used_after_reopen() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  OpenInfo newOpen = g.log.open();
  TEST_ASSERT_TRUE(newOpen.ok);
  TEST_ASSERT_FALSE(rosterLogPrefixCurrent(g.log, g.audit, newOpen, g.prefix));
  RosterLogPrefix nextPrefix;
  TEST_ASSERT_TRUE(captureRosterLogPrefix(g.log, g.audit, newOpen, nextPrefix));
}

void test_replay_rejects_prefix_with_nonmatching_count_and_sequence() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  g.prefix.lastSeq += 1;
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_PREFIX_INVALID, result.code);
}


void test_replay_allows_deactivation_on_day_of_later_name_revision() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 110));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_OK, result.code);
  TEST_ASSERT_EQUAL_UINT32(1, result.intervals);
  RosterIntervalView interval;
  TEST_ASSERT_TRUE(scratch.intervalAt(0, interval));
  TEST_ASSERT_EQUAL_INT32(100, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(110, interval.endDayExclusive);
}

void test_replay_allows_activation_on_day_of_later_name_revision() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 120));
  TEST_ASSERT_TRUE(g.activate(8, 120));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_OK, result.code);
  TEST_ASSERT_EQUAL_UINT32(2, result.intervals);
  RosterIntervalView interval;
  TEST_ASSERT_TRUE(scratch.intervalAt(0, interval));
  TEST_ASSERT_EQUAL_INT32(100, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(110, interval.endDayExclusive);
  TEST_ASSERT_TRUE(scratch.intervalAt(1, interval));
  TEST_ASSERT_EQUAL_INT32(120, interval.startDayInclusive);
  TEST_ASSERT_EQUAL_INT32(INT32_MAX, interval.endDayExclusive);
}

void test_replay_still_rejects_lifecycle_action_backdated_before_later_rename() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 110));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 120));
  TEST_ASSERT_TRUE(g.activate(8, 119));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_ACTIVATE_NOT_FORWARD, result.transition.code);
}

void test_replay_same_day_rename_does_not_bypass_same_day_lifecycle_guard() {
  Rig g;
  TEST_ASSERT_TRUE(g.set(8, "Ana", 100));
  TEST_ASSERT_TRUE(g.set(8, "Ana Reyes", 100));
  TEST_ASSERT_TRUE(g.deactivate(8, 100));
  TEST_ASSERT_TRUE(g.seal()); TEST_ASSERT_TRUE(g.capture());
  RosterStore scratch(480); RosterTransitionAudit transitions;
  RosterPrefixReplayResult result = replayRosterPrefix(g.log, g.audit, g.opened,
      g.prefix, 480, scratch, transitions);
  TEST_ASSERT_EQUAL_INT(RPR_TRANSITION_INVALID, result.code);
  TEST_ASSERT_EQUAL_INT(RTA_DEACTIVATE_NOT_FORWARD, result.transition.code);
}


void test_transition_audit_rejects_zero_student_id() {
  RosterTransitionAudit audit;
  Event event = {EV_ROSTER, FL_MORE, 0, noon(100), 0, RA_SET};
  TEST_ASSERT_FALSE(audit.consume(event, 480));
  TEST_ASSERT_EQUAL_INT(RTA_BAD_ID, audit.finding().code);
}

void test_transition_audit_rejects_pre_epoch_effective_day() {
  RosterTransitionAudit audit;
  Event event = {EV_ROSTER, FL_MORE, 8, 0, 0, RA_SET};
  TEST_ASSERT_FALSE(audit.consume(event, 480));
  TEST_ASSERT_EQUAL_INT(RTA_PRE_EPOCH, audit.finding().code);
}

void test_transition_audit_rejects_deactivation_of_unknown_id() {
  RosterTransitionAudit audit;
  Event event = {EV_ROSTER, 0, 8, noon(100), 0, RA_DEACTIVATE};
  TEST_ASSERT_FALSE(audit.consume(event, 480));
  TEST_ASSERT_EQUAL_INT(RTA_DEACTIVATE_UNKNOWN, audit.finding().code);
}

void test_transition_audit_rejects_unknown_roster_action() {
  RosterTransitionAudit audit;
  Event event = {EV_ROSTER, 0, 8, noon(100), 0, 99};
  TEST_ASSERT_FALSE(audit.consume(event, 480));
  TEST_ASSERT_EQUAL_INT(RTA_BAD_ACTION, audit.finding().code);
}

void test_transition_audit_reports_backdated_name_revision() {
  RosterTransitionAudit audit;
  Event first = {EV_ROSTER, FL_MORE, 8, noon(110), 0, RA_SET};
  Event backdated = {EV_ROSTER, FL_MORE, 8, noon(105), 0, RA_SET};
  TEST_ASSERT_TRUE(audit.consume(first, 480));
  TEST_ASSERT_FALSE(audit.consume(backdated, 480));
  TEST_ASSERT_EQUAL_INT(RTA_SET_BACKWARD_DAY, audit.finding().code);
  TEST_ASSERT_EQUAL_UINT16(8, audit.finding().studentId);
  TEST_ASSERT_EQUAL_INT32(105, audit.finding().effectiveDay);
  TEST_ASSERT_EQUAL_INT32(110, audit.finding().previousDay);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_capture_prefix_pins_open_count_last_sequence_and_audit_generation);
  RUN_TEST(test_capture_refuses_missing_or_untrusted_audit);
  RUN_TEST(test_prefix_replay_reconstructs_half_open_intervals_without_mutating_live_roster);
  RUN_TEST(test_replay_rejects_scratch_roster_with_different_timezone);
  RUN_TEST(test_replay_rejects_deactivation_before_initial_enrollment_day);
  RUN_TEST(test_replay_rejects_duplicate_deactivation_transition);
  RUN_TEST(test_replay_rejects_activation_of_an_already_active_student);
  RUN_TEST(test_replay_rejects_activation_before_first_set);
  RUN_TEST(test_replay_rejects_backdated_name_revision_after_a_later_name);
  RUN_TEST(test_replay_rejects_same_day_deactivate_and_reactivate);
  RUN_TEST(test_replay_allows_same_day_name_revisions_in_source_sequence_order);
  RUN_TEST(test_existing_reader_constructor_still_detects_any_append);
  RUN_TEST(test_explicit_prefix_name_reader_streams_only_its_captured_prefix);
  RUN_TEST(test_explicit_prefix_reader_rejects_prefix_if_log_has_appended);
  RUN_TEST(test_prefix_replay_rejects_log_revision_change_after_capture);
  RUN_TEST(test_old_open_info_cannot_be_used_after_reopen);
  RUN_TEST(test_replay_rejects_prefix_with_nonmatching_count_and_sequence);
  RUN_TEST(test_replay_allows_deactivation_on_day_of_later_name_revision);
  RUN_TEST(test_replay_allows_activation_on_day_of_later_name_revision);
  RUN_TEST(test_replay_still_rejects_lifecycle_action_backdated_before_later_rename);
  RUN_TEST(test_replay_same_day_rename_does_not_bypass_same_day_lifecycle_guard);
  RUN_TEST(test_transition_audit_rejects_zero_student_id);
  RUN_TEST(test_transition_audit_rejects_pre_epoch_effective_day);
  RUN_TEST(test_transition_audit_rejects_deactivation_of_unknown_id);
  RUN_TEST(test_transition_audit_rejects_unknown_roster_action);
  RUN_TEST(test_transition_audit_reports_backdated_name_revision);
  return UNITY_END();
}
