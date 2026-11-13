#include <unity.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "littlefs_file_set.h"
#include "littlefs_file_set_test.h"
#include "month_set.h"
#include "log_store.h"

using namespace att;

namespace {
char gRoot[80];
char gLog[112];

enum AppendWriteMode {
  WRITE_ALL,
  WRITE_SHORT,
  WRITE_EINTR_ONCE,
  WRITE_PREFIX_THEN_ERROR
};

struct AppendFault {
  AppendFault()
      : writeMode(WRITE_ALL), writeCalls(0), syncCalls(0), preadCalls(0),
        closeCalls(0), statCalls(0), openCalls(0), fstatCalls(0),
        opendirCalls(0), readdirCalls(0), closedirCalls(0),
        failStatCall(0), shortWriteBytes(5), failSync(false), failPread(false),
        preadEintrOnce(false), preadEintrIssued(false),
        corruptPread(false), failClose(false), failOpen(false),
        failFstat(false), failOpendir(false), failReaddir(false),
        failClosedir(false), failReaddirAfterEntry(false),
        readdirSawEntry(false) {}

  AppendWriteMode writeMode;
  int writeCalls;
  int syncCalls;
  int preadCalls;
  int closeCalls;
  int statCalls;
  int openCalls;
  int fstatCalls;
  int opendirCalls;
  int readdirCalls;
  int closedirCalls;
  int failStatCall;
  size_t shortWriteBytes;
  bool failSync;
  bool failPread;
  bool preadEintrOnce;
  bool preadEintrIssued;
  bool corruptPread;
  bool failClose;
  bool failOpen;
  bool failFstat;
  bool failOpendir;
  bool failReaddir;
  bool failClosedir;
  bool failReaddirAfterEntry;
  bool readdirSawEntry;
};

ssize_t faultWrite(void* context, int fd, const void* data, size_t count) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.writeCalls;
  if (fault.writeMode == WRITE_EINTR_ONCE && fault.writeCalls == 1) {
    errno = EINTR;
    return -1;
  }
  if (fault.writeMode == WRITE_PREFIX_THEN_ERROR && fault.writeCalls > 1) {
    errno = EIO;
    return -1;
  }

  size_t limit = count;
  if (fault.writeMode == WRITE_SHORT ||
      fault.writeMode == WRITE_PREFIX_THEN_ERROR) {
    if (limit > fault.shortWriteBytes) limit = fault.shortWriteBytes;
  }
  return ::write(fd, data, limit);
}

int faultFsync(void* context, int fd) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.syncCalls;
  if (fault.failSync) {
    errno = EIO;
    return -1;
  }
  return ::fsync(fd);
}

ssize_t faultPread(void* context, int fd, void* data, size_t count,
                   off_t offset) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.preadCalls;
  if (fault.preadEintrOnce && !fault.preadEintrIssued) {
    fault.preadEintrIssued = true;
    errno = EINTR;
    return -1;
  }
  if (fault.failPread) {
    errno = EIO;
    return -1;
  }
  const ssize_t got = ::pread(fd, data, count, offset);
  if (got > 0 && fault.corruptPread)
    static_cast<uint8_t*>(data)[0] ^= 0x80u;
  return got;
}

int faultClose(void* context, int fd) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.closeCalls;
  const int rc = ::close(fd);
  if (rc == 0 && fault.failClose) {
    errno = EIO;
    return -1;
  }
  return rc;
}

int faultStat(void* context, const char* path, struct stat* st) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.statCalls;
  if (fault.failStatCall > 0 && fault.statCalls == fault.failStatCall) {
    errno = EIO;
    return -1;
  }
  return ::lstat(path, st);
}

int faultOpen(void* context, const char* path, int flags, mode_t mode) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.openCalls;
  if (fault.failOpen) {
    errno = EIO;
    return -1;
  }
  return ::open(path, flags, mode);
}

int faultFstat(void* context, int fd, struct stat* st) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.fstatCalls;
  if (fault.failFstat) {
    errno = EIO;
    return -1;
  }
  return ::fstat(fd, st);
}

DIR* faultOpendir(void* context, const char* path) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.opendirCalls;
  if (fault.failOpendir) {
    fault.failOpendir = false;
    errno = EIO;
    return nullptr;
  }
  return ::opendir(path);
}

