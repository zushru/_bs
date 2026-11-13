#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <esp_random.h>

#include "esp32_ram_setup_code_verifier.h"
#include "i2c_bus_lock.h"
#include "oled_test_state.h"

using namespace att;

TwoWire Wire;

namespace oled_test {
State& state() {
  static State value;
  return value;
}

void reset() {
  State& s = state();
  memset(&s, 0, sizeof s);
  s.beginResult = true;
  for (size_t row = 0; row < 4; ++row) {
    memset(s.bufferLines[row], ' ', sizeof s.bufferLines[row] - 1);
    s.bufferLines[row][sizeof s.bufferLines[row] - 1] = '\0';
    memset(s.visibleLines[row], ' ', sizeof s.visibleLines[row] - 1);
    s.visibleLines[row][sizeof s.visibleLines[row] - 1] = '\0';
  }
}
}  // namespace oled_test

static uint32_t gRandomWord = 7;
static unsigned gRandomCalls = 0;

extern "C" void esp_fill_random(void* buffer, size_t length) {
  ++gRandomCalls;
  uint8_t* out = static_cast<uint8_t*>(buffer);
  for (size_t i = 0; i < length; ++i) {
    const unsigned shift = static_cast<unsigned>(3 - (i % 4)) * 8;
    out[i] = static_cast<uint8_t>(gRandomWord >> shift);
  }
}

class TestI2cBusLock : public II2cBusLock {
 public:
  TestI2cBusLock() : allow(true), held(false), acquireCalls(0) {}
  bool acquire() override {
    ++acquireCalls;
    if (!allow || held) return false;
    held = true;
    return true;
  }
  void release() override { held = false; }

  bool allow;
  bool held;
  unsigned acquireCalls;
};

class TestSecureRandom : public Esp32SecureRandom {
 public:
  TestSecureRandom() : fail(false), fillCalls(0) {}
  bool fill(uint8_t* out, size_t length) override {
    ++fillCalls;
    if (fail) return false;
    return Esp32SecureRandom::fill(out, length);
  }

  bool fail;
  unsigned fillCalls;
};

class FakeMonotonic : public IClockMonotonicSource {
 public:
  FakeMonotonic() : now(100), valid(true), calls(0), failOnCall(0) {}
  bool nowMicros(uint64_t& out) override {
    ++calls;
    if (!valid || (failOnCall != 0 && calls == failOnCall)) return false;
    out = now;
    return true;
  }

  uint64_t now;
  bool valid;
  unsigned calls;
  unsigned failOnCall;
};

namespace att {
struct Esp32RamSetupCodeVerifierTestAccess {
  static bool codeBufferIsZero(const Esp32RamSetupCodeVerifier& verifier) {
    for (size_t i = 0; i < sizeof verifier.code_; ++i) {
      if (verifier.code_[i] != '\0') return false;
    }
    return true;
  }
};
}  // namespace att

struct Fixture {
  TestSecureRandom random;
  FakeMonotonic monotonic;
  TestI2cBusLock lock;
  Esp32Oled oled;
  Esp32RamSetupCodeVerifier verifier;

  Fixture()
      : random(), monotonic(), lock(), oled(Wire, lock),
        verifier(random, monotonic, oled) {
    TEST_ASSERT_EQUAL_INT(EOI_BUFFER_READY, oled.begin());
  }

  void startRf() { random.notifyRfStarted(); }
};

void setUp() {
  oled_test::reset();
  gRandomWord = 7;
  gRandomCalls = 0;
}
void tearDown() {}

void test_issue_refuses_before_rf_is_ready_and_wipes_state() {
  Fixture f;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_EQUAL_UINT32(0, gRandomCalls);
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[row]);
  }
}

void test_rf_loss_after_issue_invalidates_active_code() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.random.notifyRfStopped();
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_random_source_failure_latches_fault_without_issuing_code() {
  Fixture f;
  f.startRf();
  f.random.fail = true;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_EQUAL_UINT32(1, f.random.fillCalls);
  TEST_ASSERT_EQUAL_UINT32(0, gRandomCalls);
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_valid_issue_displays_code_and_clears_software_framebuffer() {
  Fixture f;
  f.startRf();
  gRandomWord = 482731;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_EQUAL_STRING("  482731", oled_test::state().visibleLines[2]);
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().bufferLines[row]);
  }
}

void test_leading_zeroes_are_preserved_on_oled_and_in_verifier() {
  Fixture f;
  f.startRf();
  gRandomWord = 7;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_EQUAL_STRING("  000007", oled_test::state().visibleLines[2]);
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("000007", 6));
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[row]);
  }
}

void test_duplicate_issue_is_refused_while_code_is_active() {
  Fixture f;
  f.startRf();
  gRandomWord = 482731;
  TEST_ASSERT_TRUE(f.verifier.issue());
  gRandomWord = 123;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_EQUAL_UINT32(1, gRandomCalls);
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("482731", 6));
}

