#include "admin_session.h"

#include <string.h>

namespace att {
namespace {

void secureClear(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

PinAuthResult pinResult(PinAuthCode code) {
  const PinAuthResult result = {code, 0};
  return result;
}

ApiResult unauthenticated() {
  const ApiResult result = {A_UNAUTH, R_NO_TOKEN, 0};
  return result;
}

bool requiresSetupCode(Ep endpoint) {
  return endpoint == EP_SETUP || endpoint == EP_PIN_RESET ||
         endpoint == EP_RESTORE || endpoint == EP_ROLLOVER;
}

ApiResult badField() {
  const ApiResult result = {A_BAD, R_BAD_FIELD, 0};
  return result;
}

}  // namespace

AdminSessionBoundary::AdminSessionBoundary(
    PinAuthCoordinator& auth, IPinVerifier& verifier,
    ISecureRandomSource& random)
    : auth_(auth), verifier_(verifier), random_(random), sessions_(),
      pendingToken_{0}, pending_(false), faulted_(false) {}

PinAuthResult AdminSessionBoundary::beginLogin(
    const char* pin, size_t length, uint32_t nowS, uint8_t tokenOut[16]) {
  if (tokenOut == nullptr) return pinResult(PAC_BAD_INPUT);
  secureClear(tokenOut, 16);

  if (auth_.faulted()) latchFault_();
  if (faulted_) return pinResult(PAC_NOT_READY);
  if (!auth_.ready()) return pinResult(PAC_NOT_READY);
  if (pending_) return pinResult(PAC_ACTION_REFUSED);

  TokenSessionSuccessAction action(sessions_, random_);
  const PinAuthResult authenticated =
      auth_.authenticateAndRun(pin, length, nowS, verifier_, action);

  if (auth_.faulted()) {
    latchFault_();
    return authenticated;
  }
  if (authenticated.code != PAC_OK) return authenticated;

  // Copy into the boundary before the one-shot action is destroyed. Its
  // destructor clears its own copy; no TokenTable entry exists yet.
  if (!action.takeToken(pendingToken_)) {
    latchFault_();
    return pinResult(PAC_ACTION_INDETERMINATE);
  }

  memcpy(tokenOut, pendingToken_, sizeof pendingToken_);
  pending_ = true;
  return authenticated;
}

bool AdminSessionBoundary::confirmLoginResponseSent(uint32_t nowS) {
  if (auth_.faulted()) latchFault_();
  if (faulted_ || !pending_) return false;

  // No other session-table operation is permitted while pending_. The HTTP
  // owner must serialize this complete request/response lifecycle.
  if (!sessions_.issue(pendingToken_, nowS)) {
    latchFault_();
    return false;
  }

  clearPending_();
  return true;
}

void AdminSessionBoundary::abortLoginResponse() {
  clearPending_();
}

ApiResult AdminSessionBoundary::authorize(
    Ep endpoint, HState health, const uint8_t requestToken[16],
    uint32_t nowS) {
  if (auth_.faulted()) latchFault_();

  // LOGIN is not a token authorization decision. It must execute the PIN
  // coordinator and response-gated success action through beginLogin().
  if (endpoint == EP_LOGIN) return unauthenticated();

  // Public status remains available for diagnosis. Every other route is
  // denied while credentials are unavailable, the PIN coordinator is faulted,
  // or a token response is awaiting delivery.
  if (endpoint == EP_STATUS)
    return authorizeRequest(endpoint, health, nullptr, nowS, sessions_,
                            false);

  if (faulted_ || !auth_.ready() || pending_)
    return unauthenticated();

  // Setup-code routes must not refresh an EP_ROLLOVER session until both
  // factors pass. Their preflight uses a read-only token match; the successful
  // setup-code action phase performs the normal refreshing authorization.
  if (requiresSetupCode(endpoint))
    return authorizeSetupCode_(endpoint, health, requestToken, nowS, false);

  // authorizeRequest validates precisely the token supplied by this request.
  // It also revokes only that token after an authorized logout.
  return authorizeRequest(endpoint, health, requestToken, nowS, false);
}

ApiResult AdminSessionBoundary::authorizeSetupCode_(
    Ep endpoint, HState health, const uint8_t requestToken[16],
    uint32_t nowS, bool verified) {
  if (!requiresSetupCode(endpoint)) return badField();
  if (auth_.faulted()) latchFault_();
  if (faulted_ || !auth_.ready() || pending_) return unauthenticated();

  // Before verification, check a token-required rollover request without
  // refreshing its idle timer. Failed secondary authentication must not keep
  // a bearer session alive. After verification, use the normal authorization
  // path so a fully authorized request refreshes the token and checks health.
  if (!verified) {
    if (endpoint == EP_ROLLOVER &&
        !sessions_.matches(requestToken, nowS))
      return unauthenticated();
    const ApiResult setupDenied = {A_UNAUTH, R_BAD_SETUP, 0};
    return setupDenied;
  }
  return authorizeRequest(endpoint, health, requestToken, nowS, sessions_,
                          true);
}

AdminPinChangeDispatchResult AdminSessionBoundary::changeAdminPin(
    const uint8_t requestToken[16], HState health,
    const char* currentPin, size_t currentLength,
    const char* newPin, size_t newLength, uint32_t nowS,
    IAdminPinWriter& writer) {
  const ApiResult authorization =
      authorize(EP_PIN_CHANGE, health, requestToken, nowS);
  if (authorization.code != A_OK) {
    const AdminPinChangeDispatchResult denied = {
        APCD_AUTHORIZATION_DENIED, authorization,
        pinResult(PAC_NOT_READY)};
    return denied;
  }

  const PinAuthResult changed = auth_.changeAdminPin(
      currentPin, currentLength, newPin, newLength, nowS, verifier_, writer);

  // A credential update with an indeterminate commit cannot leave any
  // previously issued bearer token usable.
  if (auth_.faulted() || changed.code == PAC_ACTION_INDETERMINATE) {
    latchFault_();
    const AdminPinChangeDispatchResult failed = {
        APCD_CHANGE_REJECTED, authorization, changed};
    return failed;
  }

  if (changed.code == PAC_OK) {
    // The credential changed. Require every client, including the requester,
    // to authenticate again using the new PIN.
    revokeAll();
    const AdminPinChangeDispatchResult committed = {
        APCD_CHANGE_COMMITTED, authorization, changed};
    return committed;
  }

  const AdminPinChangeDispatchResult rejected = {
      APCD_CHANGE_REJECTED, authorization, changed};
  return rejected;
}

class AdminSetupCodeBoundary::DispatchAction : public IPinSuccessAction {
 public:
  DispatchAction(AdminSetupCodeBoundary& owner, Ep endpoint, HState health,
                 const uint8_t requestToken[16], uint32_t nowS,
                 IPinSuccessAction& action, ApiResult& authorization,
                 bool& authorizationChecked)
      : owner_(owner), endpoint_(endpoint), health_(health),
        requestToken_(requestToken), nowS_(nowS), action_(action),
        authorization_(authorization),
        authorizationChecked_(authorizationChecked) {}

