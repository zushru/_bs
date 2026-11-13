#include <unity.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <type_traits>

#include "event_codec.h"
#include "log_store.h"
#include "month_set.h"
#include "esp32_littlefs_mount.h"
#include "littlefs_mount_owner.h"
#include "roster_store.h"
#include "time_rules.h"

using namespace att;

static_assert(!std::is_copy_constructible<Esp32LittleFsMountBackend>::value,
              "mount ownership must not be copied");
static_assert(!std::is_copy_assignable<Esp32LittleFsMountBackend>::value,
              "mount ownership must not be copy-assigned");
static_assert(!std::is_move_constructible<Esp32LittleFsMountBackend>::value,
              "mount ownership must not be moved");
static_assert(!std::is_move_assignable<Esp32LittleFsMountBackend>::value,
              "mount ownership must not be move-assigned");

namespace {
char gRoot[96];

bool copyArg(char* out, size_t capacity, const char* value) {
  if (!out || !value || capacity == 0) return false;
  const size_t length = strlen(value);
  if (length == 0 || length >= capacity) return false;
  memcpy(out, value, length + 1);
  return true;
}

struct FakeMountBackend : ILittleFsMountBackend {
  FakeMountBackend()
      : allowMount(true), allowUnmount(true), createRoot(true),
        malformedLog(false), calls(0), unmountCalls(0),
        ownerToReenter(nullptr), reentryResult(false),
        receivedMountpoint{0}, receivedLabel{0} {}

  bool allowMount;
  bool allowUnmount;
  bool createRoot;
  bool malformedLog;
  int calls;
  int unmountCalls;
  LittleFsStorageOwner* ownerToReenter;
  bool reentryResult;
  char receivedMountpoint[32];
  char receivedLabel[17];

