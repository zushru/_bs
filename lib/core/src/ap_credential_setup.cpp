#include "ap_credential_setup.h"
#include <string.h>

namespace att {
namespace {

// ConfigBootCoordinator accepts a gate only for the duration of its synchronous
// provision call. This stack adapter binds that call to the current request's
// authorization object; the coordinator never retains that object.
class RequestProvisionGate : public IConfigProvisionGate {
 public:
  RequestProvisionGate(const ApCredentialSetupCoordinator& setup,
                       const IConfigProvisionGate& requestAuthGate)
      : setup_(setup), requestAuthGate_(requestAuthGate) {}
  bool mayProvision() const override {
    return setup_.mayProvision(requestAuthGate_);
  }
 private:
  const ApCredentialSetupCoordinator& setup_;
  const IConfigProvisionGate& requestAuthGate_;
};

}  // namespace

ApCredentialSetupCoordinator::ApCredentialSetupCoordinator(
    ConfigBootCoordinator& boot, ApCredentialBootstrap& bootstrap,
    IApCredentialDisplay& display, ICredentialSoftAp& ap)
    : boot_(boot), bootstrap_(bootstrap), display_(display), ap_(ap),
      state_(ACSS_COLD) {}

void ApCredentialSetupCoordinator::clearSecret_(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

bool ApCredentialSetupCoordinator::validDisclosureSecret_(const char* value,
                                                            size_t length) {
  if (value == nullptr || length != AP_CREDENTIAL_RANDOM_BYTES * 2) return false;
  for (size_t i = 0; i < length; ++i) {
    const char ch = value[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  }
  return value[length] == '\0';
}

bool ApCredentialSetupCoordinator::requestFaultClear_() {
  if (display_.requestBlankFrame()) {
    state_ = ACSS_FAULT;
    return true;
  }
  state_ = ACSS_STARTUP_CLEAR_PENDING;
  return false;
}

ApCredentialSetupCode ApCredentialSetupCoordinator::start() {
  if (state_ != ACSS_COLD) return ACSE_ALREADY_STARTED;
  if (!boot_.needsSetup()) return ACSE_BOOT_NOT_SETUP_REQUIRED;

  char passphrase[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  const ApCredentialResult credential =
      bootstrap_.prepare(boot_.state(), nullptr, passphrase);
  if (credential.code != APCB_OK || !credential.disclose) {
    clearSecret_(passphrase, sizeof passphrase);
    state_ = ACSS_FAULT;
    return ACSE_BOOTSTRAP_FAILED;
  }

  size_t length = 0;
  while (length < sizeof passphrase && passphrase[length] != '\0') ++length;
  if (!validDisclosureSecret_(passphrase, length)) {
    clearSecret_(passphrase, sizeof passphrase);
    state_ = ACSS_FAULT;
    return ACSE_BOOTSTRAP_FAILED;
  }

  // The secured AP is deliberately not started until the credential frame has
  // been requested. Adafruit's API cannot confirm actual pixels; successful
  // DHCP assignment later provides end-to-end evidence that a client could use
  // the random secret that was disclosed through the OLED-only channel.
  if (!display_.requestCredentialDisplay(credential, passphrase, length)) {
    requestFaultClear_();
    clearSecret_(passphrase, sizeof passphrase);
    return ACSE_DISPLAY_FAILED;
  }

  if (!ap_.start(kProductionSoftApSsid, sizeof kProductionSoftApSsid - 1,
                 passphrase, length, kProductionSoftApChannel)) {
    requestFaultClear_();
    clearSecret_(passphrase, sizeof passphrase);
    return ACSE_AP_START_FAILED;
  }

  clearSecret_(passphrase, sizeof passphrase);
  state_ = ACSS_WAITING_FOR_CLIENT;
  return ACSE_STARTED;
}

bool ApCredentialSetupCoordinator::mayProvision(
    const IConfigProvisionGate& requestAuthGate) const {
  return state_ == ACSS_WAITING_FOR_CLIENT && boot_.needsSetup() &&
         ap_.ready() && ap_.hasAssignedClientIp() &&
         requestAuthGate.mayProvision();
}

ApCredentialSetupCode ApCredentialSetupCoordinator::provision(
    const Config& initial, const IConfigProvisionGate& requestAuthGate,
    ConfigBootResult& bootResult) {
  if (state_ != ACSS_WAITING_FOR_CLIENT || !mayProvision(requestAuthGate))
    return ACSE_NOT_READY_FOR_PROVISION;

  // Re-read the durable credential and make it authoritative in the config
  // being committed. Otherwise a caller-supplied Config::wifiPass could differ
  // from the AP password just disclosed/used, causing the next READY boot to
  // fail the bootstrap mismatch check and strand the device.
  char passphrase[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  const ApCredentialResult credential =
      bootstrap_.prepare(boot_.state(), nullptr, passphrase);
  if (credential.code != APCB_OK || !credential.disclose) {
    clearSecret_(passphrase, sizeof passphrase);
    requestFaultClear_();
    return ACSE_BOOTSTRAP_FAILED;
  }
  size_t passphraseLength = 0;
  while (passphraseLength < sizeof passphrase &&
         passphrase[passphraseLength] != '\0') ++passphraseLength;
  if (!validDisclosureSecret_(passphrase, passphraseLength)) {
    clearSecret_(passphrase, sizeof passphrase);
    requestFaultClear_();
    return ACSE_BOOTSTRAP_FAILED;
  }

  Config provisionConfig = initial;
  memset(provisionConfig.wifiPass, 0, sizeof provisionConfig.wifiPass);
  memcpy(provisionConfig.wifiPass, passphrase, passphraseLength);
  clearSecret_(passphrase, sizeof passphrase);

  // Bind the core provisioner to this operation's request authorization. The
  // stack gate forwards the AP/DHCP/auth conditions and cannot outlive this call.
  RequestProvisionGate compositeGate(*this, requestAuthGate);
  bootResult = boot_.provision(provisionConfig, compositeGate);
  clearSecret_(provisionConfig.wifiPass, sizeof provisionConfig.wifiPass);
  if (bootResult.code == CFG_BOOT_PROVISION_REJECTED) {
    return ACSE_CONFIG_REJECTED;
  }
  if (bootResult.code != CFG_BOOT_OK) {
    if (boot_.faulted()) {
      requestFaultClear_();
      return ACSE_PROVISION_FAULTED;
    }
    return ACSE_CONFIG_REJECTED;
  }

  if (!display_.requestBlankFrame()) {
    state_ = ACSS_CLEAR_PENDING;
    return ACSE_PROVISIONED_CLEAR_PENDING;
  }
  state_ = ACSS_PROVISIONED;
  return ACSE_PROVISIONED_CLEAR_REQUESTED;
}

ApCredentialSetupCode ApCredentialSetupCoordinator::retryBlankFrame() {
  if (state_ == ACSS_STARTUP_CLEAR_PENDING) {
    if (!display_.requestBlankFrame()) return ACSE_STARTUP_CLEAR_PENDING;
    state_ = ACSS_FAULT;
    return ACSE_STARTUP_CLEAR_REQUESTED;
  }
  if (state_ != ACSS_CLEAR_PENDING || !boot_.ready())
    return ACSE_NOT_READY_FOR_PROVISION;
  if (!display_.requestBlankFrame()) return ACSE_PROVISIONED_CLEAR_PENDING;
  state_ = ACSS_PROVISIONED;
  return ACSE_PROVISIONED_CLEAR_REQUESTED;
}

}  // namespace att
