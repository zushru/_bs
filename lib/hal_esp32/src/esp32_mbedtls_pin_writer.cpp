#include "esp32_mbedtls_pin_writer.h"
#include "api_logic.h"

#include <limits.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>

namespace att {

const uint32_t kE35AdminPinPbkdf2Iterations = 2048;

namespace {

void secureClear(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

bool allZero(const uint8_t* value, size_t length) {
  uint8_t aggregate = 0;
  for (size_t i = 0; i < length; ++i) aggregate |= value[i];
  return aggregate == 0;
}

bool derivePinHash(const char* pin, size_t length, const uint8_t salt[8],
                   uint32_t iterations, uint8_t hash[32]) {
  if (pin == nullptr || salt == nullptr || hash == nullptr ||
      iterations == 0 ||
      iterations > static_cast<uint32_t>(UINT_MAX)) {
    return false;
  }

  const mbedtls_md_info_t* digest =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (digest == nullptr) return false;

  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  int result = mbedtls_md_setup(&context, digest, 1);
  if (result == 0) {
    result = mbedtls_pkcs5_pbkdf2_hmac(
        &context,
        reinterpret_cast<const unsigned char*>(pin), length,
        salt, 8, static_cast<unsigned int>(iterations),
        32, hash);
  }
  mbedtls_md_free(&context);
  return result == 0;
}

}  // namespace

Esp32MbedTlsInitialPinPreparer::Esp32MbedTlsInitialPinPreparer(
    ConfigBootCoordinator& configBoot, ISecureRandomSource& random)
    : configBoot_(configBoot), random_(random) {}

InitialPinCredentialCode Esp32MbedTlsInitialPinPreparer::prepare(
    const char* pin, size_t length, Config& config) {
  if (!configBoot_.needsSetup()) return IPCC_NOT_NEEDS_SETUP;
  if (!validPin(pin, length) || weakPin(pin, length)) return IPCC_INVALID_PIN;
  uint8_t salt[8] = {};
  uint8_t hash[32] = {};
  if (!random_.fill(salt, sizeof salt)) {
    secureClear(salt, sizeof salt); secureClear(hash, sizeof hash);
    return IPCC_RANDOM_FAILED;
  }
  if (allZero(salt, sizeof salt)) {
    secureClear(salt, sizeof salt); secureClear(hash, sizeof hash);
    return IPCC_INVALID_SALT;
  }
  if (!derivePinHash(pin, length, salt, kE35AdminPinPbkdf2Iterations, hash) ||
      allZero(hash, sizeof hash)) {
    secureClear(salt, sizeof salt); secureClear(hash, sizeof hash);
    return IPCC_KDF_FAILED;
  }
  Config candidate = config;
  candidate.pinSet = 1;
  memcpy(candidate.pinSalt, salt, sizeof salt);
  memcpy(candidate.pinHash, hash, sizeof hash);
  const bool valid = validateConfig(candidate);
  if (valid) {
    config.pinSet = candidate.pinSet;
    memcpy(config.pinSalt, candidate.pinSalt, sizeof config.pinSalt);
    memcpy(config.pinHash, candidate.pinHash, sizeof config.pinHash);
  }
  secureClear(&candidate, sizeof candidate);
  secureClear(salt, sizeof salt);
  secureClear(hash, sizeof hash);
  return valid ? IPCC_PREPARED : IPCC_INVALID_CONFIG;
}

Esp32MbedTlsAdminPinWriter::Esp32MbedTlsAdminPinWriter(
    ConfigBootCoordinator& configBoot,
    Esp32MbedTlsPinConfigRuntime& configRuntime,
    ISecureRandomSource& random, uint32_t iterations)
    : configBoot_(configBoot), configRuntime_(configRuntime),
      random_(random), iterations_(iterations) {}

Esp32MbedTlsAdminPinWriter::~Esp32MbedTlsAdminPinWriter() {
  iterations_ = 0;
}

bool Esp32MbedTlsAdminPinWriter::ready() const {
  const Config* bootConfig = configBoot_.config();
  const Config* runtimeConfig = configRuntime_.config();
  return bootConfig != nullptr && runtimeConfig != nullptr &&
         configRuntime_.configBoot() == &configBoot_ &&
         configEqual(*bootConfig, *runtimeConfig) &&
         configRuntime_.iterations() == iterations_ && iterations_ != 0 &&
         iterations_ <= static_cast<uint32_t>(UINT_MAX);
}

PinActionOutcome Esp32MbedTlsAdminPinWriter::commitNewPin(
    const char* pin, size_t length) {
  if (!ready() || !validPin(pin, length) || weakPin(pin, length))
    return PA_REFUSED;

  const Config* current = configBoot_.config();
  if (current == nullptr) return PA_REFUSED;

  uint8_t salt[8] = {};
  uint8_t hash[32] = {};

  if (!random_.fill(salt, sizeof salt)) {
    secureClear(salt, sizeof salt);
    secureClear(hash, sizeof hash);
    return PA_REFUSED;
  }

  // A zero salt or collision is treated as an unusable entropy result. Do not
  // silently reuse the previous salt and do not retry with a hidden policy.
  if (allZero(salt, sizeof salt) ||
      ctEqual(salt, current->pinSalt, sizeof salt)) {
    secureClear(salt, sizeof salt);
    secureClear(hash, sizeof hash);
    return PA_REFUSED;
  }

  if (!derivePinHash(pin, length, salt, iterations_, hash) ||
      allZero(hash, sizeof hash)) {
    secureClear(salt, sizeof salt);
    secureClear(hash, sizeof hash);
    return PA_INDETERMINATE;
  }

  const ConfigAdminPinUpdateResult committed =
      configBoot_.commitAdminPinCredential(salt, hash, configRuntime_);

  secureClear(salt, sizeof salt);
  secureClear(hash, sizeof hash);

  switch (committed.code) {
    case CFG_ADMIN_PIN_UPDATE_OK:
      return PA_DONE;

    case CFG_ADMIN_PIN_UPDATE_NOT_READY:
    case CFG_ADMIN_PIN_UPDATE_INVALID:
    case CFG_ADMIN_PIN_UPDATE_BUSY:
    case CFG_ADMIN_PIN_UPDATE_SEQUENCE_EXHAUSTED:
      return PA_REFUSED;

    case CFG_ADMIN_PIN_UPDATE_VERIFY_FAILED:
    case CFG_ADMIN_PIN_UPDATE_STORAGE_INDETERMINATE:
    case CFG_ADMIN_PIN_UPDATE_RUNTIME_FAILED:
    default:
      return PA_INDETERMINATE;
  }
}

}  // namespace att
