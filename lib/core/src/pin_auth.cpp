#include "pin_auth.h"

namespace att {
namespace {
PinAuthResult pinResult(PinAuthCode code, uint32_t retryAfterS = 0) {
  PinAuthResult result = {code, retryAfterS};
  return result;
}

class PinWriterAction : public IPinSuccessAction {
 public:
  PinWriterAction(IAdminPinWriter& writer, const char* pin, size_t length)
      : writer_(writer), pin_(pin), length_(length) {}
  PinActionOutcome run() override {
    return writer_.commitNewPin(pin_, length_);
  }
 private:
  IAdminPinWriter& writer_;
  const char* pin_;
  size_t length_;
};
}  // namespace

bool PinAuthCoordinator::boot(uint32_t nowS) {
  if (ready_ || faulted_) return false;
  uint16_t persisted = 0;
  if (!store_.loadFailureCount(persisted)) {
    faulted_ = true;
    return false;
  }
  lockout_.boot(persisted, nowS);
  ready_ = true;
  return true;
}

PinAuthResult PinAuthCoordinator::failClosed_(PinAuthCode code) {
  faulted_ = true;
  ready_ = false;
  return pinResult(code);
}

PinAuthResult PinAuthCoordinator::authenticateAndRun(
    const char* candidate, size_t length, uint32_t nowS,
    IPinVerifier& verifier, IPinSuccessAction& action) {
  return run_(candidate, length, nowS, verifier, action);
}

PinAuthResult PinAuthCoordinator::run_(const char* candidate, size_t length,
                                       uint32_t nowS, IPinVerifier& verifier,
                                       IPinSuccessAction& action) {
  if (!ready()) return pinResult(PAC_NOT_READY);
  if (!validPin(candidate, length)) return pinResult(PAC_BAD_INPUT);

  const LockGate gate = lockout_.begin(nowS);
  if (!gate.allowed) return pinResult(PAC_LOCKED, gate.retryAfterS);

  // The failure count is committed BEFORE the candidate reaches the verifier.
  // If this write is uncertain, do not compare the secret or continue serving.
  if (!store_.persistFailureCount(lockout_.fails()))
    return failClosed_(PAC_STORAGE_ERROR);

  const PinVerifyOutcome verified = verifier.verify(candidate, length);
  if (verified == PV_MISMATCH) {
    lockout_.fail(nowS);
    return pinResult(PAC_BAD_SECRET);
  }
  if (verified != PV_MATCH) return failClosed_(PAC_VERIFY_ERROR);

  // Clear the durable counter before a token or privileged action can be
  // created. A reset-write failure must never be followed by the action.
  lockout_.success();
  if (!store_.persistFailureCount(0))
    return failClosed_(PAC_STORAGE_ERROR);

  const PinActionOutcome outcome = action.run();
  if (outcome == PA_DONE) return pinResult(PAC_OK);
  if (outcome == PA_INDETERMINATE)
    return failClosed_(PAC_ACTION_INDETERMINATE);
  return pinResult(PAC_ACTION_REFUSED);
}

PinAuthResult PinAuthCoordinator::changeAdminPin(
    const char* currentPin, size_t currentLength, const char* newPin,
    size_t newLength, uint32_t nowS, IPinVerifier& currentVerifier,
    IAdminPinWriter& writer) {
  if (!validPin(newPin, newLength)) return pinResult(PAC_BAD_INPUT);
  if (weakPin(newPin, newLength)) return pinResult(PAC_WEAK_NEW_PIN);
  PinWriterAction action(writer, newPin, newLength);
  return run_(currentPin, currentLength, nowS, currentVerifier, action);
}

}  // namespace att
