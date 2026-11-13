#pragma once

#include <stddef.h>
#include <stdint.h>

#include "clock_confirmation.h"
#include "esp32_oled.h"
#include "esp32_secure_random.h"
#include "pin_auth.h"
#include "setup_code_sequence.h"

namespace att {

struct Esp32RamSetupCodeVerifierTestAccess;

class Esp32RamSetupCodeVerifier final : public ISetupCodeVerifier,
                                        public ISetupCodeIssuer {
 public:
  static const uint64_t kLifetimeMicros = 600000000ULL;

  // All dependencies must outlive this object. Calls are serialized on the
  // application owner task; the random source must be the same instance owned
  // by Esp32WifiAp.
  Esp32RamSetupCodeVerifier(Esp32SecureRandom& random,
                            IClockMonotonicSource& monotonic,
                            Esp32Oled& oled);
  ~Esp32RamSetupCodeVerifier() override;

  Esp32RamSetupCodeVerifier(const Esp32RamSetupCodeVerifier&) = delete;
  Esp32RamSetupCodeVerifier& operator=(const Esp32RamSetupCodeVerifier&) = delete;

  // ISetupCodeIssuer: generate and display only after the AP's RF gate is open.
  bool issue() override;

  // ISetupCodeVerifier: mismatch preserves the live code; a match consumes it.
  PinVerifyOutcome verify(const char* candidate, size_t length) override;

  // Serialized application-loop hook. Expiry clears RAM before requesting a
  // blank transfer; success does not acknowledge physical panel visibility.
  bool serviceExpiry();

  bool active() const override { return active_ && !faulted_; }
  bool faulted() const { return faulted_; }

 private:
  bool expireAndBlank_();
  bool requestBlank_();
  bool observeMonotonic_(uint64_t nowMicros);
  void wipeCode_();
  void latchFault_();
  void failClosed_();

  Esp32SecureRandom& random_;
  IClockMonotonicSource& monotonic_;
  Esp32Oled& oled_;
  char code_[7];
  uint64_t issuedAtMicros_;
  uint64_t lastObservedMicros_;
  bool hasObservedMicros_;
  bool active_;
  bool faulted_;

  friend struct Esp32RamSetupCodeVerifierTestAccess;
};

}  // namespace att
