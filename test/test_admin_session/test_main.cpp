#include <string.h>

#include <string>

#include <unity.h>

#include "admin_session.h"

using namespace att;

namespace {

struct CounterStore : IPinAttemptStore {
  uint16_t value;
  bool ok;
  uint32_t persistCalls;
  uint32_t failPersistCall;
  CounterStore() : value(0), ok(true), persistCalls(0), failPersistCall(0) {}
  bool loadFailureCount(uint16_t& out) override {
    if (!ok) return false;
    out = value;
    return true;
  }
  bool persistFailureCount(uint16_t next) override {
    ++persistCalls;
    if (!ok || (failPersistCall != 0 && persistCalls == failPersistCall))
      return false;
    value = next;
    return true;
  }
};

struct FakeVerifier : IPinVerifier {
  PinVerifyOutcome outcome;
  uint32_t calls;
  FakeVerifier() : outcome(PV_MATCH), calls(0) {}
  PinVerifyOutcome verify(const char*, size_t) override {
    ++calls;
    return outcome;
  }
};

struct FakeRandom : ISecureRandomSource {
  uint8_t bytes[16];
  bool ok;
  uint32_t calls;
  FakeRandom() : bytes{0}, ok(true), calls(0) {
    for (size_t i = 0; i < sizeof bytes; ++i)
      bytes[i] = static_cast<uint8_t>(i + 1);
  }
  bool fill(uint8_t* out, size_t length) override {
    ++calls;
    if (!ok || !out || length != sizeof bytes) return false;
    memcpy(out, bytes, sizeof bytes);
    return true;
  }
};

struct FakeWriter : IAdminPinWriter {
  CounterStore& store;
  PinActionOutcome outcome;
  uint32_t calls;
  uint16_t observedCounter;
  std::string committedPin;

  explicit FakeWriter(CounterStore& value)
      : store(value), outcome(PA_DONE), calls(0), observedCounter(0) {}

  PinActionOutcome commitNewPin(const char* pin, size_t length) override {
    ++calls;
    observedCounter = store.value;
    if (pin != nullptr) committedPin.assign(pin, length);
    return outcome;
  }
};

struct FakeSetupVerifier : ISetupCodeVerifier {
  PinVerifyOutcome outcome;
  uint32_t calls;
  FakeSetupVerifier() : outcome(PV_MATCH), calls(0) {}
  PinVerifyOutcome verify(const char*, size_t) override {
    ++calls;
    return outcome;
  }
};

struct FakeAction : IPinSuccessAction {
  CounterStore& store;
  PinActionOutcome outcome;
  uint32_t calls;
  uint16_t observedCounter;
  explicit FakeAction(CounterStore& value)
      : store(value), outcome(PA_DONE), calls(0), observedCounter(0) {}
  PinActionOutcome run() override {
    ++calls;
    observedCounter = store.value;
    return outcome;
  }
};

struct Fixture {
  CounterStore store;
  CounterStore setupStore;
  PinAuthCoordinator auth;
  PinAuthCoordinator setupAuth;
  FakeVerifier verifier;
  FakeSetupVerifier setupVerifier;
  FakeRandom random;
  AdminSessionBoundary boundary;
  AdminSetupCodeBoundary setupBoundary;

  Fixture()
      : auth(store), setupAuth(setupStore),
        boundary(auth, verifier, random),
        setupBoundary(boundary, setupAuth, setupVerifier) {
    auth.boot(10);
    setupAuth.boot(10);
  }
};

void test_login_token_is_inactive_until_full_response_is_confirmed() {
  Fixture f;
  uint8_t token[16] = {};
  const PinAuthResult login = f.boundary.beginLogin(
      "483920", 6, 20, token);

  TEST_ASSERT_EQUAL_INT(PAC_OK, login.code);
  TEST_ASSERT_EQUAL_MEMORY(f.random.bytes, token, sizeof token);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(20));

  const ApiResult pending = f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 20);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, pending.code);

  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(21));
  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 21).code);
}

void test_aborted_login_response_never_activates_token() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  f.boundary.abortLoginResponse();

  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(21));
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 21).code);
  TEST_ASSERT_FALSE(f.boundary.confirmLoginResponseSent(21));
}

