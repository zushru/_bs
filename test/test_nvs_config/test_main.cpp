#include <unity.h>
#include <string.h>
#include <type_traits>
#include "nvs_config_backend.h"
#include "nvs_pin_attempt_store.h"
#include "esp32_pin_auth_runtime.h"
#include "esp32_nvs_startup_guard.h"

// Keep this fake API aligned with the pinned ESP-IDF 5.5 error-code values.
static_assert(ESP_ERR_NVS_NOT_INITIALIZED == 0x1101, "NVS NOT_INITIALIZED code drift");
static_assert(ESP_ERR_NVS_NOT_FOUND == 0x1102, "NVS NOT_FOUND code drift");
static_assert(ESP_ERR_NVS_TYPE_MISMATCH == 0x1103, "NVS TYPE_MISMATCH code drift");
static_assert(ESP_ERR_NVS_NOT_ENOUGH_SPACE == 0x1105, "NVS NOT_ENOUGH_SPACE code drift");
static_assert(ESP_ERR_NVS_INVALID_LENGTH == 0x110C, "NVS INVALID_LENGTH code drift");
static_assert(ESP_ERR_NVS_NO_FREE_PAGES == 0x110D, "NVS NO_FREE_PAGES code drift");
static_assert(ESP_ERR_NVS_NEW_VERSION_FOUND == 0x1110, "NVS NEW_VERSION_FOUND code drift");
static_assert(!std::is_copy_constructible<att::Esp32NvsPinAttemptStore>::value, "NVS attempt store must remain single-owner");
static_assert(!std::is_constructible<att::Esp32NvsPinAttemptStore, const char*>::value,
              "PIN-attempt NVS access must require validated config boot");
static_assert(std::is_constructible<att::Esp32NvsPinAttemptStore, const char*,
                                    const att::ConfigBootCoordinator&>::value,
              "PIN-attempt store must bind to one config boot coordinator");
static_assert(!std::is_copy_constructible<att::Esp32PinAuthRuntime>::value, "PIN auth runtime must remain single-owner");

using namespace att;

