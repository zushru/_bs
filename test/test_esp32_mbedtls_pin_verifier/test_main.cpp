#include <unity.h>
#include <string.h>

#include "esp32_mbedtls_pin_verifier.h"
#include "esp32_mbedtls_pin_writer.h"
#include "esp32_mbedtls_pin_config_runtime.h"
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

using namespace att;

namespace {
const mbedtls_md_info_t kSha256 = {MBEDTLS_MD_SHA256};
bool gInfoAvailable;
int gSetupResult;
int gPbkdfResult;
int gInfoCalls, gInitCalls, gSetupCalls, gPbkdfCalls, gFreeCalls;
int gHmac;
mbedtls_md_type_t gDigest;
char gPassword[8];
size_t gPasswordLength, gSaltLength;
uint8_t gSalt[16];
unsigned int gIterations;
uint32_t gKeyLength;
uint8_t gDerived[32];

void resetFake() {
  gInfoAvailable = true;
  gSetupResult = 0;
  gPbkdfResult = 0;
  gInfoCalls = gInitCalls = gSetupCalls = gPbkdfCalls = gFreeCalls = 0;
  gHmac = 0;
  gDigest = MBEDTLS_MD_NONE;
  memset(gPassword, 0, sizeof gPassword);
  gPasswordLength = gSaltLength = 0;
  memset(gSalt, 0, sizeof gSalt);
  gIterations = 0;
  gKeyLength = 0;
  memset(gDerived, 0, sizeof gDerived);
}

Config validConfig() {
  Config config = defaultConfig();
  config.termStart = 100;
  config.termEnd = 400;
  config.pinSet = 1;
  memset(config.pinHash, 0xA5, sizeof config.pinHash);
  memset(config.pinSalt, 0x5A, sizeof config.pinSalt);
  strcpy(config.wifiPass, "classroom-ap-77");
  return config;
}

class WriterBackend : public IConfigBootBackend {
 public:
  WriterBackend() : started(false), marker(true), writeOk(true),
      persistOnWriteError(true), reloadFailAfterWrite(false), reads(0), writes(0),
      failReadAt(-1) { memset(slots, 0xFF, sizeof slots); }

  bool begin() override { started = true; return true; }
  bool load(Config& out, CfgPick& pick) override {
    if (!started || !readConfigSlots(*this, out, pick)) return false;
    if (pick.boot == CB_OK && !marker) marker = true;
    return !(reloadFailAfterWrite && writes > 0);
  }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    ++reads;
    if (!started || slot > 1 || out == nullptr || reads == failReadAt)
      return false;
    memcpy(out, slots[slot], CFG_SLOT_SIZE);
    return true;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    ++writes;
    if (!started || slot > 1 || data == nullptr) return false;
    if (writeOk || persistOnWriteError) {
      memcpy(slots[slot], data, CFG_SLOT_SIZE);
      marker = true;
    }
    return writeOk;
  }
  bool readProvisionMarker(bool& present) override {
    if (!started) return false;
    present = marker;
    return true;
  }

  uint8_t slots[2][CFG_SLOT_SIZE];
  bool started, marker, writeOk, persistOnWriteError, reloadFailAfterWrite;
  int reads, writes, failReadAt;
};

void seedWriterBackend(WriterBackend& backend, const Config& config,
                       uint32_t generation, uint8_t slot) {
  uint8_t encoded[CFG_SLOT_SIZE];
  TEST_ASSERT_EQUAL_UINT32(CFG_SLOT_SIZE, encodeConfig(config, generation, encoded));
  memcpy(backend.slots[slot], encoded, sizeof encoded);
}

class FakePinRandom : public ISecureRandomSource {
 public:
  FakePinRandom() : allowed(true), calls(0) { memset(bytes, 0x6B, sizeof bytes); }
  bool fill(uint8_t* out, size_t length) override {
    ++calls;
    if (!allowed || out == nullptr || length != sizeof bytes) return false;
    memcpy(out, bytes, sizeof bytes);
    return true;
  }
  bool allowed;
  int calls;
  uint8_t bytes[8];
};
}  // namespace