void test_request_authorization_uses_this_requests_token_bytes() {
  Fixture f;
  uint8_t issued[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, issued).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  uint8_t other[16];
  memcpy(other, issued, sizeof other);
  other[0] ^= 0x80u;
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_MARK, H_OK, other, 22).code);
  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_MARK, H_OK, issued, 22).code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_MARK, H_OK, nullptr, 23).code);
}

void test_clock_route_requires_request_token_and_clock_capable_health() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_CLOCK, H_CLOCK_FAULT, nullptr, 22).code);

  uint8_t other[16];
  memcpy(other, token, sizeof other);
  other[0] ^= 0x01u;
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_CLOCK, H_CLOCK_FAULT, other, 22).code);

  const HState allowed[] = {H_OK, H_CLOCK_FAULT, H_SENSOR_FAULT};
  for (size_t i = 0; i < sizeof allowed / sizeof allowed[0]; ++i) {
    const ApiResult result = f.boundary.authorize(
        EP_CLOCK, allowed[i], token, 23 + static_cast<uint32_t>(i));
    TEST_ASSERT_EQUAL_INT(A_OK, result.code);
  }

  const HState denied[] = {
      H_UNPROVISIONED, H_CONFIG_FAULT, H_STORAGE_FAULT};
  for (size_t i = 0; i < sizeof denied / sizeof denied[0]; ++i) {
    const ApiResult result = f.boundary.authorize(
        EP_CLOCK, denied[i], token, 26 + static_cast<uint32_t>(i));
    TEST_ASSERT_EQUAL_INT(A_LOCKED, result.code);
    TEST_ASSERT_EQUAL_INT(R_STATE, result.reason);
  }
}

void test_generic_route_gate_cannot_authorize_login_without_pin_check() {
  Fixture f;
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_LOGIN, H_OK, nullptr, 20).code);
  TEST_ASSERT_EQUAL_UINT32(0, f.verifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, f.random.calls);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(20));
}

void test_setup_code_routes_fail_closed_without_dedicated_verifier() {
  Fixture f;
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_SETUP, H_UNPROVISIONED, nullptr, 20).code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_PIN_RESET, H_OK, nullptr, 20).code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_RESTORE, H_OK, nullptr, 20).code);

  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 21, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(22));
  const ApiResult rollover = f.boundary.authorize(
      EP_ROLLOVER, H_OK, token, 23);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, rollover.code);
  TEST_ASSERT_EQUAL_INT(R_BAD_SETUP, rollover.reason);
}

void test_public_status_never_requires_or_refreshes_a_session() {
  Fixture f;
  uint8_t issued[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, issued).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_STATUS, H_STORAGE_FAULT, nullptr, 22).code);
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(22));
}

void test_logout_revokes_only_the_matching_request_session() {
  Fixture f;
  uint8_t first[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, first).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  f.random.bytes[0] ^= 0x40u;
  uint8_t second[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 22, second).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(23));
  TEST_ASSERT_EQUAL_INT(2, f.boundary.activeSessions(23));

  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_LOGOUT, H_OK, nullptr, 24).code);
  TEST_ASSERT_EQUAL_INT(2, f.boundary.activeSessions(24));
  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_LOGOUT, H_OK, first, 25).code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, first, 26).code);
  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, second, 26).code);
}

void test_expired_token_is_rejected_using_monotonic_idle_age() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(1000));

  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 1001 + TOKEN_IDLE_S).code);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(1001 + TOKEN_IDLE_S));
}

void test_pin_coordinator_fault_revokes_existing_sessions_and_latches_boundary() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(21));

  f.verifier.outcome = PV_ERROR;
  uint8_t unused[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_VERIFY_ERROR,
      f.boundary.beginLogin("483920", 6, 22, unused).code);
  TEST_ASSERT_TRUE(f.boundary.faulted());

  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 23).code);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(23));
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY,
      f.boundary.beginLogin("483920", 6, 24, unused).code);
}