struct dirent* faultReaddir(void* context, DIR* dir) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.readdirCalls;
  if (fault.failReaddir) {
    fault.failReaddir = false;
    errno = EIO;
    return nullptr;
  }
  if (fault.failReaddirAfterEntry && fault.readdirSawEntry) {
    fault.failReaddirAfterEntry = false;
    errno = EIO;
    return nullptr;
  }
  struct dirent* entry = ::readdir(dir);
  if (entry && strcmp(entry->d_name, ".") != 0 &&
      strcmp(entry->d_name, "..") != 0)
    fault.readdirSawEntry = true;
  return entry;
}

int faultClosedir(void* context, DIR* dir) {
  AppendFault& fault = *static_cast<AppendFault*>(context);
  ++fault.closedirCalls;
  const bool fail = fault.failClosedir;
  fault.failClosedir = false;
  const int rc = ::closedir(dir);
  if (rc == 0 && fail) {
    errno = EIO;
    return -1;
  }
  return rc;
}

LittleFsFileSetTestOps makeAppendOps(AppendFault& fault) {
  LittleFsFileSetTestOps ops = {
      &fault, faultWrite, faultFsync, faultPread, faultClose,
      faultStat, faultOpen, faultFstat, faultOpendir, faultReaddir,
      faultClosedir};
  return ops;
}

void assertMonthBytes(LittleFsFileSet& files, const uint8_t* expected,
                      size_t count) {
  size_t size = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(count, size);
  uint8_t actual[64] = {0};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, count));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, count);
}

bool makePath(char* out, size_t cap, const char* suffix) {
  int n = snprintf(out, cap, "%s%s", gRoot, suffix);
  return n > 0 && (size_t)n < cap;
}

bool rawFile(const char* suffix, const uint8_t* bytes, size_t n) {
  char path[160];
  if (!makePath(path, sizeof path, suffix)) return false;
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  size_t done = 0;
  while (done < n) {
    ssize_t w = write(fd, bytes + done, n - done);
    if (w <= 0) { close(fd); return false; }
    done += (size_t)w;
  }
  return close(fd) == 0;
}

bool rawAppend(const char* suffix, const uint8_t* bytes, size_t n) {
  char path[160];
  if (!makePath(path, sizeof path, suffix)) return false;
  int fd = open(path, O_WRONLY | O_APPEND);
  if (fd < 0) return false;
  size_t done = 0;
  while (done < n) {
    ssize_t w = write(fd, bytes + done, n - done);
    if (w < 0 && errno == EINTR) continue;
    if (w <= 0) { close(fd); return false; }
    done += (size_t)w;
  }
  const bool synced = fsync(fd) == 0;
  return close(fd) == 0 && synced;
}

void cleanupTree() {
  if (!gRoot[0]) return;
  DIR* dir = opendir(gLog);
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
      char path[160];
      if (snprintf(path, sizeof path, "%s/%s", gLog, entry->d_name) <= 0) continue;
      struct stat st;
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) rmdir(path);
      else unlink(path);
    }
    closedir(dir);
    rmdir(gLog);
  } else {
    unlink(gLog);
  }
  rmdir(gRoot);
  gRoot[0] = 0;
  gLog[0] = 0;
}
}

void setUp() {
  strcpy(gRoot, "/tmp/att-lfs-test-XXXXXX");
  TEST_ASSERT_NOT_NULL(mkdtemp(gRoot));
  TEST_ASSERT_TRUE(makePath(gLog, sizeof gLog, "/log"));
}

void tearDown() { cleanupTree(); }

void test_missing_log_directory_is_valid_empty() {
  LittleFsFileSet files(gRoot);
  size_t count = 99, size = 99;
  uint32_t key = 0;
  TEST_ASSERT_TRUE(files.valid());
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_FALSE(files.monthKeyChecked(0, key));
  TEST_ASSERT_FALSE(files.fileSizeChecked(0, size));
}

void test_month_keys_are_sorted_numerically_not_by_creation_order() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", &empty, 0));
  TEST_ASSERT_TRUE(rawFile("/log/202609.bin", &empty, 0));
  LittleFsFileSet files(gRoot);
  size_t count = 0;
  uint32_t key = 0;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(2, count);
  TEST_ASSERT_TRUE(files.monthKeyChecked(0, key));
  TEST_ASSERT_EQUAL_UINT32(202609, key);
  TEST_ASSERT_TRUE(files.monthKeyChecked(1, key));
  TEST_ASSERT_EQUAL_UINT32(202611, key);
}

