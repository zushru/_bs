#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "littlefs_serialized_fileset.h"

using namespace att;

namespace {
struct TestLock : ILittleFsStorageLock {
  TestLock() : fail(false), held(false), acquisitions(0), releases(0) {}
  bool fail;
  bool held;
  int acquisitions;
  int releases;

  bool acquire() override {
    ++acquisitions;
    if (fail || held) return false;
    held = true;
    return true;
  }
  void release() override {
    ++releases;
    held = false;
  }
};

struct TestFileSet : IFileSet {
  explicit TestFileSet(TestLock& lock)
      : lock_(lock), calls(0), failRead(false) {}

  TestLock& lock_;
  int calls;
  bool failRead;

  bool guarded() {
    ++calls;
    return lock_.held;
  }
  bool countChecked(size_t& out) override {
    if (!guarded()) return false;
    out = 3;
    return true;
  }
  bool monthKeyChecked(size_t index, uint32_t& out) override {
    if (!guarded() || index != 1) return false;
    out = 202611;
    return true;
  }
  bool fileSizeChecked(size_t index, size_t& out) override {
    if (!guarded() || index != 1) return false;
    out = 32;
    return true;
  }
  bool readFile(size_t index, size_t offset, uint8_t* out,
                size_t count) override {
    if (!guarded() || failRead || index != 1 || offset != 4 ||
        !out || count != 3) return false;
    memset(out, 0x5a, count);
    return true;
  }
  bool appendFile(size_t index, const uint8_t* data, size_t count) override {
    return guarded() && index == 1 && data && count == 16;
  }
  bool truncateFile(size_t index, size_t length) override {
    (void)length;
    return guarded() && index == 1 && false;
  }
  bool addFile(uint32_t monthKey) override {
    return guarded() && monthKey == 202612;
  }
  bool quarantineCopy(size_t index) override {
    return guarded() && index == 1 && false;
  }
  bool quarantineFrom(size_t index) override {
    return guarded() && index == 1 && false;
  }
};
}

void setUp() {}
void tearDown() {}

void test_all_file_set_operations_are_delegated_inside_lock() {
  TestLock lock;
  TestFileSet backend(lock);
  SerializedFileSet files(backend, lock);

  size_t count = 0, size = 0;
  uint32_t month = 0;
  uint8_t output[3] = {0};
  const uint8_t input[16] = {0};

  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(3, count);
  TEST_ASSERT_TRUE(files.monthKeyChecked(1, month));
  TEST_ASSERT_EQUAL_UINT32(202611, month);
  TEST_ASSERT_TRUE(files.fileSizeChecked(1, size));
  TEST_ASSERT_EQUAL_UINT32(32, size);
  TEST_ASSERT_TRUE(files.readFile(1, 4, output, sizeof output));
  TEST_ASSERT_EQUAL_UINT8(0x5a, output[0]);
  TEST_ASSERT_TRUE(files.appendFile(1, input, sizeof input));
  TEST_ASSERT_TRUE(files.addFile(202612));

  TEST_ASSERT_FALSE(files.truncateFile(1, 0));
  TEST_ASSERT_FALSE(files.quarantineCopy(1));
  TEST_ASSERT_FALSE(files.quarantineFrom(1));

  TEST_ASSERT_EQUAL_INT(9, backend.calls);
  TEST_ASSERT_EQUAL_INT(9, lock.acquisitions);
  TEST_ASSERT_EQUAL_INT(9, lock.releases);
  TEST_ASSERT_FALSE(lock.held);
}

void test_lock_failure_skips_backend_and_clears_metadata_outputs() {
  TestLock lock;
  lock.fail = true;
  TestFileSet backend(lock);
  SerializedFileSet files(backend, lock);

  size_t count = 99;
  uint32_t month = 99;
  size_t size = 99;

  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_FALSE(files.monthKeyChecked(0, month));
  TEST_ASSERT_EQUAL_UINT32(0, month);
  TEST_ASSERT_FALSE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(0, size);
  TEST_ASSERT_EQUAL_INT(0, backend.calls);
  TEST_ASSERT_EQUAL_INT(0, lock.releases);
}

void test_delegate_failure_releases_lock() {
  TestLock lock;
  TestFileSet backend(lock);
  backend.failRead = true;
  SerializedFileSet files(backend, lock);
  uint8_t output[3] = {1, 2, 3};

  TEST_ASSERT_FALSE(files.readFile(1, 4, output, sizeof output));
  TEST_ASSERT_EQUAL_INT(1, lock.acquisitions);
  TEST_ASSERT_EQUAL_INT(1, lock.releases);
  TEST_ASSERT_FALSE(lock.held);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all_file_set_operations_are_delegated_inside_lock);
  RUN_TEST(test_lock_failure_skips_backend_and_clears_metadata_outputs);
  RUN_TEST(test_delegate_failure_releases_lock);
  return UNITY_END();
}
