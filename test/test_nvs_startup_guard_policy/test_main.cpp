#include <unity.h>
#include <stddef.h>

#include "../../lib/hal_esp32/src/nvs_startup_guard_policy.h"

using namespace att;

void setUp() {}
void tearDown() {}

void test_blocks_exact_full_data_nvs_partition_erase() {
  TEST_ASSERT_TRUE(shouldBlockNvsPartitionErase(true, false, 0, 0x5000, 0x5000));
}

void test_allows_sector_erase_at_partition_start() {
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(true, false, 0, 0x1000, 0x5000));
}

void test_allows_sector_erase_after_partition_start() {
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(true, false, 0x1000, 0x1000, 0x5000));
}

void test_allows_partial_range_at_partition_start() {
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(true, false, 0, 0x4FFF, 0x5000));
}

void test_allows_full_erase_of_non_nvs_partition() {
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(false, false, 0, 0x5000, 0x5000));
}

void test_allows_erase_when_partition_size_is_unknown() {
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(true, false, 0, 0, 0));
}

void test_blocks_all_data_nvs_ranges_after_failed_first_init() {
  TEST_ASSERT_TRUE(shouldBlockNvsPartitionErase(true, true, 0, 0x1000, 0x5000));
  TEST_ASSERT_TRUE(shouldBlockNvsPartitionErase(true, true, 0x1000, 0x1000, 0x5000));
  TEST_ASSERT_TRUE(shouldBlockNvsPartitionErase(true, true, 0, 0x1000, 0));
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(false, true, 0, 0x1000, 0x5000));
  TEST_ASSERT_FALSE(shouldBlockNvsPartitionErase(true, true, 0, 0, 0));
}

void test_nvs_is_safe_only_after_exactly_one_successful_init() {
  TEST_ASSERT_FALSE(mayUseNvsAfterStartup(0, true, false));
  TEST_ASSERT_TRUE(mayUseNvsAfterStartup(1, true, false));
  TEST_ASSERT_FALSE(mayUseNvsAfterStartup(2, true, false));
}

void test_nvs_startup_fails_closed_on_init_error() {
  TEST_ASSERT_FALSE(mayUseNvsAfterStartup(1, false, false));
}

void test_nvs_startup_fails_closed_after_blocked_erase_attempt() {
  TEST_ASSERT_FALSE(mayUseNvsAfterStartup(1, true, true));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_blocks_exact_full_data_nvs_partition_erase);
  RUN_TEST(test_allows_sector_erase_at_partition_start);
  RUN_TEST(test_allows_sector_erase_after_partition_start);
  RUN_TEST(test_allows_partial_range_at_partition_start);
  RUN_TEST(test_allows_full_erase_of_non_nvs_partition);
  RUN_TEST(test_allows_erase_when_partition_size_is_unknown);
  RUN_TEST(test_blocks_all_data_nvs_ranges_after_failed_first_init);
  RUN_TEST(test_nvs_is_safe_only_after_exactly_one_successful_init);
  RUN_TEST(test_nvs_startup_fails_closed_on_init_error);
  RUN_TEST(test_nvs_startup_fails_closed_after_blocked_erase_attempt);
  return UNITY_END();
}