namespace {
struct FakeBlob {
  bool present;
  size_t length;
  uint8_t bytes[CFG_SLOT_SIZE + 8];
};
FakeBlob blobs[3];
Esp32NvsStartupStatus fakeStartupStatus;
struct FakePinNamespace {
  char name[16];
  bool exists;
  FakeBlob blob;  // Used to model a wrong-type value under pin_fails.
  bool u16Present;
  uint16_t failures;
  bool pending;
  uint16_t pendingFailures;
};
FakePinNamespace pinNamespaces[2];
bool initialized, namespaceExists, persistOnCommitError;
bool pendingValid[3];
uint8_t pendingBytes[3][CFG_SLOT_SIZE];
size_t pendingLength[3];
uint32_t initCalls, openCalls, getCalls, setCalls, commitCalls, closeCalls;
uint32_t getFailOnCall;
esp_err_t initResult, openResult, getResult, setResult, commitResult;
nvs_open_mode_t lastOpenMode;

int slotFor(const char* key) {
  if (strcmp(key, "slot_a") == 0) return 0;
  if (strcmp(key, "slot_b") == 0) return 1;
  if (strcmp(key, "provisioned") == 0) return 2;
  return -1;
}
int pinNamespaceFor(const char* name) {
  if (!name) return -1;
  if (strcmp(name, "attsec") == 0) return 0;
  if (strcmp(name, "attsetup") == 0) return 1;
  return -1;
}
void fakeSeedPin(uint8_t domain, uint16_t failures) {
  if (domain >= 2) return;
  pinNamespaces[domain].exists = true;
  pinNamespaces[domain].u16Present = true;
  pinNamespaces[domain].failures = failures;
}
void fakeSeedPinBlob(uint8_t domain, const uint8_t* data, size_t n) {
  if (domain >= 2) return;
  pinNamespaces[domain].exists = true;
  pinNamespaces[domain].blob.present = true;
  pinNamespaces[domain].blob.length = n;
  if (n <= sizeof pinNamespaces[domain].blob.bytes)
    memcpy(pinNamespaces[domain].blob.bytes, data, n);
}
void fakeReset() {
  memset(blobs, 0, sizeof blobs);
  memset(pinNamespaces, 0, sizeof pinNamespaces);
  strcpy(pinNamespaces[0].name, "attsec");
  strcpy(pinNamespaces[1].name, "attsetup");
  initialized = true;  // Model Arduino's successful pre-setup NVS initialization.
  namespaceExists = persistOnCommitError = false;
  fakeStartupStatus = Esp32NvsStartupStatus{true, 1, ESP_OK, false, true};
  memset(pendingValid, 0, sizeof pendingValid);
  memset(pendingLength, 0, sizeof pendingLength);
  memset(pendingBytes, 0, sizeof pendingBytes);
  initCalls = openCalls = getCalls = setCalls = commitCalls = closeCalls = 0;
  getFailOnCall = 0;
  initResult = openResult = getResult = setResult = commitResult = ESP_OK;
  lastOpenMode = NVS_READONLY;
}
void fakeSeed(uint8_t slot, const uint8_t* data, size_t n) {
  namespaceExists = true;
  blobs[slot].present = true;
  blobs[slot].length = n;
  if (n <= sizeof blobs[slot].bytes) memcpy(blobs[slot].bytes, data, n);
}
Config goodConfig() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}
class FakeConfigBootBackend : public IConfigBootBackend {
 public:
  explicit FakeConfigBootBackend(bool unprovisioned)
      : unprovisioned_(unprovisioned), beginOk_(true), loadOk_(true) {}
  bool begin() override { return beginOk_; }
  bool load(Config& out, CfgPick& pick) override {
    if (!loadOk_) return false;
    if (unprovisioned_) {
      pick = CfgPick{CB_UNPROVISIONED, -1, 0};
      return true;
    }
    out = goodConfig();
    pick = CfgPick{CB_OK, 0, 1};
    return validateConfig(out);
  }
  bool readSlot(uint8_t, uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool writeSlot(uint8_t, const uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool readProvisionMarker(bool&) override { return false; }
  void failBegin() { beginOk_ = false; }
  void failLoad() { loadOk_ = false; }
 private:
  bool unprovisioned_;
  bool beginOk_;
  bool loadOk_;
};

struct BootFixture {
  FakeConfigBootBackend backend;
  ConfigBootCoordinator coordinator;
  explicit BootFixture(bool unprovisioned = true)
      : backend(unprovisioned), coordinator(backend) { coordinator.start(); }
};

class FakeRuntime : public IConfigApplyRuntime {
 public:
  FakeRuntime() : can_(true), applyCalls_(0) {}
  bool canApply(const Config&) const override { return can_; }
  bool applyAfterCommit(const Config& c) override {
    applied_ = c; ++applyCalls_; return true;
  }
  void can(bool v) { can_ = v; }
  uint32_t applyCalls() const { return applyCalls_; }
  const Config& applied() const { return applied_; }
 private:
  bool can_;
  uint32_t applyCalls_;
  Config applied_;
};
ConfigGateIn gate(uint32_t seq) {
  ConfigGateIn g;
  g.h = H_OK; g.expectGen = seq; g.curGen = 0xDEADBEEFu;
  g.windowOpen = false; g.unclosedPast = false;
  return g;
}
}

namespace att {
Esp32NvsStartupStatus Esp32NvsStartupGuard::status() {
  return fakeStartupStatus;
}
}  // namespace att

extern "C" esp_err_t nvs_flash_init(void) {
  ++initCalls;
  if (initResult == ESP_OK) initialized = true;
  return initResult;
}
extern "C" esp_err_t nvs_open(const char* name, nvs_open_mode_t mode,
                               nvs_handle_t* handle) {
  ++openCalls;
  lastOpenMode = mode;
  if (!initialized) return ESP_ERR_NVS_NOT_INITIALIZED;
  if (!name || !handle) return ESP_ERR_INVALID_ARG;
  if (openResult != ESP_OK) return openResult;
  if (strcmp(name, "attcfg") == 0) {
    if (mode == NVS_READONLY && !namespaceExists) return ESP_ERR_NVS_NOT_FOUND;
    if (mode == NVS_READWRITE) namespaceExists = true;
    *handle = 1;
    return ESP_OK;
  }
  const int domain = pinNamespaceFor(name);
  if (domain < 0) return ESP_ERR_INVALID_ARG;
  if (mode == NVS_READONLY && !pinNamespaces[domain].exists)
    return ESP_ERR_NVS_NOT_FOUND;
  if (mode == NVS_READWRITE) pinNamespaces[domain].exists = true;
  *handle = static_cast<nvs_handle_t>(2 + domain);
  return ESP_OK;
}
extern "C" esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key,
                                   void* out, size_t* length) {
  ++getCalls;
  if (!length) return ESP_ERR_INVALID_ARG;
  if (getResult != ESP_OK) return getResult;
  if (getFailOnCall != 0 && getCalls == getFailOnCall) return ESP_FAIL;
  if (handle == 1) {
    const int slot = key ? slotFor(key) : -1;
    if (slot < 0 || !blobs[slot].present) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) { *length = blobs[slot].length; return ESP_OK; }
    if (*length < blobs[slot].length) {
      *length = blobs[slot].length;
      return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out, blobs[slot].bytes, blobs[slot].length);
    *length = blobs[slot].length;
    return ESP_OK;
  }
  if (handle == 2 || handle == 3) {
    const uint8_t domain = static_cast<uint8_t>(handle - 2);
    FakeBlob& blob = pinNamespaces[domain].blob;
    if (!key || strcmp(key, "pin_fails") != 0 || !blob.present)
      return ESP_ERR_NVS_NOT_FOUND;
    if (!out) { *length = blob.length; return ESP_OK; }
    if (*length < blob.length) {
      *length = blob.length;
      return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out, blob.bytes, blob.length);
    *length = blob.length;
    return ESP_OK;
  }
  return ESP_ERR_INVALID_ARG;
}
extern "C" esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key,
                                  uint16_t* out) {
  ++getCalls;
  if (!out) return ESP_ERR_INVALID_ARG;
  if (getResult != ESP_OK) return getResult;
  if (getFailOnCall != 0 && getCalls == getFailOnCall) return ESP_FAIL;
  if ((handle != 2 && handle != 3) || !key || strcmp(key, "pin_fails") != 0)
    return ESP_ERR_INVALID_ARG;
  FakePinNamespace& space = pinNamespaces[handle - 2];
  if (space.u16Present) {
    *out = space.failures;
    return ESP_OK;
  }
  if (space.blob.present) return ESP_ERR_NVS_TYPE_MISMATCH;
  return ESP_ERR_NVS_NOT_FOUND;
}
extern "C" esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key,
                                  uint16_t value) {
  ++setCalls;
  if (setResult != ESP_OK) return setResult;
  if ((handle != 2 && handle != 3) || !key || strcmp(key, "pin_fails") != 0)
    return ESP_ERR_INVALID_ARG;
  FakePinNamespace& space = pinNamespaces[handle - 2];
  space.pendingFailures = value;
  space.pending = true;
  return ESP_OK;
}
extern "C" esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key,
                                   const void* data, size_t length) {
  ++setCalls;
  if (!data) return ESP_ERR_INVALID_ARG;
  if (setResult != ESP_OK) return setResult;
  if (handle == 1) {
    const int slot = key ? slotFor(key) : -1;
    if (slot < 0 || (slot < 2 && length != CFG_SLOT_SIZE) || (slot == 2 && length != 4))
      return ESP_ERR_INVALID_ARG;
    if (length > sizeof pendingBytes[slot]) return ESP_ERR_INVALID_ARG;
    memcpy(pendingBytes[slot], data, length);
    pendingLength[slot] = length;
    pendingValid[slot] = true;
    return ESP_OK;
  }
  return ESP_ERR_INVALID_ARG;
}
extern "C" esp_err_t nvs_commit(nvs_handle_t handle) {
  ++commitCalls;
  if (handle == 1) {
    if (commitResult == ESP_OK || persistOnCommitError) {
      for (uint8_t i = 0; i < 3; ++i)
        if (pendingValid[i]) fakeSeed(i, pendingBytes[i], pendingLength[i]);
    }
    memset(pendingValid, 0, sizeof pendingValid);
    return commitResult;
  }
  if (handle == 2 || handle == 3) {
    FakePinNamespace& space = pinNamespaces[handle - 2];
    if ((commitResult == ESP_OK || persistOnCommitError) && space.pending)
      fakeSeedPin(static_cast<uint8_t>(handle - 2), space.pendingFailures);
    space.pending = false;
    return commitResult;
  }
  return ESP_ERR_INVALID_ARG;
}
extern "C" void nvs_close(nvs_handle_t handle) {
  if (handle >= 1 && handle <= 3) ++closeCalls;
}