void test_add_file_creates_canonical_empty_month() {
  LittleFsFileSet files(gRoot);
  size_t count = 0, size = 99;
  uint32_t key = 0;
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(1, count);
  TEST_ASSERT_TRUE(files.monthKeyChecked(0, key));
  TEST_ASSERT_EQUAL_UINT32(202611, key);
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(0, size);
}


void test_add_file_open_failure_is_reported_without_creating_entry() {
  AppendFault fault;
  fault.failOpen = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.addFile(202611));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
  size_t count = 99;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_add_file_fsync_failure_is_reported_and_preserves_created_entry() {
  AppendFault fault;
  fault.failSync = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.addFile(202611));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  size_t count = 0, size = 99;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(1, count);
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(0, size);
  TEST_ASSERT_FALSE(files.addFile(202611));
}

void test_add_file_close_failure_is_reported_and_preserves_created_entry() {
  AppendFault fault;
  fault.failClose = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.addFile(202611));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  size_t count = 0, size = 99;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(1, count);
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(0, size);
  TEST_ASSERT_FALSE(files.addFile(202611));
}

void test_add_file_rejects_invalid_month_keys() {
  LittleFsFileSet files(gRoot);
  TEST_ASSERT_FALSE(files.addFile(0));
  TEST_ASSERT_FALSE(files.addFile(202600));
  TEST_ASSERT_FALSE(files.addFile(202613));
  size_t count = 99;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_add_file_refuses_existing_and_nonincreasing_key() {
  LittleFsFileSet files(gRoot);
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_FALSE(files.addFile(202611));
  TEST_ASSERT_FALSE(files.addFile(202610));
  size_t count = 0;
  TEST_ASSERT_TRUE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(1, count);
}

void test_read_file_returns_exact_requested_range() {
  LittleFsFileSet files(gRoot);
  uint8_t input[32];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(i * 3 + 1);
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_TRUE(files.appendFile(0, input, sizeof input));
  uint8_t output[12] = {0};
  TEST_ASSERT_TRUE(files.readFile(0, 7, output, sizeof output));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(input + 7, output, sizeof output);
}


void test_read_file_open_failure_is_reported() {
  LittleFsFileSet setup(gRoot);
  uint8_t input[16] = {0}, output[16] = {0};
  TEST_ASSERT_TRUE(setup.addFile(202611));
  TEST_ASSERT_TRUE(setup.appendFile(0, input, sizeof input));

  AppendFault fault;
  fault.failOpen = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
}

void test_read_file_pread_failure_is_reported() {
  LittleFsFileSet setup(gRoot);
  uint8_t input[16] = {0}, output[16] = {0};
  TEST_ASSERT_TRUE(setup.addFile(202611));
  TEST_ASSERT_TRUE(setup.appendFile(0, input, sizeof input));

  AppendFault fault;
  fault.failPread = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_EQUAL_INT(1, fault.preadCalls);
}

void test_read_file_close_failure_is_reported() {
  LittleFsFileSet setup(gRoot);
  uint8_t input[16] = {0}, output[16] = {0};
  TEST_ASSERT_TRUE(setup.addFile(202611));
  TEST_ASSERT_TRUE(setup.appendFile(0, input, sizeof input));

  AppendFault fault;
  fault.failClose = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
}

void test_read_file_retries_interrupted_pread() {
  LittleFsFileSet setup(gRoot);
  uint8_t input[16], output[16] = {0};
  for (size_t i = 0; i < sizeof input; ++i)
    input[i] = (uint8_t)(0x40u + i);
  TEST_ASSERT_TRUE(setup.addFile(202611));
  TEST_ASSERT_TRUE(setup.appendFile(0, input, sizeof input));

  AppendFault fault;
  fault.preadEintrOnce = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_TRUE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_TRUE(fault.preadCalls >= 2);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(input, output, sizeof input);
}

void test_read_file_rejects_out_of_range_and_null_buffer() {
  LittleFsFileSet files(gRoot);
  uint8_t input[16] = {0}, output[16] = {0};
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_TRUE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_FALSE(files.readFile(0, 15, output, 2));
  TEST_ASSERT_FALSE(files.readFile(0, 0, nullptr, 1));
  TEST_ASSERT_FALSE(files.readFile(1, 0, output, 1));
}

void test_append_requires_complete_records_and_nonempty_input() {
  LittleFsFileSet files(gRoot);
  uint8_t input[16] = {0};
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_FALSE(files.appendFile(0, input, 15));
  TEST_ASSERT_FALSE(files.appendFile(0, input, 0));
  TEST_ASSERT_FALSE(files.appendFile(0, nullptr, sizeof input));
  size_t size = 99;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(0, size);
}

void test_append_appends_and_verifies_multiple_records() {
  LittleFsFileSet files(gRoot);
  uint8_t first[16], second[32], output[48];
  for (size_t i = 0; i < sizeof first; ++i) first[i] = (uint8_t)(0x10 + i);
  for (size_t i = 0; i < sizeof second; ++i) second[i] = (uint8_t)(0x80 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_TRUE(files.appendFile(0, first, sizeof first));
  TEST_ASSERT_TRUE(files.appendFile(0, second, sizeof second));
  size_t size = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(sizeof output, size);
  TEST_ASSERT_TRUE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(first, output, sizeof first);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(second, output + sizeof first, sizeof second);
}

void test_append_retries_short_writes() {
  AppendFault fault;
  fault.writeMode = WRITE_SHORT;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0x20 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_TRUE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_TRUE(fault.writeCalls > 1);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_TRUE(fault.preadCalls > 0);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  assertMonthBytes(files, input, sizeof input);
}

void test_append_retries_write_eintr() {
  AppendFault fault;
  fault.writeMode = WRITE_EINTR_ONCE;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0x40 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_TRUE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(2, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  assertMonthBytes(files, input, sizeof input);
}

void test_append_write_error_after_prefix_leaves_indeterminate_tail() {
  AppendFault fault;
  fault.writeMode = WRITE_PREFIX_THEN_ERROR;
  fault.shortWriteBytes = 7;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0x60 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_FALSE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(2, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  assertMonthBytes(files, input, 7);
}

void test_append_fsync_failure_is_false_even_when_bytes_are_visible() {
  AppendFault fault;
  fault.failSync = false;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0x80 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  fault.failSync = true;
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_FALSE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(1, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  assertMonthBytes(files, input, sizeof input);
}

void test_append_readback_io_failure_is_false() {
  AppendFault fault;
  fault.failPread = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0xa0 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_FALSE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(1, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  LittleFsFileSet verify(gRoot);
  assertMonthBytes(verify, input, sizeof input);
}

void test_append_readback_mismatch_is_false() {
  AppendFault fault;
  fault.corruptPread = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0xc0 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_FALSE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(1, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  LittleFsFileSet verify(gRoot);
  assertMonthBytes(verify, input, sizeof input);
}

void test_append_close_failure_is_false_after_full_write() {
  AppendFault fault;
  fault.failClose = false;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  uint8_t input[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(0xe0 + i);
  TEST_ASSERT_TRUE(files.addFile(202611));
  fault.failClose = true;
  // Ignore the month-creation syscalls; assertions below cover append only.
  fault.syncCalls = 0;
  fault.closeCalls = 0;

  TEST_ASSERT_FALSE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_EQUAL_INT(1, fault.writeCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.syncCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.preadCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);
  LittleFsFileSet verify(gRoot);
  assertMonthBytes(verify, input, sizeof input);
}

void test_append_refuses_existing_partial_record_file() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  uint8_t partial[15] = {0};
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", partial, sizeof partial));
  LittleFsFileSet files(gRoot);
  uint8_t record[16] = {0};
  TEST_ASSERT_FALSE(files.appendFile(0, record, sizeof record));
  size_t size = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, size));
  TEST_ASSERT_EQUAL_UINT32(sizeof partial, size);
}

void test_metadata_rejects_noncanonical_filename() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/notes.txt", &empty, 0));
  LittleFsFileSet files(gRoot);
  size_t count = 0;
  TEST_ASSERT_FALSE(files.countChecked(count));
}

void test_metadata_rejects_unexpected_subdirectory() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  char path[160];
  TEST_ASSERT_TRUE(makePath(path, sizeof path, "/log/unexpected"));
  TEST_ASSERT_EQUAL_INT(0, mkdir(path, 0700));
  LittleFsFileSet files(gRoot);
  size_t count = 0;
  TEST_ASSERT_FALSE(files.countChecked(count));
}

void test_log_path_regular_file_is_not_treated_as_empty() {
  const uint8_t marker = 1;
  TEST_ASSERT_TRUE(rawFile("/log", &marker, 1));
  LittleFsFileSet files(gRoot);
  size_t count = 0;
  TEST_ASSERT_FALSE(files.countChecked(count));
}

void test_missing_mount_root_fails_closed() {
  char missing[96];
  TEST_ASSERT_TRUE(makePath(missing, sizeof missing, "/not-mounted"));
  LittleFsFileSet files(missing);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
}

void test_destructive_operations_are_disabled_and_nonmutating() {
  LittleFsFileSet files(gRoot);
  uint8_t input[16], output[16];
  for (size_t i = 0; i < sizeof input; ++i) input[i] = (uint8_t)(i + 2);
  TEST_ASSERT_TRUE(files.addFile(202611));
  TEST_ASSERT_TRUE(files.appendFile(0, input, sizeof input));
  TEST_ASSERT_FALSE(files.truncateFile(0, 0));
  TEST_ASSERT_FALSE(files.quarantineCopy(0));
  TEST_ASSERT_FALSE(files.quarantineFrom(0));
  TEST_ASSERT_TRUE(files.readFile(0, 0, output, sizeof output));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(input, output, sizeof input);
}

void test_symlinked_mount_root_fails_closed() {
  char target[160], targetLog[192];
  char alias[] = "/tmp/lfsXXXXXX";
  TEST_ASSERT_TRUE(makePath(target, sizeof target, "/external"));
  TEST_ASSERT_EQUAL_INT(0, mkdir(target, 0700));
  TEST_ASSERT_TRUE(makePath(targetLog, sizeof targetLog, "/external/log"));
  TEST_ASSERT_EQUAL_INT(0, mkdir(targetLog, 0700));
  int tempFd = mkstemp(alias);
  TEST_ASSERT_TRUE(tempFd >= 0);
  TEST_ASSERT_EQUAL_INT(0, close(tempFd));
  TEST_ASSERT_EQUAL_INT(0, unlink(alias));
  TEST_ASSERT_EQUAL_INT(0, symlink(target, alias));

  LittleFsFileSet files(alias);
  const bool valid = files.valid();
  size_t count = 99;
  const bool accepted = files.countChecked(count);

  TEST_ASSERT_EQUAL_INT(0, unlink(alias));
  TEST_ASSERT_EQUAL_INT(0, rmdir(targetLog));
  TEST_ASSERT_EQUAL_INT(0, rmdir(target));
  TEST_ASSERT_TRUE(valid);
  TEST_ASSERT_FALSE(accepted);
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_invalid_mountpoint_is_rejected() {
  LittleFsFileSet empty(nullptr);
  LittleFsFileSet relative("tmp/not-absolute");
  LittleFsFileSet traversal("/tmp/../root");
  TEST_ASSERT_FALSE(empty.valid());
  TEST_ASSERT_FALSE(relative.valid());
  TEST_ASSERT_FALSE(traversal.valid());
}

void test_metadata_rejects_symlink_month_file() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  char path[160];
  TEST_ASSERT_TRUE(makePath(path, sizeof path, "/log/202611.bin"));
  TEST_ASSERT_EQUAL_INT(0, symlink("/etc/passwd", path));
  LittleFsFileSet files(gRoot);
  size_t count = 0;
  TEST_ASSERT_FALSE(files.countChecked(count));
}

void test_monthset_logstore_rollover_and_reopen_on_posix_filesystem() {
  LittleFsFileSet files(gRoot);
  MonthSet storage(files);
  LogStore log(storage);
  OpenInfo opened = log.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(0, opened.records);

  storage.setMonth(202611);
  Event first = {EV_SCAN, 0, 5, 1800000000u, 0, ST_PRESENT};
  TEST_ASSERT_TRUE(log.append(&first, 1));
  storage.setMonth(202612);
  Event second = {EV_MARK, 0, 7, 1800000060u, 0, ST_LATE};
  TEST_ASSERT_TRUE(log.append(&second, 1));

  size_t filesCount = 0, totalBytes = 0;
  TEST_ASSERT_TRUE(files.countChecked(filesCount));
  TEST_ASSERT_EQUAL_UINT32(2, filesCount);
  TEST_ASSERT_TRUE(storage.sizeChecked(totalBytes));
  TEST_ASSERT_EQUAL_UINT32(2 * REC_SIZE, totalBytes);
  uint32_t key = 0;
  TEST_ASSERT_TRUE(files.monthKeyChecked(0, key));
  TEST_ASSERT_EQUAL_UINT32(202611, key);
  TEST_ASSERT_TRUE(files.monthKeyChecked(1, key));
  TEST_ASSERT_EQUAL_UINT32(202612, key);

  LittleFsFileSet reopenedFiles(gRoot);
  MonthSet reopenedStorage(reopenedFiles);
  LogStore reopenedLog(reopenedStorage);
  opened = reopenedLog.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(2, opened.records);
  TEST_ASSERT_EQUAL_UINT32(2, opened.lastSeq);
  Event actual;
  TEST_ASSERT_TRUE(reopenedLog.at(0, actual));
  TEST_ASSERT_EQUAL_UINT16(5, actual.a);
  TEST_ASSERT_EQUAL_UINT32(1800000000u, actual.epoch);
  TEST_ASSERT_TRUE(reopenedLog.at(1, actual));
  TEST_ASSERT_EQUAL_UINT16(7, actual.a);
  TEST_ASSERT_EQUAL_UINT32(1800000060u, actual.epoch);
}

void test_torn_tail_fails_closed_without_mutating_file() {
  LittleFsFileSet files(gRoot);
  MonthSet storage(files);
  LogStore log(storage);
  TEST_ASSERT_TRUE(log.open().ok);
  storage.setMonth(202611);
  Event event = {EV_SCAN, 0, 9, 1800000000u, 0, ST_PRESENT};
  TEST_ASSERT_TRUE(log.append(&event, 1));

  const uint8_t torn[7] = {1, 2, 3, 4, 5, 6, 7};
  TEST_ASSERT_TRUE(rawAppend("/log/202611.bin", torn, sizeof torn));
  size_t before = 0, after = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, before));
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE + sizeof torn, before);

  LittleFsFileSet reopenedFiles(gRoot);
  MonthSet reopenedStorage(reopenedFiles);
  LogStore reopenedLog(reopenedStorage);
  TEST_ASSERT_FALSE(reopenedLog.open().ok);
  TEST_ASSERT_TRUE(reopenedLog.faulted());
  TEST_ASSERT_TRUE(reopenedFiles.fileSizeChecked(0, after));
  TEST_ASSERT_EQUAL_UINT32(before, after);
}