  bool mountWithoutFormatting(const char* mountpoint,
                              const char* partitionLabel) override {
    ++calls;
    if (!copyArg(receivedMountpoint, sizeof receivedMountpoint, mountpoint) ||
        !copyArg(receivedLabel, sizeof receivedLabel, partitionLabel))
      return false;
    if (ownerToReenter) reentryResult = ownerToReenter->begin();
    if (!allowMount) return false;
    if (!createRoot) return true;
    if (mkdir(mountpoint, 0700) != 0 && errno != EEXIST) return false;
    if (!malformedLog) return true;

    char logPath[128];
    const int n = snprintf(logPath, sizeof logPath, "%s/log", mountpoint);
    if (n <= 0 || (size_t)n >= sizeof logPath) return false;
    const int fd = open(logPath, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return false;
    const uint8_t marker = 1;
    const bool wrote = write(fd, &marker, sizeof marker) == sizeof marker;
    return close(fd) == 0 && wrote;
  }

  bool unmountOwned() override {
    ++unmountCalls;
    return allowUnmount;
  }
};

bool seedEventFile(const char* name, const Event* events, size_t count) {
  char logPath[128], filePath[160];
  int n = snprintf(logPath, sizeof logPath, "%s/log", gRoot);
  if (n <= 0 || (size_t)n >= sizeof logPath) return false;
  if (mkdir(logPath, 0700) != 0 && errno != EEXIST) return false;
  n = snprintf(filePath, sizeof filePath, "%s/%s", logPath, name);
  if (n <= 0 || (size_t)n >= sizeof filePath) return false;
  const int fd = open(filePath, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  bool ok = true;
  for (size_t i = 0; ok && i < count; ++i) {
    uint8_t record[REC_SIZE];
    encode(events[i], record);
    size_t done = 0;
    while (done < sizeof record) {
      const ssize_t put = write(fd, record + done, sizeof record - done);
      if (put <= 0) { ok = false; break; }
      done += (size_t)put;
    }
  }
  return close(fd) == 0 && ok;
}

bool seedRosterSetFile(const char* name, uint16_t id, int32_t day,
                       uint32_t firstSeq, const Event* trailing = nullptr,
                       bool invalidUtf8 = false, bool dirtyPadding = false) {
  char displayName[ROSTER_NAME_MAX] = {0};
  const char* source = "Roster Test";
  memcpy(displayName, source, strlen(source));
  if (invalidUtf8) displayName[0] = (char)0xC0;
  if (dirtyPadding) displayName[20] = 'X';
  Event events[6];
  events[0] = Event{EV_ROSTER, FL_MORE, id, dayStartEpoch(day, 480),
                    firstSeq, RA_SET};
  for (size_t word = 0; word < 4; ++word) {
    uint64_t bits = 0;
    for (size_t byte = 0; byte < 8; ++byte)
      bits |= (uint64_t)(uint8_t)displayName[word * 8 + byte] << (8u * byte);
    events[word + 1] = makePayload(bits, firstSeq + (uint32_t)word + 1u,
                                   word < 3);
  }
  size_t count = 5;
  if (trailing) {
    events[5] = *trailing;
    count = 6;
  }
  return seedEventFile(name, events, count);
}

bool seedMonthFile(const char* name, const uint8_t* data, size_t count) {
  char logPath[128], filePath[160];
  int n = snprintf(logPath, sizeof logPath, "%s/log", gRoot);
  if (n <= 0 || (size_t)n >= sizeof logPath) return false;
  if (mkdir(logPath, 0700) != 0 && errno != EEXIST) return false;
  n = snprintf(filePath, sizeof filePath, "%s/%s", logPath, name);
  if (n <= 0 || (size_t)n >= sizeof filePath) return false;
  const int fd = open(filePath, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  size_t done = 0;
  while (done < count) {
    const ssize_t put = write(fd, data + done, count - done);
    if (put <= 0) { close(fd); return false; }
    done += (size_t)put;
  }
  return close(fd) == 0;
}

void removeRoot() {
  if (!gRoot[0]) return;
  char logPath[128];
  const int n = snprintf(logPath, sizeof logPath, "%s/log", gRoot);
  if (n > 0 && (size_t)n < sizeof logPath) {
    unlink(logPath);
    rmdir(logPath);
  }
  rmdir(gRoot);
  gRoot[0] = '\0';
}
}  // namespace

void setUp() {
  strcpy(gRoot, "/tmp/att-lfs-own-XXXXXX");
  TEST_ASSERT_NOT_NULL(mkdtemp(gRoot));
}

void tearDown() { removeRoot(); }

void test_file_set_is_hidden_until_mount_and_root_validation_succeed() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
  TEST_ASSERT_EQUAL_STRING(gRoot, backend.receivedMountpoint);
  TEST_ASSERT_EQUAL_STRING("spiffs", backend.receivedLabel);

  IFileSet* files = owner.fileSet();
  TEST_ASSERT_NOT_NULL(files);
  size_t count = 99;
  TEST_ASSERT_TRUE(files->countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_partial_record_file_fails_closed_without_modifying_bytes() {
  uint8_t partial[15];
  for (size_t i = 0; i < sizeof partial; ++i)
    partial[i] = (uint8_t)(0x40u + i);
  TEST_ASSERT_TRUE(seedMonthFile("202611.bin", partial, sizeof partial));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);

  char path[160];
  TEST_ASSERT_TRUE(snprintf(path, sizeof path, "%s/log/202611.bin", gRoot) > 0);
  const int fd = open(path, O_RDONLY);
  TEST_ASSERT_TRUE(fd >= 0);
  uint8_t actual[sizeof partial] = {0};
  TEST_ASSERT_EQUAL_INT((int)sizeof actual, read(fd, actual, sizeof actual));
  TEST_ASSERT_EQUAL_INT(0, close(fd));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(partial, actual, sizeof partial);
}

void test_valid_records_keep_global_sequence_across_month_files() {
  const uint8_t scanFlags = makeFlags(0, ST_PRESENT, false, false, false);
  Event first = {EV_SCAN, scanFlags, 1, 100, 1, 0};
  Event second = {EV_SCAN, scanFlags, 2, 200, 2, 0};
  Event third = {EV_SCAN, scanFlags, 3, 300, 3, 0};
  TEST_ASSERT_TRUE(seedMonthFile("202611.bin", nullptr, 0));
  TEST_ASSERT_TRUE(seedEventFile("202612.bin", &first, 1));
  TEST_ASSERT_TRUE(seedEventFile("202701.bin", &second, 1));
  TEST_ASSERT_TRUE(seedEventFile("202702.bin", &third, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());
  TEST_ASSERT_NOT_NULL(owner.fileSet());
}

void test_crc_valid_but_semantically_invalid_scan_fails_closed() {
  Event event = {EV_SCAN, makeFlags(0, ST_CLEAR, false, false, false),
                 0, 700, 1, 0};
  TEST_ASSERT_TRUE(seedEventFile("202611.bin", &event, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);
}

void test_invalid_roster_lifecycle_across_month_files_fails_closed() {
  const int32_t enrollDay = 2500;
  const int32_t deactivateDay = 2510;
  const int32_t duplicateDay = 2540;
  Event deactivate = {EV_ROSTER, 0, 7, dayStartEpoch(deactivateDay, 480),
                      6, RA_DEACTIVATE};
  TEST_ASSERT_TRUE(seedRosterSetFile("202611.bin", 7, enrollDay, 1,
                                     &deactivate));
  Event duplicate = {EV_ROSTER, 0, 7, dayStartEpoch(duplicateDay, 480),
                     7, RA_DEACTIVATE};
  TEST_ASSERT_TRUE(seedEventFile("202612.bin", &duplicate, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);
}

void test_valid_roster_lifecycle_across_month_files_is_accepted() {
  Event deactivate = {EV_ROSTER, 0, 7, dayStartEpoch(2510, 480),
                      6, RA_DEACTIVATE};
  TEST_ASSERT_TRUE(seedRosterSetFile("202611.bin", 7, 2500, 1,
                                     &deactivate));
  Event activate = {EV_ROSTER, 0, 7, dayStartEpoch(2540, 480),
                    7, RA_ACTIVATE};
  TEST_ASSERT_TRUE(seedEventFile("202612.bin", &activate, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());
  TEST_ASSERT_NOT_NULL(owner.fileSet());
}

void test_invalid_utf8_roster_name_fails_closed() {
  TEST_ASSERT_TRUE(seedRosterSetFile("202611.bin", 7, 2500, 1,
                                     nullptr, true, false));
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_nonzero_roster_name_padding_fails_closed() {
  TEST_ASSERT_TRUE(seedRosterSetFile("202611.bin", 7, 2500, 1,
                                     nullptr, false, true));
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_invalid_utc_offset_is_rejected_before_mount() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", -721);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(0, backend.calls);
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_crc_corruption_in_aligned_file_fails_closed_without_modifying_bytes() {
  Event event = {EV_SCAN, 0, 7, 700, 1, 9};
  uint8_t corrupt[REC_SIZE];
  encode(event, corrupt);
  corrupt[3] ^= 0x40u;
  TEST_ASSERT_TRUE(seedMonthFile("202611.bin", corrupt, sizeof corrupt));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);

  char path[160];
  TEST_ASSERT_TRUE(snprintf(path, sizeof path, "%s/log/202611.bin", gRoot) > 0);
  const int fd = open(path, O_RDONLY);
  TEST_ASSERT_TRUE(fd >= 0);
  uint8_t actual[REC_SIZE] = {0};
  TEST_ASSERT_EQUAL_INT((int)sizeof actual, read(fd, actual, sizeof actual));
  TEST_ASSERT_EQUAL_INT(0, close(fd));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(corrupt, actual, sizeof corrupt);
}

void test_sequence_gap_fails_closed() {
  Event event = {EV_SCAN, 0, 7, 700, 2, 9};
  TEST_ASSERT_TRUE(seedEventFile("202611.bin", &event, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_commit_group_cannot_cross_month_file_boundary() {
  Event first = {EV_SCAN, FL_MORE, 7, 700, 1, 9};
  Event second = {EV_SCAN, 0, 8, 800, 2, 9};
  TEST_ASSERT_TRUE(seedEventFile("202611.bin", &first, 1));
  TEST_ASSERT_TRUE(seedEventFile("202612.bin", &second, 1));

  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_begin_is_idempotent_after_ready() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
  TEST_ASSERT_NOT_NULL(owner.fileSet());
}

void test_mount_failure_latches_fault_and_never_exposes_storage() {
  FakeMountBackend backend;
  backend.allowMount = false;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());

  backend.allowMount = true;
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_successful_mount_without_expected_root_fails_closed() {
  TEST_ASSERT_EQUAL_INT(0, rmdir(gRoot));
  FakeMountBackend backend;
  backend.createRoot = false;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
}

void test_invalid_log_layout_fails_closed_after_mount() {
  FakeMountBackend backend;
  backend.malformedLog = true;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);
  TEST_ASSERT_FALSE(owner.cleanupFailed());
}

void test_failed_unmount_is_reported_and_owner_stays_faulted() {
  FakeMountBackend backend;
  backend.malformedLog = true;
  backend.allowUnmount = false;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_FAULT, owner.state());
  TEST_ASSERT_NULL(owner.fileSet());
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);
  TEST_ASSERT_TRUE(owner.cleanupFailed());
  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
  TEST_ASSERT_EQUAL_INT(1, backend.unmountCalls);
}

void test_invalid_mountpoint_is_rejected_before_backend_call() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, "/tmp/../outside", "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(0, backend.calls);
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_partition_label_must_fit_esp_idf_label_buffer() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "partition-label-123", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(0, backend.calls);
  TEST_ASSERT_NULL(owner.fileSet());
}

void test_reentrant_begin_is_rejected_while_mount_is_starting() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
  backend.ownerToReenter = &owner;

  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_FALSE(backend.reentryResult);
  TEST_ASSERT_EQUAL_INT(1, backend.calls);
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());
}

void test_sixteen_character_partition_label_is_accepted() {
  FakeMountBackend backend;
  const char label[] = "1234567890123456";
  LittleFsStorageOwner owner(backend, gRoot, label, 480);

  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL_STRING(label, backend.receivedLabel);
}

void test_mountpoint_over_fileset_capacity_is_rejected() {
  FakeMountBackend backend;
  LittleFsStorageOwner owner(
      backend, "/this/path/is/longer/than/the/supported/fileset/root", "spiffs", 480);

  TEST_ASSERT_FALSE(owner.begin());
  TEST_ASSERT_EQUAL_INT(0, backend.calls);
  TEST_ASSERT_NULL(owner.fileSet());
}


void test_mounted_owner_to_monthset_logstore_reopen() {
  {
    FakeMountBackend backend;
    LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
    TEST_ASSERT_TRUE(owner.begin());
    IFileSet* files = owner.fileSet();
    TEST_ASSERT_NOT_NULL(files);

    MonthSet months(*files);
    months.setMonth(202611);
    LogStore log(months);
    const OpenInfo empty = log.open();
    TEST_ASSERT_TRUE(empty.ok);
    TEST_ASSERT_EQUAL_UINT32(0, empty.records);

    Event scan = {EV_SCAN, makeFlags(0, ST_PRESENT, false, false, false),
                  7, dayStartEpoch(2500, 480), 0, 0};
    TEST_ASSERT_TRUE(log.append(&scan, 1));
    TEST_ASSERT_EQUAL_UINT32(1, log.count());
    Event stored = {0, 0, 0, 0, 0, 0};
    TEST_ASSERT_TRUE(log.at(0, stored));
    TEST_ASSERT_EQUAL_UINT32(1, stored.seq);
    TEST_ASSERT_EQUAL_UINT16(7, stored.a);
  }

  // A new backend/owner models a fresh boot over the same persistent files.
  {
    FakeMountBackend backend;
    LittleFsStorageOwner owner(backend, gRoot, "spiffs", 480);
    TEST_ASSERT_TRUE(owner.begin());
    TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());
    IFileSet* files = owner.fileSet();
    TEST_ASSERT_NOT_NULL(files);

    MonthSet months(*files);
    LogStore reopened(months);
    const OpenInfo result = reopened.open();
    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL_UINT32(1, result.records);
    TEST_ASSERT_EQUAL_UINT32(1, result.lastSeq);
    Event stored = {0, 0, 0, 0, 0, 0};
    TEST_ASSERT_TRUE(reopened.at(0, stored));
    TEST_ASSERT_EQUAL_UINT8(EV_SCAN, stored.type);
    TEST_ASSERT_EQUAL_UINT16(7, stored.a);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_file_set_is_hidden_until_mount_and_root_validation_succeed);
  RUN_TEST(test_mounted_owner_to_monthset_logstore_reopen);
  RUN_TEST(test_partial_record_file_fails_closed_without_modifying_bytes);
  RUN_TEST(test_valid_records_keep_global_sequence_across_month_files);
  RUN_TEST(test_crc_valid_but_semantically_invalid_scan_fails_closed);
  RUN_TEST(test_invalid_roster_lifecycle_across_month_files_fails_closed);
  RUN_TEST(test_valid_roster_lifecycle_across_month_files_is_accepted);
  RUN_TEST(test_invalid_utf8_roster_name_fails_closed);
  RUN_TEST(test_nonzero_roster_name_padding_fails_closed);
  RUN_TEST(test_invalid_utc_offset_is_rejected_before_mount);
  RUN_TEST(test_crc_corruption_in_aligned_file_fails_closed_without_modifying_bytes);
  RUN_TEST(test_sequence_gap_fails_closed);
  RUN_TEST(test_commit_group_cannot_cross_month_file_boundary);
  RUN_TEST(test_begin_is_idempotent_after_ready);
  RUN_TEST(test_mount_failure_latches_fault_and_never_exposes_storage);
  RUN_TEST(test_successful_mount_without_expected_root_fails_closed);
  RUN_TEST(test_invalid_log_layout_fails_closed_after_mount);
  RUN_TEST(test_failed_unmount_is_reported_and_owner_stays_faulted);
  RUN_TEST(test_invalid_mountpoint_is_rejected_before_backend_call);
  RUN_TEST(test_partition_label_must_fit_esp_idf_label_buffer);
  RUN_TEST(test_reentrant_begin_is_rejected_while_mount_is_starting);
  RUN_TEST(test_sixteen_character_partition_label_is_accepted);
  RUN_TEST(test_mountpoint_over_fileset_capacity_is_rejected);
  return UNITY_END();
}
