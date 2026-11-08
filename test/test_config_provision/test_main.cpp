#include <unity.h>
#include <string.h>
#include "config_provision.h"

using namespace att;

namespace {
class FakeBackend : public IConfigProvisionBackend {
 public:
  FakeBackend() : readCalls(0), failReadAt(0), writes(0), lastWriteSlot(255),
                  writeResult(true), persistOnWriteError(true), corruptAfterWrite(false),
                  dirtyOtherSlotAfterWrite(false), markerPresent(false), markerReadOk(true),
                  markerReads(0) {
    memset(slots, 0xFF, sizeof slots);
  }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    ++readCalls;
    if (slot > 1 || out == nullptr || (failReadAt && readCalls == failReadAt))
      return false;
    memcpy(out, slots[slot], CFG_SLOT_SIZE);
    return true;
  }
  bool readProvisionMarker(bool& present) override {
    ++markerReads;
    present = markerPresent;
    return markerReadOk;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    ++writes;
    lastWriteSlot = slot;
    if (slot > 1 || !data) return false;
    if (writeResult || persistOnWriteError) {
      memcpy(slots[slot], data, CFG_SLOT_SIZE);
      markerPresent = true;  // Model slot + ATTP marker in the same durable commit.
    }
    if (corruptAfterWrite) slots[slot][20] ^= 0x01;
    if (dirtyOtherSlotAfterWrite) slots[1 - slot][0] = 0x00;
    return writeResult;
  }
  uint8_t slots[2][CFG_SLOT_SIZE];
  uint32_t readCalls, failReadAt, writes;
  uint8_t lastWriteSlot;
  bool writeResult, persistOnWriteError, corruptAfterWrite, dirtyOtherSlotAfterWrite;
  bool markerPresent, markerReadOk;
  uint32_t markerReads;
};
class FakeGate : public IConfigProvisionGate {
 public:
  explicit FakeGate(bool allowed) : allowed_(allowed) {}
  bool mayProvision() const override { return allowed_; }
 private:
  bool allowed_;
};
Config validInitial() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}
CfgPick unprovisioned() { return CfgPick{CB_UNPROVISIONED, -1, 0}; }
}

void setUp() {}
void tearDown() {}

void test_unauthorized_provision_does_not_read_or_write() {
  FakeBackend backend; FakeGate gate(false);
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_NOT_AUTHORIZED, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_rejects_non_unprovisioned_boot_result() {
  FakeBackend backend; FakeGate gate(true);
  ConfigProvisioner provisioner(backend, gate);
  CfgPick selected = {CB_OK, 0, 7};
  ConfigProvisionResult result = provisioner.provision(validInitial(), selected);
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_NOT_UNPROVISIONED, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_rechecks_slots_and_refuses_stale_unprovisioned_result() {
  FakeBackend backend; FakeGate gate(true);
  Config existing = validInitial(); uint8_t raw[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(existing, 4, raw));
  memcpy(backend.slots[1], raw, sizeof raw);
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_NOT_UNPROVISIONED, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_surviving_marker_blocks_reprovision_when_both_slots_are_blank() {
  FakeBackend backend; FakeGate gate(true);
  backend.markerPresent = true;  // Slot keys may have been lost independently.
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_NOT_UNPROVISIONED, result.code);
  TEST_ASSERT_EQUAL_UINT32(1, backend.markerReads);
  TEST_ASSERT_EQUAL_UINT32(0, backend.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_marker_read_error_blocks_provision_without_writing() {
  FakeBackend backend; FakeGate gate(true);
  backend.markerReadOk = false;
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_READ_FAILED, result.code);
  TEST_ASSERT_EQUAL_UINT32(1, backend.markerReads);
  TEST_ASSERT_EQUAL_UINT32(0, backend.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_invalid_initial_config_is_rejected_before_storage_access() {
  FakeBackend backend; FakeGate gate(true);
  ConfigProvisioner provisioner(backend, gate);
  Config invalid = validInitial(); invalid.pinSet = 0;
  ConfigProvisionResult result = provisioner.provision(invalid, unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_INVALID_CONFIG, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_preflight_read_failure_does_not_write() {
  FakeBackend backend; FakeGate gate(true);
  backend.failReadAt = 1;
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_READ_FAILED, result.code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
  TEST_ASSERT_FALSE(provisioner.faulted());
}

void test_first_provision_writes_only_slot_a_generation_one_and_verifies() {
  FakeBackend backend; FakeGate gate(true);
  Config initial = validInitial();
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(initial, unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_OK, result.code);
  TEST_ASSERT_EQUAL_INT(CB_OK, result.selected.boot);
  TEST_ASSERT_EQUAL_INT(0, result.selected.active);
  TEST_ASSERT_EQUAL_UINT32(1, result.selected.seq);
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
  TEST_ASSERT_EQUAL_UINT8(0, backend.lastWriteSlot);
  TEST_ASSERT_TRUE(backend.markerPresent);
  TEST_ASSERT_FALSE(provisioner.faulted());
}

void test_ambiguous_write_failure_latches_and_prevents_retry() {
  FakeBackend backend; FakeGate gate(true);
  backend.writeResult = false;  // Model an error after bytes might have persisted.
  ConfigProvisioner provisioner(backend, gate);
  Config initial = validInitial();
  ConfigProvisionResult first = provisioner.provision(initial, unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_STORAGE_INDETERMINATE, first.code);
  TEST_ASSERT_TRUE(provisioner.faulted());
  ConfigProvisionResult second = provisioner.provision(initial, unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_FAULTED, second.code);
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
}

void test_nonblank_other_slot_after_provision_is_rejected() {
  FakeBackend backend; FakeGate gate(true);
  backend.dirtyOtherSlotAfterWrite = true;
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_VERIFY_FAILED, result.code);
  TEST_ASSERT_TRUE(provisioner.faulted());
}

void test_post_write_readback_fault_latches_and_never_reports_success() {
  FakeBackend backend; FakeGate gate(true);
  backend.failReadAt = 3;  // First slot read after the write.
  ConfigProvisioner provisioner(backend, gate);
  ConfigProvisionResult result = provisioner.provision(validInitial(), unprovisioned());
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_VERIFY_FAILED, result.code);
  TEST_ASSERT_TRUE(provisioner.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unauthorized_provision_does_not_read_or_write);
  RUN_TEST(test_rejects_non_unprovisioned_boot_result);
  RUN_TEST(test_rechecks_slots_and_refuses_stale_unprovisioned_result);
  RUN_TEST(test_surviving_marker_blocks_reprovision_when_both_slots_are_blank);
  RUN_TEST(test_marker_read_error_blocks_provision_without_writing);
  RUN_TEST(test_invalid_initial_config_is_rejected_before_storage_access);
  RUN_TEST(test_preflight_read_failure_does_not_write);
  RUN_TEST(test_first_provision_writes_only_slot_a_generation_one_and_verifies);
  RUN_TEST(test_ambiguous_write_failure_latches_and_prevents_retry);
  RUN_TEST(test_nonblank_other_slot_after_provision_is_rejected);
  RUN_TEST(test_post_write_readback_fault_latches_and_never_reports_success);
  return UNITY_END();
}