void test_pin_change_rejects_missing_and_wrong_request_tokens() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  uint8_t wrong[16];
  memcpy(wrong, token, sizeof wrong);
  wrong[0] ^= 0x80u;
  const uint32_t verifierCalls = f.verifier.calls;
  const uint32_t persistCalls = f.store.persistCalls;
  FakeWriter writer(f.store);

  const AdminPinChangeDispatchResult missing =
      f.boundary.changeAdminPin(nullptr, H_OK, "483920", 6,
                                "627194", 6, 22, writer);
  TEST_ASSERT_EQUAL_INT(APCD_AUTHORIZATION_DENIED, missing.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, missing.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY, missing.pinChange.code);

  const AdminPinChangeDispatchResult rejected =
      f.boundary.changeAdminPin(wrong, H_OK, "483920", 6,
                                "627194", 6, 23, writer);
  TEST_ASSERT_EQUAL_INT(APCD_AUTHORIZATION_DENIED, rejected.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, rejected.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY, rejected.pinChange.code);
  TEST_ASSERT_EQUAL_UINT32(verifierCalls, f.verifier.calls);
  TEST_ASSERT_EQUAL_UINT32(persistCalls, f.store.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, writer.calls);
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(24));
}

void test_pin_change_health_denial_precedes_pin_check_and_writer() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  const uint32_t verifierCalls = f.verifier.calls;
  const uint32_t persistCalls = f.store.persistCalls;
  FakeWriter writer(f.store);
  const AdminPinChangeDispatchResult denied =
      f.boundary.changeAdminPin(token, H_STORAGE_FAULT, "483920", 6,
                                "627194", 6, 22, writer);

  TEST_ASSERT_EQUAL_INT(APCD_AUTHORIZATION_DENIED, denied.code);
  TEST_ASSERT_EQUAL_INT(A_LOCKED, denied.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_STATE, denied.authorization.reason);
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY, denied.pinChange.code);
  TEST_ASSERT_EQUAL_UINT32(verifierCalls, f.verifier.calls);
  TEST_ASSERT_EQUAL_UINT32(persistCalls, f.store.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, writer.calls);
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_pin_change_wrong_current_pin_never_reaches_writer() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  f.verifier.outcome = PV_MISMATCH;
  FakeWriter writer(f.store);

  const AdminPinChangeDispatchResult rejected =
      f.boundary.changeAdminPin(token, H_OK, "111111", 6,
                                "627194", 6, 22, writer);

  TEST_ASSERT_EQUAL_INT(APCD_CHANGE_REJECTED, rejected.code);
  TEST_ASSERT_EQUAL_INT(A_OK, rejected.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, rejected.pinChange.code);
  TEST_ASSERT_EQUAL_UINT32(0, writer.calls);
  TEST_ASSERT_EQUAL_UINT16(1, f.store.value);
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(23));
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_pin_change_clean_writer_refusal_preserves_existing_session() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  FakeWriter writer(f.store);
  writer.outcome = PA_REFUSED;

  const AdminPinChangeDispatchResult rejected =
      f.boundary.changeAdminPin(token, H_OK, "483920", 6,
                                "627194", 6, 22, writer);

  TEST_ASSERT_EQUAL_INT(APCD_CHANGE_REJECTED, rejected.code);
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_REFUSED, rejected.pinChange.code);
  TEST_ASSERT_EQUAL_UINT32(1, writer.calls);
  TEST_ASSERT_EQUAL_UINT16(0, writer.observedCounter);
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(23));
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_pin_change_commit_revokes_every_active_session() {
  Fixture f;
  uint8_t first[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, first).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  f.random.bytes[0] ^= 0x20u;
  uint8_t second[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 22, second).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(23));
  TEST_ASSERT_EQUAL_INT(2, f.boundary.activeSessions(23));
  FakeWriter writer(f.store);

  const AdminPinChangeDispatchResult changed =
      f.boundary.changeAdminPin(first, H_OK, "483920", 6,
                                "627194", 6, 24, writer);

  TEST_ASSERT_EQUAL_INT(APCD_CHANGE_COMMITTED, changed.code);
  TEST_ASSERT_EQUAL_INT(A_OK, changed.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_OK, changed.pinChange.code);
  TEST_ASSERT_EQUAL_UINT32(1, writer.calls);
  TEST_ASSERT_EQUAL_STRING("627194", writer.committedPin.c_str());
  TEST_ASSERT_EQUAL_UINT16(0, writer.observedCounter);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(25));
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_CLOCK, H_CLOCK_FAULT, second, 25).code);
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_pin_change_indeterminate_writer_faults_and_revokes_sessions() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  FakeWriter writer(f.store);
  writer.outcome = PA_INDETERMINATE;

  const AdminPinChangeDispatchResult rejected =
      f.boundary.changeAdminPin(token, H_OK, "483920", 6,
                                "627194", 6, 22, writer);

  TEST_ASSERT_EQUAL_INT(APCD_CHANGE_REJECTED, rejected.code);
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_INDETERMINATE, rejected.pinChange.code);
  TEST_ASSERT_TRUE(f.boundary.faulted());
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(23));
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 23).code);
}


