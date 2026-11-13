#include <unity.h>
#include <string.h>
#include "config_runtime_guard.h"

using namespace att;

namespace {
class FakeBootBackend : public IConfigBootBackend {
 public:
  FakeBootBackend() : ready(false), beginOk(true), loadOk(true), marker(false) {
    memset(slots, 0xFF, sizeof slots);
  }
  bool begin() override { ready = beginOk; return beginOk; }
  bool load(Config& out, CfgPick& pick) override {
    if (!ready || !loadOk) return false;
    if (!readConfigSlots(*this, out, pick)) return false;
    if (pick.boot == CB_UNPROVISIONED && marker) {
      pick = CfgPick{CB_FAULT, -1, 0};
      return false;
    }
    return true;
  }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    if (!ready || slot > 1 || !out) return false;
    memcpy(out, slots[slot], CFG_SLOT_SIZE);
    return true;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    if (!ready || slot > 1 || !data) return false;
    memcpy(slots[slot], data, CFG_SLOT_SIZE);
    marker = true;
    return true;
  }
  bool readProvisionMarker(bool& present) override {
    if (!ready) return false;
    present = marker;
    return true;
  }
  void seed(const Config& c, uint32_t seq) {
    encodeConfig(c, seq, slots[0]);
    marker = true;
  }
  uint8_t slots[2][CFG_SLOT_SIZE];
  bool ready, beginOk, loadOk, marker;
};

Config goodConfig() {
  Config c = defaultConfig();
  c.termStart = 5;
  c.termEnd = 25;
  c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "config-guard-test");
  return c;
}

HealthInputs healthyInputs() {
  return HealthInputs{true, true, false, false, false, true, true, true, false, true};
}

uint32_t epochAt(int32_t day, uint16_t minute) {
  return dayStartEpoch(day, 480) + (uint32_t)minute * 60u;
}
}

void setUp() {}
void tearDown() {}

void test_boot_needs_setup_maps_only_to_unprovisioned_health() {
  FakeBootBackend backend;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  HealthFsm health;
  TEST_ASSERT_EQUAL_INT(H_UNPROVISIONED,
      updateHealthFromConfigBoot(health, boot, healthyInputs()));
  TEST_ASSERT_EQUAL_UINT8(HB_UNPROV, health.mask());
}

void test_boot_ready_maps_config_health_without_erasing_sensor_fault() {
  FakeBootBackend backend;
  backend.seed(goodConfig(), 9);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  HealthFsm health;
  HealthInputs inputs = healthyInputs();
  inputs.sensorOk = false;
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT,
      updateHealthFromConfigBoot(health, boot, inputs));
  TEST_ASSERT_EQUAL_UINT8(HB_SENSOR, health.mask());
}

void test_boot_fault_maps_to_config_fault_not_unprovisioned() {
  FakeBootBackend backend;
  backend.beginOk = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_BACKEND_BEGIN_FAILED, boot.start().code);
  HealthFsm health;
  TEST_ASSERT_EQUAL_INT(H_CONFIG_FAULT,
      updateHealthFromConfigBoot(health, boot, healthyInputs()));
  TEST_ASSERT_EQUAL_UINT8(HB_CONFIG, health.mask());
}

void test_cold_boot_state_fails_closed_as_config_fault() {
  FakeBootBackend backend;
  ConfigBootCoordinator boot(backend);
  HealthFsm health;
  TEST_ASSERT_EQUAL_INT(H_CONFIG_FAULT,
      updateHealthFromConfigBoot(health, boot, healthyInputs()));
  TEST_ASSERT_EQUAL_UINT8(HB_CONFIG, health.mask());
}

void test_window_derivation_uses_configured_scan_lead_boundary() {
  Config c = goodConfig();
  bool open = false;
  TEST_ASSERT_EQUAL_INT(CFG_WINDOW_OK,
      deriveConfigWindowOpen(c, epochAt(5, 464), true, open));
  TEST_ASSERT_FALSE(open);
  TEST_ASSERT_EQUAL_INT(CFG_WINDOW_OK,
      deriveConfigWindowOpen(c, epochAt(5, 465), true, open));
  TEST_ASSERT_TRUE(open);
}

void test_window_derivation_fails_closed_when_clock_is_untrusted() {
  Config c = goodConfig();
  bool open = false;
  TEST_ASSERT_EQUAL_INT(CFG_WINDOW_CLOCK_UNTRUSTED,
      deriveConfigWindowOpen(c, epochAt(5, 479), false, open));
  TEST_ASSERT_TRUE(open);
}

void test_window_derivation_fails_closed_for_invalid_config() {
  Config c = goodConfig();
  c.params.scanLeadMin = 61;
  bool open = false;
  TEST_ASSERT_EQUAL_INT(CFG_WINDOW_CONFIG_INVALID,
      deriveConfigWindowOpen(c, epochAt(5, 479), true, open));
  TEST_ASSERT_TRUE(open);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_needs_setup_maps_only_to_unprovisioned_health);
  RUN_TEST(test_boot_ready_maps_config_health_without_erasing_sensor_fault);
  RUN_TEST(test_boot_fault_maps_to_config_fault_not_unprovisioned);
  RUN_TEST(test_cold_boot_state_fails_closed_as_config_fault);
  RUN_TEST(test_window_derivation_uses_configured_scan_lead_boundary);
  RUN_TEST(test_window_derivation_fails_closed_when_clock_is_untrusted);
  RUN_TEST(test_window_derivation_fails_closed_for_invalid_config);
  return UNITY_END();
}