extern "C" const mbedtls_md_info_t* mbedtls_md_info_from_type(
    mbedtls_md_type_t type) {
  ++gInfoCalls;
  return gInfoAvailable && type == MBEDTLS_MD_SHA256 ? &kSha256 : nullptr;
}

extern "C" void mbedtls_md_init(mbedtls_md_context_t* ctx) {
  ++gInitCalls;
  ctx->md_info = nullptr;
  ctx->hmac = 0;
}

extern "C" int mbedtls_md_setup(mbedtls_md_context_t* ctx,
                                 const mbedtls_md_info_t* info, int hmac) {
  ++gSetupCalls;
  ctx->md_info = info;
  ctx->hmac = hmac;
  gHmac = hmac;
  gDigest = info == nullptr ? MBEDTLS_MD_NONE : info->type;
  return gSetupResult;
}

extern "C" void mbedtls_md_free(mbedtls_md_context_t* ctx) {
  ++gFreeCalls;
  ctx->md_info = nullptr;
  ctx->hmac = 0;
}

extern "C" int mbedtls_pkcs5_pbkdf2_hmac(
    mbedtls_md_context_t* ctx,
    const unsigned char* password, size_t passwordLength,
    const unsigned char* salt, size_t saltLength,
    unsigned int iterations, uint32_t keyLength, unsigned char* output) {
  ++gPbkdfCalls;
  if (ctx == nullptr || ctx->md_info == nullptr || password == nullptr ||
      salt == nullptr || output == nullptr) return -1;
  gPasswordLength = passwordLength;
  if (passwordLength >= sizeof gPassword) return -1;
  memcpy(gPassword, password, passwordLength);
  gSaltLength = saltLength;
  if (saltLength > sizeof gSalt) return -1;
  memcpy(gSalt, salt, saltLength);
  gIterations = iterations;
  gKeyLength = keyLength;
  if (gPbkdfResult != 0) return gPbkdfResult;
  if (keyLength != sizeof gDerived) return -1;
  memcpy(output, gDerived, sizeof gDerived);
  return 0;
}

void setUp() { resetFake(); }
void tearDown() {}