bool initNvsForTest() {
  Esp32NvsConfigBackend configBackend;
  return configBackend.begin();
}
void seedPinCount(const char* name, uint16_t count) {
  const int domain = pinNamespaceFor(name);
  if (domain < 0) return;
  fakeSeedPin(static_cast<uint8_t>(domain), count);
}

void setUp() { fakeReset(); }
void tearDown() {}

void test_begin_rejects_no_free_pages_without_opening_storage() {
  Esp32NvsConfigBackend backend;
  initialized = false;
  fakeStartupStatus.firstInitResult = ESP_ERR_NVS_NO_FREE_PAGES;
  fakeStartupStatus.wholePartitionEraseBlocked = true;
  fakeStartupStatus.safeForApplication = false;
  TEST_ASSERT_FALSE(backend.begin());
  TEST_ASSERT_FALSE(backend.ready());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NO_FREE_PAGES, backend.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, initCalls);
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

void test_begin_rejects_new_nvs_version_without_destructive_recovery() {
  Esp32NvsConfigBackend backend;
  initialized = false;
  fakeStartupStatus.firstInitResult = ESP_ERR_NVS_NEW_VERSION_FOUND;
  fakeStartupStatus.wholePartitionEraseBlocked = true;
  fakeStartupStatus.safeForApplication = false;
  TEST_ASSERT_FALSE(backend.begin());
  TEST_ASSERT_FALSE(backend.ready());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NEW_VERSION_FOUND, backend.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, initCalls);
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

void test_begin_rejects_ambiguous_arduino_startup_without_retrying_nvs_init() {
  Esp32NvsConfigBackend backend;
  fakeStartupStatus.initCallCount = 2;
  fakeStartupStatus.safeForApplication = false;
  TEST_ASSERT_FALSE(backend.begin());
  TEST_ASSERT_FALSE(backend.ready());
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backend.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, initCalls);
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

void test_begin_uses_arduino_initialized_nvs_without_second_init_call() {
  Esp32NvsConfigBackend backend;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.ready());
  TEST_ASSERT_EQUAL_INT(ESP_OK, backend.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, initCalls);
}

void test_load_before_begin_is_config_fault() {
  Esp32NvsConfigBackend backend;
  Config out = goodConfig();
  CfgPick pick = CfgPick{CB_OK, 0, 9};
  TEST_ASSERT_FALSE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
  TEST_ASSERT_EQUAL_INT(-1, pick.active);
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

void test_missing_namespace_loads_unprovisioned_not_default_config() {
  Esp32NvsConfigBackend backend;
  Config out = goodConfig();
  out.minScore = 4321;
  CfgPick pick = CfgPick{CB_FAULT, -1, 0};
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_UNPROVISIONED, pick.boot);
  TEST_ASSERT_EQUAL_INT(-1, pick.active);
  TEST_ASSERT_EQUAL_UINT16(4321, out.minScore);
}

void test_missing_key_returns_exact_blank_slot() {
  Esp32NvsConfigBackend backend;
  uint8_t out[CFG_SLOT_SIZE];
  memset(out, 0, sizeof out);
  TEST_ASSERT_TRUE(backend.begin());
  namespaceExists = true;
  TEST_ASSERT_TRUE(backend.readSlot(1, out));
  for (size_t i = 0; i < sizeof out; ++i) TEST_ASSERT_EQUAL_UINT8(0xFF, out[i]);
}

