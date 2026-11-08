#include <unity.h>
#include <string.h>
#include "config_apply.h"

using namespace att;

namespace {
class FakeSlots : public IConfigSlotBackend {
 public:
  FakeSlots() : reads_(0), writes_(0), failReadAt_(0), failWrite_(false), corruptReadback_(false), lastWriteSlot_(0) {
    memset(slots_, 0xFF, sizeof slots_);
  }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    reads_++;
    if (slot > 1 || (failReadAt_ && reads_ == failReadAt_)) return false;
    memcpy(out, slots_[slot], CFG_SLOT_SIZE);
    if (corruptReadback_ && writes_ && slot == lastWriteSlot_) out[20] ^= 1;
    return true;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    writes_++;
    if (slot > 1 || failWrite_) return false;
    memcpy(slots_[slot], data, CFG_SLOT_SIZE);
    lastWriteSlot_ = slot;
    return true;
  }
  void seed(uint8_t slot, const Config& c, uint32_t seq) {
    encodeConfig(c, seq, slots_[slot]);
  }
  void failReadAt(uint32_t n) { failReadAt_ = n; }
  void failWrite(bool v) { failWrite_ = v; }
  void corruptReadback(bool v) { corruptReadback_ = v; }
  uint32_t writes() const { return writes_; }
  uint8_t lastWriteSlot() const { return lastWriteSlot_; }
 private:
  uint8_t slots_[2][CFG_SLOT_SIZE];
  uint32_t reads_, writes_, failReadAt_;
  bool failWrite_, corruptReadback_;
  uint8_t lastWriteSlot_;
};

class FakeRuntime : public IConfigApplyRuntime {
 public:
  FakeRuntime() : can_(true), apply_(true), canCalls_(0), applyCalls_(0) {}
  bool canApply(const Config&) const override {
    canCalls_++;
    return can_;
  }
  bool applyAfterCommit(const Config& c) override {
    applyCalls_++;
    if (!apply_) return false;
    applied_ = c;
    return true;
  }
  void can(bool v) { can_ = v; }
  void apply(bool v) { apply_ = v; }
  uint32_t canCalls() const { return canCalls_; }
  uint32_t applyCalls() const { return applyCalls_; }
  const Config& applied() const { return applied_; }
 private:
  bool can_, apply_;
  mutable uint32_t canCalls_;
  uint32_t applyCalls_;
  Config applied_;
};

Config good() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}

ConfigGateIn gate(uint32_t expected = 7, bool window = false, bool unclosed = false) {
  ConfigGateIn g;
  g.h = H_OK; g.expectGen = expected; g.curGen = 0xDEADBEEFu;
  g.windowOpen = window; g.unclosedPast = unclosed;
  return g;
}

bool boot(FakeSlots& slots, Config& current, CfgPick& pick) {
  return readConfigSlots(slots, current, pick) && pick.boot == CB_OK;
}
}

void setUp() {}
void tearDown() {}

void test_read_slots_selects_highest_valid_generation() {
  FakeSlots slots;
  Config a = good(), b = good(), out = defaultConfig();
  b.soundMode = 1;
  slots.seed(0, a, 7); slots.seed(1, b, 8);
  CfgPick pick = CfgPick{CB_FAULT, -1, 0};
  TEST_ASSERT_TRUE(readConfigSlots(slots, out, pick));
  TEST_ASSERT_EQUAL_INT(CB_OK, pick.boot);
  TEST_ASSERT_EQUAL_INT(1, pick.active);
  TEST_ASSERT_EQUAL_UINT32(8, pick.seq);
  TEST_ASSERT_TRUE(configEqual(b, out));
}

void test_read_io_failure_is_not_treated_as_blank() {
  FakeSlots slots;
  Config out = good(); out.minScore = 4321;
  CfgPick pick = CfgPick{CB_OK, 0, 2};
  slots.failReadAt(2);
  TEST_ASSERT_FALSE(readConfigSlots(slots, out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
  TEST_ASSERT_EQUAL_UINT16(4321, out.minScore);
}

void test_both_blank_slots_report_unprovisioned() {
  FakeSlots slots;
  Config out = good(); CfgPick pick = CfgPick{CB_FAULT, -1, 0};
  TEST_ASSERT_TRUE(readConfigSlots(slots, out, pick));
  TEST_ASSERT_EQUAL_INT(CB_UNPROVISIONED, pick.boot);
  TEST_ASSERT_EQUAL_INT(-1, pick.active);
}

void test_apply_writes_inactive_slot_verifies_then_applies_runtime() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7);
  CfgPick pick;
  TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.soundMode = 2;
  ConfigApplyResult r = coordinator.apply(next, gate());
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_OK, r.code);
  TEST_ASSERT_EQUAL_UINT32(8, r.generation);
  TEST_ASSERT_EQUAL_INT(1, coordinator.activeSlot());
  TEST_ASSERT_EQUAL_UINT32(1, slots.writes());
  TEST_ASSERT_EQUAL_UINT8(1, slots.lastWriteSlot());
  TEST_ASSERT_EQUAL_UINT32(1, runtime.applyCalls());
  TEST_ASSERT_TRUE(configEqual(next, coordinator.current()));
}

