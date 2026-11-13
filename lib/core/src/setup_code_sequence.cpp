#include "setup_code_sequence.h"
#include "api_logic.h"

namespace att {
namespace {

void clearSecret(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

ApiResult setupResult(ApiCode code, ApiReason reason) {
  return ApiResult{code, reason, 0};
}

class CurrentSetupProvisionGate : public IConfigProvisionGate {
 public:
  CurrentSetupProvisionGate() : authorized_(false) {}
  void authorize() { authorized_ = true; }
  bool mayProvision() const override { return authorized_; }

 private:
  bool authorized_;
};

}  // namespace

FirstRunSetupCodeSequence::FirstRunSetupCodeSequence(
    ApCredentialSetupCoordinator& apSetup,
    ISetupCodeIssuer& setupCodeIssuer)
    : apSetup_(apSetup), setupCodeIssuer_(setupCodeIssuer),
      started_(false), ready_(false), code_(FSC_COLD) {}

FirstRunSetupCodeSequenceCode FirstRunSetupCodeSequence::start() {
  if (started_) return FSC_ALREADY_STARTED;
  started_ = true;

  if (apSetup_.start() != ACSE_STARTED) {
    code_ = FSC_AP_SETUP_FAILED;
    return code_;
  }
  if (apSetup_.state() != ACSS_WAITING_FOR_CLIENT || !apSetup_.apReady()) {
    code_ = FSC_AP_NOT_READY;
    return code_;
  }
  code_ = FSC_WAITING_FOR_CLIENT;
  return code_;
}

FirstRunSetupCodeSequenceCode FirstRunSetupCodeSequence::issueAfterClientAssigned() {
  if (!started_) return FSC_COLD;
  if (ready_) {
    if (setupCodeIssuer_.active()) return FSC_ALREADY_ISSUED;
    // Expiry or consumption permits a replacement only while initial setup
    // remains pending and the same AP owner is still ready.
    if (apSetup_.state() != ACSS_WAITING_FOR_CLIENT || !apSetup_.apReady())
      return FSC_ALREADY_ISSUED;
    ready_ = false;
    code_ = FSC_WAITING_FOR_CLIENT;
  }
  if (code_ != FSC_WAITING_FOR_CLIENT) return code_;
  if (!apSetup_.apReady()) {
    code_ = FSC_AP_NOT_READY;
    return code_;
  }
  if (!apSetup_.hasAssignedClientIp()) return FSC_WAITING_FOR_CLIENT;
  if (!setupCodeIssuer_.issue()) {
    code_ = FSC_CODE_ISSUE_FAILED;
    return code_;
  }
  ready_ = true;
  code_ = FSC_STARTED;
  return code_;
}

ApCredentialSetupValidationCode
FirstRunSetupCodeSequence::validateProvisionConfig(const Config& initial) {
  if (!ready_ || !setupCodeIssuer_.active()) return ACSV_NOT_READY;
  return apSetup_.validateProvisionConfig(initial);
}

bool FirstRunSetupCodeSequence::mayProvision(
    const IConfigProvisionGate& requestAuthGate) const {
  return ready_ && setupCodeIssuer_.active() &&
         apSetup_.mayProvision(requestAuthGate);
}

ApCredentialSetupCode FirstRunSetupCodeSequence::provision(
    const Config& initial, const IConfigProvisionGate& requestAuthGate,
    ConfigBootResult& bootResult) {
  if (!ready_) return ACSE_NOT_READY_FOR_PROVISION;
  return apSetup_.provision(initial, requestAuthGate, bootResult);
}

ApCredentialSetupCode FirstRunSetupCodeSequence::retryBlankFrame() {
  return apSetup_.retryBlankFrame();
}

class FirstRunSetupCodeBoundary::ProvisionAction : public IPinSuccessAction {
 public:
  ProvisionAction(FirstRunSetupCodeSequence& sequence, HState health,
                  const char* initialPin, size_t initialPinLength,
                  const Config& initial, ConfigBootResult& bootResult,
                  IInitialPinCredentialPreparer& initialPinPreparer,
                  InitialPinCredentialCode& pinCredential,
                  CurrentSetupProvisionGate& gate, ApiResult& authorization,
                  ApCredentialSetupCode& provisioning)
      : sequence_(sequence), health_(health), initialPin_(initialPin),
        initialPinLength_(initialPinLength), initial_(initial),
        bootResult_(bootResult), initialPinPreparer_(initialPinPreparer),
        pinCredential_(pinCredential), gate_(gate),
        authorization_(authorization), provisioning_(provisioning) {}

  PinActionOutcome run() override {
    // Verify code + durably reset the counter before reaching this action.
    authorization_ = authorize(EP_SETUP, health_, false, true);
    if (authorization_.code != A_OK) return PA_REFUSED;

    Config prepared = initial_;
    pinCredential_ = initialPinPreparer_.prepare(
        initialPin_, initialPinLength_, prepared);
    if (pinCredential_ != IPCC_PREPARED) {
      clearSecret(&prepared, sizeof prepared);
      return PA_INDETERMINATE;
    }

    gate_.authorize();
    provisioning_ = sequence_.provision(prepared, gate_, bootResult_);
    clearSecret(&prepared, sizeof prepared);
    if ((provisioning_ == ACSE_PROVISIONED_CLEAR_REQUESTED ||
         provisioning_ == ACSE_PROVISIONED_CLEAR_PENDING) &&
        bootResult_.code == CFG_BOOT_OK &&
        bootResult_.state == CFG_BOOT_READY) return PA_DONE;
    return PA_INDETERMINATE;
  }

