#include <unity.h>
#include <string.h>
#include "ap_credential_setup.h"

using namespace att;

namespace {
const char kGeneratedPass[] = "101112131415161718191a1b1c1d1e1f";
int gSequence = 0;

class FakeConfigBackend : public IConfigBootBackend {
 public:
  FakeConfigBackend() : ready(false), markerPresent(false), markerReadOk(true),
      writesOk(true), beginCalls(0), loadCalls(0), writeCalls(0) {
    memset(slots, 0xFF, sizeof slots);
  }
  bool begin() override { ++beginCalls; ready = true; return true; }
  bool load(Config& out, CfgPick& pick) override {
    ++loadCalls;
    if (!ready || !readConfigSlots(*this, out, pick)) return false;
    if (pick.boot == CB_UNPROVISIONED && markerPresent) return false;
    if (pick.boot == CB_OK && !markerPresent) markerPresent = true;
    return true;
  }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    if (!ready || slot > 1 || out == nullptr) return false;
    memcpy(out, slots[slot], CFG_SLOT_SIZE);
    return true;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    ++writeCalls;
    if (!ready || slot > 1 || data == nullptr || !writesOk) return false;
    memcpy(slots[slot], data, CFG_SLOT_SIZE);
    markerPresent = true;
    return true;
  }
  bool readProvisionMarker(bool& present) override {
    if (!ready || !markerReadOk) return false;
    present = markerPresent;
    return true;
  }
  bool ready;
  bool markerPresent;
  bool markerReadOk;
  bool writesOk;
  int beginCalls;
  int loadCalls;
  int writeCalls;
  uint8_t slots[2][CFG_SLOT_SIZE];
};

class FakeCredentialStore : public IApCredentialStore {
 public:
  FakeCredentialStore() : exists(false), readFails(false), writeFails(false),
      reads(0), writes(0) { memset(value, 0, sizeof value); }
  ApCredentialStoreRead load(char out[AP_CREDENTIAL_BUFFER_SIZE]) override {
    ++reads;
    if (out == nullptr || readFails) return APCR_READ_ERROR;
    memset(out, 0, AP_CREDENTIAL_BUFFER_SIZE);
    if (!exists) return APCR_NOT_FOUND;
    memcpy(out, value, sizeof value);
    return APCR_READ_OK;
  }
  ApCredentialStoreWrite createIfAbsent(const char* passphrase) override {
    ++writes;
    if (passphrase == nullptr || writeFails) return APCW_WRITE_ERROR;
    if (exists) return APCW_ALREADY_EXISTS;
    strncpy(value, passphrase, sizeof value - 1);
    exists = true;
    return APCW_CREATED;
  }
  bool exists;
  bool readFails;
  bool writeFails;
  int reads;
  int writes;
  char value[AP_CREDENTIAL_BUFFER_SIZE];
};

class FakeRandom : public ISecureRandomSource {
 public:
  FakeRandom() : ok(true), calls(0) {}
  bool fill(uint8_t* out, size_t length) override {
    ++calls;
    if (!ok || out == nullptr) return false;
    for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(0x10 + i);
    return true;
  }
  bool ok;
  int calls;
};

class FakeDisplay : public IApCredentialDisplay {
 public:
  FakeDisplay() : credentialOk(true), blankOk(true), credentialCalls(0),
      blankCalls(0), sequenceAtCredential(0), sequenceAtBlank(0) {
    memset(passphrase, 0, sizeof passphrase);
  }
  bool requestCredentialDisplay(const ApCredentialResult& result,
                                const char* secret, size_t length) override {
    ++credentialCalls;
    sequenceAtCredential = ++gSequence;
    if (result.code != APCB_OK || !result.disclose || secret == nullptr ||
        length >= sizeof passphrase) return false;
    memcpy(passphrase, secret, length);
    passphrase[length] = '\0';
    return credentialOk;
  }
  bool requestBlankFrame() override {
    ++blankCalls;
    sequenceAtBlank = ++gSequence;
    return blankOk;
  }
  bool credentialOk;
  bool blankOk;
  int credentialCalls;
  int blankCalls;
  int sequenceAtCredential;
  int sequenceAtBlank;
  char passphrase[AP_CREDENTIAL_BUFFER_SIZE];
};

