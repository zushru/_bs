#include <unity.h>
#include <string.h>
#include "ap_credential_bootstrap.h"

using namespace att;

namespace {
class FakeStore : public IApCredentialStore {
 public:
  FakeStore() : readResult(APCR_NOT_FOUND), createResult(APCW_CREATED),
      reads(0), creates(0), stored{0}, persistDespiteCreateError(false),
      failReadAfterCreate(false), raceOnCreate(false), raceValue{0} {}

  ApCredentialStoreRead load(char out[AP_CREDENTIAL_BUFFER_SIZE]) override {
    ++reads;
    if (out == nullptr) return APCR_READ_ERROR;
    memset(out, 0, AP_CREDENTIAL_BUFFER_SIZE);
    if (failReadAfterCreate && creates > 0) return APCR_READ_ERROR;
    if (readResult == APCR_READ_OK) memcpy(out, stored, sizeof stored);
    return readResult;
  }

  ApCredentialStoreWrite createIfAbsent(const char* passphrase) override {
    ++creates;
    if (passphrase == nullptr) return APCW_WRITE_ERROR;
    if (raceOnCreate) {
      strncpy(stored, raceValue, sizeof stored - 1);
      readResult = APCR_READ_OK;
      return APCW_ALREADY_EXISTS;
    }
    if (readResult == APCR_READ_OK) return APCW_ALREADY_EXISTS;
    if (createResult == APCW_CREATED || persistDespiteCreateError) {
      strncpy(stored, passphrase, sizeof stored - 1);
      readResult = APCR_READ_OK;
    }
    return createResult;
  }

  void seed(const char* passphrase) {
    memset(stored, 0, sizeof stored);
    strncpy(stored, passphrase, sizeof stored - 1);
    readResult = APCR_READ_OK;
  }

  ApCredentialStoreRead readResult;
  ApCredentialStoreWrite createResult;
  int reads;
  int creates;
  char stored[AP_CREDENTIAL_BUFFER_SIZE];
  bool persistDespiteCreateError;
  bool failReadAfterCreate;
  bool raceOnCreate;
  char raceValue[AP_CREDENTIAL_BUFFER_SIZE];
};

class FakeRandom : public ISecureRandomSource {
 public:
  FakeRandom() : ok(true), calls(0), requested(0), next(0x10) {}
  bool fill(uint8_t* out, size_t length) override {
    ++calls;
    requested = length;
    if (!ok || out == nullptr) return false;
    for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(next + i);
    return true;
  }
  bool ok;
  int calls;
  size_t requested;
  uint8_t next;
};
}

void setUp() {}
void tearDown() {}

void test_cold_boot_refuses_without_touching_store() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE];
  ApCredentialResult r = manager.prepare(CFG_BOOT_COLD, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_INVALID_BOOT_STATE, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.reads);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_generates_128_bit_hex_secret_and_requests_disclosure() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_TRUE(r.disclose);
  TEST_ASSERT_EQUAL_UINT32(16, random.requested);
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(1, store.creates);
  TEST_ASSERT_EQUAL_INT(2, store.reads);
  TEST_ASSERT_EQUAL_STRING("101112131415161718191a1b1c1d1e1f", out);
  TEST_ASSERT_EQUAL_STRING(out, store.stored);
}

void test_setup_boot_reuses_secret_and_discloses_it_again_without_rng() {
  FakeStore store; store.seed("abcdef0123456789abcdef0123456789");
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_TRUE(r.disclose);
  TEST_ASSERT_EQUAL_STRING(store.stored, out);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
}

