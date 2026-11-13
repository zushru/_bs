#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config_boot.h"
#include "esp32_mbedtls_pin_config_runtime.h"
#include "pin_auth.h"
#include "pin_auth_session.h"
#include "setup_code_sequence.h"

namespace att {

extern const uint32_t kE35AdminPinPbkdf2Iterations;

class Esp32MbedTlsInitialPinPreparer : public IInitialPinCredentialPreparer {
 public:
  Esp32MbedTlsInitialPinPreparer(ConfigBootCoordinator& configBoot,
                                ISecureRandomSource& random);
  InitialPinCredentialCode prepare(const char* pin, size_t length,
                                   Config& config) override;
 private:
  ConfigBootCoordinator& configBoot_;
  ISecureRandomSource& random_;
};


// PBKDF2-SHA256 administrator PIN writer for the existing Config format:
// 8-byte salt and 32-byte derived hash. The iteration count is mandatory
// constructor input; this adapter deliberately chooses no policy default.
//
// The concrete PIN runtime must use the same iteration count and hold the
// exact current Config snapshot as ConfigBootCoordinator. The writer refuses
// mismatched policy/state before generating a new credential.
//
// Invoke only through PinAuthCoordinator::changeAdminPin(), after current-PIN
// verification and durable attempt-counter reset. Secure random must be ready
// (Esp32SecureRandom requires the Wi-Fi RF lifecycle notification).
class Esp32MbedTlsAdminPinWriter : public IAdminPinWriter {
 public:
  Esp32MbedTlsAdminPinWriter(ConfigBootCoordinator& configBoot,
                             Esp32MbedTlsPinConfigRuntime& configRuntime,
                             ISecureRandomSource& random,
                             uint32_t iterations);
  ~Esp32MbedTlsAdminPinWriter() override;

  Esp32MbedTlsAdminPinWriter(const Esp32MbedTlsAdminPinWriter&) = delete;
  Esp32MbedTlsAdminPinWriter& operator=(
      const Esp32MbedTlsAdminPinWriter&) = delete;

  PinActionOutcome commitNewPin(const char* pin, size_t length) override;
  bool ready() const;

 private:
  ConfigBootCoordinator& configBoot_;
  Esp32MbedTlsPinConfigRuntime& configRuntime_;
  ISecureRandomSource& random_;
  uint32_t iterations_;
};

}  // namespace att