void test_setup_code_boundary_rejects_non_setup_endpoint() {
  Fixture f;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_MARK, H_OK, nullptr, "482731", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHORIZATION_DENIED, result.code);
  TEST_ASSERT_EQUAL_INT(A_BAD, result.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_BAD_FIELD, result.authorization.reason);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}

void test_rollover_request_token_is_checked_before_setup_code() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  uint8_t wrong[16];
  memcpy(wrong, token, sizeof wrong);
  wrong[0] ^= 0x40u;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult denied =
      f.setupBoundary.authenticateAndRun(
          EP_ROLLOVER, H_OK, wrong, "482731", 6, 22, action);
  TEST_ASSERT_EQUAL_INT(ASCD_AUTHORIZATION_DENIED, denied.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, denied.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_NO_TOKEN, denied.authorization.reason);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);

  const AdminSetupCodeDispatchResult allowed =
      f.setupBoundary.authenticateAndRun(
          EP_ROLLOVER, H_OK, token, "482731", 6, 23, action);
  TEST_ASSERT_EQUAL_INT(ASCD_ACTION_COMPLETED, allowed.code);
  TEST_ASSERT_EQUAL_INT(A_OK, allowed.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_OK, allowed.authentication.code);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(1, action.calls);
  TEST_ASSERT_EQUAL_UINT16(0, action.observedCounter);
  TEST_ASSERT_EQUAL_UINT32(2, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(24));
}

void test_failed_rollover_setup_code_does_not_refresh_session_idle_timer() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));

  f.setupVerifier.outcome = PV_MISMATCH;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult rejected =
      f.setupBoundary.authenticateAndRun(
          EP_ROLLOVER, H_OK, token, "000001", 6, 500, action);
  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, rejected.code);
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, rejected.authentication.code);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);

  // The failed second factor did not refresh the session at t=500.
  // It therefore expires 901 seconds after its last successful use at t=21.
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_ROSTER_GET, H_OK, token, 21 + TOKEN_IDLE_S + 1).code);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(
      21 + TOKEN_IDLE_S + 1));
}

void test_wrong_setup_code_is_durably_counted_before_verification() {
  Fixture f;
  f.setupVerifier.outcome = PV_MISMATCH;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "000001", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, result.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_BAD_SETUP, result.authorization.reason);
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT16(1, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}

void test_setup_code_health_policy_runs_after_success_and_counter_reset() {
  Fixture f;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_PIN_RESET, H_STORAGE_FAULT, nullptr, "482731", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHORIZATION_DENIED, result.code);
  TEST_ASSERT_EQUAL_INT(A_LOCKED, result.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_STATE, result.authorization.reason);
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_REFUSED, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT16(0, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(2, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}

void test_successful_pin_reset_runs_after_reset_and_revokes_sessions() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  TEST_ASSERT_EQUAL_INT(1, f.boundary.activeSessions(21));

  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_PIN_RESET, H_OK, nullptr, "482731", 6, 22, action);

  TEST_ASSERT_EQUAL_INT(ASCD_ACTION_COMPLETED, result.code);
  TEST_ASSERT_EQUAL_INT(A_OK, result.authorization.code);
  TEST_ASSERT_EQUAL_INT(PAC_OK, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT32(1, action.calls);
  TEST_ASSERT_EQUAL_UINT16(0, action.observedCounter);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(23));
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_CLOCK, H_CLOCK_FAULT, token, 23).code);
}