void test_stale_generation_rejected_before_storage_or_runtime() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.soundMode = 1;
  ConfigApplyResult r = coordinator.apply(next, gate(6));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_REJECTED, r.code);
  TEST_ASSERT_EQUAL_INT(A_CONFLICT, r.gate.code);
  TEST_ASSERT_EQUAL_INT(R_GEN_MISMATCH, r.gate.reason);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
  TEST_ASSERT_EQUAL_UINT32(0, runtime.applyCalls());
}

void test_caller_current_generation_is_ignored() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_OK, coordinator.apply(next, gate(7)).code);
}

void test_schedule_change_blocked_during_open_window() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.params.lateMin++;
  ConfigApplyResult r = coordinator.apply(next, gate(7, true, false));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_REJECTED, r.code);
  TEST_ASSERT_EQUAL_INT(R_WINDOW_OPEN, r.gate.reason);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
}

void test_schedule_change_blocked_with_unclosed_past() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.params.lateMin++;
  ConfigApplyResult r = coordinator.apply(next, gate(7, false, true));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_REJECTED, r.code);
  TEST_ASSERT_EQUAL_INT(R_UNCLOSED, r.gate.reason);
}

void test_non_schedule_setting_can_change_inside_scan_window() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_OK, coordinator.apply(next, gate(7, true, true)).code);
}

void test_runtime_busy_refuses_without_persisting() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  runtime.can(false); next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_BUSY, coordinator.apply(next, gate()).code);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
  TEST_ASSERT_EQUAL_UINT32(0, runtime.applyCalls());
}

void test_backend_write_failure_latches_indeterminate_fault() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  slots.failWrite(true); next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_STORAGE_INDETERMINATE, coordinator.apply(next, gate()).code);
  TEST_ASSERT_TRUE(coordinator.faulted());
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_FAULTED, coordinator.apply(next, gate()).code);
}

void test_readback_failure_latches_fault_after_write() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  slots.failReadAt(3); next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_VERIFY_FAILED, coordinator.apply(next, gate()).code);
  TEST_ASSERT_TRUE(coordinator.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, runtime.applyCalls());
}

void test_corrupt_readback_latches_fault() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  slots.corruptReadback(true); next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_VERIFY_FAILED, coordinator.apply(next, gate()).code);
  TEST_ASSERT_TRUE(coordinator.faulted());
}

void test_runtime_apply_failure_latches_fault_after_durable_commit() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  runtime.apply(false); next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_RUNTIME_FAILED, coordinator.apply(next, gate()).code);
  TEST_ASSERT_TRUE(coordinator.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, slots.writes());
  TEST_ASSERT_EQUAL_UINT32(1, runtime.applyCalls());
}

void test_unchanged_config_does_not_write_or_advance_generation() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig();
  slots.seed(0, cur, 7); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  ConfigApplyResult r = coordinator.apply(cur, gate());
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_UNCHANGED, r.code);
  TEST_ASSERT_EQUAL_UINT32(7, r.generation);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
}

void test_sequence_exhaustion_refuses_without_wrapping() {
  FakeSlots slots; FakeRuntime runtime;
  Config cur = good(), loaded = defaultConfig(), next = good();
  slots.seed(0, cur, UINT32_MAX); CfgPick pick; TEST_ASSERT_TRUE(boot(slots, loaded, pick));
  ConfigApplyCoordinator coordinator(slots, runtime, loaded, pick);
  next.soundMode = 1;
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_SEQUENCE_EXHAUSTED, coordinator.apply(next, gate(UINT32_MAX)).code);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, coordinator.generation());
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_read_slots_selects_highest_valid_generation);
  RUN_TEST(test_read_io_failure_is_not_treated_as_blank);
  RUN_TEST(test_both_blank_slots_report_unprovisioned);
  RUN_TEST(test_apply_writes_inactive_slot_verifies_then_applies_runtime);
  RUN_TEST(test_stale_generation_rejected_before_storage_or_runtime);
  RUN_TEST(test_caller_current_generation_is_ignored);
  RUN_TEST(test_schedule_change_blocked_during_open_window);
  RUN_TEST(test_schedule_change_blocked_with_unclosed_past);
  RUN_TEST(test_non_schedule_setting_can_change_inside_scan_window);
  RUN_TEST(test_runtime_busy_refuses_without_persisting);
  RUN_TEST(test_backend_write_failure_latches_indeterminate_fault);
  RUN_TEST(test_readback_failure_latches_fault_after_write);
  RUN_TEST(test_corrupt_readback_latches_fault);
  RUN_TEST(test_runtime_apply_failure_latches_fault_after_durable_commit);
  RUN_TEST(test_unchanged_config_does_not_write_or_advance_generation);
  RUN_TEST(test_sequence_exhaustion_refuses_without_wrapping);
  return UNITY_END();
}
