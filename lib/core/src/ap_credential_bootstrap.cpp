#include "ap_credential_bootstrap.h"
#include <string.h>

namespace att {
namespace {
const char HEX[] = "0123456789abcdef";
}

void ApCredentialBootstrap::clear_(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

size_t ApCredentialBootstrap::passphraseLength_(const char* passphrase) {
  if (passphrase == nullptr) return AP_CREDENTIAL_BUFFER_SIZE;
  size_t length = 0;
  while (length < AP_CREDENTIAL_BUFFER_SIZE && passphrase[length] != '\0') ++length;
  return length;
}

bool ApCredentialBootstrap::validPassphrase(const char* passphrase) {
  const size_t length = passphraseLength_(passphrase);
  if (length < 8 || length > 63) return false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t ch = static_cast<uint8_t>(passphrase[i]);
    if (ch < 0x20 || ch > 0x7E) return false;
  }
  return true;
}

bool ApCredentialBootstrap::equalSecret_(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) return false;
  const size_t aLength = passphraseLength_(a);
  const size_t bLength = passphraseLength_(b);
  uint8_t difference = static_cast<uint8_t>(aLength ^ bLength);
  for (size_t i = 0; i < AP_CREDENTIAL_BUFFER_SIZE; ++i) {
    const uint8_t av = i < aLength ? static_cast<uint8_t>(a[i]) : 0;
    const uint8_t bv = i < bLength ? static_cast<uint8_t>(b[i]) : 0;
    difference = static_cast<uint8_t>(difference | (av ^ bv));
  }
  return difference == 0;
}

void ApCredentialBootstrap::encodeHex_(
    const uint8_t raw[AP_CREDENTIAL_RANDOM_BYTES],
    char out[AP_CREDENTIAL_BUFFER_SIZE]) {
  memset(out, 0, AP_CREDENTIAL_BUFFER_SIZE);
  for (size_t i = 0; i < AP_CREDENTIAL_RANDOM_BYTES; ++i) {
    out[i * 2] = HEX[(raw[i] >> 4) & 0x0F];
    out[i * 2 + 1] = HEX[raw[i] & 0x0F];
  }
}

ApCredentialResult ApCredentialBootstrap::fail_(
    ApCredentialBootstrapCode code,
    char out[AP_CREDENTIAL_BUFFER_SIZE]) const {
  if (out != nullptr) clear_(out, AP_CREDENTIAL_BUFFER_SIZE);
  return ApCredentialResult{code, false};
}

bool ApCredentialBootstrap::readAndValidate_(
    char out[AP_CREDENTIAL_BUFFER_SIZE], ApCredentialStoreRead& readCode,
    ApCredentialBootstrapCode& failureCode) {
  memset(out, 0, AP_CREDENTIAL_BUFFER_SIZE);
  readCode = store_.load(out);
  failureCode = APCB_STORE_READ_FAILED;
  if (readCode == APCR_NOT_FOUND) return false;
  if (readCode != APCR_READ_OK) return false;
  if (!validPassphrase(out)) {
    failureCode = APCB_STORED_CREDENTIAL_INVALID;
    return false;
  }
  return true;
}

bool ApCredentialBootstrap::persistAndReadBack_(
    const char* candidate, bool requireCandidateMatch,
    char out[AP_CREDENTIAL_BUFFER_SIZE], ApCredentialResult& failure) {
  const ApCredentialStoreWrite write = store_.createIfAbsent(candidate);
  if (write == APCW_WRITE_ERROR) {
    failure = fail_(APCB_STORE_WRITE_FAILED, out);
    return false;
  }

  char readBack[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  const ApCredentialStoreRead read = store_.load(readBack);
  if (read != APCR_READ_OK) {
    clear_(readBack, sizeof readBack);
    failure = fail_(APCB_STORE_READ_FAILED, out);
    return false;
  }
  if (!validPassphrase(readBack)) {
    clear_(readBack, sizeof readBack);
    failure = fail_(APCB_STORED_CREDENTIAL_INVALID, out);
    return false;
  }
  if (requireCandidateMatch && !equalSecret_(candidate, readBack)) {
    clear_(readBack, sizeof readBack);
    failure = fail_(APCB_CONFIG_MISMATCH, out);
    return false;
  }
  memcpy(out, readBack, sizeof readBack);
  clear_(readBack, sizeof readBack);
  return true;
}

ApCredentialResult ApCredentialBootstrap::prepare(
    ConfigBootState bootState, const char* configuredPassphrase,
    char out[AP_CREDENTIAL_BUFFER_SIZE]) {
  if (out == nullptr) return ApCredentialResult{APCB_INVALID_ARGUMENT, false};
  clear_(out, AP_CREDENTIAL_BUFFER_SIZE);

  if (bootState == CFG_BOOT_NEEDS_SETUP) {
    char existing[AP_CREDENTIAL_BUFFER_SIZE] = {0};
    ApCredentialStoreRead readCode = APCR_READ_ERROR;
    ApCredentialBootstrapCode failureCode = APCB_STORE_READ_FAILED;
    if (readAndValidate_(existing, readCode, failureCode)) {
      memcpy(out, existing, sizeof existing);
      clear_(existing, sizeof existing);
      return ApCredentialResult{APCB_OK, true};
    }
    clear_(existing, sizeof existing);
    if (readCode == APCR_READ_ERROR) return fail_(failureCode, out);
    if (readCode != APCR_NOT_FOUND) return fail_(failureCode, out);

    uint8_t raw[AP_CREDENTIAL_RANDOM_BYTES] = {0};
    if (!random_.fill(raw, sizeof raw)) {
      clear_(raw, sizeof raw);
      return fail_(APCB_RANDOM_FAILED, out);
    }
    char candidate[AP_CREDENTIAL_BUFFER_SIZE] = {0};
    encodeHex_(raw, candidate);
    clear_(raw, sizeof raw);
    ApCredentialResult persisted = {APCB_STORE_WRITE_FAILED, false};
    const bool ok = persistAndReadBack_(candidate, false, out, persisted);
    clear_(candidate, sizeof candidate);
    if (!ok) return persisted;
    return ApCredentialResult{APCB_OK, true};
  }

  if (bootState == CFG_BOOT_READY) {
    if (!validPassphrase(configuredPassphrase))
      return fail_(APCB_CONFIG_CREDENTIAL_INVALID, out);

    char existing[AP_CREDENTIAL_BUFFER_SIZE] = {0};
    ApCredentialStoreRead readCode = APCR_READ_ERROR;
    ApCredentialBootstrapCode failureCode = APCB_STORE_READ_FAILED;
    if (readAndValidate_(existing, readCode, failureCode)) {
      const bool matches = equalSecret_(existing, configuredPassphrase);
      if (matches) memcpy(out, existing, sizeof existing);
      clear_(existing, sizeof existing);
      if (!matches) return fail_(APCB_CONFIG_MISMATCH, out);
      return ApCredentialResult{APCB_OK, false};
    }
    clear_(existing, sizeof existing);
    if (readCode == APCR_READ_ERROR) return fail_(failureCode, out);
    if (readCode != APCR_NOT_FOUND) return fail_(failureCode, out);

    ApCredentialResult persisted = {APCB_STORE_WRITE_FAILED, false};
    if (!persistAndReadBack_(configuredPassphrase, true, out, persisted)) return persisted;
    return ApCredentialResult{APCB_OK, false};
  }

  return fail_(APCB_INVALID_BOOT_STATE, out);
}

}  // namespace att
