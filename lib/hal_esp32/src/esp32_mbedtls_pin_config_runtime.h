#pragma once

#include "config_boot.h"
#include "esp32_mbedtls_pin_verifier.h"

namespace att {

// Main-loop-only bridge for the credential-only A/B config transaction.
// Owns the live PBKDF2 verifier and the matching applied Config snapshot.
// It rejects changes to every field except pinSalt/pinHash; it is not a
// general ConfigApplyRuntime and must not be used for timetable changes.
class Esp32MbedTlsPinConfigRuntime : public IConfigApplyRuntime {
 public:
  Esp32MbedTlsPinConfigRuntime(ConfigBootCoordinator& configBoot,
                               uint32_t iterations);
  ~Esp32MbedTlsPinConfigRuntime() override;
  Esp32MbedTlsPinConfigRuntime(const Esp32MbedTlsPinConfigRuntime&) = delete;
  Esp32MbedTlsPinConfigRuntime& operator=(
      const Esp32MbedTlsPinConfigRuntime&) = delete;

  bool canApply(const Config& next) const override;
  bool applyAfterCommit(const Config& next) override;

  bool ready() const {
    const Config* bootConfig = configBoot_.config();
    return !faulted_ && bootConfig != nullptr && verifier_.ready() &&
           validateConfig(current_) && configEqual(*bootConfig, current_);
  }
  bool faulted() const { return faulted_; }
  const ConfigBootCoordinator* configBoot() const { return &configBoot_; }
  uint32_t iterations() const { return verifier_.iterations(); }
  const Config* config() const { return ready() ? &current_ : nullptr; }
  Esp32MbedTlsPinVerifier* verifier() {
    return ready() ? &verifier_ : nullptr;
  }

 private:
  static bool sameNonPinFields_(const Config& a, const Config& b);
  ConfigBootCoordinator& configBoot_;
  Config current_;
  Esp32MbedTlsPinVerifier verifier_;
  bool faulted_;
};

}  // namespace att