void test_load_selects_highest_valid_config_generation() {
  Esp32NvsConfigBackend backend;
  Config a = goodConfig(), b = goodConfig(), out = defaultConfig();
  uint8_t rawA[CFG_SLOT_SIZE], rawB[CFG_SLOT_SIZE];
  b.soundMode = 1;
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(a, 7, rawA));
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(b, 8, rawB));
  fakeSeed(0, rawA, sizeof rawA); fakeSeed(1, rawB, sizeof rawB);
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_OK, pick.boot);
  TEST_ASSERT_EQUAL_INT(1, pick.active);
  TEST_ASSERT_EQUAL_UINT32(8, pick.seq);
  TEST_ASSERT_TRUE(configEqual(b, out));
}

void test_malformed_blob_length_is_not_treated_as_blank() {
  Esp32NvsConfigBackend backend;
  uint8_t shortBlob[CFG_SLOT_SIZE - 1] = {0};
  uint8_t out[CFG_SLOT_SIZE];
  memset(out, 0x5A, sizeof out);
  fakeSeed(0, shortBlob, sizeof shortBlob);
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_FALSE(backend.readSlot(0, out));
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_INVALID_LENGTH, backend.lastError());
  for (size_t i = 0; i < sizeof out; ++i) TEST_ASSERT_EQUAL_UINT8(0x5A, out[i]);
}

void test_nvs_read_error_does_not_overwrite_output() {
  Esp32NvsConfigBackend backend;
  uint8_t raw[CFG_SLOT_SIZE], out[CFG_SLOT_SIZE];
  Config c = goodConfig();
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 1, raw));
  fakeSeed(0, raw, sizeof raw);
  memset(out, 0xA7, sizeof out);
  TEST_ASSERT_TRUE(backend.begin());
  getResult = ESP_FAIL;
  TEST_ASSERT_FALSE(backend.readSlot(0, out));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backend.lastError());
  for (size_t i = 0; i < sizeof out; ++i) TEST_ASSERT_EQUAL_UINT8(0xA7, out[i]);
  TEST_ASSERT_EQUAL_UINT32(1, closeCalls);
}

void test_read_provision_marker_distinguishes_absent_from_valid() {
  Esp32NvsConfigBackend backend;
  bool present = true;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.readProvisionMarker(present));
  TEST_ASSERT_FALSE(present);

  const uint8_t marker[4] = {'A', 'T', 'T', 'P'};
  fakeSeed(2, marker, sizeof marker);
  TEST_ASSERT_TRUE(backend.readProvisionMarker(present));
  TEST_ASSERT_TRUE(present);
  TEST_ASSERT_EQUAL_INT(NVS_READONLY, lastOpenMode);
}

void test_read_provision_marker_rejects_corrupt_marker_and_nvs_error() {
  Esp32NvsConfigBackend backend;
  bool present = true;
  const uint8_t corrupt[4] = {'B', 'A', 'D', '!'};
  fakeSeed(2, corrupt, sizeof corrupt);
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_FALSE(backend.readProvisionMarker(present));
  TEST_ASSERT_FALSE(present);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backend.lastError());

  fakeReset();
  Esp32NvsConfigBackend backendWithReadError;
  TEST_ASSERT_TRUE(backendWithReadError.begin());
  namespaceExists = true;
  getResult = ESP_FAIL;
  present = true;
  TEST_ASSERT_FALSE(backendWithReadError.readProvisionMarker(present));
  TEST_ASSERT_FALSE(present);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backendWithReadError.lastError());
}

void test_write_commits_before_return_and_round_trips() {
  Esp32NvsConfigBackend backend;
  uint8_t raw[CFG_SLOT_SIZE], out[CFG_SLOT_SIZE];
  Config c = goodConfig();
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 3, raw));
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.writeSlot(1, raw));
  TEST_ASSERT_EQUAL_INT(NVS_READWRITE, lastOpenMode);
  TEST_ASSERT_EQUAL_UINT32(2, setCalls);  // slot plus first-provision marker
  TEST_ASSERT_EQUAL_UINT32(1, commitCalls);
  TEST_ASSERT_EQUAL_UINT32(1, closeCalls);
  TEST_ASSERT_TRUE(blobs[2].present);
  TEST_ASSERT_TRUE(backend.readSlot(1, out));
  TEST_ASSERT_EQUAL_MEMORY(raw, out, CFG_SLOT_SIZE);
}

void test_set_failure_does_not_commit_and_still_closes() {
  Esp32NvsConfigBackend backend;
  uint8_t raw[CFG_SLOT_SIZE] = {0};
  TEST_ASSERT_TRUE(backend.begin());
  setResult = ESP_FAIL;
  TEST_ASSERT_FALSE(backend.writeSlot(0, raw));
  TEST_ASSERT_EQUAL_UINT32(1, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
  TEST_ASSERT_EQUAL_UINT32(1, closeCalls);
}

void test_commit_error_is_indeterminate_even_if_blob_survives_reboot() {
  Esp32NvsConfigBackend backend;
  uint8_t raw[CFG_SLOT_SIZE], out[CFG_SLOT_SIZE];
  Config c = goodConfig();
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 4, raw));
  TEST_ASSERT_TRUE(backend.begin());
  persistOnCommitError = true;
  commitResult = ESP_FAIL;
  TEST_ASSERT_FALSE(backend.writeSlot(1, raw));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backend.lastError());
  TEST_ASSERT_TRUE(backend.readSlot(1, out));
  TEST_ASSERT_EQUAL_MEMORY(raw, out, CFG_SLOT_SIZE);
}


