#include <unity.h>
#include <string.h>
#include "config_boot.h"

using namespace att;

namespace {
class FakeBackend : public IConfigBootBackend {
 public:
  FakeBackend() : beginCalls(0), loadCalls(0), reads(0), writes(0), ready(false),
      beginOk(true), loadOk(true), writeOk(true), persistOnWriteError(true),
      markerPresent(false), markerReadOk(true), reloadFailAfterWrite(false), forceInvalidPick(false) {
    memset(slots, 0xFF, sizeof slots);
  }

  bool begin() override {
    ++beginCalls;
    ready = beginOk;
    return beginOk;
  }

  bool load(Config& out, CfgPick& pick) override {
    ++loadCalls;
    if (!ready || !loadOk) return false;
    if (forceInvalidPick) {
      out = defaultConfig();
      pick = CfgPick{CB_OK, -1, 1};
      return true;
    }
    if (!readConfigSlots(*this, out, pick)) return false;
    if (pick.boot == CB_UNPROVISIONED && markerPresent) {
      pick = CfgPick{CB_FAULT, -1, 0};
      return false;
    }
    if (pick.boot == CB_OK && !markerPresent) markerPresent = true;  // legacy migration
    if (reloadFailAfterWrite && writes > 0) return false;
    return true;
  }

  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    ++reads;
    if (!ready || slot > 1 || !out) return false;
    memcpy(out, slots[slot], CFG_SLOT_SIZE);
    return true;
  }

  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    ++writes;
    if (!ready || slot > 1 || !data) return false;
    if (writeOk || persistOnWriteError) {
      memcpy(slots[slot], data, CFG_SLOT_SIZE);
      markerPresent = true;
    }
    return writeOk;
  }

  bool readProvisionMarker(bool& present) override {
    if (!ready || !markerReadOk) return false;
    present = markerPresent;
    return true;
  }

  uint8_t slots[2][CFG_SLOT_SIZE];
  uint32_t beginCalls, loadCalls, reads, writes;
  bool ready, beginOk, loadOk, writeOk, persistOnWriteError;
  bool markerPresent, markerReadOk, reloadFailAfterWrite, forceInvalidPick;
};

class FakeGate : public IConfigProvisionGate {
 public:
  explicit FakeGate(bool allowed) : allowed_(allowed) {}
  bool mayProvision() const override { return allowed_; }
 private:
  bool allowed_;
};

Config initialConfig() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "boot-path-test");
  return c;
}

void seed(FakeBackend& backend, const Config& c, uint32_t seq, uint8_t slot) {
  uint8_t encoded[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, seq, encoded));
  memcpy(backend.slots[slot], encoded, sizeof encoded);
  backend.markerPresent = true;
}
}

void setUp() {}
void tearDown() {}

void test_cold_coordinator_exposes_no_config_or_selection() {
  FakeBackend backend;
  ConfigBootCoordinator boot(backend);
  CfgPick selected = {CB_OK, 0, 99};
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_COLD, boot.state());
  TEST_ASSERT_NULL(boot.config());
  TEST_ASSERT_FALSE(boot.selection(selected));
  TEST_ASSERT_EQUAL_INT(CB_OK, selected.boot);  // untouched on failure
}

void test_nvs_begin_failure_latches_fault_and_does_not_load() {
  FakeBackend backend; backend.beginOk = false;
  ConfigBootCoordinator boot(backend);
  ConfigBootResult result = boot.start();
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_BACKEND_BEGIN_FAILED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_FAULT, boot.state());
  TEST_ASSERT_EQUAL_UINT32(0, backend.loadCalls);
  TEST_ASSERT_NULL(boot.config());
}

void test_load_failure_latches_fault_without_exposing_defaults() {
  FakeBackend backend; backend.loadOk = false;
  ConfigBootCoordinator boot(backend);
  ConfigBootResult result = boot.start();
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_LOAD_FAILED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_FAULT, boot.state());
  TEST_ASSERT_NULL(boot.config());
}

void test_blank_nvs_enters_setup_state_without_default_runtime_config() {
  FakeBackend backend;
  ConfigBootCoordinator boot(backend);
  ConfigBootResult result = boot.start();
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_NEEDS_SETUP, boot.state());
  TEST_ASSERT_TRUE(boot.needsSetup());
  TEST_ASSERT_NULL(boot.config());
}

void test_existing_valid_slot_loads_exact_config_and_generation() {
  FakeBackend backend; Config expected = initialConfig();
  seed(backend, expected, 7, 1);
  ConfigBootCoordinator boot(backend);
  ConfigBootResult result = boot.start();
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, result.code);
  TEST_ASSERT_TRUE(boot.ready());
  TEST_ASSERT_NOT_NULL(boot.config());
  TEST_ASSERT_TRUE(configEqual(*boot.config(), expected));
  CfgPick selected = {CB_FAULT, -1, 0};
  TEST_ASSERT_TRUE(boot.selection(selected));
  TEST_ASSERT_EQUAL_INT(1, selected.active);
  TEST_ASSERT_EQUAL_UINT32(7, selected.seq);
}

void test_backend_cannot_report_ready_with_an_invalid_slot_selection() {
  FakeBackend backend; backend.forceInvalidPick = true;
  ConfigBootCoordinator boot(backend);
  ConfigBootResult result = boot.start();
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_INVALID_SELECTION, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_FAULT, boot.state());
  TEST_ASSERT_NULL(boot.config());
}

