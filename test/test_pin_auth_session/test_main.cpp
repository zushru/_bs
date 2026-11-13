#include <unity.h>
#include <string.h>
#include "pin_auth_session.h"

using namespace att;

void setUp() {}
void tearDown() {}

struct CounterStore : IPinAttemptStore {
  uint16_t value;
  bool ok;
  CounterStore() : value(0), ok(true) {}
  bool loadFailureCount(uint16_t& out) override {
    if (!ok) return false;
    out = value;
    return true;
  }
  bool persistFailureCount(uint16_t next) override {
    if (!ok) return false;
    value = next;
    return true;
  }
};

struct MatchVerifier : IPinVerifier {
  PinVerifyOutcome result;
  MatchVerifier() : result(PV_MATCH) {}
  PinVerifyOutcome verify(const char*, size_t) override { return result; }
};

struct SequenceRandom : ISecureRandomSource {
  uint8_t next[16];
  bool ok;
  int calls;
  size_t requested;
  SequenceRandom() : next{0}, ok(true), calls(0), requested(0) {}
  bool fill(uint8_t* out, size_t length) override {
    ++calls;
    requested = length;
    if (!ok || out == nullptr || length != sizeof next) return false;
    memcpy(out, next, sizeof next);
    return true;
  }
  void seed(uint8_t value) {
    for (size_t i = 0; i < sizeof next; ++i)
      next[i] = static_cast<uint8_t>(value + i * 13u + 1u);
  }
};

static void authenticate(PinAuthCoordinator& auth, MatchVerifier& verifier,
                         TokenSessionSuccessAction& action, PinAuthCode expected) {
  const PinAuthResult result = auth.authenticateAndRun(
      "483920", 6, 100, verifier, action);
  TEST_ASSERT_EQUAL_INT(expected, result.code);
}

void test_token_action_issues_16_bytes_and_commits_only_after_delivery() {
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;
  random.seed(0x21);
  TokenTable sessions;
  {
    TokenSessionSuccessAction action(sessions, random);
    authenticate(auth, verifier, action, PAC_OK);
    TEST_ASSERT_EQUAL_INT(1, random.calls);
    TEST_ASSERT_EQUAL_UINT32(16, random.requested);
    uint8_t token[16] = {0};
    TEST_ASSERT_TRUE(action.takeToken(token));
    TEST_ASSERT_FALSE(sessions.check(token, 101));  // not active before response delivery
    TEST_ASSERT_EQUAL_INT(0, sessions.active(101));
    TEST_ASSERT_FALSE(action.takeToken(token));
    TEST_ASSERT_TRUE(action.confirmResponseSent(101));
    TEST_ASSERT_TRUE(sessions.check(token, 101));
    TEST_ASSERT_FALSE(action.confirmResponseSent(101));
  }
  uint8_t expected[16];
  random.seed(0x21);
  memcpy(expected, random.next, sizeof expected);
  TEST_ASSERT_TRUE(sessions.check(expected, 102));
  TEST_ASSERT_EQUAL_INT(1, sessions.active(102));
}

void test_random_source_failure_latches_auth_without_creating_session() {
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;
  random.ok = false;
  TokenTable sessions;
  TokenSessionSuccessAction action(sessions, random);
  authenticate(auth, verifier, action, PAC_ACTION_INDETERMINATE);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_INT(0, sessions.active(101));
  TEST_ASSERT_FALSE(action.takeToken(nullptr));
}

void test_all_zero_random_output_fails_closed() {
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;  // zero-filled output is forbidden by TokenTable
  TokenTable sessions;
  TokenSessionSuccessAction action(sessions, random);
  authenticate(auth, verifier, action, PAC_ACTION_INDETERMINATE);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_INT(0, sessions.active(101));
}

void test_duplicate_random_token_fails_closed_without_replacing_existing_session() {
  TokenTable sessions;
  SequenceRandom random;
  random.seed(0x30);
  uint8_t existing[16];
  memcpy(existing, random.next, sizeof existing);
  TEST_ASSERT_TRUE(sessions.issue(existing, 90));

  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  TokenSessionSuccessAction action(sessions, random);
  authenticate(auth, verifier, action, PAC_ACTION_INDETERMINATE);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_INT(1, sessions.active(101));
  TEST_ASSERT_TRUE(sessions.check(existing, 101));
}

void test_pending_token_never_becomes_a_session_when_response_is_abandoned() {
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;
  random.seed(0x51);
  TokenTable sessions;
  uint8_t leaked[16] = {0};
  {
    TokenSessionSuccessAction action(sessions, random);
    authenticate(auth, verifier, action, PAC_OK);
    TEST_ASSERT_TRUE(action.takeToken(leaked));
    TEST_ASSERT_EQUAL_INT(0, sessions.active(101));
    // No confirmResponseSent(nowS): destructor clears the pending token; no session was inserted.
  }
  TEST_ASSERT_FALSE(sessions.check(leaked, 102));
  TEST_ASSERT_EQUAL_INT(0, sessions.active(102));
}

void test_explicit_abort_discards_pending_token_before_insertion() {
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;
  random.seed(0x63);
  TokenTable sessions;
  TokenSessionSuccessAction action(sessions, random);
  authenticate(auth, verifier, action, PAC_OK);
  uint8_t token[16];
  TEST_ASSERT_TRUE(action.takeToken(token));
  action.abort();
  TEST_ASSERT_EQUAL_INT(0, sessions.active(102));
  TEST_ASSERT_FALSE(sessions.check(token, 102));
  TEST_ASSERT_FALSE(action.confirmResponseSent(101));
}