class FakeAp : public ICredentialSoftAp {
 public:
  FakeAp() : startOk(true), isReady(false), hasLease(false), startCalls(0),
      sequenceAtStart(0), channelSeen(-1) {
    memset(ssid, 0, sizeof ssid);
    memset(passphrase, 0, sizeof passphrase);
  }
  bool start(const char* name, size_t nameLength, const char* secret,
             size_t secretLength, int channel) override {
    ++startCalls;
    sequenceAtStart = ++gSequence;
    if (!startOk || name == nullptr || secret == nullptr ||
        nameLength >= sizeof ssid || secretLength >= sizeof passphrase) return false;
    memcpy(ssid, name, nameLength); ssid[nameLength] = '\0';
    memcpy(passphrase, secret, secretLength); passphrase[secretLength] = '\0';
    channelSeen = channel;
    isReady = true;
    return true;
  }
  bool ready() const override { return isReady; }
  bool hasAssignedClientIp() const override { return isReady && hasLease; }
  bool startOk;
  bool isReady;
  bool hasLease;
  int startCalls;
  int sequenceAtStart;
  int channelSeen;
  char ssid[40];
  char passphrase[AP_CREDENTIAL_BUFFER_SIZE];
};

class FakeSetupAuth : public IConfigProvisionGate {
 public:
  explicit FakeSetupAuth(bool allow) : allowed(allow), calls(0) {}
  bool mayProvision() const override { ++calls; return allowed; }
  bool allowed;
  mutable int calls;
};

class AllowOnlyFirstSetupAuth : public IConfigProvisionGate {
 public:
  AllowOnlyFirstSetupAuth() : calls(0) {}
  bool mayProvision() const override { return ++calls == 1; }
  mutable int calls;
};

Config validConfig() {
  Config c = defaultConfig();
  c.termStart = 100;
  c.termEnd = 400;
  c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "caller-supplied-not-the-ap-secret");
  return c;
}

struct Fixture {
  FakeConfigBackend backend;
  ConfigBootCoordinator boot;
  FakeCredentialStore store;
  FakeRandom random;
  ApCredentialBootstrap bootstrap;
  FakeDisplay display;
  FakeAp ap;
  FakeSetupAuth auth;
  ApCredentialSetupCoordinator setup;
  Fixture(bool authorized = true)
      : boot(backend), bootstrap(store, random), auth(authorized),
        setup(boot, bootstrap, display, ap) {}
  void enterSetup() { TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code); }
  void startAp() {
    TEST_ASSERT_EQUAL_INT(ACSE_STARTED,
        setup.start());
  }
};
}

void setUp() { gSequence = 0; }
void tearDown() {}

void test_cold_boot_does_not_disclose_or_start_ap() {
  Fixture f;
  TEST_ASSERT_EQUAL_INT(ACSE_BOOT_NOT_SETUP_REQUIRED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(0, f.store.reads);
  TEST_ASSERT_EQUAL_INT(0, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(0, f.ap.startCalls);
}

void test_credential_is_requested_on_display_before_secured_ap_start() {
  Fixture f; f.enterSetup(); f.startAp();
  TEST_ASSERT_EQUAL_INT(1, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
  TEST_ASSERT_TRUE(f.display.sequenceAtCredential < f.ap.sequenceAtStart);
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.display.passphrase);
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.ap.passphrase);
  TEST_ASSERT_EQUAL_STRING("att", f.ap.ssid);
  TEST_ASSERT_EQUAL_INT(1, f.ap.channelSeen);
  TEST_ASSERT_TRUE(f.store.exists);
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.store.value);
  TEST_ASSERT_EQUAL_INT(ACSS_WAITING_FOR_CLIENT, f.setup.state());
}

void test_production_ssid_and_channel_are_fixed() {
  Fixture f; f.enterSetup(); f.startAp();
  TEST_ASSERT_EQUAL_STRING("att", f.ap.ssid);
  TEST_ASSERT_EQUAL_INT(1, f.ap.channelSeen);
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
}

