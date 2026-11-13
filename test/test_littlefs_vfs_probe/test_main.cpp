#include <unity.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "littlefs_vfs_probe.h"

using namespace att;

void setUp() {}
void tearDown() {}

void test_vfs_probe_verifies_io_and_removes_only_its_directory() {
  char root[] = "/tmp/att_lfs_vfs_probe_XXXXXX";
  TEST_ASSERT_NOT_NULL(mkdtemp(root));

  LittleFsVfsProbeResult result = {};
  const bool passed = runLittleFsVfsProbe(root, 0x1234abcdU, result);
  TEST_ASSERT_TRUE(passed);
  TEST_ASSERT_TRUE(result.directoryCreated);
  TEST_ASSERT_TRUE(result.fileCreated);
  TEST_ASSERT_TRUE(result.writeComplete);
  TEST_ASSERT_TRUE(result.fileSyncComplete);
  TEST_ASSERT_TRUE(result.fileStatMatches);
  TEST_ASSERT_TRUE(result.readbackMatches);
  TEST_ASSERT_EQUAL(512, result.bytesWritten);
  TEST_ASSERT_EQUAL(512, result.bytesRead);
  TEST_ASSERT_TRUE(result.fileCloseComplete);
  TEST_ASSERT_TRUE(result.renameComplete);
  TEST_ASSERT_TRUE(result.directoryEnumerationComplete);
  TEST_ASSERT_EQUAL(1, result.directoryEntries);
  TEST_ASSERT_TRUE(result.reopenedFileOpened);
  TEST_ASSERT_TRUE(result.reopenedReadbackMatches);
  TEST_ASSERT_TRUE(result.reopenedCloseComplete);
  TEST_ASSERT_FALSE(result.directorySyncCloseFailed);
  TEST_ASSERT_TRUE(result.fileRemoved);
  TEST_ASSERT_TRUE(result.directoryRemoved);
  TEST_ASSERT_TRUE(result.cleanupComplete);

  DIR* directory = opendir(root);
  TEST_ASSERT_NOT_NULL(directory);
  errno = 0;
  bool unexpectedEntry = false;
  struct dirent* entry = nullptr;
  while ((entry = readdir(directory)) != nullptr) {
    if (strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0)
      unexpectedEntry = true;
  }
  TEST_ASSERT_EQUAL_INT(0, errno);
  TEST_ASSERT_FALSE(unexpectedEntry);
  TEST_ASSERT_EQUAL_INT(0, closedir(directory));
  TEST_ASSERT_EQUAL_INT(0, rmdir(root));
}

void test_existing_nonce_directory_is_never_modified() {
  char root[] = "/tmp/att_lfs_vfs_collision_XXXXXX";
  TEST_ASSERT_NOT_NULL(mkdtemp(root));

  char directory[128] = {};
  char sentinel[160] = {};
  snprintf(directory, sizeof directory, "%s/.att_c13_vfs_76543210", root);
  snprintf(sentinel, sizeof sentinel, "%s/sentinel.bin", directory);
  TEST_ASSERT_EQUAL_INT(0, mkdir(directory, 0700));
  const int createFd = open(sentinel, O_CREAT | O_EXCL | O_WRONLY, 0600);
  TEST_ASSERT_TRUE(createFd >= 0);
  const uint8_t expected[] = {0x41, 0x54, 0x54, 0x31};
  TEST_ASSERT_EQUAL_INT(sizeof expected,
                        write(createFd, expected, sizeof expected));
  TEST_ASSERT_EQUAL_INT(0, close(createFd));

  LittleFsVfsProbeResult result = {};
  TEST_ASSERT_FALSE(runLittleFsVfsProbe(root, 0x76543210U, result));
  TEST_ASSERT_FALSE(result.directoryCreated);
  TEST_ASSERT_EQUAL_INT(EEXIST, result.firstError);

  uint8_t actual[sizeof expected] = {};
  const int readFd = open(sentinel, O_RDONLY);
  TEST_ASSERT_TRUE(readFd >= 0);
  TEST_ASSERT_EQUAL_INT(sizeof actual, read(readFd, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, sizeof expected);
  TEST_ASSERT_EQUAL_INT(0, close(readFd));

  TEST_ASSERT_EQUAL_INT(0, unlink(sentinel));
  TEST_ASSERT_EQUAL_INT(0, rmdir(directory));
  TEST_ASSERT_EQUAL_INT(0, rmdir(root));
}

void test_invalid_mountpoints_are_rejected_before_io() {
  LittleFsVfsProbeResult result = {};
  TEST_ASSERT_FALSE(runLittleFsVfsProbe(nullptr, 1, result));
  TEST_ASSERT_EQUAL_INT(EINVAL, result.firstError);
  TEST_ASSERT_FALSE(runLittleFsVfsProbe("", 1, result));
  TEST_ASSERT_EQUAL_INT(EINVAL, result.firstError);
  TEST_ASSERT_FALSE(runLittleFsVfsProbe("/", 1, result));
  TEST_ASSERT_EQUAL_INT(EINVAL, result.firstError);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_vfs_probe_verifies_io_and_removes_only_its_directory);
  RUN_TEST(test_existing_nonce_directory_is_never_modified);
  RUN_TEST(test_invalid_mountpoints_are_rejected_before_io);
  return UNITY_END();
}