void test_delivery_cannot_be_confirmed_before_token_is_copied() {
  TokenTable sessions;
  SequenceRandom random;
  random.seed(0x72);
  TokenSessionSuccessAction action(sessions, random);
  TEST_ASSERT_EQUAL_INT(PA_DONE, action.run());
  TEST_ASSERT_FALSE(action.confirmResponseSent(102));
  TEST_ASSERT_EQUAL_INT(0, sessions.active(1));
}

void test_token_idle_expiry_starts_at_confirmed_delivery_time() {
  TokenTable sessions;
  SequenceRandom random;
  random.seed(0x8D);
  TokenSessionSuccessAction action(sessions, random);
  TEST_ASSERT_EQUAL_INT(PA_DONE, action.run());
  uint8_t token[16] = {0};
  TEST_ASSERT_TRUE(action.takeToken(token));
  TEST_ASSERT_TRUE(action.confirmResponseSent(1000));
  // At exactly 900 seconds from delivery the session remains live. The token
  // must not be aged from the earlier PIN-check/request-construction time.
  TEST_ASSERT_TRUE(sessions.check(token, 1000 + TOKEN_IDLE_S));
}
void test_action_is_one_shot_and_second_run_fails_indeterminate() {
  TokenTable sessions;
  SequenceRandom random;
  random.seed(0x83);
  TokenSessionSuccessAction action(sessions, random);
  TEST_ASSERT_EQUAL_INT(PA_DONE, action.run());
  TEST_ASSERT_EQUAL_INT(PA_INDETERMINATE, action.run());
  TEST_ASSERT_EQUAL_INT(1, random.calls);
  TEST_ASSERT_EQUAL_INT(0, sessions.active(1));
}

void test_abandoned_login_does_not_evict_existing_lru_session() {
  TokenTable sessions;
  uint8_t a[16], b[16];
  for (size_t i = 0; i < sizeof a; ++i) {
    a[i] = static_cast<uint8_t>(i + 5);
    b[i] = static_cast<uint8_t>(i + 90);
  }
  TEST_ASSERT_TRUE(sessions.issue(a, 1));
  TEST_ASSERT_TRUE(sessions.issue(b, 2));
  TEST_ASSERT_TRUE(sessions.check(a, 3));  // b is least recently used
  CounterStore store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(100));
  MatchVerifier verifier;
  SequenceRandom random;
  random.seed(0xA5);
  uint8_t abandoned[16] = {0};
  {
    TokenSessionSuccessAction action(sessions, random);
    authenticate(auth, verifier, action, PAC_OK);
    TEST_ASSERT_FALSE(action.evictedExistingSession());
    TEST_ASSERT_EQUAL_INT(2, sessions.active(4));
    TEST_ASSERT_TRUE(action.takeToken(abandoned));
    // Response is abandoned. No session insertion or LRU eviction is allowed.
  }
  TEST_ASSERT_EQUAL_INT(2, sessions.active(5));
  TEST_ASSERT_TRUE(sessions.check(a, 5));
  TEST_ASSERT_TRUE(sessions.check(b, 5));
  TEST_ASSERT_FALSE(sessions.check(abandoned, 5));
}
void test_action_reports_existing_session_eviction() {
  TokenTable sessions;
  SequenceRandom random;
  uint8_t a[16], b[16];
  for (size_t i = 0; i < sizeof a; ++i) {
    a[i] = static_cast<uint8_t>(i + 1);
    b[i] = static_cast<uint8_t>(i + 80);
  }
  TEST_ASSERT_TRUE(sessions.issue(a, 1));
  TEST_ASSERT_TRUE(sessions.issue(b, 2));
  TEST_ASSERT_TRUE(sessions.check(a, 3));  // b is least recently used
  random.seed(0x94);
  TokenSessionSuccessAction action(sessions, random);
  TEST_ASSERT_EQUAL_INT(PA_DONE, action.run());
  TEST_ASSERT_FALSE(action.evictedExistingSession());
  TEST_ASSERT_EQUAL_INT(2, sessions.active(4));
  uint8_t issued[16];
  TEST_ASSERT_TRUE(action.takeToken(issued));
  TEST_ASSERT_TRUE(action.confirmResponseSent(4));
  TEST_ASSERT_TRUE(action.evictedExistingSession());
  TEST_ASSERT_TRUE(sessions.check(a, 5));
  TEST_ASSERT_FALSE(sessions.check(b, 5));
  TEST_ASSERT_TRUE(sessions.check(issued, 5));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_token_action_issues_16_bytes_and_commits_only_after_delivery);
  RUN_TEST(test_random_source_failure_latches_auth_without_creating_session);
  RUN_TEST(test_all_zero_random_output_fails_closed);
  RUN_TEST(test_duplicate_random_token_fails_closed_without_replacing_existing_session);
  RUN_TEST(test_pending_token_never_becomes_a_session_when_response_is_abandoned);
  RUN_TEST(test_explicit_abort_discards_pending_token_before_insertion);
  RUN_TEST(test_delivery_cannot_be_confirmed_before_token_is_copied);
  RUN_TEST(test_token_idle_expiry_starts_at_confirmed_delivery_time);
  RUN_TEST(test_action_is_one_shot_and_second_run_fails_indeterminate);
  RUN_TEST(test_abandoned_login_does_not_evict_existing_lru_session);
  RUN_TEST(test_action_reports_existing_session_eviction);
  return UNITY_END();
}
