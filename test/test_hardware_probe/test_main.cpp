#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../target/esp32_hardware_probe/probe_credential.h"

using namespace att;

void setUp() {}
void tearDown() {}

void test_encode_probe_credential_matches_exact_lowercase_hex() {
  const uint8_t entropy[16] = {
      0x00, 0x01, 0x02, 0x03, 0x0A, 0x0B, 0x0C, 0x0D,
      0x10, 0x11, 0x7E, 0x7F, 0xF0, 0xFE, 0xFF, 0x99};
  char output[33];
  TEST_ASSERT_TRUE(encodeProbeCredential(entropy, sizeof entropy,
                                         output, sizeof output));
  TEST_ASSERT_EQUAL_STRING("000102030a0b0c0d10117e7ff0feff99", output);
  TEST_ASSERT_EQUAL_UINT8(0, output[32]);
}

void test_encoder_rejects_null_entropy_and_clears_output() {
  char output[40];
  memset(output, 'X', sizeof output);
  TEST_ASSERT_FALSE(encodeProbeCredential(nullptr, 16, output, sizeof output));
  for (size_t i = 0; i < sizeof output; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, output[i]);
  }
}

void test_encoder_rejects_wrong_entropy_length() {
  const uint8_t entropy[16] = {};
  char output[40];
  memset(output, 'X', sizeof output);
  TEST_ASSERT_FALSE(encodeProbeCredential(entropy, 15, output, sizeof output));
  for (size_t i = 0; i < sizeof output; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, output[i]);
  }
  memset(output, 'X', sizeof output);
  TEST_ASSERT_FALSE(encodeProbeCredential(entropy, 17, output, sizeof output));
  for (size_t i = 0; i < sizeof output; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, output[i]);
  }
}

void test_encoder_rejects_short_or_null_output() {
  const uint8_t entropy[16] = {};
  char shortOutput[32];
  memset(shortOutput, 'X', sizeof shortOutput);
  TEST_ASSERT_FALSE(encodeProbeCredential(entropy, sizeof entropy,
                                          shortOutput, sizeof shortOutput));
  for (size_t i = 0; i < sizeof shortOutput; ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, shortOutput[i]);
  }
  TEST_ASSERT_FALSE(encodeProbeCredential(entropy, sizeof entropy, nullptr, 33));
}


int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_encode_probe_credential_matches_exact_lowercase_hex);
  RUN_TEST(test_encoder_rejects_null_entropy_and_clears_output);
  RUN_TEST(test_encoder_rejects_wrong_entropy_length);
  RUN_TEST(test_encoder_rejects_short_or_null_output);
  return UNITY_END();
}
