#pragma once

#include <stddef.h>
#include <stdint.h>
#include "api_logic.h"

namespace att {

// Persistent attempt-counter boundary. Implementations must return true only
// after the value is durably committed and read-back verified. A mere
// successful enqueue/write call is not sufficient.
class IPinAttemptStore {
 public:
  virtual ~IPinAttemptStore() {}
  virtual bool loadFailureCount(uint16_t& out) = 0;
  virtual bool persistFailureCount(uint16_t failures) = 0;
};

enum PinVerifyOutcome : uint8_t { PV_MATCH, PV_MISMATCH, PV_ERROR };
class IPinVerifier {
 public:
  virtual ~IPinVerifier() {}
  // Implement the signed PBKDF2-SHA256 credential check in the platform adapter.
  // PV_ERROR is distinct from a wrong PIN and must fail the coordinator closed.
  virtual PinVerifyOutcome verify(const char* candidate, size_t length) = 0;
};

enum PinActionOutcome : uint8_t { PA_DONE, PA_REFUSED, PA_INDETERMINATE };
class IPinSuccessAction {
 public:
  virtual ~IPinSuccessAction() {}
  // Called only after a matching PIN and a durable failure-counter reset.
  // On PA_INDETERMINATE the action must ensure no partially issued session or
  // other privilege remains usable; the coordinator then latches until reboot.
  virtual PinActionOutcome run() = 0;
};

class IAdminPinWriter {
 public:
  virtual ~IAdminPinWriter() {}
  // Must derive/store the salted hash, commit it durably and verify read-back.
  // Never store the raw PIN. Return PA_INDETERMINATE if commit state is unclear.
  virtual PinActionOutcome commitNewPin(const char* pin, size_t length) = 0;
};

enum PinAuthCode : uint8_t {
  PAC_OK,
  PAC_BAD_SECRET,
  PAC_LOCKED,
  PAC_BAD_INPUT,
  PAC_WEAK_NEW_PIN,
  PAC_NOT_READY,
  PAC_STORAGE_ERROR,
  PAC_VERIFY_ERROR,
  PAC_ACTION_REFUSED,
  PAC_ACTION_INDETERMINATE
};
struct PinAuthResult {
  PinAuthCode code;
  uint32_t retryAfterS;
};

// One long-lived coordinator per chosen lockout-counter domain. The caller
// decides which secret routes share that domain according to the signed and
// delegated product policy; do not construct a fresh instance per request.
// Synchronous and serialized-main-loop only; protect the entire operation with
// one external mutex if a future server invokes it from multiple tasks.
// This module does not implement NVS, PBKDF2, HTTP, token randomness, or HAL.
class PinAuthCoordinator {
 public:
  explicit PinAuthCoordinator(IPinAttemptStore& store)
      : store_(store), ready_(false), faulted_(false) {}

  // Call once during boot, before accepting any PIN/setup-code requests.
  // Storage failure latches the coordinator and prevents all checks.
  bool boot(uint32_t nowS);

  // Used for login and setup-code-gated operations. Every successful verify
  // must pass through a required action (e.g. issue a session or continue the
  // setup operation); callers must not separately verify the same secret.
  PinAuthResult authenticateAndRun(const char* candidate, size_t length,
                                    uint32_t nowS, IPinVerifier& verifier,
                                    IPinSuccessAction& action);

  // PIN change always authenticates the CURRENT PIN first. New PIN syntax and
  // weak-pattern checks happen before consuming an old-PIN attempt. The writer
  // owns PBKDF2 salt/hash generation and durable config commit.
  PinAuthResult changeAdminPin(const char* currentPin, size_t currentLength,
                               const char* newPin, size_t newLength,
                               uint32_t nowS, IPinVerifier& currentVerifier,
                               IAdminPinWriter& writer);

  bool ready() const { return ready_ && !faulted_; }
  bool faulted() const { return faulted_; }
  uint16_t failures() const { return lockout_.fails(); }

 private:
  PinAuthResult failClosed_(PinAuthCode code);
  PinAuthResult run_(const char* candidate, size_t length, uint32_t nowS,
                     IPinVerifier& verifier, IPinSuccessAction& action);

  IPinAttemptStore& store_;
  PinLockout lockout_;
  bool ready_;
  bool faulted_;
};

}  // namespace att