void test_valid_pin_uses_sha256_hmac_pbkdf2_and_exact_stored_fields() {
  Config config = validConfig();
  memcpy(gDerived, config.pinHash, sizeof gDerived);
  Esp32MbedTlsPinVerifier verifier(config, 24000);

  TEST_ASSERT_TRUE(verifier.ready());
  TEST_ASSERT_EQUAL_INT(PV_MATCH, verifier.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(1, gInfoCalls);
  TEST_ASSERT_EQUAL_INT(1, gInitCalls);
  TEST_ASSERT_EQUAL_INT(1, gSetupCalls);
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(1, gFreeCalls);
  TEST_ASSERT_EQUAL_INT(MBEDTLS_MD_SHA256, gDigest);
  TEST_ASSERT_EQUAL_INT(1, gHmac);
  TEST_ASSERT_EQUAL_STRING("483920", gPassword);
  TEST_ASSERT_EQUAL_UINT32(6, gPasswordLength);
  TEST_ASSERT_EQUAL_UINT32(sizeof config.pinSalt, gSaltLength);
  TEST_ASSERT_EQUAL_MEMORY(config.pinSalt, gSalt, sizeof config.pinSalt);
  TEST_ASSERT_EQUAL_UINT32(24000, gIterations);
  TEST_ASSERT_EQUAL_UINT32(sizeof config.pinHash, gKeyLength);
}

void test_wrong_derived_hash_is_a_mismatch_not_an_adapter_error() {
  Config config = validConfig();
  memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsPinVerifier verifier(config, 1000);

  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, verifier.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
}

void test_zero_iterations_or_invalid_config_never_creates_ready_verifier() {
  Config config = validConfig();
  Esp32MbedTlsPinVerifier zeroIterations(config, 0);
  TEST_ASSERT_FALSE(zeroIterations.ready());
  TEST_ASSERT_EQUAL_INT(PV_ERROR, zeroIterations.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(0, gInfoCalls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);

  config.pinSet = 0;
  Esp32MbedTlsPinVerifier invalidConfig(config, 1000);
  TEST_ASSERT_FALSE(invalidConfig.ready());
  TEST_ASSERT_EQUAL_INT(PV_ERROR, invalidConfig.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
}

void test_malformed_candidate_is_rejected_before_mbedtls() {
  Config config = validConfig();
  Esp32MbedTlsPinVerifier verifier(config, 1000);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, verifier.verify("48a920", 6));
  TEST_ASSERT_EQUAL_INT(PV_ERROR, verifier.verify(nullptr, 6));
  TEST_ASSERT_EQUAL_INT(PV_ERROR, verifier.verify("483920", 5));
  TEST_ASSERT_EQUAL_INT(0, gInfoCalls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
}

void test_missing_digest_or_mbedtls_setup_error_fails_closed() {
  Config config = validConfig();
  gInfoAvailable = false;
  Esp32MbedTlsPinVerifier noDigest(config, 1000);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, noDigest.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(0, gSetupCalls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);

  resetFake();
  gSetupResult = -0x1234;
  Esp32MbedTlsPinVerifier setupFailure(config, 1000);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, setupFailure.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(1, gSetupCalls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(1, gFreeCalls);
}

void test_pbkdf_failure_is_not_treated_as_wrong_pin() {
  Config config = validConfig();
  gPbkdfResult = -0x2345;
  Esp32MbedTlsPinVerifier verifier(config, 1000);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, verifier.verify("483920", 6));
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(1, gFreeCalls);
}

void test_pin_writer_derives_with_injected_iterations_and_commits_new_salt_hash() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 24000);
  FakePinRandom random;
  memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 24000);

  TEST_ASSERT_TRUE(writer.ready());
  TEST_ASSERT_EQUAL_INT(PA_DONE, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(MBEDTLS_MD_SHA256, gDigest);
  TEST_ASSERT_EQUAL_INT(1, gHmac);
  TEST_ASSERT_EQUAL_UINT32(24000, gIterations);
  TEST_ASSERT_EQUAL_UINT32(8, gSaltLength);
  TEST_ASSERT_EQUAL_MEMORY(random.bytes, gSalt, sizeof random.bytes);
  TEST_ASSERT_EQUAL_UINT32(32, gKeyLength);
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);

  Config expected = initial;
  memcpy(expected.pinSalt, random.bytes, sizeof expected.pinSalt);
  memcpy(expected.pinHash, gDerived, sizeof expected.pinHash);
  TEST_ASSERT_TRUE(configEqual(*boot.config(), expected));
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_TRUE(configEqual(*runtime.config(), expected));
  TEST_ASSERT_NOT_NULL(runtime.verifier());
  TEST_ASSERT_EQUAL_INT(PV_MATCH,
                       runtime.verifier()->verify("627194", 6));
  memcpy(gDerived, initial.pinHash, sizeof gDerived);
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH,
                       runtime.verifier()->verify("483920", 6));
}