void test_provision_marker_prevents_silent_reprovision_when_both_slots_missing() {
  Esp32NvsConfigBackend backend;
  const uint8_t marker[4] = {'A', 'T', 'T', 'P'};
  fakeSeed(2, marker, sizeof marker);
  Config out = goodConfig();
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_FALSE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
}

void test_corrupt_provision_marker_is_not_treated_as_unprovisioned() {
  Esp32NvsConfigBackend backend;
  const uint8_t marker[4] = {'B', 'A', 'D', '!'};
  fakeSeed(2, marker, sizeof marker);
  Config out = goodConfig();
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_FALSE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
}


void test_load_migrates_legacy_valid_slot_by_committing_marker() {
  Esp32NvsConfigBackend backend;
  Config c = goodConfig(), out = defaultConfig();
  uint8_t raw[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 5, raw));
  fakeSeed(0, raw, sizeof raw);  // v0.2.12 slot: no provision marker yet.
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_OK, pick.boot);
  TEST_ASSERT_TRUE(blobs[2].present);
  TEST_ASSERT_EQUAL_UINT32(1, setCalls);
  TEST_ASSERT_EQUAL_UINT32(1, commitCalls);
  TEST_ASSERT_TRUE(configEqual(c, out));
}

void test_legacy_marker_migration_commit_failure_blocks_boot() {
  Esp32NvsConfigBackend backend;
  Config c = goodConfig(), out = defaultConfig();
  uint8_t raw[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 5, raw));
  fakeSeed(0, raw, sizeof raw);
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  commitResult = ESP_FAIL;
  TEST_ASSERT_FALSE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, backend.lastError());
}

void test_corrupt_marker_faults_even_when_one_config_slot_is_valid() {
  Esp32NvsConfigBackend backend;
  Config c = goodConfig(), out = defaultConfig();
  uint8_t raw[CFG_SLOT_SIZE];
  const uint8_t badMarker[4] = {'B', 'A', 'D', '!'};
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(c, 5, raw));
  fakeSeed(0, raw, sizeof raw);
  fakeSeed(2, badMarker, sizeof badMarker);
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_FALSE(backend.load(out, pick));
  TEST_ASSERT_EQUAL_INT(CB_FAULT, pick.boot);
}

void test_invalid_slot_is_rejected_before_nvs_open() {
  Esp32NvsConfigBackend backend;
  uint8_t raw[CFG_SLOT_SIZE] = {0};
  TEST_ASSERT_TRUE(backend.begin());
  const uint32_t before = openCalls;
  TEST_ASSERT_FALSE(backend.writeSlot(2, raw));
  TEST_ASSERT_EQUAL_UINT32(before, openCalls);
}

void test_config_coordinator_uses_nvs_inactive_slot_and_reloads_generation() {
  Esp32NvsConfigBackend backend;
  Config current = goodConfig(), loaded = defaultConfig(), next = goodConfig();
  uint8_t raw[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(186, encodeConfig(current, 12, raw));
  fakeSeed(0, raw, sizeof raw);
  CfgPick pick;
  TEST_ASSERT_TRUE(backend.begin());
  TEST_ASSERT_TRUE(backend.load(loaded, pick));
  FakeRuntime runtime;
  ConfigApplyCoordinator coordinator(backend, runtime, loaded, pick);
  next.soundMode = 2;
  ConfigApplyResult result = coordinator.apply(next, gate(12));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_OK, result.code);
  TEST_ASSERT_EQUAL_UINT32(13, result.generation);
  TEST_ASSERT_EQUAL_INT(1, coordinator.activeSlot());
  TEST_ASSERT_EQUAL_UINT32(1, runtime.applyCalls());
  Config rebooted = defaultConfig();
  CfgPick rebootPick;
  TEST_ASSERT_TRUE(backend.load(rebooted, rebootPick));
  TEST_ASSERT_EQUAL_INT(CB_OK, rebootPick.boot);
  TEST_ASSERT_EQUAL_INT(1, rebootPick.active);
  TEST_ASSERT_EQUAL_UINT32(13, rebootPick.seq);
  TEST_ASSERT_TRUE(configEqual(next, rebooted));
}

void test_pin_attempt_store_load_before_nvs_init_fails_closed() {
  BootFixture boot;
  initialized = false;  // This case explicitly models an uninitialized NVS driver.
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  uint16_t failures = 765;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(765, failures);
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_INITIALIZED, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(1, openCalls);
}

void test_pin_attempt_store_rejects_unsafe_startup_before_any_nvs_access() {
  BootFixture boot;
  fakeStartupStatus = Esp32NvsStartupStatus{true, 2, ESP_OK, false, false};
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  uint16_t failures = 765;

  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(765, failures);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);

  TEST_ASSERT_FALSE(store.initializeFailureCount());
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);

  TEST_ASSERT_FALSE(store.persistFailureCount(7));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_attempt_store_open_error_fails_closed_without_output_change() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  uint16_t failures = 876;
  openResult = ESP_FAIL;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(876, failures);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, closeCalls);
}

