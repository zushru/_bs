#include <unity.h>
#include <stdint.h>

#include "esp32_clock_monotonic.h"

using namespace att;

namespace {

int64_t gTimerValue;

}  // namespace

extern "C" int64_t esp_timer_get_time(void) {
  return gTimerValue;
}

void setUp() { gTimerValue = 0; }
void tearDown() {}

void test_zero_is_a_valid_elapsed_timer_value() {
  Esp32ClockMonotonicSource source;
  uint64_t elapsedUs = UINT64_MAX;

  TEST_ASSERT_TRUE(source.nowMicros(elapsedUs));
  TEST_ASSERT_EQUAL_UINT64(0, elapsedUs);
}

void test_positive_microseconds_are_forwarded_without_unit_conversion() {
  Esp32ClockMonotonicSource source;
  gTimerValue = 123456789;
  uint64_t elapsedUs = 0;

  TEST_ASSERT_TRUE(source.nowMicros(elapsedUs));
  TEST_ASSERT_EQUAL_UINT64(123456789ULL, elapsedUs);
}

void test_largest_nonnegative_timer_value_is_representable() {
  Esp32ClockMonotonicSource source;
  gTimerValue = INT64_MAX;
  uint64_t elapsedUs = 0;

  TEST_ASSERT_TRUE(source.nowMicros(elapsedUs));
  TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(INT64_MAX), elapsedUs);
}

void test_negative_timer_value_fails_without_changing_output() {
  Esp32ClockMonotonicSource source;
  gTimerValue = -1;
  uint64_t elapsedUs = 0x123456789ABCDEF0ULL;

  TEST_ASSERT_FALSE(source.nowMicros(elapsedUs));
  TEST_ASSERT_EQUAL_UINT64(0x123456789ABCDEF0ULL, elapsedUs);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_zero_is_a_valid_elapsed_timer_value);
  RUN_TEST(test_positive_microseconds_are_forwarded_without_unit_conversion);
  RUN_TEST(test_largest_nonnegative_timer_value_is_representable);
  RUN_TEST(test_negative_timer_value_fails_without_changing_output);
  return UNITY_END();
}