void test_record_validation_uses_sorted_snapshot_and_exact_file_count() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));

  const uint8_t scanFlags = makeFlags(0, ST_PRESENT, false, false, false);
  Event earlier = {EV_SCAN, scanFlags, 11, 1800000000u, 1, 0};
  Event later = {EV_SCAN, scanFlags, 12, 1800000060u, 2, 0};
  uint8_t earlierBytes[REC_SIZE], laterBytes[REC_SIZE];
  encode(earlier, earlierBytes);
  encode(later, laterBytes);

  // Create in reverse chronological order to ensure validation sorts keys.
  TEST_ASSERT_TRUE(rawFile("/log/202612.bin", laterBytes, sizeof laterBytes));
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", earlierBytes, sizeof earlierBytes));

  LittleFsFileSet files(gRoot);
  TEST_ASSERT_TRUE(files.validateRecordFiles(2, 480));
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_FALSE(files.validateRecordFiles(3, 480));
}

void test_record_validation_fails_closed_on_pread_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));

  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 41, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.preadEintrOnce = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t before = 0;
  size_t after = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, before));
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, before);
  TEST_ASSERT_TRUE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_TRUE(fault.preadEintrIssued);
  TEST_ASSERT_TRUE(fault.preadCalls >= 2);

  fault.failPread = true;
  fault.preadCalls = 0;
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_TRUE(fault.preadCalls > 0);
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, after));
  TEST_ASSERT_EQUAL_UINT32(before, after);
  uint8_t actual[REC_SIZE] = {};
  LittleFsFileSet verify(gRoot);
  TEST_ASSERT_TRUE(verify.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_scan_root_stat_error_is_not_treated_as_empty() {
  AppendFault fault;
  fault.failStatCall = 1;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_TRUE(fault.statCalls >= 1);
}

void test_scan_log_stat_error_is_not_treated_as_empty() {
  AppendFault fault;
  fault.failStatCall = 2;  // Root succeeds; /log metadata fails with EIO.
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_TRUE(fault.statCalls >= 2);
}

void test_scan_fails_closed_on_month_stat_error_without_publishing_count() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", &empty, 0));

  AppendFault fault;
  fault.failStatCall = 3;  // Root, /log, then the month-file metadata.
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_EQUAL_INT(3, fault.statCalls);
}