void test_setup_boot_fails_closed_on_store_read_error() {
  FakeStore store; store.readResult = APCR_READ_ERROR;
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_STORE_READ_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_fails_if_entropy_source_fails() {
  FakeStore store; FakeRandom random; random.ok = false;
  ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_RANDOM_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_fails_if_initial_store_write_fails() {
  FakeStore store; store.createResult = APCW_WRITE_ERROR;
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_STORE_WRITE_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_does_not_use_ambiguous_write_without_readback() {
  FakeStore store; store.failReadAfterCreate = true;
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_STORE_READ_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_fails_on_malformed_existing_secret_and_never_replaces_it() {
  FakeStore store; store.seed("short");
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_STORED_CREDENTIAL_INVALID, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
  TEST_ASSERT_EQUAL_STRING("short", store.stored);
}

void test_ready_boot_uses_matching_stored_credential_without_disclosure() {
  FakeStore store; store.seed("config-passphrase");
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "config-passphrase", out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_STRING("config-passphrase", out);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
}

void test_ready_boot_migrates_existing_config_secret_when_store_is_missing() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "legacy-pass-123", out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_STRING("legacy-pass-123", out);
  TEST_ASSERT_EQUAL_STRING("legacy-pass-123", store.stored);
  TEST_ASSERT_EQUAL_INT(1, store.creates);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
}

void test_ready_boot_migrates_after_restore_without_generating_new_password() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "restore-from-backup", out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_STRING("restore-from-backup", out);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
}

void test_ready_boot_rejects_mismatched_stored_credential() {
  FakeStore store; store.seed("old-config-pass");
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "new-config-pass", out);
  TEST_ASSERT_EQUAL_INT(APCB_CONFIG_MISMATCH, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_ready_boot_fails_closed_on_store_io_error_instead_of_migrating() {
  FakeStore store; store.readResult = APCR_READ_ERROR;
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "config-passphrase", out);
  TEST_ASSERT_EQUAL_INT(APCB_STORE_READ_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_ready_boot_refuses_invalid_config_passphrase_before_store_access() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "short", out);
  TEST_ASSERT_EQUAL_INT(APCB_CONFIG_CREDENTIAL_INVALID, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.reads);
  TEST_ASSERT_EQUAL_INT(0, store.creates);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_setup_boot_handles_create_race_by_reloading_existing_value() {
  FakeStore store; store.createResult = APCW_ALREADY_EXISTS;
  store.raceOnCreate = true;
  strncpy(store.raceValue, "concurrent-winner", sizeof store.raceValue - 1);
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_OK, r.code);
  TEST_ASSERT_TRUE(r.disclose);
  TEST_ASSERT_EQUAL_STRING("concurrent-winner", out);
  TEST_ASSERT_EQUAL_INT(1, random.calls);
}

void test_setup_boot_persisted_write_error_is_not_assumed_success() {
  FakeStore store; store.createResult = APCW_WRITE_ERROR;
  store.persistDespiteCreateError = true;
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, out);
  TEST_ASSERT_EQUAL_INT(APCB_STORE_WRITE_FAILED, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_ready_boot_fails_if_legacy_migration_races_with_a_different_value() {
  FakeStore store; store.raceOnCreate = true;
  strncpy(store.raceValue, "different-race-winner", sizeof store.raceValue - 1);
  FakeRandom random; ApCredentialBootstrap manager(store, random);
  char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  ApCredentialResult r = manager.prepare(CFG_BOOT_READY, "restore-from-backup", out);
  TEST_ASSERT_EQUAL_INT(APCB_CONFIG_MISMATCH, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, out[0]);
  TEST_ASSERT_EQUAL_STRING("different-race-winner", store.stored);
  TEST_ASSERT_EQUAL_INT(0, random.calls);
}

void test_null_output_refuses_without_store_access() {
  FakeStore store; FakeRandom random; ApCredentialBootstrap manager(store, random);
  ApCredentialResult r = manager.prepare(CFG_BOOT_NEEDS_SETUP, nullptr, nullptr);
  TEST_ASSERT_EQUAL_INT(APCB_INVALID_ARGUMENT, r.code);
  TEST_ASSERT_FALSE(r.disclose);
  TEST_ASSERT_EQUAL_INT(0, store.reads);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_cold_boot_refuses_without_touching_store);
  RUN_TEST(test_setup_boot_generates_128_bit_hex_secret_and_requests_disclosure);
  RUN_TEST(test_setup_boot_reuses_secret_and_discloses_it_again_without_rng);
  RUN_TEST(test_setup_boot_fails_closed_on_store_read_error);
  RUN_TEST(test_setup_boot_fails_if_entropy_source_fails);
  RUN_TEST(test_setup_boot_fails_if_initial_store_write_fails);
  RUN_TEST(test_setup_boot_does_not_use_ambiguous_write_without_readback);
  RUN_TEST(test_setup_boot_fails_on_malformed_existing_secret_and_never_replaces_it);
  RUN_TEST(test_ready_boot_uses_matching_stored_credential_without_disclosure);
  RUN_TEST(test_ready_boot_migrates_existing_config_secret_when_store_is_missing);
  RUN_TEST(test_ready_boot_migrates_after_restore_without_generating_new_password);
  RUN_TEST(test_ready_boot_rejects_mismatched_stored_credential);
  RUN_TEST(test_ready_boot_fails_closed_on_store_io_error_instead_of_migrating);
  RUN_TEST(test_ready_boot_refuses_invalid_config_passphrase_before_store_access);
  RUN_TEST(test_setup_boot_handles_create_race_by_reloading_existing_value);
  RUN_TEST(test_setup_boot_persisted_write_error_is_not_assumed_success);
  RUN_TEST(test_ready_boot_fails_if_legacy_migration_races_with_a_different_value);
  RUN_TEST(test_null_output_refuses_without_store_access);
  return UNITY_END();
}
