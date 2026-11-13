#include "esp32_mbedtls_pin_verifier.h"

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>

namespace att {
namespace {

void secureClear(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

}  // namespace

Esp32MbedTlsPinVerifier::Esp32MbedTlsPinVerifier(
    const Config& config, uint32_t iterations)
    : salt_{0}, hash_{0}, iterations_(iterations), ready_(false) {
  // validateConfig() also enforces pinSet and a nonzero stored hash.
  // A zero iteration count is never treated as a usable verifier.
  if (iterations_ == 0 || !validateConfig(config)) return;
  memcpy(salt_, config.pinSalt, sizeof salt_);
  memcpy(hash_, config.pinHash, sizeof hash_);
  ready_ = true;
}

Esp32MbedTlsPinVerifier::~Esp32MbedTlsPinVerifier() {
  secureClear(salt_, sizeof salt_);
  secureClear(hash_, sizeof hash_);
  iterations_ = 0;
  ready_ = false;
}

PinVerifyOutcome Esp32MbedTlsPinVerifier::verify(
    const char* candidate, size_t length) {
  if (!ready_ || !validPin(candidate, length)) return PV_ERROR;

  uint8_t derived[sizeof hash_] = {};
  const mbedtls_md_info_t* digest =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (digest == nullptr) {
    secureClear(derived, sizeof derived);
    return PV_ERROR;
  }

  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  int result = mbedtls_md_setup(&context, digest, 1);
  if (result == 0) {
    result = mbedtls_pkcs5_pbkdf2_hmac(
        &context,
        reinterpret_cast<const unsigned char*>(candidate), length,
        salt_, sizeof salt_, iterations_,
        static_cast<uint32_t>(sizeof derived), derived);
  }
  mbedtls_md_free(&context);

  if (result != 0) {
    secureClear(derived, sizeof derived);
    return PV_ERROR;
  }

  const bool matches = ctEqual(derived, hash_, sizeof derived);
  secureClear(derived, sizeof derived);
  return matches ? PV_MATCH : PV_MISMATCH;
}

bool Esp32MbedTlsPinVerifier::updateCredential(const Config& config) {
  if (!ready_ || iterations_ == 0 || !validateConfig(config)) return false;

  // Copy only after validation. No KDF is performed during the post-commit
  // apply step; the exact salt/hash bytes were already committed and read back.
  uint8_t nextSalt[sizeof salt_];
  uint8_t nextHash[sizeof hash_];
  memcpy(nextSalt, config.pinSalt, sizeof nextSalt);
  memcpy(nextHash, config.pinHash, sizeof nextHash);

  secureClear(salt_, sizeof salt_);
  secureClear(hash_, sizeof hash_);
  memcpy(salt_, nextSalt, sizeof salt_);
  memcpy(hash_, nextHash, sizeof hash_);
  secureClear(nextSalt, sizeof nextSalt);
  secureClear(nextHash, sizeof nextHash);
  return true;
}

}  // namespace att