void test_start_is_one_shot_even_after_setup_required() {
  FakeBackend backend;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_ALREADY_STARTED, boot.start().code);
  TEST_ASSERT_EQUAL_UINT32(1, backend.beginCalls);
}

void test_provision_before_start_is_rejected() {
  FakeBackend backend; FakeGate gate(true);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_NOT_SETUP_REQUIRED, boot.provision(initialConfig(), gate).code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_unauthorized_provision_keeps_setup_state_and_does_not_write() {
  FakeBackend backend; FakeGate gate(false);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  ConfigBootResult result = boot.provision(initialConfig(), gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_PROVISION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_NOT_AUTHORIZED, result.provisionCode);
  TEST_ASSERT_TRUE(boot.needsSetup());
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_invalid_initial_config_can_be_corrected_without_storage_mutation() {
  FakeBackend backend; FakeGate gate(true);
  ConfigBootCoordinator boot(backend);
  boot.start();
  Config invalid = initialConfig(); invalid.pinSet = 0;
  ConfigBootResult result = boot.provision(invalid, gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_PROVISION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_INVALID_CONFIG, result.provisionCode);
  TEST_ASSERT_TRUE(boot.needsSetup());
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_successful_first_provision_reloads_before_becoming_ready() {
  FakeBackend backend; FakeGate gate(true); Config expected = initialConfig();
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  ConfigBootResult result = boot.provision(expected, gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_READY, boot.state());
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
  TEST_ASSERT_EQUAL_UINT32(2, backend.loadCalls);
  TEST_ASSERT_TRUE(configEqual(*boot.config(), expected));
  CfgPick selected = {CB_FAULT, -1, 0};
  TEST_ASSERT_TRUE(boot.selection(selected));
  TEST_ASSERT_EQUAL_INT(0, selected.active);
  TEST_ASSERT_EQUAL_UINT32(1, selected.seq);
}

void test_ambiguous_provision_write_latches_boot_fault() {
  FakeBackend backend; FakeGate gate(true); backend.writeOk = false;
  ConfigBootCoordinator boot(backend);
  boot.start();
  ConfigBootResult result = boot.provision(initialConfig(), gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_PROVISION_FAILED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_STORAGE_INDETERMINATE, result.provisionCode);
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_NULL(boot.config());
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_NOT_SETUP_REQUIRED, boot.provision(initialConfig(), gate).code);
}

void test_reload_failure_after_provision_latches_boot_fault() {
  FakeBackend backend; FakeGate gate(true); backend.reloadFailAfterWrite = true;
  ConfigBootCoordinator boot(backend);
  boot.start();
  ConfigBootResult result = boot.provision(initialConfig(), gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_RELOAD_FAILED, result.code);
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_NULL(boot.config());
}

void test_provision_marker_read_error_latches_boot_fault() {
  FakeBackend backend; FakeGate gate(true); backend.markerReadOk = false;
  ConfigBootCoordinator boot(backend);
  boot.start();
  ConfigBootResult result = boot.provision(initialConfig(), gate);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_PROVISION_FAILED, result.code);
  TEST_ASSERT_EQUAL_INT(CFG_PROVISION_READ_FAILED, result.provisionCode);
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_successful_boot_cannot_be_reprovisioned() {
  FakeBackend backend; FakeGate gate(true); Config expected = initialConfig();
  seed(backend, expected, 3, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_NOT_SETUP_REQUIRED, boot.provision(expected, gate).code);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_selection_output_is_unchanged_when_not_ready() {
  FakeBackend backend;
  ConfigBootCoordinator boot(backend);
  boot.start();
  CfgPick selected = {CB_OK, 1, 55};
  TEST_ASSERT_FALSE(boot.selection(selected));
  TEST_ASSERT_EQUAL_INT(CB_OK, selected.boot);
  TEST_ASSERT_EQUAL_INT(1, selected.active);
  TEST_ASSERT_EQUAL_UINT32(55, selected.seq);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cold_coordinator_exposes_no_config_or_selection);
  RUN_TEST(test_nvs_begin_failure_latches_fault_and_does_not_load);
  RUN_TEST(test_load_failure_latches_fault_without_exposing_defaults);
  RUN_TEST(test_blank_nvs_enters_setup_state_without_default_runtime_config);
  RUN_TEST(test_existing_valid_slot_loads_exact_config_and_generation);
  RUN_TEST(test_backend_cannot_report_ready_with_an_invalid_slot_selection);
  RUN_TEST(test_start_is_one_shot_even_after_setup_required);
  RUN_TEST(test_provision_before_start_is_rejected);
  RUN_TEST(test_unauthorized_provision_keeps_setup_state_and_does_not_write);
  RUN_TEST(test_invalid_initial_config_can_be_corrected_without_storage_mutation);
  RUN_TEST(test_successful_first_provision_reloads_before_becoming_ready);
  RUN_TEST(test_ambiguous_provision_write_latches_boot_fault);
  RUN_TEST(test_reload_failure_after_provision_latches_boot_fault);
  RUN_TEST(test_provision_marker_read_error_latches_boot_fault);
  RUN_TEST(test_successful_boot_cannot_be_reprovisioned);
  RUN_TEST(test_selection_output_is_unchanged_when_not_ready);
  return UNITY_END();
}