void test_pin_attempt_store_missing_record_fails_closed_without_mutating_output() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  uint16_t failures = 4321;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(4321, failures);
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_FOUND, store.lastError());
}

void test_pin_attempt_store_explicit_initialization_commits_zero_and_reads_back() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_TRUE(store.initializeFailureCount());
  uint16_t failures = 99;
  TEST_ASSERT_TRUE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(0, failures);
  TEST_ASSERT_EQUAL_UINT32(1, commitCalls);
}

void test_pin_attempt_store_initialization_never_overwrites_existing_counter() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  seedPinCount("attsec", 4);
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_FALSE(store.initializeFailureCount());
  uint16_t failures = 0;
  TEST_ASSERT_TRUE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(4, failures);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_attempt_store_initialization_does_not_replace_wrong_type_value() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  const uint8_t wrongType[2] = {0x34, 0x12};
  fakeSeedPinBlob(0, wrongType, sizeof wrongType);
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_FALSE(store.initializeFailureCount());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_TYPE_MISMATCH, store.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_attempt_store_persist_commits_then_reopens_and_verifies() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_TRUE(store.initializeFailureCount());
  TEST_ASSERT_TRUE(store.persistFailureCount(7));
  uint16_t failures = 0;
  TEST_ASSERT_TRUE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(7, failures);
  TEST_ASSERT_EQUAL_UINT32(2, commitCalls);
  TEST_ASSERT_TRUE(closeCalls >= 4);
}

void test_pin_attempt_store_set_error_does_not_commit_and_closes() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_TRUE(store.initializeFailureCount());
  setResult = ESP_FAIL;
  const uint32_t commitsBefore = commitCalls;
  const uint32_t closesBefore = closeCalls;
  TEST_ASSERT_FALSE(store.persistFailureCount(4));
  TEST_ASSERT_EQUAL_UINT32(commitsBefore, commitCalls);
  TEST_ASSERT_EQUAL_UINT32(closesBefore + 1, closeCalls);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_pin_attempt_store_initialize_commit_error_is_indeterminate() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  commitResult = ESP_FAIL;
  persistOnCommitError = true;
  TEST_ASSERT_FALSE(store.initializeFailureCount());
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  uint16_t failures = 99;
  TEST_ASSERT_TRUE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(0, failures);
}

void test_pin_attempt_store_does_not_create_missing_record_on_persist() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_FALSE(store.persistFailureCount(1));
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
  uint16_t failures = 123;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(123, failures);
}

void test_pin_attempt_store_commit_error_returns_false_even_when_value_survives() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_TRUE(store.initializeFailureCount());
  commitResult = ESP_FAIL;
  persistOnCommitError = true;
  TEST_ASSERT_FALSE(store.persistFailureCount(5));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
  uint16_t failures = 0;
  TEST_ASSERT_TRUE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(5, failures);
}

void test_pin_attempt_store_wrong_nvs_type_fails_closed() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  const uint8_t wrongType[2] = {0, 0};
  fakeSeedPinBlob(0, wrongType, sizeof wrongType);
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  uint16_t failures = 777;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(777, failures);
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_TYPE_MISMATCH, store.lastError());
}

void test_pin_attempt_store_readback_error_returns_false() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("attsec", boot.coordinator);
  TEST_ASSERT_TRUE(store.initializeFailureCount());
  getCalls = 0;
  getFailOnCall = 2;  // The store's post-commit read-back, not its pre-write read.
  TEST_ASSERT_FALSE(store.persistFailureCount(6));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, store.lastError());
}

void test_pin_attempt_store_namespaces_are_independent_for_delegated_domains() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore pinStore("attsec", boot.coordinator);
  Esp32NvsPinAttemptStore setupStore("attsetup", boot.coordinator);
  TEST_ASSERT_TRUE(pinStore.initializeFailureCount());
  TEST_ASSERT_TRUE(setupStore.initializeFailureCount());
  TEST_ASSERT_TRUE(pinStore.persistFailureCount(3));
  uint16_t pinFailures = 0, setupFailures = 99;
  TEST_ASSERT_TRUE(pinStore.loadFailureCount(pinFailures));
  TEST_ASSERT_TRUE(setupStore.loadFailureCount(setupFailures));
  TEST_ASSERT_EQUAL_UINT16(3, pinFailures);
  TEST_ASSERT_EQUAL_UINT16(0, setupFailures);
}

void test_pin_attempt_store_rejects_invalid_namespace_length_before_nvs() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore store("namespace_name_too_long", boot.coordinator);
  TEST_ASSERT_FALSE(store.configured());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, store.lastError());
  const uint32_t before = openCalls;
  uint16_t failures = 0;
  TEST_ASSERT_FALSE(store.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT32(before, openCalls);
}

void test_pin_attempt_store_rejects_empty_null_and_config_namespace() {
  BootFixture boot;
  TEST_ASSERT_TRUE(initNvsForTest());
  Esp32NvsPinAttemptStore emptyNamespace("", boot.coordinator);
  Esp32NvsPinAttemptStore nullNamespace(nullptr, boot.coordinator);
  Esp32NvsPinAttemptStore configNamespace("attcfg", boot.coordinator);
  Esp32NvsPinAttemptStore maxLengthNamespace("123456789012345", boot.coordinator);
  TEST_ASSERT_TRUE(maxLengthNamespace.configured());
  TEST_ASSERT_FALSE(emptyNamespace.configured());
  TEST_ASSERT_FALSE(nullNamespace.configured());
  TEST_ASSERT_FALSE(configNamespace.configured());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, emptyNamespace.lastError());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, nullNamespace.lastError());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_ARG, configNamespace.lastError());
  const uint32_t before = openCalls;
  uint16_t count = 0;
  TEST_ASSERT_FALSE(configNamespace.loadFailureCount(count));
  TEST_ASSERT_EQUAL_UINT32(before, openCalls);
}

