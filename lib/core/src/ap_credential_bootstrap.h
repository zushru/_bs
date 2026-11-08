#pragma once

#include <stddef.h>
#include <stdint.h>
#include "config_boot.h"
#include "pin_auth_session.h"

namespace att {

// AP passphrase storage is separate from the PIN-attempt namespace and from
// the attendance config slot. The config still carries the active passphrase;
// this store makes first-boot creation durable before the setup AP starts.
static const size_t AP_CREDENTIAL_BUFFER_SIZE = 64;
static const size_t AP_CREDENTIAL_RANDOM_BYTES = 16;

enum ApCredentialStoreRead : uint8_t {
  APCR_READ_OK,
  APCR_NOT_FOUND,
  APCR_READ_ERROR
};

enum ApCredentialStoreWrite : uint8_t {
  APCW_CREATED,
  APCW_ALREADY_EXISTS,
  APCW_WRITE_ERROR
};

class IApCredentialStore {
 public:
  virtual ~IApCredentialStore() {}
  virtual ApCredentialStoreRead load(char out[AP_CREDENTIAL_BUFFER_SIZE]) = 0;
  // Must never replace an existing value. The caller performs a fresh read
  // after either APCW_CREATED or APCW_ALREADY_EXISTS.
  virtual ApCredentialStoreWrite createIfAbsent(const char* passphrase) = 0;
};

enum ApCredentialBootstrapCode : uint8_t {
  APCB_OK,
  APCB_INVALID_ARGUMENT,
  APCB_INVALID_BOOT_STATE,
  APCB_CONFIG_CREDENTIAL_INVALID,
  APCB_STORE_READ_FAILED,
  APCB_STORE_WRITE_FAILED,
  APCB_STORED_CREDENTIAL_INVALID,
  APCB_RANDOM_FAILED,
  APCB_CONFIG_MISMATCH
};

struct ApCredentialResult {
  ApCredentialBootstrapCode code;
  // True only while config boot still requires initial setup. The UI must
  // display the passphrase on every such boot until config reaches READY.
  bool disclose;
};

// Resolves the WPA2 AP passphrase before SoftAP startup.
//
// CFG_BOOT_NEEDS_SETUP: use the saved credential or create a 128-bit random
// 32-character lowercase-hex credential, persist it without overwrite, read
// it back, and request on-device disclosure on every setup-required boot.
//
// CFG_BOOT_READY: the validated Config::wifiPass is authoritative. If the
// separate credential record is missing (upgrade/restore), copy it once into
// the dedicated namespace. If a record exists but differs, fail closed.
//
// Calls must be serialized before AP/RF start. The output buffer contains a
// secret and must be cleared by the caller after AP configuration succeeds.
class ApCredentialBootstrap {
 public:
  ApCredentialBootstrap(IApCredentialStore& store, ISecureRandomSource& random)
      : store_(store), random_(random) {}

  ApCredentialBootstrap(const ApCredentialBootstrap&) = delete;
  ApCredentialBootstrap& operator=(const ApCredentialBootstrap&) = delete;

  ApCredentialResult prepare(ConfigBootState bootState,
                             const char* configuredPassphrase,
                             char out[AP_CREDENTIAL_BUFFER_SIZE]);

  static bool validPassphrase(const char* passphrase);

 private:
  static void clear_(void* value, size_t length);
  static size_t passphraseLength_(const char* passphrase);
  static bool equalSecret_(const char* a, const char* b);
  static void encodeHex_(const uint8_t raw[AP_CREDENTIAL_RANDOM_BYTES],
                         char out[AP_CREDENTIAL_BUFFER_SIZE]);
  ApCredentialResult fail_(ApCredentialBootstrapCode code,
                           char out[AP_CREDENTIAL_BUFFER_SIZE]) const;
  bool readAndValidate_(char out[AP_CREDENTIAL_BUFFER_SIZE],
                        ApCredentialStoreRead& readCode,
                        ApCredentialBootstrapCode& failureCode);
  bool persistAndReadBack_(const char* candidate, bool requireCandidateMatch,
                           char out[AP_CREDENTIAL_BUFFER_SIZE],
                           ApCredentialResult& failure);

  IApCredentialStore& store_;
  ISecureRandomSource& random_;
};

}  // namespace att
