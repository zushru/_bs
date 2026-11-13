#include <unity.h>
#include <string.h>
#include <vector>
#include "roster_event_writer.h"
using namespace att;
void setUp() {}
void tearDown() {}
struct MemStorage : IStorage {
  enum Failure { FAIL_NONE, FAIL_PARTIAL, FAIL_COMPLETE };
  MemStorage() : failure(FAIL_NONE), partialBytes(0), appendCalls(0) {}
  std::vector<uint8_t> bytes;
  Failure failure; size_t partialBytes; size_t appendCalls;
  bool sizeChecked(size_t& out) override { out = bytes.size(); return true; }
  bool read(size_t off, uint8_t* out, size_t len) override {
    if (off > bytes.size() || len > bytes.size() - off) return false;
    memcpy(out, bytes.data() + off, len); return true;
  }
  bool append(const uint8_t* data, size_t len) override {
    ++appendCalls;
    if (failure == FAIL_PARTIAL) {
      const size_t take = partialBytes < len ? partialBytes : len;
      bytes.insert(bytes.end(), data, data + take); failure = FAIL_NONE; return false;
    }
    bytes.insert(bytes.end(), data, data + len);
    if (failure == FAIL_COMPLETE) { failure = FAIL_NONE; return false; }
    return true;
  }
  bool truncate(size_t len) override {
    if (len > bytes.size()) return false; bytes.resize(len); return true;
  }
  void failPartialOnce(size_t len) { failure = FAIL_PARTIAL; partialBytes = len; }
  void failCompleteOnce() { failure = FAIL_COMPLETE; }
};
static uint32_t noon(int32_t day) { return dayStartEpoch(day, 480) + 43200u; }
struct Rig {
  MemStorage storage; LogStore log; RosterStore roster; OpenInfo opened; bool attached;
  Rig() : log(storage), roster(480), opened(log.open()), attached(log.attach(&roster)) {}
};
void test_writer_commits_set_rename_deactivate_and_activate() {
  Rig rig; TEST_ASSERT_TRUE(rig.opened.ok); TEST_ASSERT_TRUE(rig.attached);
  RosterEventWriter writer(rig.log, rig.roster);
  RosterWriteOutcome result = writer.set(5, "Ana", 3, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state); TEST_ASSERT_EQUAL_INT(RE_OK, result.rosterError);
  TEST_ASSERT_TRUE(rig.roster.known(5)); TEST_ASSERT_TRUE(rig.roster.active(5));
  TEST_ASSERT_EQUAL_UINT32(5, rig.log.count());
  result = writer.set(5, "Ana Reyes", 9, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state); TEST_ASSERT_EQUAL_UINT32(1, rig.roster.intervalCount());
  char name[ROSTER_NAME_MAX + 1] = {0};
  TEST_ASSERT_EQUAL_UINT32(9, rig.roster.nameOf(5, name)); TEST_ASSERT_EQUAL_STRING("Ana Reyes", name);
  TEST_ASSERT_TRUE(rig.roster.active(5));
  result = writer.deactivate(5, noon(102)); TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state);
  TEST_ASSERT_FALSE(rig.roster.active(5));
  result = writer.activate(5, noon(103)); TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state);
  TEST_ASSERT_TRUE(rig.roster.active(5)); TEST_ASSERT_EQUAL_UINT32(12, rig.log.count());
  TEST_ASSERT_TRUE(rig.roster.historyComplete()); TEST_ASSERT_FALSE(writer.faulted());
}
void test_writer_noops_and_planning_rejections_do_not_append() {
  Rig rig; TEST_ASSERT_TRUE(rig.opened.ok); TEST_ASSERT_TRUE(rig.attached);
  RosterEventWriter writer(rig.log, rig.roster);
  TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, writer.set(5, "Ana", 3, noon(100)).state);
  const size_t calls = rig.storage.appendCalls;
  RosterWriteOutcome result = writer.set(5, "Ana", 3, 0);
  TEST_ASSERT_EQUAL_INT(RWS_NOOP, result.state); TEST_ASSERT_EQUAL_INT(RE_NOOP, result.rosterError);
  result = writer.set(5, "Ana R", 5, noon(99));
  TEST_ASSERT_EQUAL_INT(RWS_REJECTED, result.state); TEST_ASSERT_EQUAL_INT(RE_BACKDATED, result.rosterError);
  result = writer.deactivate(5, noon(99));
  TEST_ASSERT_EQUAL_INT(RWS_REJECTED, result.state); TEST_ASSERT_EQUAL_INT(RE_BACKDATED, result.rosterError);
  result = writer.deactivate(99, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_REJECTED, result.state); TEST_ASSERT_EQUAL_INT(RE_UNKNOWN, result.rosterError);
  result = writer.activate(99, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_REJECTED, result.state); TEST_ASSERT_EQUAL_INT(RE_UNKNOWN, result.rosterError);
  result = writer.set(6, "", 0, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_REJECTED, result.state); TEST_ASSERT_EQUAL_INT(RE_BAD_NAME, result.rosterError);
  TEST_ASSERT_EQUAL_UINT32(calls, rig.storage.appendCalls); TEST_ASSERT_EQUAL_UINT32(5, rig.log.count());
  TEST_ASSERT_FALSE(writer.faulted());
}
void test_writer_requires_open_bound_log_and_exact_observer_generation() {
  MemStorage storage; LogStore log(storage); RosterStore roster(480);
  RosterEventWriter beforeOpen(log, roster);
  RosterWriteOutcome result = beforeOpen.set(1, "A", 1, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_BLOCKED, result.state); TEST_ASSERT_EQUAL_UINT32(0, storage.appendCalls);
  TEST_ASSERT_TRUE(log.open().ok);
  RosterEventWriter withoutObserver(log, roster);
  result = withoutObserver.set(1, "A", 1, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_BLOCKED, result.state); TEST_ASSERT_EQUAL_UINT32(0, storage.appendCalls);
  TEST_ASSERT_TRUE(log.attach(&roster));
  RosterEventWriter oldGeneration(log, roster);
  TEST_ASSERT_TRUE(log.open().ok);
  result = oldGeneration.set(1, "A", 1, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_BLOCKED, result.state); TEST_ASSERT_EQUAL_UINT32(0, storage.appendCalls);
  RosterEventWriter current(log, roster);
  result = current.set(1, "A", 1, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state); TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
}
void test_writer_blocks_incomplete_replayed_roster_history() {
  Rig rig; TEST_ASSERT_TRUE(rig.opened.ok); TEST_ASSERT_TRUE(rig.attached);
  Event invalid = {EV_ROSTER, 0, 0, noon(100), 0, RA_SET};
  TEST_ASSERT_TRUE(rig.log.append(&invalid, 1)); TEST_ASSERT_FALSE(rig.roster.historyComplete());
  const size_t calls = rig.storage.appendCalls;
  RosterEventWriter writer(rig.log, rig.roster);
  RosterWriteOutcome result = writer.set(1, "A", 1, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_BLOCKED, result.state); TEST_ASSERT_EQUAL_UINT32(calls, rig.storage.appendCalls);
}
void test_writer_latches_full_append_false_and_requires_replay_reconciliation() {
  Rig rig; TEST_ASSERT_TRUE(rig.opened.ok); TEST_ASSERT_TRUE(rig.attached);
  RosterEventWriter writer(rig.log, rig.roster); rig.storage.failCompleteOnce();
  RosterWriteOutcome result = writer.set(5, "Ana", 3, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_UNCERTAIN, result.state); TEST_ASSERT_TRUE(writer.faulted());
  TEST_ASSERT_TRUE(rig.log.faulted()); TEST_ASSERT_FALSE(rig.roster.known(5));
  TEST_ASSERT_EQUAL_UINT32(1, rig.storage.appendCalls);
  result = writer.set(5, "Ana", 3, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_UNCERTAIN, result.state); TEST_ASSERT_EQUAL_UINT32(1, rig.storage.appendCalls);
  LogStore reopened(rig.storage); const OpenInfo info = reopened.open();
  TEST_ASSERT_TRUE(info.ok); TEST_ASSERT_EQUAL_UINT32(5, info.records);
  RosterStore replayed(480); TEST_ASSERT_TRUE(reopened.attach(&replayed));
  TEST_ASSERT_TRUE(replayed.historyComplete()); TEST_ASSERT_TRUE(replayed.known(5));
  char name[ROSTER_NAME_MAX + 1] = {0};
  TEST_ASSERT_EQUAL_UINT32(3, replayed.nameOf(5, name)); TEST_ASSERT_EQUAL_STRING("Ana", name);
  RosterEventWriter reconciled(reopened, replayed);
  result = reconciled.set(5, "Ana Reyes", 9, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_COMMITTED, result.state);
}
void test_writer_latches_partial_append_and_never_repairs_or_retries() {
  Rig rig; TEST_ASSERT_TRUE(rig.opened.ok); TEST_ASSERT_TRUE(rig.attached);
  RosterEventWriter writer(rig.log, rig.roster); rig.storage.failPartialOnce(7);
  RosterWriteOutcome result = writer.set(5, "Ana", 3, noon(100));
  TEST_ASSERT_EQUAL_INT(RWS_UNCERTAIN, result.state); TEST_ASSERT_TRUE(writer.faulted());
  TEST_ASSERT_TRUE(rig.log.faulted()); TEST_ASSERT_EQUAL_UINT32(1, rig.storage.appendCalls);
  result = writer.deactivate(5, noon(101));
  TEST_ASSERT_EQUAL_INT(RWS_UNCERTAIN, result.state); TEST_ASSERT_EQUAL_UINT32(1, rig.storage.appendCalls);
  LogStore reopened(rig.storage); const OpenInfo info = reopened.open();
  TEST_ASSERT_FALSE(info.ok); TEST_ASSERT_TRUE(reopened.faulted());
  TEST_ASSERT_EQUAL_UINT32(7, info.tornBytes); TEST_ASSERT_EQUAL_UINT32(7, rig.storage.bytes.size());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_writer_commits_set_rename_deactivate_and_activate);
  RUN_TEST(test_writer_noops_and_planning_rejections_do_not_append);
  RUN_TEST(test_writer_requires_open_bound_log_and_exact_observer_generation);
  RUN_TEST(test_writer_blocks_incomplete_replayed_roster_history);
  RUN_TEST(test_writer_latches_full_append_false_and_requires_replay_reconciliation);
  RUN_TEST(test_writer_latches_partial_append_and_never_repairs_or_retries);
  return UNITY_END();
}
