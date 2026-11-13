#pragma once

#include "ap_credential_setup.h"
#include "pin_auth.h"

namespace att {

class ISetupCodeIssuer {
 public:
  virtual ~ISetupCodeIssuer() {}
  // On failure, wipe internal code bytes and request a blank frame.
  virtual bool issue() = 0;
  virtual bool active() const = 0;
};

enum FirstRunSetupCodeSequenceCode : uint8_t {
  FSC_COLD,
  FSC_STARTED,
  FSC_ALREADY_STARTED,
  FSC_AP_SETUP_FAILED,
  FSC_AP_NOT_READY,
  FSC_WAITING_FOR_CLIENT,
  FSC_ALREADY_ISSUED,
  FSC_CODE_ISSUE_FAILED
};

// First-run disclosure order: AP credential display -> secured AP_START/READY
// -> DHCP assignment -> setup-code issue/display. The session-free
// FirstRunSetupCodeBoundary supplies the code-only EP_SETUP authorization.
// Keep all calls on the serialized owner.
class FirstRunSetupCodeSequence {
 public:
  FirstRunSetupCodeSequence(ApCredentialSetupCoordinator& apSetup,
                            ISetupCodeIssuer& setupCodeIssuer);
  FirstRunSetupCodeSequence(const FirstRunSetupCodeSequence&) = delete;
  FirstRunSetupCodeSequence& operator=(const FirstRunSetupCodeSequence&) = delete;

  FirstRunSetupCodeSequenceCode start();
  FirstRunSetupCodeSequenceCode issueAfterClientAssigned();
  bool ready() const { return ready_; }
  ApCredentialSetupValidationCode validateProvisionConfig(
      const Config& initial);
  bool faulted() const { return code_ == FSC_AP_SETUP_FAILED ||
                              code_ == FSC_AP_NOT_READY ||
                              code_ == FSC_CODE_ISSUE_FAILED; }
  FirstRunSetupCodeSequenceCode code() const { return code_; }
  bool provisioningCleanupComplete() const {
    return apSetup_.state() == ACSS_PROVISIONED;
  }

  bool mayProvision(const IConfigProvisionGate& requestAuthGate) const;
  ApCredentialSetupCode provision(const Config& initial,
                                  const IConfigProvisionGate& requestAuthGate,
                                  ConfigBootResult& bootResult);
  ApCredentialSetupCode retryBlankFrame();

 private:
  ApCredentialSetupCoordinator& apSetup_;
  ISetupCodeIssuer& setupCodeIssuer_;
  bool started_;
  bool ready_;
  FirstRunSetupCodeSequenceCode code_;
};

enum InitialPinCredentialCode : uint8_t {
  IPCC_NOT_ATTEMPTED,
  IPCC_PREPARED,
  IPCC_NOT_NEEDS_SETUP,
  IPCC_INVALID_PIN,
  IPCC_RANDOM_FAILED,
  IPCC_INVALID_SALT,
  IPCC_KDF_FAILED,
  IPCC_INVALID_CONFIG
};

class IInitialPinCredentialPreparer {
 public:
  virtual ~IInitialPinCredentialPreparer() {}
  virtual InitialPinCredentialCode prepare(
      const char* pin, size_t pinLength, Config& config) = 0;
};

enum FirstRunSetupDispatchCode : uint8_t {
  FRSD_NOT_READY,
  FRSD_INVALID_CONFIG,
  FRSD_INVALID_PIN,
  FRSD_CREDENTIAL_UNAVAILABLE,
  FRSD_AUTHENTICATION_REJECTED,
  FRSD_HEALTH_REJECTED,
  FRSD_PIN_PREPARATION_FAILED,
  FRSD_PROVISION_FAILED,
  FRSD_PROVISIONED
};

struct FirstRunSetupDispatchResult {
  FirstRunSetupDispatchCode code;
  ApiResult authorization;
  PinAuthResult authentication;
  ApCredentialSetupCode provisioning;
  InitialPinCredentialCode pinCredential;
};

// Session-free, code-only EP_SETUP boundary for CFG_BOOT_NEEDS_SETUP. It is
// distinct from AdminSetupCodeBoundary, which uses the provisioned session graph.
class FirstRunSetupCodeBoundary {
 public:
  FirstRunSetupCodeBoundary(PinAuthCoordinator& setupCodeAuth,
                            ISetupCodeVerifier& verifier,
                            FirstRunSetupCodeSequence& sequence,
                            IInitialPinCredentialPreparer& initialPinPreparer);
  FirstRunSetupCodeBoundary(const FirstRunSetupCodeBoundary&) = delete;
  FirstRunSetupCodeBoundary& operator=(
      const FirstRunSetupCodeBoundary&) = delete;
  FirstRunSetupDispatchResult provision(
      HState health, const char* setupCode, size_t setupCodeLength,
      uint32_t nowS, const char* initialPin, size_t initialPinLength,
      const Config& initial, ConfigBootResult& bootResult);
  bool faulted() const { return faulted_ || setupCodeAuth_.faulted() ||
                              sequence_.faulted(); }
  bool completed() const { return completed_; }
  bool restartReady() const {
    return completed_ && sequence_.provisioningCleanupComplete();
  }
 private:
  class ProvisionAction;
  PinAuthCoordinator& setupCodeAuth_;
  ISetupCodeVerifier& verifier_;
  FirstRunSetupCodeSequence& sequence_;
  IInitialPinCredentialPreparer& initialPinPreparer_;
  bool faulted_;
  bool completed_;
};

class IApplicationRestart {
 public:
  virtual ~IApplicationRestart() {}
  virtual void restartAfterProvisioning() = 0;
};

// Called only after the current request handler has returned. A successful
// first provision must reboot into the ordinary READY composition root; this
// gate makes that request one-shot even if the platform restart call returns.
class FirstRunProvisioningRestartGate {
 public:
  FirstRunProvisioningRestartGate() : requested_(false) {}
  bool service(const FirstRunSetupCodeBoundary& boundary,
               IApplicationRestart& restart);
  bool requested() const { return requested_; }

 private:
  bool requested_;
};

}  // namespace att
