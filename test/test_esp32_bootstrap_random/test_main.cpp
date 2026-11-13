#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp32_bootstrap_random.h"

using namespace att;

namespace {
int gSequence[8];
int gSequenceLength;
int gFillCalls;
size_t gFilledLength;
uint8_t gFillValue;
}

extern "C" void bootloader_random_enable(void) { gSequence[gSequenceLength++] = 1; }
extern "C" void bootloader_random_disable(void) { gSequence[gSequenceLength++] = 3; }
extern "C" void esp_fill_random(void* out, size_t length) {
  ++gFillCalls; gFilledLength = length; gSequence[gSequenceLength++] = 2;
  uint8_t* bytes = static_cast<uint8_t*>(out);
  for (size_t i = 0; i < length; ++i) bytes[i] = static_cast<uint8_t>(gFillValue + i);
}

void setUp() {
  memset(gSequence, 0, sizeof gSequence); gSequenceLength = 0;
  gFillCalls = 0; gFilledLength = 0; gFillValue = 0xA0;
}
void tearDown() {}

void test_bootstrap_rng_enables_entropy_fills_then_disables() {
  Esp32BootstrapRandom random; uint8_t bytes[16] = {0};
  TEST_ASSERT_TRUE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_INT(3, gSequenceLength);
  TEST_ASSERT_EQUAL_INT(1, gSequence[0]);
  TEST_ASSERT_EQUAL_INT(2, gSequence[1]);
  TEST_ASSERT_EQUAL_INT(3, gSequence[2]);
  TEST_ASSERT_EQUAL_INT(1, gFillCalls);
  TEST_ASSERT_EQUAL_UINT32(sizeof bytes, gFilledLength);
  for (size_t i = 0; i < sizeof bytes; ++i)
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(0xA0 + i), bytes[i]);
}

void test_bootstrap_rng_rejects_null_or_zero_without_enabling_entropy() {
  Esp32BootstrapRandom random; uint8_t bytes[16] = {0};
  TEST_ASSERT_FALSE(random.fill(nullptr, sizeof bytes));
  TEST_ASSERT_FALSE(random.fill(bytes, 0));
  TEST_ASSERT_EQUAL_INT(0, gSequenceLength);
  TEST_ASSERT_EQUAL_INT(0, gFillCalls);
}

void test_bootstrap_rng_allows_small_one_shot_request() {
  Esp32BootstrapRandom random; uint8_t bytes[1] = {0};
  TEST_ASSERT_TRUE(random.fill(bytes, sizeof bytes));
  TEST_ASSERT_EQUAL_UINT8(0xA0, bytes[0]);
  TEST_ASSERT_EQUAL_INT(3, gSequenceLength);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bootstrap_rng_enables_entropy_fills_then_disables);
  RUN_TEST(test_bootstrap_rng_rejects_null_or_zero_without_enabling_entropy);
  RUN_TEST(test_bootstrap_rng_allows_small_one_shot_request);
  return UNITY_END();
}
