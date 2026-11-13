#include "esp32_mbedtls_pin_config_runtime.h"

#include <string.h>

namespace att {
namespace {

void secureClear(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

Config bootConfigOrEmpty(ConfigBootCoordinator& configBoot) {
  const Config* current = configBoot.config();
  if (current != nullptr) return *current;
  Config empty = {};
  return empty;
}

}  // namespace

Esp32MbedTlsPinConfigRuntime::Esp32MbedTlsPinConfigRuntime(
    ConfigBootCoordinator& configBoot, uint32_t iterations)
    : configBoot_(configBoot), current_(bootConfigOrEmpty(configBoot)),
      verifier_(current_, iterations), faulted_(false) {
  if (!configBoot_.ready() || !validateConfig(current_) || !verifier_.ready())
    faulted_ = true;
}

Esp32MbedTlsPinConfigRuntime::~Esp32MbedTlsPinConfigRuntime() {
  faulted_ = true;
  secureClear(&current_, sizeof current_);
}

bool Esp32MbedTlsPinConfigRuntime::sameNonPinFields_(
    const Config& a, const Config& b) {
  Config left = a;
  Config right = b;
  left.pinSet = right.pinSet = 1;
  memset(left.pinHash, 0, sizeof left.pinHash);
  memset(right.pinHash, 0, sizeof right.pinHash);
  memset(left.pinSalt, 0, sizeof left.pinSalt);
  memset(right.pinSalt, 0, sizeof right.pinSalt);
  const bool same = configEqual(left, right);
  secureClear(&left, sizeof left);
  secureClear(&right, sizeof right);
  return same;
}

bool Esp32MbedTlsPinConfigRuntime::canApply(const Config& next) const {
  if (!ready() || !validateConfig(next) ||
      !sameNonPinFields_(current_, next)) {
    return false;
  }

  // A PIN update must actually replace at least one persisted credential
  // field. The higher-level writer always changes the salt as well.
  return memcmp(current_.pinSalt, next.pinSalt,
                sizeof current_.pinSalt) != 0 ||
         memcmp(current_.pinHash, next.pinHash,
                sizeof current_.pinHash) != 0;
}

bool Esp32MbedTlsPinConfigRuntime::applyAfterCommit(const Config& next) {
  if (faulted_) return false;
  if (!canApply(next) || !verifier_.updateCredential(next)) {
    // The caller invokes this after the new slot is durably selected. If the
    // in-RAM verifier cannot follow that state, this owner must remain unusable.
    faulted_ = true;
    return false;
  }

  current_ = next;
  return true;
}

}  // namespace att
