#include "esp32_ram_setup_code_verifier.h"

namespace att {
namespace {

void secureWipe(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

}  // namespace

Esp32RamSetupCodeVerifier::Esp32RamSetupCodeVerifier(
    Esp32SecureRandom& random, IClockMonotonicSource& monotonic,
    Esp32Oled& oled)
    : random_(random), monotonic_(monotonic), oled_(oled), code_{0},
      issuedAtMicros_(0), lastObservedMicros_(0), hasObservedMicros_(false),
      active_(false), faulted_(false) {}

Esp32RamSetupCodeVerifier::~Esp32RamSetupCodeVerifier() {
  if (active_) (void)requestBlank_();
  wipeCode_();
  active_ = false;
  issuedAtMicros_ = 0;
}

void Esp32RamSetupCodeVerifier::wipeCode_() {
  secureWipe(code_, sizeof code_);
}

bool Esp32RamSetupCodeVerifier::requestBlank_() {
  return oled_.clear() == EOA_TRANSFER_REQUESTED;
}

bool Esp32RamSetupCodeVerifier::observeMonotonic_(uint64_t nowMicros) {
  if (hasObservedMicros_ && nowMicros < lastObservedMicros_) return false;
  lastObservedMicros_ = nowMicros;
  hasObservedMicros_ = true;
  return true;
}

void Esp32RamSetupCodeVerifier::latchFault_() {
  active_ = false;
  issuedAtMicros_ = 0;
  wipeCode_();
  faulted_ = true;
}

void Esp32RamSetupCodeVerifier::failClosed_() {
  latchFault_();
  (void)requestBlank_();
}

bool Esp32RamSetupCodeVerifier::expireAndBlank_() {
  active_ = false;
  issuedAtMicros_ = 0;
  wipeCode_();
  if (requestBlank_()) return true;
  // Do not make a second bus/display request after a failed blank transfer.
  latchFault_();
  return false;
}

bool Esp32RamSetupCodeVerifier::issue() {
  if (faulted_) return false;
  if (!random_.rfStarted()) {
    failClosed_();
    return false;
  }

  uint64_t observedMicros = 0;
  if (!monotonic_.nowMicros(observedMicros) ||
      !observeMonotonic_(observedMicros)) {
    failClosed_();
    return false;
  }

  if (active_) {
    if (observedMicros - issuedAtMicros_ < kLifetimeMicros) {
      return false;
    }
    // The old code is invalidated and a blank transfer is requested before
    // any replacement code is generated or displayed.
    if (!expireAndBlank_()) return false;
  }

  if (generateSetupCode(random_, code_) != SCGC_OK) {
    failClosed_();
    return false;
  }

  if (oled_.showSetupCode(code_, 6) != EOA_TRANSFER_REQUESTED) {
    failClosed_();
    return false;
  }

  // Start the lifetime after the display transfer was requested. A failed or
  // regressing clock read invalidates the newly displayed code immediately.
  uint64_t issuedAt = 0;
  if (!monotonic_.nowMicros(issuedAt) || issuedAt < observedMicros ||
      !observeMonotonic_(issuedAt)) {
    failClosed_();
    return false;
  }

  issuedAtMicros_ = issuedAt;
  active_ = true;
  return true;
}

PinVerifyOutcome Esp32RamSetupCodeVerifier::verify(const char* candidate,
                                                    size_t length) {
  if (faulted_) return PV_ERROR;
  if (!active_) return PV_MISMATCH;
  if (!random_.rfStarted()) {
    failClosed_();
    return PV_ERROR;
  }

  uint64_t nowMicros = 0;
  if (!monotonic_.nowMicros(nowMicros) ||
      !observeMonotonic_(nowMicros) || nowMicros < issuedAtMicros_) {
    failClosed_();
    return PV_ERROR;
  }

  if (nowMicros - issuedAtMicros_ >= kLifetimeMicros) {
    return expireAndBlank_() ? PV_MISMATCH : PV_ERROR;
  }

  // Always compare six positions. Length/null/digit validity contribute to
  // the accumulated difference; a mismatch never consumes the active code.
  uint8_t difference = static_cast<uint8_t>(
      candidate == nullptr || length != 6 ? 1 : 0);
  for (size_t i = 0; i < 6; ++i) {
    uint8_t supplied = 0;
    if (candidate != nullptr && i < length) {
      supplied = static_cast<uint8_t>(candidate[i]);
    }
    const uint8_t isDigit =
        static_cast<uint8_t>(supplied >= '0' && supplied <= '9');
    difference |= static_cast<uint8_t>(supplied ^
                                       static_cast<uint8_t>(code_[i]));
    difference |= static_cast<uint8_t>(isDigit ^ 1U);
  }

  if (difference != 0) return PV_MISMATCH;

  // Consume before attempting the blank transfer. A display/bus failure must
  // never make an already-matched code reusable or permit the protected action.
  active_ = false;
  issuedAtMicros_ = 0;
  wipeCode_();
  if (!requestBlank_()) {
    // The failed transfer was the one blank attempt; latch without retrying it.
    latchFault_();
    return PV_ERROR;
  }
  return PV_MATCH;
}

bool Esp32RamSetupCodeVerifier::serviceExpiry() {
  if (faulted_) return false;
  if (!active_) return true;

  if (!random_.rfStarted()) {
    failClosed_();
    return false;
  }

  uint64_t nowMicros = 0;
  if (!monotonic_.nowMicros(nowMicros) ||
      !observeMonotonic_(nowMicros) || nowMicros < issuedAtMicros_) {
    failClosed_();
    return false;
  }

  if (nowMicros - issuedAtMicros_ < kLifetimeMicros) return true;
  return expireAndBlank_();
}

}  // namespace att