void test_pin_writer_requires_explicit_nonzero_iteration_policy() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 0);
  FakePinRandom random;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 0);

  TEST_ASSERT_FALSE(writer.ready());
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);

  Esp32MbedTlsPinConfigRuntime differentIterations(boot, 1000);
  Esp32MbedTlsAdminPinWriter mismatchedPolicy(
      boot, differentIterations, random, 2000);
  TEST_ASSERT_FALSE(mismatchedPolicy.ready());
  TEST_ASSERT_EQUAL_INT(PA_REFUSED,
                       mismatchedPolicy.commitNewPin("627194", 6));

  WriterBackend staleBackend;
  Config differentConfig = initial;
  strcpy(differentConfig.wifiPass, "different-ap-key");
  seedWriterBackend(staleBackend, differentConfig, 4, 0);
  ConfigBootCoordinator staleBoot(staleBackend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, staleBoot.start().code);
  Esp32MbedTlsPinConfigRuntime staleRuntime(staleBoot, 1000);
  Esp32MbedTlsAdminPinWriter mismatchedSnapshot(
      boot, staleRuntime, random, 1000);
  TEST_ASSERT_FALSE(mismatchedSnapshot.ready());
  TEST_ASSERT_EQUAL_INT(PA_REFUSED,
                       mismatchedSnapshot.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_writer_rejects_weak_pin_before_entropy_or_pbkdf() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("123456", 6));
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("12a456", 6));
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_writer_refuses_rng_unavailable_without_storage_mutation() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  random.allowed = false;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_writer_rejects_zero_or_reused_salt() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  memset(random.bytes, 0, sizeof random.bytes);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  memset(random.bytes, 0x5A, sizeof random.bytes);
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(2, random.calls);
  TEST_ASSERT_EQUAL_INT(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_writer_kdf_error_is_indeterminate_and_never_commits() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  gPbkdfResult = -0x3456;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_config_runtime_preflights_pin_only_changes_without_mutating() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 24000);
  TEST_ASSERT_TRUE(runtime.ready());

  Config updated = initial;
  memset(updated.pinSalt, 0x6B, sizeof updated.pinSalt);
  memset(updated.pinHash, 0xC3, sizeof updated.pinHash);
  TEST_ASSERT_TRUE(runtime.canApply(updated));
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_TRUE(configEqual(*runtime.config(), initial));
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_pin_config_runtime_rejects_non_pin_config_changes() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 24000);
  Config changed = initial;
  strcpy(changed.wifiPass, "different-ap-key");
  memset(changed.pinSalt, 0x6B, sizeof changed.pinSalt);
  memset(changed.pinHash, 0xC3, sizeof changed.pinHash);

  TEST_ASSERT_FALSE(runtime.canApply(changed));
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_TRUE(configEqual(*runtime.config(), initial));
  TEST_ASSERT_FALSE(runtime.applyAfterCommit(changed));
  TEST_ASSERT_TRUE(runtime.faulted());
  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_NULL(runtime.config());
}

void test_pin_config_runtime_requires_explicit_usable_iteration_policy() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 0);
  Config updated = initial;
  memset(updated.pinSalt, 0x6B, sizeof updated.pinSalt);
  memset(updated.pinHash, 0xC3, sizeof updated.pinHash);

  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_FALSE(runtime.canApply(updated));
  TEST_ASSERT_FALSE(runtime.applyAfterCommit(updated));
  TEST_ASSERT_TRUE(runtime.faulted());
}

void test_pin_writer_requires_ready_config_boot_before_entropy() {
  WriterBackend backend;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_FALSE(writer.ready());
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(0, random.calls);
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

void test_pin_writer_generation_exhaustion_refuses_without_write() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, UINT32_MAX, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_TRUE(boot.ready());
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_pin_writer_canonical_read_failure_faults_boot_and_runtime() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);
  backend.failReadAt = backend.reads + 1;

  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE,
                       writer.commitNewPin("627194", 6));
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_NULL(runtime.verifier());
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_INT(1, random.calls);
}

void test_pin_writer_indeterminate_slot_write_faults_without_retry() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);
  backend.writeOk = false;

  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE,
                       writer.commitNewPin("627194", 6));
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
  TEST_ASSERT_EQUAL_INT(PA_REFUSED, writer.commitNewPin("627194", 6));
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
  TEST_ASSERT_EQUAL_INT(1, random.calls);
}

void test_pin_writer_reload_failure_faults_boot_after_slot_write() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);
  backend.reloadFailAfterWrite = true;

  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE,
                       writer.commitNewPin("627194", 6));
  TEST_ASSERT_TRUE(boot.faulted());
  TEST_ASSERT_FALSE(runtime.ready());
  TEST_ASSERT_EQUAL_UINT32(1, backend.writes);
  TEST_ASSERT_NULL(runtime.verifier());
}

void test_pin_writer_zero_derived_hash_is_indeterminate_without_write() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 12000);
  FakePinRandom random;
  memset(gDerived, 0, sizeof gDerived);
  Esp32MbedTlsAdminPinWriter writer(boot, runtime, random, 12000);

  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE,
                       writer.commitNewPin("627194", 6));
  TEST_ASSERT_TRUE(boot.ready());
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_pin_runtime_invalid_credential_update_is_rejected_without_mutation() {
  WriterBackend backend;
  Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Esp32MbedTlsPinConfigRuntime runtime(boot, 24000);
  Config invalid = initial;
  invalid.pinSet = 0;

  TEST_ASSERT_FALSE(runtime.canApply(invalid));
  TEST_ASSERT_TRUE(runtime.ready());
  TEST_ASSERT_TRUE(configEqual(*runtime.config(), initial));
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}