void test_setup_code_lockout_blocks_verifier_after_five_failures() {
  Fixture f;
  f.setupVerifier.outcome = PV_MISMATCH;
  FakeAction action(f.setupStore);
  for (uint32_t i = 0; i < 5; ++i) {
    const AdminSetupCodeDispatchResult failed =
        f.setupBoundary.authenticateAndRun(
            EP_SETUP, H_UNPROVISIONED, nullptr, "000001", 6, 100 + i,
            action);
    TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, failed.authentication.code);
  }

  f.setupVerifier.outcome = PV_MATCH;
  const AdminSetupCodeDispatchResult locked =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "482731", 6, 105, action);
  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, locked.code);
  TEST_ASSERT_EQUAL_INT(PAC_LOCKED, locked.authentication.code);
  TEST_ASSERT_EQUAL_UINT32(59, locked.authentication.retryAfterS);
  TEST_ASSERT_EQUAL_UINT16(5, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(5, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(5, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}

void test_setup_code_counter_write_failure_precedes_verifier_and_action() {
  Fixture f;
  f.setupStore.ok = false;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "482731", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(PAC_STORAGE_ERROR, result.authentication.code);
  TEST_ASSERT_TRUE(f.setupAuth.faulted());
  TEST_ASSERT_EQUAL_UINT32(1, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}


void test_setup_code_malformed_input_is_rejected_before_persist_verify_and_action() {
  Fixture f;
  FakeAction action(f.setupStore);
  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "12-456", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, result.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_BAD_SETUP, result.authorization.reason);
  TEST_ASSERT_EQUAL_INT(PAC_BAD_INPUT, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT16(0, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(0, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
  TEST_ASSERT_FALSE(f.setupAuth.faulted());
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_setup_code_verifier_error_latches_coordinator_and_never_runs_action() {
  Fixture f;
  f.setupVerifier.outcome = PV_ERROR;
  FakeAction action(f.setupStore);

  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "482731", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(PAC_VERIFY_ERROR, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT16(1, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
  TEST_ASSERT_TRUE(f.setupAuth.faulted());
  TEST_ASSERT_FALSE(f.setupAuth.ready());

  const AdminSetupCodeDispatchResult retry =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "482731", 6, 21, action);
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY, retry.authentication.code);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
}

void test_setup_code_counter_reset_persist_failure_never_runs_privileged_action() {
  Fixture f;
  f.setupStore.failPersistCall = 2;
  FakeAction action(f.setupStore);

  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_SETUP, H_UNPROVISIONED, nullptr, "482731", 6, 20, action);

  TEST_ASSERT_EQUAL_INT(ASCD_AUTHENTICATION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(PAC_STORAGE_ERROR, result.authentication.code);
  TEST_ASSERT_EQUAL_UINT16(1, f.setupStore.value);
  TEST_ASSERT_EQUAL_UINT32(2, f.setupStore.persistCalls);
  TEST_ASSERT_EQUAL_UINT32(1, f.setupVerifier.calls);
  TEST_ASSERT_EQUAL_UINT32(0, action.calls);
  TEST_ASSERT_TRUE(f.setupAuth.faulted());
  TEST_ASSERT_FALSE(f.setupAuth.ready());
}

void test_indeterminate_setup_action_faults_and_revokes_sessions() {
  Fixture f;
  uint8_t token[16] = {};
  TEST_ASSERT_EQUAL_INT(PAC_OK,
      f.boundary.beginLogin("483920", 6, 20, token).code);
  TEST_ASSERT_TRUE(f.boundary.confirmLoginResponseSent(21));
  FakeAction action(f.setupStore);
  action.outcome = PA_INDETERMINATE;

  const AdminSetupCodeDispatchResult result =
      f.setupBoundary.authenticateAndRun(
          EP_RESTORE, H_OK, nullptr, "482731", 6, 22, action);

  TEST_ASSERT_EQUAL_INT(ASCD_ACTION_REJECTED, result.code);
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_INDETERMINATE,
                        result.authentication.code);
  TEST_ASSERT_TRUE(f.setupAuth.faulted());
  TEST_ASSERT_TRUE(f.boundary.faulted());
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(23));
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, f.boundary.authorize(
      EP_CLOCK, H_CLOCK_FAULT, token, 23).code);
}

void test_bad_login_does_not_create_or_refresh_a_session() {
  Fixture f;
  f.verifier.outcome = PV_MISMATCH;
  uint8_t token[16] = {};
  const PinAuthResult result = f.boundary.beginLogin(
      "483920", 6, 20, token);

  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, result.code);
  TEST_ASSERT_EQUAL_INT(1, f.store.value);
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(21));
  TEST_ASSERT_FALSE(f.boundary.faulted());
}

void test_random_failure_latches_auth_and_clears_pending_or_live_sessions() {
  Fixture f;
  f.random.ok = false;
  uint8_t token[16] = {};
  const PinAuthResult result = f.boundary.beginLogin(
      "483920", 6, 20, token);

  TEST_ASSERT_EQUAL_INT(PAC_ACTION_INDETERMINATE, result.code);
  TEST_ASSERT_TRUE(f.boundary.faulted());
  TEST_ASSERT_EQUAL_INT(0, f.boundary.activeSessions(21));
  TEST_ASSERT_EQUAL_INT(A_OK, f.boundary.authorize(
      EP_STATUS, H_CONFIG_FAULT, nullptr, 21).code);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_login_token_is_inactive_until_full_response_is_confirmed);
  RUN_TEST(test_aborted_login_response_never_activates_token);
  RUN_TEST(test_request_authorization_uses_this_requests_token_bytes);
  RUN_TEST(test_clock_route_requires_request_token_and_clock_capable_health);
  RUN_TEST(test_generic_route_gate_cannot_authorize_login_without_pin_check);
  RUN_TEST(test_setup_code_routes_fail_closed_without_dedicated_verifier);
  RUN_TEST(test_public_status_never_requires_or_refreshes_a_session);
  RUN_TEST(test_logout_revokes_only_the_matching_request_session);
  RUN_TEST(test_expired_token_is_rejected_using_monotonic_idle_age);
  RUN_TEST(test_pin_coordinator_fault_revokes_existing_sessions_and_latches_boundary);
  RUN_TEST(test_pin_change_rejects_missing_and_wrong_request_tokens);
  RUN_TEST(test_pin_change_health_denial_precedes_pin_check_and_writer);
  RUN_TEST(test_pin_change_wrong_current_pin_never_reaches_writer);
  RUN_TEST(test_pin_change_clean_writer_refusal_preserves_existing_session);
  RUN_TEST(test_pin_change_commit_revokes_every_active_session);
  RUN_TEST(test_pin_change_indeterminate_writer_faults_and_revokes_sessions);
  RUN_TEST(test_setup_code_boundary_rejects_non_setup_endpoint);
  RUN_TEST(test_rollover_request_token_is_checked_before_setup_code);
  RUN_TEST(test_failed_rollover_setup_code_does_not_refresh_session_idle_timer);
  RUN_TEST(test_wrong_setup_code_is_durably_counted_before_verification);
  RUN_TEST(test_setup_code_health_policy_runs_after_success_and_counter_reset);
  RUN_TEST(test_successful_pin_reset_runs_after_reset_and_revokes_sessions);
  RUN_TEST(test_setup_code_lockout_blocks_verifier_after_five_failures);
  RUN_TEST(test_setup_code_counter_write_failure_precedes_verifier_and_action);
  RUN_TEST(test_setup_code_malformed_input_is_rejected_before_persist_verify_and_action);
  RUN_TEST(test_setup_code_verifier_error_latches_coordinator_and_never_runs_action);
  RUN_TEST(test_setup_code_counter_reset_persist_failure_never_runs_privileged_action);
  RUN_TEST(test_indeterminate_setup_action_faults_and_revokes_sessions);
  RUN_TEST(test_bad_login_does_not_create_or_refresh_a_session);
  RUN_TEST(test_random_failure_latches_auth_and_clears_pending_or_live_sessions);
  return UNITY_END();
}
