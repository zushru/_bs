#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp32_oled.h"
#include "oled_test_state.h"

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

using namespace att;

static const char kPassphrase[] = "0123456789abcdef0123456789abcdef";

void setUp() { oled_test::reset(); }
void tearDown() {}

void test_constructor_does_not_initialize_shared_bus_or_display() {
  Esp32Oled oled(Wire);
  TEST_ASSERT_FALSE(oled.initialized());
  TEST_ASSERT_EQUAL_UINT32(0, oled_test::state().beginCalls);
  TEST_ASSERT_EQUAL_UINT32(0, oled_test::state().displayCalls);
}

void test_begin_uses_128x32_0x3c_and_does_not_reinitialize_wire() {
  Esp32Oled oled(Wire);
  TEST_ASSERT_EQUAL_INT(EOI_BUFFER_READY, oled.begin());
  TEST_ASSERT_TRUE(oled.initialized());
  const oled_test::State& s = oled_test::state();
  TEST_ASSERT_EQUAL_UINT8(128, s.width);
  TEST_ASSERT_EQUAL_UINT8(32, s.height);
  TEST_ASSERT_EQUAL_UINT8(SSD1306_SWITCHCAPVCC, s.vcc);
  TEST_ASSERT_EQUAL_UINT8(0x3C, s.address);
  TEST_ASSERT_TRUE(s.reset);
  TEST_ASSERT_FALSE(s.periphBegin);
  TEST_ASSERT_EQUAL_UINT32(100000UL, s.clkDuring);
  TEST_ASSERT_EQUAL_UINT32(100000UL, s.clkAfter);
  TEST_ASSERT_TRUE(s.bus == &Wire);
  TEST_ASSERT_EQUAL_UINT32(1, s.clearCalls);
  TEST_ASSERT_EQUAL_UINT32(1, s.displayCalls);
}

void test_begin_allocation_failure_does_not_enable_methods_and_can_retry() {
  Esp32Oled oled(Wire);
  oled_test::state().beginResult = false;
  TEST_ASSERT_EQUAL_INT(EOI_BUFFER_ALLOC_FAILED, oled.begin());
  TEST_ASSERT_FALSE(oled.initialized());
  ApCredentialResult result = {APCB_OK, true};
  TEST_ASSERT_EQUAL_INT(EOA_NOT_INITIALIZED,
                       oled.showSetupCredential(result, kPassphrase, 32));
  oled_test::state().beginResult = true;
  TEST_ASSERT_EQUAL_INT(EOI_BUFFER_READY, oled.begin());
  TEST_ASSERT_TRUE(oled.initialized());
  TEST_ASSERT_EQUAL_UINT32(2, oled_test::state().beginCalls);
}

void test_begin_is_idempotent() {
  Esp32Oled oled(Wire);
  TEST_ASSERT_EQUAL_INT(EOI_BUFFER_READY, oled.begin());
  TEST_ASSERT_EQUAL_INT(EOI_ALREADY_INITIALIZED, oled.begin());
  TEST_ASSERT_EQUAL_UINT32(1, oled_test::state().beginCalls);
}

void test_show_refuses_before_begin_without_display_calls() {
  Esp32Oled oled(Wire);
  ApCredentialResult result = {APCB_OK, true};
  TEST_ASSERT_EQUAL_INT(EOA_NOT_INITIALIZED,
                       oled.showSetupCredential(result, kPassphrase, 32));
  TEST_ASSERT_EQUAL_UINT32(0, oled_test::state().displayCalls);
}

void test_setup_credential_is_rendered_as_two_complete_rows() {
  Esp32Oled oled(Wire);
  TEST_ASSERT_EQUAL_INT(EOI_BUFFER_READY, oled.begin());
  ApCredentialResult result = {APCB_OK, true};
  TEST_ASSERT_EQUAL_INT(EOA_TRANSFER_REQUESTED,
                       oled.showSetupCredential(result, kPassphrase, 32));

  const oled_test::State& s = oled_test::state();
  TEST_ASSERT_EQUAL_STRING("Wi-Fi: att", s.visibleLines[0]);
  TEST_ASSERT_EQUAL_STRING("WPA2 password:", s.visibleLines[1]);
  TEST_ASSERT_EQUAL_STRING("  0123456789abcdef", s.visibleLines[2]);
  TEST_ASSERT_EQUAL_STRING("  0123456789abcdef", s.visibleLines[3]);
  TEST_ASSERT_EQUAL_UINT32(2, s.displayCalls);
  TEST_ASSERT_EQUAL_UINT32(56, s.writeCalls);
  // The rendered image remains on the OLED model, but the RAM framebuffer is
  // blank immediately after the synchronous transfer request returns.
  TEST_ASSERT_EQUAL_STRING("", s.bufferLines[2]);
  TEST_ASSERT_EQUAL_STRING("", s.bufferLines[3]);
}