void test_display_failure_prevents_ap_start_and_requests_blank_frame() {
  Fixture f; f.enterSetup(); f.display.credentialOk = false;
  TEST_ASSERT_EQUAL_INT(ACSE_DISPLAY_FAILED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(1, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(0, f.ap.startCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
}

void test_ap_start_failure_requests_clear_and_latches_fault() {
  Fixture f; f.enterSetup(); f.ap.startOk = false;
  TEST_ASSERT_EQUAL_INT(ACSE_AP_START_FAILED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(1, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
}

void test_failed_startup_clear_can_be_retried_without_restarting_ap() {
  Fixture f; f.enterSetup(); f.ap.startOk = false; f.display.blankOk = false;
  TEST_ASSERT_EQUAL_INT(ACSE_AP_START_FAILED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(ACSS_STARTUP_CLEAR_PENDING, f.setup.state());
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  TEST_ASSERT_FALSE(f.setup.mayProvision(f.auth));

  f.display.blankOk = true;
  TEST_ASSERT_EQUAL_INT(ACSE_STARTUP_CLEAR_REQUESTED, f.setup.retryBlankFrame());
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
  TEST_ASSERT_EQUAL_INT(2, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
}

void test_failed_secret_readback_clear_can_be_retried_before_fault() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  f.store.readFails = true; f.display.blankOk = false;
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_BOOTSTRAP_FAILED,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_EQUAL_INT(ACSS_STARTUP_CLEAR_PENDING, f.setup.state());
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);

  f.display.blankOk = true;
  TEST_ASSERT_EQUAL_INT(ACSE_STARTUP_CLEAR_REQUESTED, f.setup.retryBlankFrame());
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
  TEST_ASSERT_EQUAL_INT(2, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
}

void test_provision_is_blocked_until_ap_ready_and_dhcp_assignment() {
  Fixture f; f.enterSetup(); f.startAp();
  Config initial = validConfig(); ConfigBootResult result = {};
  TEST_ASSERT_FALSE(f.ap.hasAssignedClientIp());
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(initial, f.auth, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
  f.ap.isReady = false; f.ap.hasLease = true;
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(initial, f.auth, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
}

void test_provision_is_blocked_when_setup_auth_gate_is_not_satisfied() {
  Fixture f(false); f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  Config initial = validConfig(); ConfigBootResult result = {};
  TEST_ASSERT_FALSE(f.setup.mayProvision(f.auth));
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(initial, f.auth, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
}

void test_setup_authorization_is_supplied_per_provision_request_not_cached() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  Config initial = validConfig(); ConfigBootResult result = {};
  FakeSetupAuth earlierRequest(true);
  FakeSetupAuth currentRequest(false);

  // An earlier authenticated operation cannot authorize this request.
  TEST_ASSERT_TRUE(f.setup.mayProvision(earlierRequest));
  TEST_ASSERT_FALSE(f.setup.mayProvision(currentRequest));
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(initial, currentRequest, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);

  // A distinct, currently authorized request can then provision.
  FakeSetupAuth authorizedRequest(true);
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISIONED_CLEAR_REQUESTED,
      f.setup.provision(initial, authorizedRequest, result));
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
}

void test_second_auth_gate_check_fails_closed_before_config_write() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  Config initial = validConfig(); ConfigBootResult result = {};
  AllowOnlyFirstSetupAuth gate;

  // The coordinator preflights the current request, then the core provisioner
  // checks the same gate immediately before storage mutation. A non-idempotent
  // gate is rejected on the second check and must not write config.
  TEST_ASSERT_EQUAL_INT(ACSE_CONFIG_REJECTED,
      f.setup.provision(initial, gate, result));
  TEST_ASSERT_EQUAL_INT(2, gate.calls);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_PROVISION_REJECTED, result.code);
  TEST_ASSERT_TRUE(f.boot.needsSetup());
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_WAITING_FOR_CLIENT, f.setup.state());
}

void test_provision_persists_the_exact_ap_secret_not_the_callers_value() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  Config initial = validConfig(); ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISIONED_CLEAR_REQUESTED,
      f.setup.provision(initial, f.auth, result));
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, result.code);
  TEST_ASSERT_TRUE(f.boot.ready());
  TEST_ASSERT_NOT_NULL(f.boot.config());
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.boot.config()->wifiPass);
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.ap.passphrase);
  TEST_ASSERT_EQUAL_STRING(kGeneratedPass, f.store.value);
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  TEST_ASSERT_TRUE(f.display.sequenceAtCredential < f.ap.sequenceAtStart);
  TEST_ASSERT_TRUE(f.ap.sequenceAtStart < f.display.sequenceAtBlank);
  TEST_ASSERT_EQUAL_INT(ACSS_PROVISIONED, f.setup.state());
}