void test_pin_auth_runtime_requires_config_boot_state() {
  TEST_ASSERT_TRUE(initNvsForTest());
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_FALSE(runtime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_CONFIG_NOT_READY, runtime.code());
  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_NULL(runtime.coordinator());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);

  FakeConfigBootBackend failedBackend(true);
  failedBackend.failLoad();
  ConfigBootCoordinator failedConfigBoot(failedBackend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_LOAD_FAILED, failedConfigBoot.start().code);
  Esp32PinAuthRuntime failedRuntime("attsec", failedConfigBoot);
  TEST_ASSERT_FALSE(failedRuntime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_CONFIG_NOT_READY, failedRuntime.code());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

void test_pin_auth_runtime_initializes_missing_counter_only_for_needs_setup() {
  TEST_ASSERT_TRUE(initNvsForTest());
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_TRUE(runtime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_READY, runtime.code());
  TEST_ASSERT_NOT_NULL(runtime.coordinator());
  TEST_ASSERT_EQUAL_UINT16(0, runtime.coordinator()->failures());
  TEST_ASSERT_EQUAL_UINT32(1, commitCalls);
}

void test_pin_auth_runtime_preserves_counter_after_interrupted_first_setup() {
  TEST_ASSERT_TRUE(initNvsForTest());
  fakeSeedPin(0, 4);
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_TRUE(runtime.start(10));
  TEST_ASSERT_EQUAL_UINT16(4, runtime.coordinator()->failures());
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_auth_runtime_provisioned_config_never_initializes_missing_counter() {
  TEST_ASSERT_TRUE(initNvsForTest());
  FakeConfigBootBackend backend(false);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_TRUE(configBoot.start().code == CFG_BOOT_OK);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_FALSE(runtime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_COORDINATOR_BOOT_FAILED, runtime.code());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_NOT_FOUND, runtime.lastStoreError());
  TEST_ASSERT_NULL(runtime.coordinator());
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_auth_runtime_provisioned_config_loads_existing_counter() {
  TEST_ASSERT_TRUE(initNvsForTest());
  fakeSeedPin(0, 5);
  FakeConfigBootBackend backend(false);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_TRUE(runtime.start(10));
  TEST_ASSERT_EQUAL_UINT16(5, runtime.coordinator()->failures());
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_auth_runtime_wrong_type_during_first_setup_does_not_reinitialize() {
  TEST_ASSERT_TRUE(initNvsForTest());
  const uint8_t wrongType[] = {1, 2, 3, 4};
  fakeSeedPinBlob(0, wrongType, sizeof wrongType);
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_FALSE(runtime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_COUNTER_READ_FAILED, runtime.code());
  TEST_ASSERT_EQUAL_INT(ESP_ERR_NVS_TYPE_MISMATCH, runtime.lastStoreError());
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}

void test_pin_auth_runtime_start_is_one_shot() {
  TEST_ASSERT_TRUE(initNvsForTest());
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_TRUE(runtime.start(10));
  const uint32_t opens = openCalls;
  TEST_ASSERT_FALSE(runtime.start(11));
  TEST_ASSERT_EQUAL_INT(EPAR_ALREADY_STARTED, runtime.code());
  TEST_ASSERT_EQUAL_UINT32(opens, openCalls);
}

void test_pin_auth_runtime_does_not_initialize_when_counter_read_has_io_error() {
  TEST_ASSERT_TRUE(initNvsForTest());
  // Keep the namespace present but the key absent. Otherwise nvs_open(READONLY)
  // returns NOT_FOUND before the injected nvs_get_u16() I/O error is reached.
  pinNamespaces[0].exists = true;
  getResult = ESP_FAIL;
  FakeConfigBootBackend backend(true);
  ConfigBootCoordinator configBoot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, configBoot.start().code);
  Esp32PinAuthRuntime runtime("attsec", configBoot);
  TEST_ASSERT_FALSE(runtime.start(10));
  TEST_ASSERT_EQUAL_INT(EPAR_COUNTER_READ_FAILED, runtime.code());
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, runtime.lastStoreError());
  TEST_ASSERT_EQUAL_UINT32(1, openCalls);
  TEST_ASSERT_EQUAL_UINT32(1, getCalls);
  TEST_ASSERT_EQUAL_UINT32(1, closeCalls);
  TEST_ASSERT_TRUE(runtime.faulted());
  TEST_ASSERT_NULL(runtime.coordinator());
  TEST_ASSERT_EQUAL_UINT32(0, setCalls);
  TEST_ASSERT_EQUAL_UINT32(0, commitCalls);
}