void test_ready_or_non_disclosure_result_clears_previous_secret() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult setup = {APCB_OK, true};
  oled.showSetupCredential(setup, kPassphrase, 32);
  const unsigned before = oled_test::state().displayCalls;

  ApCredentialResult ready = {APCB_OK, false};
  TEST_ASSERT_EQUAL_INT(EOA_NOT_DISCLOSABLE,
                       oled.showSetupCredential(ready, kPassphrase, 32));
  TEST_ASSERT_EQUAL_UINT32(before + 1, oled_test::state().displayCalls);
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("",
                             oled_test::state().visibleLines[row]);
  }
}

void test_failed_bootstrap_result_clears_previous_secret() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult setup = {APCB_OK, true};
  oled.showSetupCredential(setup, kPassphrase, 32);
  const unsigned before = oled_test::state().displayCalls;

  ApCredentialResult failed = {APCB_RANDOM_FAILED, true};
  TEST_ASSERT_EQUAL_INT(EOA_NOT_DISCLOSABLE,
                       oled.showSetupCredential(failed, kPassphrase, 32));
  TEST_ASSERT_EQUAL_UINT32(before + 1, oled_test::state().displayCalls);
  TEST_ASSERT_EQUAL_STRING("",
                           oled_test::state().visibleLines[2]);
}

void test_invalid_credential_clears_screen_and_is_not_truncated() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult result = {APCB_OK, true};
  TEST_ASSERT_EQUAL_INT(EOA_TRANSFER_REQUESTED,
                       oled.showSetupCredential(result, kPassphrase, 32));
  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, "short-key", 9));
  TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[2]);
  TEST_ASSERT_EQUAL_STRING("",
                           oled_test::state().visibleLines[2]);

  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, kPassphrase, 31));
  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, kPassphrase, 33));
  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, nullptr, 32));
  TEST_ASSERT_EQUAL_STRING("", oled_test::state().visibleLines[2]);
}

void test_non_hex_and_uppercase_credentials_are_rejected() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult result = {APCB_OK, true};
  char candidate[33];
  memcpy(candidate, kPassphrase, sizeof candidate);
  candidate[7] = 'G';
  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, candidate, 32));
  memcpy(candidate, kPassphrase, sizeof candidate);
  candidate[0] = 'A';
  TEST_ASSERT_EQUAL_INT(EOA_INVALID_CREDENTIAL,
                       oled.showSetupCredential(result, candidate, 32));
}

void test_clear_blanks_panel_and_does_not_require_credential_data() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult result = {APCB_OK, true};
  oled.showSetupCredential(result, kPassphrase, 32);
  TEST_ASSERT_EQUAL_INT(EOA_TRANSFER_REQUESTED, oled.clear());
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("",
                             oled_test::state().visibleLines[row]);
  }
}

void test_invalid_disclosure_clears_buffer_without_modifying_caller_secret() {
  Esp32Oled oled(Wire);
  oled.begin();
  char copy[sizeof kPassphrase];
  memcpy(copy, kPassphrase, sizeof copy);
  ApCredentialResult result = {APCB_CONFIG_MISMATCH, true};
  TEST_ASSERT_EQUAL_INT(EOA_NOT_DISCLOSABLE,
                       oled.showSetupCredential(result, copy, 32));
  TEST_ASSERT_EQUAL_MEMORY(kPassphrase, copy, sizeof copy);
}

void test_display_framebuffer_does_not_retain_rendered_secret() {
  Esp32Oled oled(Wire);
  oled.begin();
  ApCredentialResult result = {APCB_OK, true};
  oled.showSetupCredential(result, kPassphrase, 32);
  for (size_t row = 0; row < 4; ++row) {
    TEST_ASSERT_EQUAL_STRING("",
                             oled_test::state().bufferLines[row]);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_constructor_does_not_initialize_shared_bus_or_display);
  RUN_TEST(test_begin_uses_128x32_0x3c_and_does_not_reinitialize_wire);
  RUN_TEST(test_begin_allocation_failure_does_not_enable_methods_and_can_retry);
  RUN_TEST(test_begin_is_idempotent);
  RUN_TEST(test_show_refuses_before_begin_without_display_calls);
  RUN_TEST(test_setup_credential_is_rendered_as_two_complete_rows);
  RUN_TEST(test_ready_or_non_disclosure_result_clears_previous_secret);
  RUN_TEST(test_failed_bootstrap_result_clears_previous_secret);
  RUN_TEST(test_invalid_credential_clears_screen_and_is_not_truncated);
  RUN_TEST(test_non_hex_and_uppercase_credentials_are_rejected);
  RUN_TEST(test_clear_blanks_panel_and_does_not_require_credential_data);
  RUN_TEST(test_invalid_disclosure_clears_buffer_without_modifying_caller_secret);
  RUN_TEST(test_display_framebuffer_does_not_retain_rendered_secret);
  return UNITY_END();
}
