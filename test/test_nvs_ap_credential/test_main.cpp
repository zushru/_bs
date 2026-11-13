#include <unity.h>
#include <string.h>
#include <type_traits>
#include "nvs_ap_credential_store.h"
#include "esp32_nvs_startup_guard.h"
#include "config_boot.h"

using namespace att;

static_assert(!std::is_default_constructible<Esp32NvsApCredentialStore>::value,
              "AP credential NVS access must require validated config boot");

namespace {
char gStored[AP_CREDENTIAL_BUFFER_SIZE];
bool gPresent;
bool gOpenReadFail, gOpenWriteFail, gGetFail, gSetFail, gCommitFail;
bool gWrongType;
int gOpens, gGets, gSets, gCommits, gCloses;
Esp32NvsStartupStatus gStartupStatus;

void resetFake() {
  memset(gStored, 0, sizeof gStored); gPresent = false;
  gOpenReadFail = gOpenWriteFail = gGetFail = gSetFail = gCommitFail = false;
  gWrongType = false;
  gOpens = gGets = gSets = gCommits = gCloses = 0;
  gStartupStatus = Esp32NvsStartupStatus{true, 1, ESP_OK, false, true};
}

class FakeBootBackend : public IConfigBootBackend {
 public:
  FakeBootBackend() : failLoad(false) {}
  bool begin() override { return true; }
  bool load(Config&, CfgPick& pick) override {
    if (failLoad) return false;
    pick = CfgPick{CB_UNPROVISIONED, -1, 0};
    return true;
  }
  bool readSlot(uint8_t, uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool writeSlot(uint8_t, const uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool readProvisionMarker(bool&) override { return false; }
  bool failLoad;
};

struct BootFixture {
  FakeBootBackend backend;
  ConfigBootCoordinator coordinator;
  BootFixture() : backend(), coordinator(backend) { coordinator.start(); }
};
}

namespace att {
Esp32NvsStartupStatus Esp32NvsStartupGuard::status() { return gStartupStatus; }
}  // namespace att

extern "C" esp_err_t nvs_open(const char* name, nvs_open_mode_t mode,
                               nvs_handle_t* handle) {
  ++gOpens;
  if (name == nullptr || strcmp(name, "attwifi") != 0 || handle == nullptr)
    return ESP_ERR_INVALID_ARG;
  if (mode == NVS_READONLY && gOpenReadFail) return ESP_FAIL;
  if (mode == NVS_READWRITE && gOpenWriteFail) return ESP_FAIL;
  *handle = 1;
  return ESP_OK;
}
extern "C" esp_err_t nvs_get_str(nvs_handle_t handle, const char* key,
                                  char* out, size_t* length) {
  ++gGets;
  if (handle != 1 || key == nullptr || strcmp(key, "ap_pass_v1") != 0 || length == nullptr)
    return ESP_ERR_INVALID_ARG;
  if (gGetFail) return ESP_FAIL;
  if (gWrongType) return ESP_ERR_NVS_TYPE_MISMATCH;
  if (!gPresent) return ESP_ERR_NVS_NOT_FOUND;
  const size_t needed = strlen(gStored) + 1;
  if (out == nullptr) { *length = needed; return ESP_OK; }
  if (*length < needed) { *length = needed; return ESP_ERR_NVS_INVALID_LENGTH; }
  memcpy(out, gStored, needed); *length = needed;
  return ESP_OK;
}
extern "C" esp_err_t nvs_set_str(nvs_handle_t handle, const char* key,
                                  const char* value) {
  ++gSets;
  if (handle != 1 || key == nullptr || strcmp(key, "ap_pass_v1") != 0 || value == nullptr)
    return ESP_ERR_INVALID_ARG;
  if (gSetFail) return ESP_FAIL;
  if (strlen(value) >= sizeof gStored) return ESP_ERR_NVS_INVALID_LENGTH;
  strcpy(gStored, value); gPresent = true;
  return ESP_OK;
}
extern "C" esp_err_t nvs_commit(nvs_handle_t handle) {
  ++gCommits;
  if (handle != 1) return ESP_ERR_INVALID_ARG;
  return gCommitFail ? ESP_FAIL : ESP_OK;
}
extern "C" void nvs_close(nvs_handle_t handle) { if (handle == 1) ++gCloses; }

void setUp() { resetFake(); }
void tearDown() {}

void test_load_missing_namespace_or_key_returns_not_found() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE];
  strcpy(out, "dirty");
  TEST_ASSERT_EQUAL_INT(APCR_NOT_FOUND, store.load(out));
  TEST_ASSERT_EQUAL_INT(0, out[0]);
  TEST_ASSERT_EQUAL_INT(ESP_OK, store.lastError());
}

void test_unsafe_startup_snapshot_blocks_ap_credential_nvs_access() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator);
  gStartupStatus = Esp32NvsStartupStatus{true, 2, ESP_OK, false, false};

  char out[AP_CREDENTIAL_BUFFER_SIZE];
  memset(out, 'x', sizeof out);
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  for (size_t i = 0; i < sizeof out; ++i) TEST_ASSERT_EQUAL_UINT8(0, out[i]);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);

  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR,
                        store.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);
  TEST_ASSERT_EQUAL_INT(0, gSets);
  TEST_ASSERT_EQUAL_INT(0, gCommits);
}

void test_create_and_load_round_trip_32_character_password() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  const char* pass = "0123456789abcdef0123456789abcdef";
  TEST_ASSERT_EQUAL_INT(APCW_CREATED, store.createIfAbsent(pass));
  TEST_ASSERT_EQUAL_INT(1, gSets);
  TEST_ASSERT_EQUAL_INT(1, gCommits);
  TEST_ASSERT_EQUAL_INT(APCR_READ_OK, store.load(out));
  TEST_ASSERT_EQUAL_STRING(pass, out);
}

