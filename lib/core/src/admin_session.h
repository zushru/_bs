#pragma once

#include "api_logic.h"
#include "pin_auth_session.h"

namespace att {

enum AdminPinChangeDispatchCode : uint8_t {
  APCD_AUTHORIZATION_DENIED,
  APCD_CHANGE_REJECTED,
  APCD_CHANGE_COMMITTED
};

struct AdminPinChangeDispatchResult {
  AdminPinChangeDispatchCode code;
  ApiResult authorization;
  PinAuthResult pinChange;
};

enum AdminSetupCodeDispatchCode : uint8_t {
  ASCD_AUTHORIZATION_DENIED,
  ASCD_AUTHENTICATION_REJECTED,
  ASCD_ACTION_REJECTED,
  ASCD_ACTION_COMPLETED
};

struct AdminSetupCodeDispatchResult {
  AdminSetupCodeDispatchCode code;
  ApiResult authorization;
  PinAuthResult authentication;
};

class AdminSetupCodeBoundary;

// One serialized owner for PIN login, response-gated token activation and
// request-bound session authorization. The HTTP adapter must invoke
// confirmLoginResponseSent() only after the complete token response is written;
// it must call abortLoginResponse() on every send/serialization failure.
class AdminSessionBoundary {
 public:
  AdminSessionBoundary(PinAuthCoordinator& auth, IPinVerifier& verifier,
                       ISecureRandomSource& random);
  AdminSessionBoundary(const AdminSessionBoundary&) = delete;
  AdminSessionBoundary& operator=(const AdminSessionBoundary&) = delete;

  // Copies a pending 16-byte token to tokenOut on PAC_OK. The token is not live
  // until confirmLoginResponseSent(). The caller must clear tokenOut afterward.
  PinAuthResult beginLogin(const char* pin, size_t length, uint32_t nowS,
                           uint8_t tokenOut[16]);
  bool confirmLoginResponseSent(uint32_t nowS);
  void abortLoginResponse();

  // requestToken is decoded from THIS HTTP request. Setup-code-gated
  // endpoints remain denied here; use AdminSetupCodeBoundary for their
  // request-bound verifier/lockout/action sequence. No caller-supplied
  // tokenValid/setupCodeOk booleans.
  ApiResult authorize(Ep endpoint, HState health,
                      const uint8_t requestToken[16], uint32_t nowS);

  // Performs PIN change as one request-bound operation. The current request
  // token and EP_PIN_CHANGE health policy are checked before the current PIN.
  // This entire call must run on the serialized owner with login/logout and
  // every other session/auth mutation; this method does not add a mutex.
  // A committed credential change revokes every active session. An
  // indeterminate/faulted change latches this boundary and revokes sessions.
  AdminPinChangeDispatchResult changeAdminPin(
      const uint8_t requestToken[16], HState health,
      const char* currentPin, size_t currentLength,
      const char* newPin, size_t newLength, uint32_t nowS,
      IAdminPinWriter& writer);

  void revokeAll();
  bool faulted() const;
  bool loginResponsePending() const { return pending_; }
  int activeSessions(uint32_t nowS) const;

 private:
  friend class AdminSetupCodeBoundary;

  ApiResult authorizeSetupCode_(Ep endpoint, HState health,
                                const uint8_t requestToken[16],
                                uint32_t nowS, bool verified);
  void latchFault_();
  void clearPending_();

  PinAuthCoordinator& auth_;
  IPinVerifier& verifier_;
  ISecureRandomSource& random_;
  TokenTable sessions_;
  uint8_t pendingToken_[16];
  bool pending_;
  bool faulted_;
};

// Long-lived request boundary for P15's setup-code-gated endpoints. Construct
// one instance for each explicitly chosen lockout-counter domain; the same
// instance must serve every route assigned to that domain. The endpoint-to-
// counter mapping (including whether PIN attempts share this counter) remains
// a composition decision and is not selected here.
//
// Calls are synchronous and must share AdminSessionBoundary's serialized
// owner. The coordinator must be booted from its persistent attempt store
// before use. No generator, credential store, concrete verifier, HTTP route or
// production composition is supplied by this source seam.
class AdminSetupCodeBoundary {
 public:
  AdminSetupCodeBoundary(AdminSessionBoundary& sessions,
                         PinAuthCoordinator& setupCodeAuth,
                         ISetupCodeVerifier& verifier);
  AdminSetupCodeBoundary(const AdminSetupCodeBoundary&) = delete;
  AdminSetupCodeBoundary& operator=(const AdminSetupCodeBoundary&) = delete;

  // Accepts only EP_SETUP, EP_PIN_RESET, EP_RESTORE and EP_ROLLOVER. For
  // EP_ROLLOVER, this request's token is validated before the code verifier.
  // PinAuthCoordinator commits the attempt before verification and clears it
  // before the action. Endpoint health is checked after successful code auth.
  // A successful P15 action revokes all sessions; an indeterminate action
  // faults the session boundary and revokes them. Caller clears code bytes.
  AdminSetupCodeDispatchResult authenticateAndRun(
      Ep endpoint, HState health, const uint8_t requestToken[16],
      const char* setupCode, size_t setupCodeLength, uint32_t nowS,
      IPinSuccessAction& action);

 private:
  class DispatchAction;

  PinActionOutcome runAction_(
      Ep endpoint, HState health, const uint8_t requestToken[16],
      uint32_t nowS, IPinSuccessAction& action,
      ApiResult& authorization, bool& authorizationChecked);

  AdminSessionBoundary& sessions_;
  PinAuthCoordinator& setupCodeAuth_;
  ISetupCodeVerifier& verifier_;
};

}  // namespace att