void test_mismatch_retains_code_and_match_consumes_it_only_once() {
  Fixture f;
  f.startRf();
  gRandomWord = 77;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("000078", 6));
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("000077", 6));
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("000077", 6));
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_malformed_candidates_never_match_or_consume_code() {
  Fixture f;
  f.startRf();
  gRandomWord = 123456;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("12-456", 6));
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("12345", 5));
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify(nullptr, 6));
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("123456", 6));
}

void test_exact_600_second_expiry_boundary_invalidates_and_blanks() {
  Fixture f;
  f.startRf();
  gRandomWord = 12;
  TEST_ASSERT_TRUE(f.verifier.issue());

  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros - 1;
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("999999", 6));
  TEST_ASSERT_TRUE(f.verifier.active());

  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros;
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("000012", 6));
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[row]);
  }
}

void test_rotation_requests_blank_before_displaying_replacement() {
  Fixture f;
  f.startRf();
  gRandomWord = 10;
  TEST_ASSERT_TRUE(f.verifier.issue());
  const unsigned before = oled_test::state().displayCalls;

  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros;
  gRandomWord = 11;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_EQUAL_UINT32(before + 2, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_STRING("  000011", oled_test::state().visibleLines[2]);
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("000011", 6));
}

void test_clock_failure_before_issue_fails_closed_without_random_use() {
  Fixture f;
  f.startRf();
  f.monotonic.valid = false;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_EQUAL_UINT32(0, gRandomCalls);
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_clock_failure_after_display_clears_code_and_requests_blank() {
  Fixture f;
  f.startRf();
  f.monotonic.failOnCall = 2;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[row]);
  }
}

void test_clock_regression_during_verification_fails_closed() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.monotonic.now = 99;
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_display_lock_failure_prevents_issue_and_latches_fault() {
  Fixture f;
  f.startRf();
  f.lock.allow = false;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}

void test_blank_frame_failure_after_match_returns_error_and_consumes_code() {
  Fixture f;
  f.startRf();
  gRandomWord = 482731;
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.lock.allow = false;
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("482731", 6));
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  // The panel may retain its prior image because the shared lock prevented
  // a blank transfer; authentication still fails closed.
  TEST_ASSERT_EQUAL_STRING("  482731", oled_test::state().visibleLines[2]);
}

void test_expiry_blank_failure_prevents_code_rotation() {
  Fixture f;
  f.startRf();
  gRandomWord = 10;
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros;
  f.lock.allow = false;
  gRandomWord = 11;
  TEST_ASSERT_FALSE(f.verifier.issue());
  TEST_ASSERT_EQUAL_UINT32(1, gRandomCalls);
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
}


void test_expiry_service_is_inactive_idempotent_noop() {
  Fixture f;
  const unsigned clockCalls = f.monotonic.calls;
  const unsigned displayCalls = oled_test::state().displayCalls;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_EQUAL_UINT32(clockCalls, f.monotonic.calls);
  TEST_ASSERT_EQUAL_UINT32(displayCalls, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(0, f.random.fillCalls);
  TEST_ASSERT_FALSE(f.verifier.faulted());
}

void test_expiry_service_before_deadline_retains_active_code() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  const unsigned fillCalls = f.random.fillCalls;
  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros - 1;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_EQUAL_UINT32(displayCalls, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_UINT32(fillCalls, f.random.fillCalls);
  TEST_ASSERT_EQUAL_STRING("  000007", oled_test::state().visibleLines[2]);
}

void test_expiry_service_at_exact_boundary_wipes_and_requests_one_blank() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  const unsigned fillCalls = f.random.fillCalls;
  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_FALSE(f.verifier.faulted());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(displayCalls + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_UINT32(fillCalls, f.random.fillCalls);
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("000007", 6));
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[row]);
  }
  const unsigned afterExpiryDisplayCalls = oled_test::state().displayCalls;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_EQUAL_UINT32(afterExpiryDisplayCalls,
                           oled_test::state().displayCalls);
}

void test_expiry_service_beyond_boundary_expires_without_rotation() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros + 1;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_FALSE(f.verifier.faulted());
  TEST_ASSERT_EQUAL_UINT32(displayCalls + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
}

void test_expiry_service_clock_failure_latches_and_blanks() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.monotonic.valid = false;
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  TEST_ASSERT_FALSE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(displayCalls + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
}

void test_expiry_service_monotonic_regression_latches_and_blanks() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.monotonic.now = 150;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  f.monotonic.now = 149;  // Still after issue time, but backwards from last observation.
  TEST_ASSERT_FALSE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(displayCalls + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
}

void test_expiry_service_rf_gate_loss_latches_and_blanks() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.random.notifyRfStopped();
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  TEST_ASSERT_FALSE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(displayCalls + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
}