  PinActionOutcome run() override {
    return owner_.runAction_(endpoint_, health_, requestToken_, nowS_,
                             action_, authorization_, authorizationChecked_);
  }

 private:
  AdminSetupCodeBoundary& owner_;
  Ep endpoint_;
  HState health_;
  const uint8_t* requestToken_;
  uint32_t nowS_;
  IPinSuccessAction& action_;
  ApiResult& authorization_;
  bool& authorizationChecked_;
};

AdminSetupCodeBoundary::AdminSetupCodeBoundary(
    AdminSessionBoundary& sessions, PinAuthCoordinator& setupCodeAuth,
    ISetupCodeVerifier& verifier)
    : sessions_(sessions), setupCodeAuth_(setupCodeAuth),
      verifier_(verifier) {}

AdminSetupCodeDispatchResult AdminSetupCodeBoundary::authenticateAndRun(
    Ep endpoint, HState health, const uint8_t requestToken[16],
    const char* setupCode, size_t setupCodeLength, uint32_t nowS,
    IPinSuccessAction& action) {
  const ApiResult preflight = sessions_.authorizeSetupCode_(
      endpoint, health, requestToken, nowS, false);
  if (preflight.code != A_UNAUTH || preflight.reason != R_BAD_SETUP) {
    const AdminSetupCodeDispatchResult denied = {
        ASCD_AUTHORIZATION_DENIED, preflight, pinResult(PAC_NOT_READY)};
    return denied;
  }

  if (!setupCodeAuth_.ready()) {
    const AdminSetupCodeDispatchResult unavailable = {
        ASCD_AUTHENTICATION_REJECTED, preflight, pinResult(PAC_NOT_READY)};
    return unavailable;
  }

  ApiResult authorization = preflight;
  bool authorizationChecked = false;
  DispatchAction dispatchAction(*this, endpoint, health, requestToken, nowS,
                                 action, authorization,
                                 authorizationChecked);
  const PinAuthResult authenticated = setupCodeAuth_.authenticateAndRun(
      setupCode, setupCodeLength, nowS, verifier_, dispatchAction);

  AdminSetupCodeDispatchCode code = ASCD_AUTHENTICATION_REJECTED;
  if (authorizationChecked) {
    if (authorization.code != A_OK)
      code = ASCD_AUTHORIZATION_DENIED;
    else if (authenticated.code == PAC_OK)
      code = ASCD_ACTION_COMPLETED;
    else
      code = ASCD_ACTION_REJECTED;
  }
  const AdminSetupCodeDispatchResult result = {
      code, authorization, authenticated};
  return result;
}

PinActionOutcome AdminSetupCodeBoundary::runAction_(
    Ep endpoint, HState health, const uint8_t requestToken[16],
    uint32_t nowS, IPinSuccessAction& action, ApiResult& authorization,
    bool& authorizationChecked) {
  authorization = sessions_.authorizeSetupCode_(
      endpoint, health, requestToken, nowS, true);
  authorizationChecked = true;
  if (authorization.code != A_OK) return PA_REFUSED;

  const PinActionOutcome outcome = action.run();
  if (outcome == PA_DONE)
    sessions_.revokeAll();
  else if (outcome == PA_INDETERMINATE)
    sessions_.latchFault_();
  return outcome;
}

void AdminSessionBoundary::revokeAll() {
  clearPending_();
  sessions_.revokeAll();
}

bool AdminSessionBoundary::faulted() const {
  return faulted_ || auth_.faulted();
}

int AdminSessionBoundary::activeSessions(uint32_t nowS) const {
  if (faulted()) return 0;
  return sessions_.active(nowS);
}

void AdminSessionBoundary::latchFault_() {
  faulted_ = true;
  clearPending_();
  sessions_.revokeAll();
}

void AdminSessionBoundary::clearPending_() {
  secureClear(pendingToken_, sizeof pendingToken_);
  pending_ = false;
}

}  // namespace att
