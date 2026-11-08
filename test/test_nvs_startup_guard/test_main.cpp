#include <unity.h>
#include <stddef.h>
#include <stdint.h>

#include "../../lib/hal_esp32/src/nvs_startup_guard_policy.h"

using att::NvsStartupGuardPolicy;

namespace {
const int32_t kEspOk = 0;
NvsStartupGuardPolicy newPolicy() {
  NvsStartupGuardPolicy policy = {false, -1, false};
  return policy;
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_startup_is_denied_until_first_nvs_init_result_is_observed() {
  NvsStartupGuardPolicy policy = newPolicy();
  TEST_ASSERT_FALSE(policy.mayStartApplication(kEspOk));
  TEST_ASSERT_FALSE(policy.initObserved);
}

void test_successful_first_nvs_init_allows_application_start() {
  NvsStartupGuardPolicy policy = newPolicy();
  policy.observeInitResult(kEspOk);
  TEST_ASSERT_TRUE(policy.initObserved);
  TEST_ASSERT_EQUAL_INT32(kEspOk, policy.firstInitResult);
  TEST_ASSERT_TRUE(policy.mayStartApplication(kEspOk));
}

void test_failed_first_nvs_init_denies_application_start() {
  NvsStartupGuardPolicy policy = newPolicy();
  policy.observeInitResult(-1);
  TEST_ASSERT_FALSE(policy.mayStartApplication(kEspOk));
}

void test_first_nvs_result_is_latched_and_later_success_cannot_hide_failure() {
  NvsStartupGuardPolicy policy = newPolicy();
  policy.observeInitResult(-1);
  policy.observeInitResult(kEspOk);
  TEST_ASSERT_EQUAL_INT32(-1, policy.firstInitResult);
  TEST_ASSERT_FALSE(policy.mayStartApplication(kEspOk));
}

void test_first_success_result_is_not_replaced_by_later_result() {
  NvsStartupGuardPolicy policy = newPolicy();
  policy.observeInitResult(kEspOk);
  policy.observeInitResult(-1);
  TEST_ASSERT_EQUAL_INT32(kEspOk, policy.firstInitResult);
  TEST_ASSERT_TRUE(policy.mayStartApplication(kEspOk));
}

void test_whole_nvs_partition_erase_is_blocked_and_latched() {
  NvsStartupGuardPolicy policy = newPolicy();
  TEST_ASSERT_TRUE(policy.blockWholeNvsPartitionErase(true, 0, 0x5000, 0x5000));
  TEST_ASSERT_TRUE(policy.fullNvsEraseBlocked);
  TEST_ASSERT_FALSE(policy.mayStartApplication(kEspOk));
}

void test_normal_nvs_sector_erases_are_allowed() {
  NvsStartupGuardPolicy policy = newPolicy();
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 0x1000, 0x1000, 0x5000));
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 0, 0x1000, 0x5000));
  TEST_ASSERT_FALSE(policy.fullNvsEraseBlocked);
}

void test_whole_non_nvs_partition_erase_is_allowed() {
  NvsStartupGuardPolicy policy = newPolicy();
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(false, 0, 0x5000, 0x5000));
  TEST_ASSERT_FALSE(policy.fullNvsEraseBlocked);
}

void test_incomplete_or_malformed_ranges_are_not_misclassified_as_full_erase() {
  NvsStartupGuardPolicy policy = newPolicy();
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 0, 0, 0));
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 0, 0, 0x5000));
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 1, 0x5000, 0x5000));
  TEST_ASSERT_FALSE(policy.blockWholeNvsPartitionErase(true, 0, 0x4FFF, 0x5000));
  TEST_ASSERT_FALSE(policy.fullNvsEraseBlocked);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_startup_is_denied_until_first_nvs_init_result_is_observed);
  RUN_TEST(test_successful_first_nvs_init_allows_application_start);
  RUN_TEST(test_failed_first_nvs_init_denies_application_start);
  RUN_TEST(test_first_nvs_result_is_latched_and_later_success_cannot_hide_failure);
  RUN_TEST(test_first_success_result_is_not_replaced_by_later_result);
  RUN_TEST(test_whole_nvs_partition_erase_is_blocked_and_latched);
  RUN_TEST(test_normal_nvs_sector_erases_are_allowed);
  RUN_TEST(test_whole_non_nvs_partition_erase_is_allowed);
  RUN_TEST(test_incomplete_or_malformed_ranges_are_not_misclassified_as_full_erase);
  return UNITY_END();
}