void test_expiry_service_blank_transfer_failure_latches_without_retry() {
  Fixture f;
  f.startRf();
  TEST_ASSERT_TRUE(f.verifier.issue());
  f.monotonic.now = 100 + Esp32RamSetupCodeVerifier::kLifetimeMicros;
  f.lock.allow = false;
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned acquireCalls = f.lock.acquireCalls;
  const unsigned randomCalls = gRandomCalls;
  TEST_ASSERT_FALSE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(displayCalls, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(acquireCalls + 1, f.lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_STRING("  000007", oled_test::state().visibleLines[2]);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
}

void test_expiry_service_after_consumption_is_idempotent() {
  Fixture f;
  f.startRf();
  gRandomWord = 123456;
  TEST_ASSERT_TRUE(f.verifier.issue());
  TEST_ASSERT_EQUAL_INT(PV_MATCH, f.verifier.verify("123456", 6));
  const unsigned displayCalls = oled_test::state().displayCalls;
  const unsigned randomCalls = gRandomCalls;
  const unsigned fillCalls = f.random.fillCalls;
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_TRUE(f.verifier.serviceExpiry());
  TEST_ASSERT_EQUAL_UINT32(displayCalls, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_UINT32(fillCalls, f.random.fillCalls);
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_FALSE(f.verifier.faulted());
}


void test_verify_detects_cross_operation_monotonic_regression_after_mismatch() {
  Fixture f;
  f.startRf();
  gRandomWord = 7;
  TEST_ASSERT_TRUE(f.verifier.issue());

  const unsigned randomCalls = gRandomCalls;
  const unsigned fillCalls = f.random.fillCalls;
  const unsigned displayCalls = oled_test::state().displayCalls;

  f.monotonic.now = 150;
  TEST_ASSERT_EQUAL_INT(PV_MISMATCH, f.verifier.verify("000008", 6));
  TEST_ASSERT_TRUE(f.verifier.active());
  TEST_ASSERT_FALSE(f.verifier.faulted());

  const unsigned acquireCalls = f.lock.acquireCalls;
  f.monotonic.now = 149;  // After issue time, but earlier than the last observation.
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
  TEST_ASSERT_TRUE(f.verifier.faulted());
  TEST_ASSERT_FALSE(f.verifier.active());
  TEST_ASSERT_TRUE(Esp32RamSetupCodeVerifierTestAccess::codeBufferIsZero(
      f.verifier));
  TEST_ASSERT_EQUAL_UINT32(acquireCalls + 1, f.lock.acquireCalls);
  TEST_ASSERT_EQUAL_UINT32(displayCalls, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_UINT32(randomCalls, gRandomCalls);
  TEST_ASSERT_EQUAL_UINT32(fillCalls, f.random.fillCalls);
  TEST_ASSERT_EQUAL_INT(PV_ERROR, f.verifier.verify("000007", 6));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_issue_refuses_before_rf_is_ready_and_wipes_state);
  RUN_TEST(test_random_source_failure_latches_fault_without_issuing_code);
  RUN_TEST(test_valid_issue_displays_code_and_clears_software_framebuffer);
  RUN_TEST(test_leading_zeroes_are_preserved_on_oled_and_in_verifier);
  RUN_TEST(test_duplicate_issue_is_refused_while_code_is_active);
  RUN_TEST(test_rf_loss_after_issue_invalidates_active_code);
  RUN_TEST(test_mismatch_retains_code_and_match_consumes_it_only_once);
  RUN_TEST(test_malformed_candidates_never_match_or_consume_code);
  RUN_TEST(test_exact_600_second_expiry_boundary_invalidates_and_blanks);
  RUN_TEST(test_rotation_requests_blank_before_displaying_replacement);
  RUN_TEST(test_clock_failure_before_issue_fails_closed_without_random_use);
  RUN_TEST(test_clock_failure_after_display_clears_code_and_requests_blank);
  RUN_TEST(test_clock_regression_during_verification_fails_closed);
  RUN_TEST(test_display_lock_failure_prevents_issue_and_latches_fault);
  RUN_TEST(test_blank_frame_failure_after_match_returns_error_and_consumes_code);
  RUN_TEST(test_expiry_blank_failure_prevents_code_rotation);
  RUN_TEST(test_expiry_service_is_inactive_idempotent_noop);
  RUN_TEST(test_expiry_service_before_deadline_retains_active_code);
  RUN_TEST(test_expiry_service_at_exact_boundary_wipes_and_requests_one_blank);
  RUN_TEST(test_expiry_service_beyond_boundary_expires_without_rotation);
  RUN_TEST(test_expiry_service_clock_failure_latches_and_blanks);
  RUN_TEST(test_expiry_service_monotonic_regression_latches_and_blanks);
  RUN_TEST(test_expiry_service_rf_gate_loss_latches_and_blanks);
  RUN_TEST(test_expiry_service_blank_transfer_failure_latches_without_retry);
  RUN_TEST(test_expiry_service_after_consumption_is_idempotent);
  RUN_TEST(test_verify_detects_cross_operation_monotonic_regression_after_mismatch);
  return UNITY_END();
}