void test_invalid_initial_config_can_be_corrected_without_restarting_setup_ap() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  Config invalid = validConfig(); invalid.pinSet = 0;
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_CONFIG_REJECTED, f.setup.provision(invalid, f.auth, result));
  TEST_ASSERT_TRUE(f.boot.needsSetup());
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_WAITING_FOR_CLIENT, f.setup.state());
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISIONED_CLEAR_REQUESTED,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
}

void test_ambiguous_storage_failure_latches_setup_coordinator_fault() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  f.backend.writesOk = false; f.display.blankOk = false;
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISION_FAULTED,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_TRUE(f.boot.faulted());
  TEST_ASSERT_EQUAL_INT(ACSS_STARTUP_CLEAR_PENDING, f.setup.state());
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  f.display.blankOk = true;
  TEST_ASSERT_EQUAL_INT(ACSE_STARTUP_CLEAR_REQUESTED, f.setup.retryBlankFrame());
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(2, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(ACSE_NOT_READY_FOR_PROVISION,
      f.setup.provision(validConfig(), f.auth, result));
}

void test_blank_frame_failure_is_retried_without_reprovisioning() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  f.display.blankOk = false;
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISIONED_CLEAR_PENDING,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_TRUE(f.boot.ready());
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
  f.display.blankOk = true;
  TEST_ASSERT_EQUAL_INT(ACSE_PROVISIONED_CLEAR_REQUESTED, f.setup.retryBlankFrame());
  TEST_ASSERT_EQUAL_INT(2, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(1, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_PROVISIONED, f.setup.state());
}

void test_bootstrap_failure_does_not_attempt_display_or_ap_start() {
  Fixture f; f.enterSetup(); f.random.ok = false;
  TEST_ASSERT_EQUAL_INT(ACSE_BOOTSTRAP_FAILED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(0, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(0, f.ap.startCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
}

void test_credential_read_failure_after_ap_start_blocks_provision_and_clears_display() {
  Fixture f; f.enterSetup(); f.startAp(); f.ap.hasLease = true;
  f.store.readFails = true;
  ConfigBootResult result = {};
  TEST_ASSERT_EQUAL_INT(ACSE_BOOTSTRAP_FAILED,
      f.setup.provision(validConfig(), f.auth, result));
  TEST_ASSERT_EQUAL_INT(0, f.backend.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, f.display.blankCalls);
  TEST_ASSERT_EQUAL_INT(ACSS_FAULT, f.setup.state());
}

void test_restart_after_success_is_refused_and_cannot_reissue_secret() {
  Fixture f; f.enterSetup(); f.startAp();
  TEST_ASSERT_EQUAL_INT(ACSE_ALREADY_STARTED,
      f.setup.start());
  TEST_ASSERT_EQUAL_INT(1, f.display.credentialCalls);
  TEST_ASSERT_EQUAL_INT(1, f.ap.startCalls);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_cold_boot_does_not_disclose_or_start_ap);
  RUN_TEST(test_credential_is_requested_on_display_before_secured_ap_start);
  RUN_TEST(test_production_ssid_and_channel_are_fixed);
  RUN_TEST(test_display_failure_prevents_ap_start_and_requests_blank_frame);
  RUN_TEST(test_ap_start_failure_requests_clear_and_latches_fault);
  RUN_TEST(test_failed_startup_clear_can_be_retried_without_restarting_ap);
  RUN_TEST(test_failed_secret_readback_clear_can_be_retried_before_fault);
  RUN_TEST(test_provision_is_blocked_until_ap_ready_and_dhcp_assignment);
  RUN_TEST(test_provision_is_blocked_when_setup_auth_gate_is_not_satisfied);
  RUN_TEST(test_setup_authorization_is_supplied_per_provision_request_not_cached);
  RUN_TEST(test_second_auth_gate_check_fails_closed_before_config_write);
  RUN_TEST(test_provision_persists_the_exact_ap_secret_not_the_callers_value);
  RUN_TEST(test_invalid_initial_config_can_be_corrected_without_restarting_setup_ap);
  RUN_TEST(test_ambiguous_storage_failure_latches_setup_coordinator_fault);
  RUN_TEST(test_blank_frame_failure_is_retried_without_reprovisioning);
  RUN_TEST(test_bootstrap_failure_does_not_attempt_display_or_ap_start);
  RUN_TEST(test_credential_read_failure_after_ap_start_blocks_provision_and_clears_display);
  RUN_TEST(test_restart_after_success_is_refused_and_cannot_reissue_secret);
  return UNITY_END();
}