void test_pin_attempt_store_requires_ready_or_needs_setup_config_boot() {
  TEST_ASSERT_TRUE(initNvsForTest());
  FakeConfigBootBackend coldBackend(true);
  ConfigBootCoordinator coldBoot(coldBackend);
  Esp32NvsPinAttemptStore coldStore("attsec", coldBoot);
  uint16_t failures = 765;
  TEST_ASSERT_FALSE(coldStore.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_UINT16(765, failures);
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, coldStore.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
  TEST_ASSERT_FALSE(coldStore.initializeFailureCount());
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, coldStore.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
  TEST_ASSERT_FALSE(coldStore.persistFailureCount(3));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, coldStore.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);

  FakeConfigBootBackend failedBackend(true);
  failedBackend.failLoad();
  ConfigBootCoordinator failedBoot(failedBackend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_LOAD_FAILED, failedBoot.start().code);
  Esp32NvsPinAttemptStore failedStore("attsec", failedBoot);
  TEST_ASSERT_FALSE(failedStore.loadFailureCount(failures));
  TEST_ASSERT_EQUAL_INT(ESP_FAIL, failedStore.lastError());
  TEST_ASSERT_EQUAL_UINT32(0, openCalls);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pin_attempt_store_load_before_nvs_init_fails_closed);
  RUN_TEST(test_pin_attempt_store_rejects_unsafe_startup_before_any_nvs_access);
  RUN_TEST(test_pin_attempt_store_requires_ready_or_needs_setup_config_boot);
  RUN_TEST(test_pin_attempt_store_open_error_fails_closed_without_output_change);
  RUN_TEST(test_pin_attempt_store_missing_record_fails_closed_without_mutating_output);
  RUN_TEST(test_pin_attempt_store_explicit_initialization_commits_zero_and_reads_back);
  RUN_TEST(test_pin_attempt_store_initialization_never_overwrites_existing_counter);
  RUN_TEST(test_pin_attempt_store_initialization_does_not_replace_wrong_type_value);
  RUN_TEST(test_pin_attempt_store_persist_commits_then_reopens_and_verifies);
  RUN_TEST(test_pin_attempt_store_set_error_does_not_commit_and_closes);
  RUN_TEST(test_pin_attempt_store_initialize_commit_error_is_indeterminate);
  RUN_TEST(test_pin_attempt_store_does_not_create_missing_record_on_persist);
  RUN_TEST(test_pin_attempt_store_commit_error_returns_false_even_when_value_survives);
  RUN_TEST(test_pin_attempt_store_wrong_nvs_type_fails_closed);
  RUN_TEST(test_pin_attempt_store_readback_error_returns_false);
  RUN_TEST(test_pin_attempt_store_namespaces_are_independent_for_delegated_domains);
  RUN_TEST(test_pin_attempt_store_rejects_invalid_namespace_length_before_nvs);
  RUN_TEST(test_pin_attempt_store_rejects_empty_null_and_config_namespace);
  RUN_TEST(test_pin_auth_runtime_requires_config_boot_state);
  RUN_TEST(test_pin_auth_runtime_initializes_missing_counter_only_for_needs_setup);
  RUN_TEST(test_pin_auth_runtime_preserves_counter_after_interrupted_first_setup);
  RUN_TEST(test_pin_auth_runtime_provisioned_config_never_initializes_missing_counter);
  RUN_TEST(test_pin_auth_runtime_provisioned_config_loads_existing_counter);
  RUN_TEST(test_pin_auth_runtime_wrong_type_during_first_setup_does_not_reinitialize);
  RUN_TEST(test_pin_auth_runtime_start_is_one_shot);
  RUN_TEST(test_pin_auth_runtime_does_not_initialize_when_counter_read_has_io_error);
  RUN_TEST(test_begin_rejects_no_free_pages_without_opening_storage);
  RUN_TEST(test_begin_rejects_new_nvs_version_without_destructive_recovery);
  RUN_TEST(test_begin_rejects_ambiguous_arduino_startup_without_retrying_nvs_init);
  RUN_TEST(test_begin_uses_arduino_initialized_nvs_without_second_init_call);
  RUN_TEST(test_load_before_begin_is_config_fault);
  RUN_TEST(test_missing_namespace_loads_unprovisioned_not_default_config);
  RUN_TEST(test_missing_key_returns_exact_blank_slot);
  RUN_TEST(test_load_selects_highest_valid_config_generation);
  RUN_TEST(test_malformed_blob_length_is_not_treated_as_blank);
  RUN_TEST(test_nvs_read_error_does_not_overwrite_output);
  RUN_TEST(test_read_provision_marker_distinguishes_absent_from_valid);
  RUN_TEST(test_read_provision_marker_rejects_corrupt_marker_and_nvs_error);
  RUN_TEST(test_write_commits_before_return_and_round_trips);
  RUN_TEST(test_set_failure_does_not_commit_and_still_closes);
  RUN_TEST(test_commit_error_is_indeterminate_even_if_blob_survives_reboot);
  RUN_TEST(test_provision_marker_prevents_silent_reprovision_when_both_slots_missing);
  RUN_TEST(test_corrupt_provision_marker_is_not_treated_as_unprovisioned);
  RUN_TEST(test_load_migrates_legacy_valid_slot_by_committing_marker);
  RUN_TEST(test_legacy_marker_migration_commit_failure_blocks_boot);
  RUN_TEST(test_corrupt_marker_faults_even_when_one_config_slot_is_valid);
  RUN_TEST(test_invalid_slot_is_rejected_before_nvs_open);
  RUN_TEST(test_config_coordinator_uses_nvs_inactive_slot_and_reloads_generation);
  return UNITY_END();
}