void test_initial_pin_preparer_uses_rf_rng_and_exact_e35_work_factor_without_persisting() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config config = validConfig(); config.pinSet = 0;
  memset(config.pinHash, 0, sizeof config.pinHash);
  memset(config.pinSalt, 0, sizeof config.pinSalt);
  FakePinRandom random; memset(gDerived, 0x3C, sizeof gDerived);
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_PREPARED, preparer.prepare("627194", 6, config));
  TEST_ASSERT_EQUAL_UINT32(1, random.calls);
  TEST_ASSERT_EQUAL_UINT32(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(kE35AdminPinPbkdf2Iterations, gIterations);
  TEST_ASSERT_EQUAL_UINT32(2048, gIterations);
  TEST_ASSERT_EQUAL_INT(MBEDTLS_MD_SHA256, gDigest);
  TEST_ASSERT_EQUAL_INT(1, gHmac);
  TEST_ASSERT_EQUAL_UINT32(8, gSaltLength);
  TEST_ASSERT_EQUAL_MEMORY(random.bytes, gSalt, sizeof random.bytes);
  TEST_ASSERT_EQUAL_UINT32(32, gKeyLength);
  TEST_ASSERT_TRUE(validateConfig(config));
  TEST_ASSERT_EQUAL_MEMORY(gDerived, config.pinHash, sizeof config.pinHash);
  TEST_ASSERT_EQUAL_MEMORY(random.bytes, config.pinSalt, sizeof config.pinSalt);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_refuses_ready_boot_before_random_or_kdf() {
  WriterBackend backend; Config initial = validConfig();
  seedWriterBackend(backend, initial, 4, 0);
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_OK, boot.start().code);
  Config candidate = initial; FakePinRandom random;
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_NOT_NEEDS_SETUP, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, initial));
  TEST_ASSERT_EQUAL_UINT32(0, random.calls);
  TEST_ASSERT_EQUAL_UINT32(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_rejects_malformed_and_weak_pins_before_entropy() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config candidate = validConfig(); candidate.pinSet = 0;
  memset(candidate.pinHash, 0, sizeof candidate.pinHash);
  memset(candidate.pinSalt, 0, sizeof candidate.pinSalt);
  const Config original = candidate; FakePinRandom random;
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_INVALID_PIN, preparer.prepare("12a456", 6, candidate));
  TEST_ASSERT_EQUAL_INT(IPCC_INVALID_PIN, preparer.prepare("123456", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  TEST_ASSERT_EQUAL_UINT32(0, random.calls);
  TEST_ASSERT_EQUAL_UINT32(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_refuses_unavailable_rf_random_without_mutation() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config candidate = validConfig(); candidate.pinSet = 0;
  memset(candidate.pinHash, 0, sizeof candidate.pinHash);
  memset(candidate.pinSalt, 0, sizeof candidate.pinSalt);
  const Config original = candidate; FakePinRandom random; random.allowed = false;
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_RANDOM_FAILED, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  TEST_ASSERT_EQUAL_UINT32(1, random.calls);
  TEST_ASSERT_EQUAL_UINT32(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_rejects_zero_salt_before_pbkdf() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config candidate = validConfig(); candidate.pinSet = 0;
  memset(candidate.pinHash, 0, sizeof candidate.pinHash);
  memset(candidate.pinSalt, 0, sizeof candidate.pinSalt);
  const Config original = candidate; FakePinRandom random;
  memset(random.bytes, 0, sizeof random.bytes);
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_INVALID_SALT, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  TEST_ASSERT_EQUAL_UINT32(1, random.calls);
  TEST_ASSERT_EQUAL_UINT32(0, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_kdf_error_and_zero_hash_never_mutate_config() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config candidate = validConfig(); candidate.pinSet = 0;
  memset(candidate.pinHash, 0, sizeof candidate.pinHash);
  memset(candidate.pinSalt, 0, sizeof candidate.pinSalt);
  const Config original = candidate; FakePinRandom random;
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  gPbkdfResult = -0x1234;
  TEST_ASSERT_EQUAL_INT(IPCC_KDF_FAILED, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  resetFake(); memset(gDerived, 0, sizeof gDerived);
  TEST_ASSERT_EQUAL_INT(IPCC_KDF_FAILED, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

void test_initial_pin_preparer_rejects_invalid_non_pin_config_after_derivation() {
  WriterBackend backend; backend.marker = false;
  ConfigBootCoordinator boot(backend);
  TEST_ASSERT_EQUAL_INT(CFG_BOOT_SETUP_REQUIRED, boot.start().code);
  Config candidate = validConfig(); candidate.pinSet = 0;
  memset(candidate.pinHash, 0, sizeof candidate.pinHash);
  memset(candidate.pinSalt, 0, sizeof candidate.pinSalt);
  candidate.termEnd = candidate.termStart - 1;
  const Config original = candidate; FakePinRandom random;
  Esp32MbedTlsInitialPinPreparer preparer(boot, random);
  TEST_ASSERT_EQUAL_INT(IPCC_INVALID_CONFIG, preparer.prepare("627194", 6, candidate));
  TEST_ASSERT_TRUE(configEqual(candidate, original));
  TEST_ASSERT_EQUAL_UINT32(1, random.calls);
  TEST_ASSERT_EQUAL_UINT32(1, gPbkdfCalls);
  TEST_ASSERT_EQUAL_UINT32(0, backend.writes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_pin_uses_sha256_hmac_pbkdf2_and_exact_stored_fields);
  RUN_TEST(test_wrong_derived_hash_is_a_mismatch_not_an_adapter_error);
  RUN_TEST(test_zero_iterations_or_invalid_config_never_creates_ready_verifier);
  RUN_TEST(test_malformed_candidate_is_rejected_before_mbedtls);
  RUN_TEST(test_missing_digest_or_mbedtls_setup_error_fails_closed);
  RUN_TEST(test_pbkdf_failure_is_not_treated_as_wrong_pin);
  RUN_TEST(test_pin_writer_derives_with_injected_iterations_and_commits_new_salt_hash);
  RUN_TEST(test_pin_writer_requires_explicit_nonzero_iteration_policy);
  RUN_TEST(test_pin_writer_rejects_weak_pin_before_entropy_or_pbkdf);
  RUN_TEST(test_pin_writer_refuses_rng_unavailable_without_storage_mutation);
  RUN_TEST(test_pin_writer_rejects_zero_or_reused_salt);
  RUN_TEST(test_pin_writer_kdf_error_is_indeterminate_and_never_commits);
  RUN_TEST(test_pin_config_runtime_preflights_pin_only_changes_without_mutating);
  RUN_TEST(test_pin_config_runtime_rejects_non_pin_config_changes);
  RUN_TEST(test_pin_config_runtime_requires_explicit_usable_iteration_policy);
  RUN_TEST(test_pin_writer_requires_ready_config_boot_before_entropy);
  RUN_TEST(test_pin_writer_generation_exhaustion_refuses_without_write);
  RUN_TEST(test_pin_writer_canonical_read_failure_faults_boot_and_runtime);
  RUN_TEST(test_pin_writer_indeterminate_slot_write_faults_without_retry);
  RUN_TEST(test_pin_writer_reload_failure_faults_boot_after_slot_write);
  RUN_TEST(test_pin_writer_zero_derived_hash_is_indeterminate_without_write);
  RUN_TEST(test_pin_runtime_invalid_credential_update_is_rejected_without_mutation);
  RUN_TEST(test_initial_pin_preparer_uses_rf_rng_and_exact_e35_work_factor_without_persisting);
  RUN_TEST(test_initial_pin_preparer_refuses_ready_boot_before_random_or_kdf);
  RUN_TEST(test_initial_pin_preparer_rejects_malformed_and_weak_pins_before_entropy);
  RUN_TEST(test_initial_pin_preparer_refuses_unavailable_rf_random_without_mutation);
  RUN_TEST(test_initial_pin_preparer_rejects_zero_salt_before_pbkdf);
  RUN_TEST(test_initial_pin_preparer_kdf_error_and_zero_hash_never_mutate_config);
  RUN_TEST(test_initial_pin_preparer_rejects_invalid_non_pin_config_after_derivation);
  return UNITY_END();
}
