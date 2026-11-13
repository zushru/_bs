#pragma once

#include "config_store.h"
#include "pin_auth.h"

namespace att {

class Esp32MbedTlsPinConfigRuntime;

// PBKDF2-SHA256 verifier for the credential already stored in Config.
// The iteration count is mandatory and injected by the eventual policy owner:
// this module deliberately does not invent or persist a KDF parameter.
class Esp32MbedTlsPinVerifier : public IPinVerifier {
 public:
  Esp32MbedTlsPinVerifier(const Config& config, uint32_t iterations);
  ~Esp32MbedTlsPinVerifier() override;

  Esp32MbedTlsPinVerifier(const Esp32MbedTlsPinVerifier&) = delete;
  Esp32MbedTlsPinVerifier& operator=(const Esp32MbedTlsPinVerifier&) = delete;

  PinVerifyOutcome verify(const char* candidate, size_t length) override;

  bool ready() const { return ready_; }
  uint32_t iterations() const { return iterations_; }

 private:
  friend class Esp32MbedTlsPinConfigRuntime;

  // Only the boot-bound PIN runtime may replace key material, and only after
  // the matching Config transaction has been verified.
  bool updateCredential(const Config& config);

  uint8_t salt_[8];
  uint8_t hash_[32];
  uint32_t iterations_;
  bool ready_;
};

}  // namespace att