void test_scan_fails_closed_on_opendir_error_without_publishing_count() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", &empty, 0));

  AppendFault fault;
  fault.failOpendir = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_EQUAL_INT(1, fault.opendirCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.readdirCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.closedirCalls);
}

void test_scan_readdir_error_discards_partial_count() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", &empty, 0));

  AppendFault fault;
  fault.failReaddirAfterEntry = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_TRUE(fault.readdirSawEntry);
  TEST_ASSERT_TRUE(fault.readdirCalls >= 2);
  TEST_ASSERT_EQUAL_INT(1, fault.closedirCalls);
}

void test_scan_fails_closed_on_closedir_error_without_publishing_count() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t empty = 0;
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", &empty, 0));

  AppendFault fault;
  fault.failClosedir = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  size_t count = 99;
  TEST_ASSERT_FALSE(files.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_TRUE(fault.readdirCalls > 0);
  TEST_ASSERT_EQUAL_INT(1, fault.closedirCalls);
}

void test_record_validation_fails_closed_on_stat_error_not_empty() {
  AppendFault fault;
  fault.failStatCall = 2;  // Root succeeds; missing /log lookup returns EIO.
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(0, 480));
  TEST_ASSERT_EQUAL_INT(2, fault.statCalls);
}

void test_record_validation_fails_closed_on_month_stat_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 42, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failStatCall = 3;  // Root, /log, then month-file metadata.
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(3, fault.statCalls);

  uint8_t actual[REC_SIZE] = {};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_open_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 43, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failOpen = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.fstatCalls);

  uint8_t actual[REC_SIZE] = {};
  LittleFsFileSet verify(gRoot);
  TEST_ASSERT_TRUE(verify.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_fstat_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 44, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failFstat = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(1, fault.openCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.fstatCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);

  uint8_t actual[REC_SIZE] = {};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_opendir_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 45, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failOpendir = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(1, fault.opendirCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.readdirCalls);
  TEST_ASSERT_EQUAL_INT(0, fault.closedirCalls);

  uint8_t actual[REC_SIZE] = {};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_readdir_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 46, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failReaddir = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(1, fault.opendirCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.readdirCalls);
  TEST_ASSERT_EQUAL_INT(1, fault.closedirCalls);

  uint8_t actual[REC_SIZE] = {};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_closedir_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));
  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 47, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failClosedir = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_EQUAL_INT(1, fault.opendirCalls);
  TEST_ASSERT_TRUE(fault.readdirCalls > 0);
  TEST_ASSERT_EQUAL_INT(1, fault.closedirCalls);

  uint8_t actual[REC_SIZE] = {};
  TEST_ASSERT_TRUE(files.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

void test_record_validation_fails_closed_on_close_error_without_mutation() {
  TEST_ASSERT_EQUAL_INT(0, mkdir(gLog, 0700));

  const uint8_t flags = makeFlags(0, ST_PRESENT, false, false, false);
  Event event = {EV_SCAN, flags, 41, 1800000000u, 1, 0};
  uint8_t bytes[REC_SIZE];
  encode(event, bytes);
  TEST_ASSERT_TRUE(rawFile("/log/202611.bin", bytes, sizeof bytes));

  AppendFault fault;
  fault.failClose = true;
  LittleFsFileSetTestOps ops = makeAppendOps(fault);
  LittleFsFileSet files(gRoot, &ops);
  TEST_ASSERT_FALSE(files.validateRecordFiles(1, 480));
  TEST_ASSERT_TRUE(fault.preadCalls > 0);
  TEST_ASSERT_EQUAL_INT(1, fault.closeCalls);

  size_t after = 0;
  TEST_ASSERT_TRUE(files.fileSizeChecked(0, after));
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, after);
  uint8_t actual[REC_SIZE] = {};
  LittleFsFileSet verify(gRoot);
  TEST_ASSERT_TRUE(verify.readFile(0, 0, actual, sizeof actual));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, actual, sizeof bytes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_record_validation_uses_sorted_snapshot_and_exact_file_count);
  RUN_TEST(test_record_validation_fails_closed_on_pread_error_without_mutation);
  RUN_TEST(test_scan_root_stat_error_is_not_treated_as_empty);
  RUN_TEST(test_scan_log_stat_error_is_not_treated_as_empty);
  RUN_TEST(test_scan_fails_closed_on_month_stat_error_without_publishing_count);
  RUN_TEST(test_scan_fails_closed_on_opendir_error_without_publishing_count);
  RUN_TEST(test_scan_readdir_error_discards_partial_count);
  RUN_TEST(test_scan_fails_closed_on_closedir_error_without_publishing_count);
  RUN_TEST(test_record_validation_fails_closed_on_stat_error_not_empty);
  RUN_TEST(test_record_validation_fails_closed_on_month_stat_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_open_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_fstat_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_opendir_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_readdir_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_closedir_error_without_mutation);
  RUN_TEST(test_record_validation_fails_closed_on_close_error_without_mutation);
  RUN_TEST(test_missing_log_directory_is_valid_empty);
  RUN_TEST(test_month_keys_are_sorted_numerically_not_by_creation_order);
  RUN_TEST(test_add_file_creates_canonical_empty_month);
  RUN_TEST(test_add_file_open_failure_is_reported_without_creating_entry);
  RUN_TEST(test_add_file_fsync_failure_is_reported_and_preserves_created_entry);
  RUN_TEST(test_add_file_close_failure_is_reported_and_preserves_created_entry);
  RUN_TEST(test_add_file_rejects_invalid_month_keys);
  RUN_TEST(test_add_file_refuses_existing_and_nonincreasing_key);
  RUN_TEST(test_read_file_returns_exact_requested_range);
  RUN_TEST(test_read_file_open_failure_is_reported);
  RUN_TEST(test_read_file_pread_failure_is_reported);
  RUN_TEST(test_read_file_close_failure_is_reported);
  RUN_TEST(test_read_file_retries_interrupted_pread);
  RUN_TEST(test_read_file_rejects_out_of_range_and_null_buffer);
  RUN_TEST(test_append_requires_complete_records_and_nonempty_input);
  RUN_TEST(test_append_appends_and_verifies_multiple_records);
  RUN_TEST(test_append_retries_short_writes);
  RUN_TEST(test_append_retries_write_eintr);
  RUN_TEST(test_append_write_error_after_prefix_leaves_indeterminate_tail);
  RUN_TEST(test_append_fsync_failure_is_false_even_when_bytes_are_visible);
  RUN_TEST(test_append_readback_io_failure_is_false);
  RUN_TEST(test_append_readback_mismatch_is_false);
  RUN_TEST(test_append_close_failure_is_false_after_full_write);
  RUN_TEST(test_append_refuses_existing_partial_record_file);
  RUN_TEST(test_metadata_rejects_noncanonical_filename);
  RUN_TEST(test_metadata_rejects_unexpected_subdirectory);
  RUN_TEST(test_log_path_regular_file_is_not_treated_as_empty);
  RUN_TEST(test_missing_mount_root_fails_closed);
  RUN_TEST(test_destructive_operations_are_disabled_and_nonmutating);
  RUN_TEST(test_symlinked_mount_root_fails_closed);
  RUN_TEST(test_invalid_mountpoint_is_rejected);
  RUN_TEST(test_metadata_rejects_symlink_month_file);
  RUN_TEST(test_monthset_logstore_rollover_and_reopen_on_posix_filesystem);
  RUN_TEST(test_torn_tail_fails_closed_without_mutating_file);
  return UNITY_END();
}