 private:
  FirstRunSetupCodeSequence& sequence_;
  HState health_;
  const char* initialPin_;
  size_t initialPinLength_;
  const Config& initial_;
  ConfigBootResult& bootResult_;
  IInitialPinCredentialPreparer& initialPinPreparer_;
  InitialPinCredentialCode& pinCredential_;
  CurrentSetupProvisionGate& gate_;
  ApiResult& authorization_;
  ApCredentialSetupCode& provisioning_;
};

FirstRunSetupCodeBoundary::FirstRunSetupCodeBoundary(
    PinAuthCoordinator& setupCodeAuth, ISetupCodeVerifier& verifier,
    FirstRunSetupCodeSequence& sequence,
    IInitialPinCredentialPreparer& initialPinPreparer)
    : setupCodeAuth_(setupCodeAuth), verifier_(verifier), sequence_(sequence),
      initialPinPreparer_(initialPinPreparer), faulted_(false), completed_(false) {}

FirstRunSetupDispatchResult FirstRunSetupCodeBoundary::provision(
    HState health, const char* setupCode, size_t setupCodeLength,
    uint32_t nowS, const char* initialPin, size_t initialPinLength,
    const Config& initial, ConfigBootResult& bootResult) {
  ApiResult authorization = setupResult(A_UNAUTH, R_BAD_SETUP);
  PinAuthResult authentication = {PAC_NOT_READY, 0};
  ApCredentialSetupCode provisioning = ACSE_NOT_READY_FOR_PROVISION;
  InitialPinCredentialCode pinCredential = IPCC_NOT_ATTEMPTED;

  if (faulted() || completed_ || !setupCodeAuth_.ready())
    return FirstRunSetupDispatchResult{
        FRSD_NOT_READY, authorization, authentication, provisioning,
        pinCredential};
  if (!validPin(initialPin, initialPinLength) ||
      weakPin(initialPin, initialPinLength))
    return FirstRunSetupDispatchResult{
        FRSD_INVALID_PIN, setupResult(A_INVALID, R_BAD_FIELD),
        authentication, provisioning, pinCredential};

  // Correctable PIN/config errors and AP credential checks happen before the
  // one-time code is consumed. PBKDF2/RNG work occurs only in the action.
  const ApCredentialSetupValidationCode preflight =
      sequence_.validateProvisionConfig(initial);
  if (preflight == ACSV_NOT_READY)
    return FirstRunSetupDispatchResult{
        FRSD_NOT_READY, setupResult(A_LOCKED, R_STATE),
        authentication, provisioning, pinCredential};
  if (preflight == ACSV_INVALID_CONFIG)
    return FirstRunSetupDispatchResult{
        FRSD_INVALID_CONFIG, setupResult(A_INVALID, R_BAD_FIELD),
        authentication, provisioning, pinCredential};
  if (preflight == ACSV_CREDENTIAL_FAULT) {
    faulted_ = true;
    return FirstRunSetupDispatchResult{
        FRSD_CREDENTIAL_UNAVAILABLE, setupResult(A_LOCKED, R_STATE),
        authentication, provisioning, pinCredential};
  }

  char pinCopy[7] = {0};
  memcpy(pinCopy, initialPin, 6);
  CurrentSetupProvisionGate gate;
  ProvisionAction action(sequence_, health, pinCopy, 6, initial, bootResult,
                         initialPinPreparer_, pinCredential, gate,
                         authorization, provisioning);
  authentication = setupCodeAuth_.authenticateAndRun(
      setupCode, setupCodeLength, nowS, verifier_, action);
  clearSecret(pinCopy, sizeof pinCopy);

  if (authentication.code == PAC_OK) {
    completed_ = true;
    return FirstRunSetupDispatchResult{
        FRSD_PROVISIONED, authorization, authentication, provisioning,
        pinCredential};
  }
  if (authentication.code == PAC_ACTION_REFUSED) {
    // The code was valid and consumed. Do not reissue within this boot after
    // the health policy refuses the operation.
    faulted_ = true;
    return FirstRunSetupDispatchResult{
        FRSD_HEALTH_REJECTED, authorization, authentication, provisioning,
        pinCredential};
  }
  if (authentication.code == PAC_ACTION_INDETERMINATE ||
      setupCodeAuth_.faulted()) {
    faulted_ = true;
    return FirstRunSetupDispatchResult{
        pinCredential == IPCC_NOT_ATTEMPTED
            ? FRSD_PROVISION_FAILED : FRSD_PIN_PREPARATION_FAILED,
        authorization, authentication, provisioning, pinCredential};
  }
  return FirstRunSetupDispatchResult{
      FRSD_AUTHENTICATION_REJECTED, authorization, authentication,
      provisioning, pinCredential};
}

bool FirstRunProvisioningRestartGate::service(
    const FirstRunSetupCodeBoundary& boundary, IApplicationRestart& restart) {
  if (requested_ || !boundary.restartReady()) return false;
  requested_ = true;
  restart.restartAfterProvisioning();
  return true;
}

}  // namespace att