void test_create_does_not_overwrite_existing_passphrase() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  strcpy(gStored, "existing-password"); gPresent = true;
  TEST_ASSERT_EQUAL_INT(APCW_ALREADY_EXISTS,
                        store.createIfAbsent("replacement-password"));
  TEST_ASSERT_EQUAL_INT(0, gSets);
  TEST_ASSERT_EQUAL_INT(0, gCommits);
  TEST_ASSERT_EQUAL_INT(APCR_READ_OK, store.load(out));
  TEST_ASSERT_EQUAL_STRING("existing-password", out);
}

void test_invalid_candidate_is_rejected_before_opening_nvs() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator);
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR, store.createIfAbsent("short"));
  TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, store.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);
}

void test_open_read_failure_is_an_error_not_not_found() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  gOpenReadFail = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_get_string_failure_is_not_treated_as_missing() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  strcpy(gStored, "existing-password"); gPresent = true; gGetFail = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_wrong_nvs_type_fails_closed() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  gWrongType = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_TYPE_MISMATCH, store.lastError());
}

void test_too_short_or_malformed_nvs_string_fails_closed() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  strcpy(gStored, "short"); gPresent = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_INVALID_LENGTH, store.lastError());
  TEST_ASSERT_EQUAL_INT(0, out[0]);
}

void test_non_printable_existing_string_fails_closed() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  memcpy(gStored, "bad\x01value", 10); gStored[10] = 0; gPresent = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_INVALID_LENGTH, store.lastError());
}

void test_set_failure_does_not_commit_or_claim_creation() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); gSetFail = true;
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR, store.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(0, gCommits);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_commit_failure_does_not_claim_creation() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); gCommitFail = true;
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR, store.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(1, gSets);
  TEST_ASSERT_EQUAL_INT(1, gCommits);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_corrupt_existing_value_is_never_overwritten() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator);
  strcpy(gStored, "tiny"); gPresent = true;
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR, store.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(0, gSets);
  TEST_ASSERT_EQUAL_INT(0, gCommits);
  TEST_ASSERT_EQUAL_STRING("tiny", gStored);
}

void test_write_open_failure_does_not_attempt_set_or_commit() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); gOpenWriteFail = true;
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR, store.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_INT(1, gOpens);
  TEST_ASSERT_EQUAL_INT(0, gSets);
  TEST_ASSERT_EQUAL_INT(0, gCommits);
}

void test_maximum_63_character_passphrase_round_trips() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  char pass[64];
  memset(pass, 'z', 63); pass[63] = '\0';
  TEST_ASSERT_EQUAL_INT(APCW_CREATED, store.createIfAbsent(pass));
  TEST_ASSERT_EQUAL_INT(APCR_READ_OK, store.load(out));
  TEST_ASSERT_EQUAL_STRING(pass, out);
}

void test_read_clears_output_on_any_error() {
  BootFixture boot;
  Esp32NvsApCredentialStore store(boot.coordinator); char out[AP_CREDENTIAL_BUFFER_SIZE];
  memset(out, 'x', sizeof out); gGetFail = true;
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, store.load(out));
  for (size_t i = 0; i < sizeof out; ++i) TEST_ASSERT_EQUAL_UINT8(0, out[i]);
}


void test_cold_or_faulted_config_boot_blocks_ap_credential_nvs() {
  FakeBootBackend coldBackend;
  ConfigBootCoordinator coldBoot(coldBackend);
  Esp32NvsApCredentialStore coldStore(coldBoot);
  char out[AP_CREDENTIAL_BUFFER_SIZE];
  memset(out, 'x', sizeof out);
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, coldStore.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, coldStore.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);
  TEST_ASSERT_EQUAL_INT(APCW_WRITE_ERROR,
                        coldStore.createIfAbsent("new-password-1"));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, coldStore.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);

  FakeBootBackend failedBackend;
  failedBackend.failLoad = true;
  ConfigBootCoordinator failedBoot(failedBackend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_LOAD_FAILED, failedBoot.start().code);
  Esp32NvsApCredentialStore failedStore(failedBoot);
  TEST_ASSERT_EQUAL_INT(APCR_READ_ERROR, failedStore.load(out));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, failedStore.lastError());
  TEST_ASSERT_EQUAL_INT(0, gOpens);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_load_missing_namespace_or_key_returns_not_found);
  RUN_TEST(test_unsafe_startup_snapshot_blocks_ap_credential_nvs_access);
  RUN_TEST(test_cold_or_faulted_config_boot_blocks_ap_credential_nvs);
  RUN_TEST(test_create_and_load_round_trip_32_character_password);
  RUN_TEST(test_create_does_not_overwrite_existing_passphrase);
  RUN_TEST(test_invalid_candidate_is_rejected_before_opening_nvs);
  RUN_TEST(test_open_read_failure_is_an_error_not_not_found);
  RUN_TEST(test_get_string_failure_is_not_treated_as_missing);
  RUN_TEST(test_wrong_nvs_type_fails_closed);
  RUN_TEST(test_too_short_or_malformed_nvs_string_fails_closed);
  RUN_TEST(test_non_printable_existing_string_fails_closed);
  RUN_TEST(test_set_failure_does_not_commit_or_claim_creation);
  RUN_TEST(test_commit_failure_does_not_claim_creation);
  RUN_TEST(test_corrupt_existing_value_is_never_overwritten);
  RUN_TEST(test_write_open_failure_does_not_attempt_set_or_commit);
  RUN_TEST(test_maximum_63_character_passphrase_round_trips);
  RUN_TEST(test_read_clears_output_on_any_error);
  return UNITY_END();
}
