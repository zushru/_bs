#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include "esp32_secure_random.h"

using namespace att;

static int gCalls = 0;
static size_t gLength = 0;
static uint8_t gNext = 0x10;

extern "C" void esp_fill_random(void* buffer, size_t length) {
  ++gCalls;
  gLength = length;
  uint8_t* out = static_cast<uint8_t*>(buffer);
  for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(gNext + i);
}

void setUp() { gCalls = 0; gLength = 0; gNext = 0x10; }
void tearDown() {}

void test_random_refuses_before_rf_started() {
  Esp32SecureRandom random;
  uint8_t bytes[16] = {0};
  TEST_ASSERT_FALSE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(0, gCalls);
}

void test_random_refuses_null_and_zero_length_even_when_rf_started() {
  Esp32SecureRandom random;
  random.notifyRfStarted();
  uint8_t bytes[16] = {0};
  TEST_ASSERT_FALSE(random.fill(nullptr, sizeof bytes));
  TEST_ASSERT_FALSE(random.fill(bytes, 0));
  TEST_ASSERT_EQUAL_INT(0, gCalls);
}

void test_random_fills_exact_requested_buffer_after_rf_started() {
  Esp32SecureRandom random;
  random.notifyRfStarted();
  uint8_t bytes[16] = {0};
  TEST_ASSERT_TRUE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(1, gCalls);
  TEST_ASSERT_EQUAL_UINT32(sizeof bytes, gLength);
  for (size_t i = 0; i < sizeof bytes; ++i)
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(0x10 + i), bytes[i]);
}

void test_random_refuses_after_rf_stopped() {
  Esp32SecureRandom random;
  random.notifyRfStarted();
  random.notifyRfStopped();
  uint8_t bytes[16] = {0};
  TEST_ASSERT_FALSE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(0, gCalls);
}

void test_rf_notifications_are_idempotent() {
  Esp32SecureRandom random;
  random.notifyRfStarted();
  random.notifyRfStarted();
  uint8_t bytes[4] = {0};
  TEST_ASSERT_TRUE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(1, gCalls);
  random.notifyRfStopped();
  random.notifyRfStopped();
  TEST_ASSERT_FALSE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(1, gCalls);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_random_refuses_before_rf_started);
  RUN_TEST(test_random_refuses_null_and_zero_length_even_when_rf_started);
  RUN_TEST(test_random_fills_exact_requested_buffer_after_rf_started);
  RUN_TEST(test_random_refuses_after_rf_stopped);
  RUN_TEST(test_rf_notifications_are_idempotent);
  return UNITY_END();
}
