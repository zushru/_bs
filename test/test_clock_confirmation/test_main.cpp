#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <unity.h>

#include "admin_clock_confirmation.h"
#include "clock_confirmation.h"
#include "clock_health_owner.h"
#include "event_codec.h"
#include "littlefs_mount_owner.h"
#include "time_rules.h"

using namespace att;

namespace {

const uint8_t kStatusRegister = 0x0F;
enum CalendarWriteMode : uint8_t {
  CALENDAR_WRITE_NORMAL,
  CALENDAR_WRITE_FULL_THEN_UNCERTAIN,
  CALENDAR_WRITE_PREFIX_THEN_UNCERTAIN
};

struct FakeRtcIo : IDs3231RegisterIo {
  uint8_t regs[16];
  uint32_t reads;
  uint32_t writes;
  uint32_t calendarWrites;
  uint32_t statusWrites;
  uint8_t advanceCalendarReadSeconds;
  CalendarWriteMode calendarMode;

  FakeRtcIo()
      : reads(0), writes(0), calendarWrites(0), statusWrites(0),
        advanceCalendarReadSeconds(0),
        calendarMode(CALENDAR_WRITE_NORMAL) {
    memset(regs, 0, sizeof regs);
  }

  bool readRegisters(uint8_t first, uint8_t* out,
                     uint8_t length) override {
    ++reads;
    if (!out || length == 0 || first >= sizeof regs ||
        static_cast<size_t>(first) + length > sizeof regs) return false;
    if (first == 0 && length == 7 && calendarWrites > 0 &&
        advanceCalendarReadSeconds > 0) {
      const uint8_t rawSecond = regs[0];
      const uint8_t second = static_cast<uint8_t>(
          ((rawSecond >> 4u) & 0x07u) * 10u + (rawSecond & 0x0Fu));
      if (second > 59 ||
          static_cast<unsigned>(second) + advanceCalendarReadSeconds > 59u)
        return false;  // This fixture deliberately avoids minute rollover.
      regs[0] = static_cast<uint8_t>(
          ((second + advanceCalendarReadSeconds) / 10u) << 4u |
          ((second + advanceCalendarReadSeconds) % 10u));
    }
    memcpy(out, regs + first, length);
    return true;
  }

  Ds3231WriteResult writeRegisters(uint8_t first, const uint8_t* data,
                                   uint8_t length) override {
    ++writes;
    if (!data || length == 0 || first >= sizeof regs ||
        static_cast<size_t>(first) + length > sizeof regs)
      return DS3231_WRITE_NOT_STARTED;

    if (first == 0 && length == 7) {
      ++calendarWrites;
      if (calendarMode == CALENDAR_WRITE_PREFIX_THEN_UNCERTAIN) {
        memcpy(regs, data, 1);
        calendarMode = CALENDAR_WRITE_NORMAL;
        return DS3231_WRITE_UNCERTAIN;
      }
      memcpy(regs, data, length);
      if (calendarMode == CALENDAR_WRITE_FULL_THEN_UNCERTAIN) {
        calendarMode = CALENDAR_WRITE_NORMAL;
        return DS3231_WRITE_UNCERTAIN;
      }
      return DS3231_WRITE_OK;
    }

    if (first == kStatusRegister && length == 1) ++statusWrites;
    memcpy(regs + first, data, length);
    return DS3231_WRITE_OK;
  }
};

struct FakeMonotonic : IClockMonotonicSource {
  uint64_t now;
  uint64_t step;
  uint32_t calls;
  bool fail;
  uint32_t failAtCall;
  uint32_t regressAtCall;
  uint64_t regressByUs;

  explicit FakeMonotonic(uint64_t initial = 0, uint64_t stepUs = 0)
      : now(initial), step(stepUs), calls(0), fail(false), failAtCall(0),
        regressAtCall(0), regressByUs(0) {}

  bool nowMicros(uint64_t& out) override {
    ++calls;
    if (fail || (failAtCall != 0 && calls == failAtCall)) return false;
    out = now;
    if (regressAtCall == calls) {
      if (regressByUs > out) return false;
      out -= regressByUs;
    }
    if (step > UINT64_MAX - now) return false;
    now += step;
    return true;
  }
};

struct TempStorageDirectory {
  char path[64];
  bool valid;

  TempStorageDirectory() : path{0}, valid(false) {
    const char pattern[] = "/tmp/att-clock-owner-XXXXXX";
    memcpy(path, pattern, sizeof pattern);
    valid = mkdtemp(path) != nullptr;
  }

  ~TempStorageDirectory() {
    if (!valid) return;
    char monthFile[128];
    char logDirectory[96];
    const int fileLength = snprintf(monthFile, sizeof monthFile,
                                    "%s/log/202001.bin", path);
    if (fileLength > 0 && (size_t)fileLength < sizeof monthFile)
      (void)unlink(monthFile);
    const int directoryLength = snprintf(logDirectory, sizeof logDirectory,
                                         "%s/log", path);
    if (directoryLength > 0 &&
        (size_t)directoryLength < sizeof logDirectory)
      (void)rmdir(logDirectory);
    (void)rmdir(path);
  }

  TempStorageDirectory(const TempStorageDirectory&) = delete;
  TempStorageDirectory& operator=(const TempStorageDirectory&) = delete;
};

// Native integration keeps the real POSIX IFileSet and serialized wrapper;
// only the no-format mount backend is replaced with this directory fixture.
struct PosixMountBackend : ILittleFsMountBackend {
  bool mountWithoutFormatting(const char* mountpoint,
                              const char* partitionLabel) override {
    if (!mountpoint || !partitionLabel ||
        strcmp(partitionLabel, "spiffs") != 0) return false;
    if (mkdir(mountpoint, 0700) == 0) return true;
    return errno == EEXIST;
  }

  bool unmountOwned() override { return true; }
};

struct MemoryFileSet : IFileSet, ILittleFsStorageOwner {
  struct Entry {
    uint32_t monthKey;
    std::vector<uint8_t> bytes;
  };

  std::vector<Entry> entries;
  bool ownerReady;
  uint32_t appendCalls;
  int32_t failAppendAt;
  bool completeAppendThenFail;
  int32_t tearBytes;

  MemoryFileSet()
      : ownerReady(true), appendCalls(0), failAppendAt(-1),
        completeAppendThenFail(false), tearBytes(-1) {}

  LittleFsOwnerState state() const override {
    return ownerReady ? LITTLEFS_OWNER_READY : LITTLEFS_OWNER_FAULT;
  }

  IFileSet* fileSet() override { return ownerReady ? this : nullptr; }

  bool countChecked(size_t& out) override {
    out = entries.size();
    return true;
  }

  bool monthKeyChecked(size_t index, uint32_t& out) override {
    if (index >= entries.size()) return false;
    out = entries[index].monthKey;
    return true;
  }

  bool fileSizeChecked(size_t index, size_t& out) override {
    if (index >= entries.size()) return false;
    out = entries[index].bytes.size();
    return true;
  }

  bool readFile(size_t index, size_t offset, uint8_t* out,
                size_t length) override {
    if (index >= entries.size() || !out ||
        offset > entries[index].bytes.size() ||
        length > entries[index].bytes.size() - offset) return false;
    if (length)
      memcpy(out, &entries[index].bytes[offset], length);
    return true;
  }

  bool appendFile(size_t index, const uint8_t* data,
                  size_t length) override {
    ++appendCalls;
    if (index >= entries.size() || !data || length == 0) return false;
    if (failAppendAt > 0 &&
        appendCalls == static_cast<uint32_t>(failAppendAt)) return false;
    std::vector<uint8_t>& bytes = entries[index].bytes;
    if (tearBytes >= 0) {
      const size_t prefix = static_cast<size_t>(tearBytes);
      const size_t written = prefix < length ? prefix : length;
      bytes.insert(bytes.end(), data, data + written);
      tearBytes = -1;
      return false;
    }
    bytes.insert(bytes.end(), data, data + length);
    if (completeAppendThenFail) {
      completeAppendThenFail = false;
      return false;
    }
    return true;
  }

  bool truncateFile(size_t index, size_t length) override {
    if (index >= entries.size() || length > entries[index].bytes.size())
      return false;
    entries[index].bytes.resize(length);
    return true;
  }

  bool addFile(uint32_t monthKey) override {
    if (!entries.empty() && monthKey <= entries.back().monthKey) return false;
    Entry entry;
    entry.monthKey = monthKey;
    entries.push_back(entry);
    return true;
  }

  bool quarantineCopy(size_t) override { return false; }
  bool quarantineFrom(size_t) override { return false; }

  size_t totalBytes() const {
    size_t total = 0;
    for (size_t i = 0; i < entries.size(); ++i)
      total += entries[i].bytes.size();
    return total;
  }
};

const Timetable kTimetable = defaultTimetable();
const Params kParams = {480, 30, 30};

Config clockTestConfig() {
  Config config = defaultConfig();
  config.params = kParams;
  config.tt = kTimetable;
  config.termStart = 100;
  config.termEnd = 400;
  config.pinSet = 1;
  memset(config.pinHash, 0xA5, sizeof config.pinHash);
  memset(config.pinSalt, 0x5A, sizeof config.pinSalt);
  strcpy(config.wifiPass, "clock-test-pass");
  return config;
}

struct ClockConfigBackend : IConfigBootBackend {
  Config value;
  bool needsSetup;
  bool started;

  explicit ClockConfigBackend(bool setup = false)
      : value(clockTestConfig()), needsSetup(setup), started(false) {}

  bool begin() override {
    started = true;
    return true;
  }

  bool load(Config& out, CfgPick& pick) override {
    if (!started) return false;
    if (needsSetup) {
      pick = CfgPick{CB_UNPROVISIONED, -1, 0};
      return true;
    }
    out = value;
    pick = CfgPick{CB_OK, 0, 1};
    return true;
  }

  bool readSlot(uint8_t, uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool writeSlot(uint8_t, const uint8_t[CFG_SLOT_SIZE]) override {
    return false;
  }
  bool readProvisionMarker(bool& present) override {
    present = true;
    return started;
  }
};

struct ClockConfigFixture {
  ClockConfigBackend backend;
  ConfigBootCoordinator boot;

  explicit ClockConfigFixture(bool needsSetup = false)
      : backend(needsSetup), boot(backend) {
    (void)boot.start();
  }
};

bool seedRtc(Ds3231Clock& rtc, FakeRtcIo& io, uint32_t epoch,
             bool oscillatorStopped) {
  if (rtc.writeUtcEpoch(epoch, 0) != DS3231_OK) return false;
  io.regs[kStatusRegister] = oscillatorStopped ? 0x80u : 0u;
  io.reads = 0;
  io.writes = 0;
  io.calendarWrites = 0;
  io.statusWrites = 0;
  return true;
}

bool seedLog(MonthSet& months, LogStore& log, uint32_t baselineEpoch) {
  uint32_t monthKey = 202001;
  if (baselineEpoch >= EPOCH_2020 &&
      !monthKeyForEpoch(baselineEpoch, kParams.utcOffsetMin, monthKey))
    return false;
  months.setMonth(monthKey);
  if (!log.open().ok) return false;
  const Event boot = {EV_BOOT, 0, 0, baselineEpoch, 0, 0};
  if (!log.append(&boot, 1)) return false;
  return log.open().ok && log.baselineEpoch() == baselineEpoch;
}

ClockConfirmRequest requestAt(uint32_t epoch) {
  const ClockConfirmRequest request = {epoch, false, false};
  return request;
}

HealthFsm healthForState(HState state) {
  HealthInputs inputs = {
      true, true, false, false, false, true, true, true, true, true};
  switch (state) {
    case H_UNPROVISIONED: inputs.provisioned = false; break;
    case H_CLOCK_FAULT: inputs.clockTrusted = false; break;
    case H_STORAGE_FAULT: inputs.storageOk = false; break;
    case H_CONFIG_FAULT: inputs.cfgOk = false; break;
    case H_SENSOR_FAULT: inputs.sensorOk = false; break;
    default: break;
  }
  HealthFsm health;
  (void)health.update(inputs);
  return health;
}

struct ClockAdminAttemptStore : IPinAttemptStore {
  uint16_t value;
  ClockAdminAttemptStore() : value(0) {}
  bool loadFailureCount(uint16_t& out) override {
    out = value;
    return true;
  }
  bool persistFailureCount(uint16_t next) override {
    value = next;
    return true;
  }
};

struct ClockAdminVerifier : IPinVerifier {
  uint32_t calls;
  ClockAdminVerifier() : calls(0) {}
  PinVerifyOutcome verify(const char*, size_t) override {
    ++calls;
    return PV_MATCH;
  }
};

struct ClockAdminRandom : ISecureRandomSource {
  uint8_t bytes[16];
  ClockAdminRandom() : bytes{0} {
    for (size_t i = 0; i < sizeof bytes; ++i)
      bytes[i] = static_cast<uint8_t>(i + 1);
  }
  bool fill(uint8_t* out, size_t length) override {
    if (!out || length != sizeof bytes) return false;
    memcpy(out, bytes, sizeof bytes);
    return true;
  }
};

struct ClockAdminFixture {
  ClockAdminAttemptStore attempts;
  PinAuthCoordinator auth;
  ClockAdminVerifier verifier;
  ClockAdminRandom random;
  AdminSessionBoundary sessions;

  ClockAdminFixture() : auth(attempts),
      sessions(auth, verifier, random) {
    auth.boot(0);
  }

  bool issueToken(uint32_t loginAtS, uint32_t deliveredAtS,
                  uint8_t token[16]) {
    if (sessions.beginLogin("483920", 6, loginAtS, token).code != PAC_OK)
      return false;
    return sessions.confirmLoginResponseSent(deliveredAtS);
  }
};

void assertClockConfirmationHealthDenied(HState state) {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, false));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(state);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_HEALTH_BLOCKED, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_FALSE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
}

void test_clock_confirmation_rejects_unprovisioned_config_and_storage_health() {
  assertClockConfirmationHealthDenied(H_UNPROVISIONED);
  assertClockConfirmationHealthDenied(H_CONFIG_FAULT);
  assertClockConfirmationHealthDenied(H_STORAGE_FAULT);
}

void test_clock_confirmation_requires_ready_config_owner() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_OK);
  ClockConfigFixture config(true);
  TEST_ASSERT_TRUE(config.boot.needsSetup());
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_CONFIG_NOT_READY, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_FALSE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(1, log.count());
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
}

void test_clock_confirmation_rejects_mismatched_month_set_before_io() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet logFiles;
  MonthSet logMonths(logFiles);
  LogStore log(logMonths);
  TEST_ASSERT_TRUE(seedLog(logMonths, log, oldEpoch));

  MemoryFileSet foreignFiles;
  MonthSet foreignMonths(foreignFiles);
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot,
                                   foreignFiles);

  const ClockConfirmResult result = service.confirm(
      log, foreignMonths, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_STORAGE_MISMATCH, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, logFiles.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(0, foreignFiles.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(1, log.count());
}

void test_clock_confirmation_requires_ready_storage_owner() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  storage.ownerReady = false;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot,
                                   storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_STORAGE_OWNER_NOT_READY, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_FALSE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
}

void test_clock_confirmation_rejects_month_set_from_foreign_owner() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  MemoryFileSet foreignOwner;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot,
                                   foreignOwner);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_STORAGE_OWNER_MISMATCH, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_FALSE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(0, foreignOwner.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
}

void test_clock_confirmation_accepts_concrete_littlefs_owner_chain() {
  TempStorageDirectory directory;
  TEST_ASSERT_TRUE(directory.valid);
  PosixMountBackend backend;
  LittleFsStorageOwner owner(backend, directory.path, "spiffs", 480);
  TEST_ASSERT_TRUE(owner.begin());
  TEST_ASSERT_EQUAL(LITTLEFS_OWNER_READY, owner.state());

  IFileSet* ownerFiles = owner.fileSet();
  TEST_ASSERT_NOT_NULL(ownerFiles);
  MonthSet months(*ownerFiles);
  LogStore log(months);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  TEST_ASSERT_TRUE(months.usesFileSet(*owner.fileSet()));
  TEST_ASSERT_TRUE(log.usesStorage(months));

  FakeRtcIo io;
  Ds3231Clock rtc(io);
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot,
                                   owner);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(3, log.count());
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
  TEST_ASSERT_EQUAL_UINT32(1, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(1, io.statusWrites);

  size_t fileCount = 0;
  size_t fileSize = 0;
  uint32_t monthKey = 0;
  TEST_ASSERT_TRUE(ownerFiles->countChecked(fileCount));
  TEST_ASSERT_EQUAL_UINT32(1, fileCount);
  TEST_ASSERT_TRUE(ownerFiles->monthKeyChecked(0, monthKey));
  TEST_ASSERT_EQUAL_UINT32(202001, monthKey);
  TEST_ASSERT_TRUE(ownerFiles->fileSizeChecked(0, fileSize));
  TEST_ASSERT_EQUAL_UINT32(3 * REC_SIZE, fileSize);
}

void test_clock_confirmation_rejects_foreign_monthset_with_concrete_owner() {
  TempStorageDirectory directory;
  TEST_ASSERT_TRUE(directory.valid);
  PosixMountBackend backend;
  LittleFsStorageOwner owner(backend, directory.path, "spiffs", 480);
  TEST_ASSERT_TRUE(owner.begin());
  IFileSet* ownerFiles = owner.fileSet();
  TEST_ASSERT_NOT_NULL(ownerFiles);

  MonthSet ownedMonths(*ownerFiles);
  LogStore log(ownedMonths);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  TEST_ASSERT_TRUE(seedLog(ownedMonths, log, oldEpoch));
  size_t originalSize = 0;
  TEST_ASSERT_TRUE(ownerFiles->fileSizeChecked(0, originalSize));
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, originalSize);

  MemoryFileSet foreignFiles;
  MonthSet foreignMonths(foreignFiles);
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot,
                                   owner);

  const ClockConfirmResult result = service.confirm(
      log, foreignMonths, requestAt(oldEpoch + 600u));

  TEST_ASSERT_EQUAL_INT(CCF_STORAGE_OWNER_MISMATCH, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(0, io.reads);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(0, foreignFiles.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(1, log.count());
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
  size_t finalSize = 0;
  TEST_ASSERT_TRUE(ownerFiles->fileSizeChecked(0, finalSize));
  TEST_ASSERT_EQUAL_UINT32(originalSize, finalSize);
}

void test_clock_audit_append_uses_requested_local_month() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 30u * 86400u +
      15u * 3600u + 59u * 60u;  // 2020-01-31 23:59 PHT
  const uint32_t newEpoch = oldEpoch + 120u;  // 2020-02-01 00:01 PHT
  uint32_t oldMonthKey = 0;
  uint32_t newMonthKey = 0;
  TEST_ASSERT_TRUE(monthKeyForEpoch(oldEpoch, kParams.utcOffsetMin,
                                    oldMonthKey));
  TEST_ASSERT_TRUE(monthKeyForEpoch(newEpoch, kParams.utcOffsetMin,
                                    newMonthKey));
  TEST_ASSERT_EQUAL_UINT32(202001, oldMonthKey);
  TEST_ASSERT_EQUAL_UINT32(202002, newMonthKey);
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(3, log.count());

  size_t count = 0;
  uint32_t firstKey = 0;
  uint32_t secondKey = 0;
  size_t firstSize = 0;
  size_t secondSize = 0;
  TEST_ASSERT_TRUE(storage.countChecked(count));
  TEST_ASSERT_EQUAL_UINT32(2, count);
  TEST_ASSERT_TRUE(storage.monthKeyChecked(0, firstKey));
  TEST_ASSERT_TRUE(storage.monthKeyChecked(1, secondKey));
  TEST_ASSERT_TRUE(storage.fileSizeChecked(0, firstSize));
  TEST_ASSERT_TRUE(storage.fileSizeChecked(1, secondSize));
  TEST_ASSERT_EQUAL_UINT32(202001, firstKey);
  TEST_ASSERT_EQUAL_UINT32(202002, secondKey);
  TEST_ASSERT_EQUAL_UINT32(REC_SIZE, firstSize);
  TEST_ASSERT_EQUAL_UINT32(2 * REC_SIZE, secondSize);

  Event oldSet = {};
  Event newSet = {};
  TEST_ASSERT_TRUE(log.at(1, oldSet));
  TEST_ASSERT_TRUE(log.at(2, newSet));
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, oldSet.type);
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, newSet.type);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, oldSet.epoch);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, newSet.epoch);
}

void test_sensor_fault_does_not_block_explicit_clock_recovery() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_SENSOR_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_FALSE(service.blocked());
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT, health.state());
  TEST_ASSERT_TRUE((health.mask() & HB_SENSOR) != 0);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
}

void test_backward_clock_change_requires_confirmation_without_mutation() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 18u * 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, false));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, 0));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_OK);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(oldEpoch - 600u));

  TEST_ASSERT_EQUAL_INT(CCF_CONFIRMATION_REQUIRED, result.code);
  TEST_ASSERT_EQUAL_INT(CA_NEED_CONFIRM_BACK, result.action);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
  TEST_ASSERT_FALSE(service.blocked());
}

void test_healthy_clock_sync_inside_scan_window_is_blocked() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t mondayMorning =
      EPOCH_2020 + 5u * 86400u + 7u * 3600u + 45u * 60u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, mondayMorning, false));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, 0));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_OK);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(mondayMorning + 60u));

  TEST_ASSERT_EQUAL_INT(CCF_BLOCKED_WINDOW, result.code);
  TEST_ASSERT_EQUAL_INT(CA_BLOCKED_WINDOW, result.action);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(1, storage.appendCalls);
}

void test_confirmed_clock_transaction_sets_utc_clears_osf_and_replays_pair() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 3600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  // The service verifies the transaction but leaves HealthFsm release to the
  // serialized application owner.
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, health.state());
  TEST_ASSERT_TRUE((health.mask() & HB_CLOCK) != 0);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, result.previousUtcEpoch);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, result.confirmedUtcEpoch);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, result.replayBaselineEpoch);
  TEST_ASSERT_EQUAL_UINT32(0, io.regs[kStatusRegister] & 0x80u);
  TEST_ASSERT_EQUAL_UINT32(1, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(1, io.statusWrites);
  TEST_ASSERT_FALSE(log.faulted());

  Ds3231Reading actual = {0, true};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, rtc.readUtcEpoch(0, actual));
  TEST_ASSERT_EQUAL_UINT32(newEpoch, actual.epochUtc);
  TEST_ASSERT_FALSE(actual.oscillatorStopped);

  Event oldSet = {};
  Event newSet = {};
  TEST_ASSERT_TRUE(log.at(log.count() - 2, oldSet));
  TEST_ASSERT_TRUE(log.at(log.count() - 1, newSet));
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, oldSet.type);
  TEST_ASSERT_EQUAL_INT(EV_CLOCK_SET, newSet.type);
  TEST_ASSERT_EQUAL_INT(SRC_MANUAL, oldSet.a);
  TEST_ASSERT_EQUAL_INT(SRC_MANUAL, newSet.a);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, oldSet.epoch);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, newSet.epoch);
  TEST_ASSERT_TRUE(flagMore(oldSet.flags));
  TEST_ASSERT_FALSE(flagMore(newSet.flags));
}

void test_full_rtc_write_reported_uncertain_is_reconciled_by_readback() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  io.calendarMode = CALENDAR_WRITE_FULL_THEN_UNCERTAIN;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, result.replayBaselineEpoch);
  TEST_ASSERT_FALSE(service.blocked());
}

void test_partial_rtc_register_write_keeps_clock_untrusted_and_osf_set() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 601u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  io.calendarMode = CALENDAR_WRITE_PREFIX_THEN_UNCERTAIN;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_RTC_WRITE_UNCERTAIN, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(1, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(0, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT8(0x80, io.regs[kStatusRegister]);
  TEST_ASSERT_EQUAL_UINT32(2, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(3, log.count());
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
}

void test_valid_gap_prefix_without_clock_pair_stays_untrusted() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 730u * 86400u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  storage.failAppendAt = 3;  // boot record, GAP prefix, then final group fails
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);
  ClockConfirmRequest request = {newEpoch, false, true};

  const ClockConfirmResult result = service.confirm(
      log, months, request);

  TEST_ASSERT_EQUAL_INT(CCF_LOG_COMMIT_INCOMPLETE, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_FALSE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(3, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(15, log.count());
  TEST_ASSERT_EQUAL_UINT32(0, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(0, io.statusWrites);

  Event last = {};
  TEST_ASSERT_TRUE(log.at(log.count() - 1, last));
  TEST_ASSERT_TRUE(last.type != EV_CLOCK_SET);
}

void test_complete_uncertain_log_append_is_reconciled_without_retry() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  storage.completeAppendThenFail = true;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_FALSE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(2, storage.appendCalls);
}

void test_torn_log_commit_fails_closed_and_latches_service() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  storage.tearBytes = 8;
  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult first = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_LOG_REPLAY_FAILED, first.code);
  TEST_ASSERT_FALSE(first.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_TRUE(log.faulted());
  TEST_ASSERT_EQUAL_UINT32(24, storage.totalBytes());
  TEST_ASSERT_EQUAL_UINT32(0, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(0, io.statusWrites);

  const uint32_t writesAfterFailure = io.writes;
  const uint32_t appendsAfterFailure = storage.appendCalls;
  const ClockConfirmResult retry = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_SERVICE_LATCHED, retry.code);
  TEST_ASSERT_EQUAL_UINT32(writesAfterFailure, io.writes);
  TEST_ASSERT_EQUAL_UINT32(appendsAfterFailure, storage.appendCalls);
}

void test_normal_rtc_second_ticks_are_accepted_with_elapsed_time_bound() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));
  io.advanceCalendarReadSeconds = 1;

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic(0, 1000000ULL);
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_OK, result.code);
  TEST_ASSERT_TRUE(result.clockTrusted);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
  TEST_ASSERT_EQUAL_UINT32(1, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT32(4, monotonic.calls);

  io.advanceCalendarReadSeconds = 0;
  Ds3231Reading actual = {0, true};
  TEST_ASSERT_EQUAL_INT(DS3231_OK, rtc.readUtcEpoch(0, actual));
  TEST_ASSERT_EQUAL_UINT32(newEpoch + 3u, actual.epochUtc);
  TEST_ASSERT_FALSE(actual.oscillatorStopped);
}

void test_rtc_advancement_beyond_monotonic_bound_is_rejected() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));
  io.advanceCalendarReadSeconds = 2;

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic(0, 100000ULL);
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_RTC_VERIFY_FAILED, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
}

void test_monotonic_failure_after_audit_commit_prevents_rtc_write() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic;
  monotonic.fail = true;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_MONOTONIC_FAILED, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(0, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(0, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
}

void test_monotonic_counter_regression_after_rtc_write_fails_closed() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));
  io.advanceCalendarReadSeconds = 1;

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  FakeMonotonic monotonic(0, 1000000ULL);
  monotonic.regressAtCall = 3;
  monotonic.regressByUs = 1500000ULL;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);

  const ClockConfirmResult result = service.confirm(
      log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(CCF_MONOTONIC_FAILED, result.code);
  TEST_ASSERT_FALSE(result.clockTrusted);
  TEST_ASSERT_TRUE(service.blocked());
  TEST_ASSERT_EQUAL_UINT32(1, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());
}

void test_admin_clock_dispatch_requires_this_requests_token() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  const uint32_t appendCallsBefore = storage.appendCalls;
  const size_t recordsBefore = log.count();

  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_CLOCK_FAULT);
  HealthInputs currentHealth = {
      true, true, false, false, false, true, true, false, false, false};
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, health.update(currentHealth));
  TEST_ASSERT_EQUAL_UINT8(HB_CLOCK | HB_SENSOR, health.mask());
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);
  ClockAdminFixture admin;
  uint8_t token[16] = {};
  TEST_ASSERT_TRUE(admin.issueToken(20, 21, token));
  AdminClockConfirmation dispatch(admin.sessions, service);
  ClockHealthOwner owner(dispatch, health, config.boot);
  TEST_ASSERT_TRUE(owner.ready());

  uint8_t otherToken[16];
  memcpy(otherToken, token, sizeof otherToken);
  otherToken[0] ^= 0x80u;
  const ClockHealthDispatchResult denied = owner.confirm(
      otherToken, 22, log, months, requestAt(newEpoch), currentHealth);

  TEST_ASSERT_TRUE(denied.dispatched);
  TEST_ASSERT_FALSE(denied.clockAccepted);
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, denied.healthState);
  TEST_ASSERT_EQUAL_INT(ACDC_AUTHORIZATION_DENIED, denied.admin.code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, denied.admin.authorization.code);
  TEST_ASSERT_FALSE(denied.admin.confirmation.clockTrusted);
  TEST_ASSERT_EQUAL_INT(CCF_BAD_INPUT, denied.admin.confirmation.code);
  TEST_ASSERT_EQUAL_UINT32(0, io.reads);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(appendCallsBefore, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(recordsBefore, log.count());
  TEST_ASSERT_FALSE(service.blocked());

  const ClockHealthDispatchResult accepted = owner.confirm(
      token, 23, log, months, requestAt(newEpoch), currentHealth);
  TEST_ASSERT_TRUE(accepted.dispatched);
  TEST_ASSERT_TRUE(accepted.clockAccepted);
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT, accepted.healthState);
  TEST_ASSERT_EQUAL_UINT8(HB_SENSOR, health.mask());
  TEST_ASSERT_TRUE(owner.clockTrusted());
  TEST_ASSERT_EQUAL_INT(ACDC_CONFIRMATION_INVOKED, accepted.admin.code);
  TEST_ASSERT_EQUAL_INT(A_OK, accepted.admin.authorization.code);
  TEST_ASSERT_EQUAL_INT(CCF_OK, accepted.admin.confirmation.code);
  TEST_ASSERT_TRUE(accepted.admin.confirmation.clockTrusted);
  // A later health refresh retains volatile trust but preserves the sensor fault.
  TEST_ASSERT_EQUAL_INT(H_SENSOR_FAULT, owner.update(currentHealth));
  TEST_ASSERT_EQUAL_UINT8(HB_SENSOR, health.mask());
  currentHealth.epochBelowLog = true;
  TEST_ASSERT_EQUAL_INT(H_CLOCK_FAULT, owner.update(currentHealth));
  TEST_ASSERT_FALSE(owner.clockTrusted());
  TEST_ASSERT_EQUAL_UINT8(HB_CLOCK | HB_SENSOR, health.mask());
  TEST_ASSERT_EQUAL_UINT32(1, io.calendarWrites);
  TEST_ASSERT_EQUAL_UINT32(1, io.statusWrites);
  TEST_ASSERT_EQUAL_UINT32(3, log.count());
  TEST_ASSERT_EQUAL_UINT32(newEpoch, log.baselineEpoch());

  // The clock owner must not clear another domain's latched fault, even if
  // its caller incorrectly claims that a complete re-verification occurred.
  HealthInputs storageFault = currentHealth;
  storageFault.storageOk = false;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, owner.update(storageFault));
  TEST_ASSERT_EQUAL_UINT8(HB_STORAGE | HB_CLOCK | HB_SENSOR, health.mask());
  HealthInputs callerClaimsReverified = currentHealth;
  callerClaimsReverified.reverified = true;
  TEST_ASSERT_EQUAL_INT(H_STORAGE_FAULT, owner.update(callerClaimsReverified));
  TEST_ASSERT_EQUAL_UINT8(HB_STORAGE | HB_CLOCK | HB_SENSOR, health.mask());
}

void test_admin_clock_dispatch_rejects_storage_fault_before_service() {
  FakeRtcIo io;
  Ds3231Clock rtc(io);
  const uint32_t oldEpoch = EPOCH_2020 + 5u * 86400u + 16u * 3600u;
  const uint32_t newEpoch = oldEpoch + 600u;
  TEST_ASSERT_TRUE(seedRtc(rtc, io, oldEpoch, true));

  MemoryFileSet storage;
  MonthSet months(storage);
  LogStore log(months);
  TEST_ASSERT_TRUE(seedLog(months, log, oldEpoch));
  const uint32_t appendCallsBefore = storage.appendCalls;

  FakeMonotonic monotonic;
  HealthFsm health = healthForState(H_STORAGE_FAULT);
  ClockConfigFixture config;
  ClockConfirmationService service(rtc, monotonic, health, config.boot, storage);
  ClockAdminFixture admin;
  uint8_t token[16] = {};
  TEST_ASSERT_TRUE(admin.issueToken(20, 21, token));
  AdminClockConfirmation dispatch(admin.sessions, service);

  const AdminClockDispatchResult denied = dispatch.confirm(
      token, 22, log, months, requestAt(newEpoch));

  TEST_ASSERT_EQUAL_INT(ACDC_AUTHORIZATION_DENIED, denied.code);
  TEST_ASSERT_EQUAL_INT(A_LOCKED, denied.authorization.code);
  TEST_ASSERT_EQUAL_INT(R_STATE, denied.authorization.reason);
  TEST_ASSERT_EQUAL_INT(CCF_BAD_INPUT, denied.confirmation.code);
  TEST_ASSERT_EQUAL_UINT32(0, io.reads);
  TEST_ASSERT_EQUAL_UINT32(0, io.writes);
  TEST_ASSERT_EQUAL_UINT32(appendCallsBefore, storage.appendCalls);
  TEST_ASSERT_EQUAL_UINT32(oldEpoch, log.baselineEpoch());
  TEST_ASSERT_FALSE(service.blocked());
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_confirmation_rejects_unprovisioned_config_and_storage_health);
  RUN_TEST(test_clock_confirmation_requires_ready_config_owner);
  RUN_TEST(test_clock_confirmation_rejects_mismatched_month_set_before_io);
  RUN_TEST(test_clock_confirmation_requires_ready_storage_owner);
  RUN_TEST(test_clock_confirmation_rejects_month_set_from_foreign_owner);
  RUN_TEST(test_clock_confirmation_accepts_concrete_littlefs_owner_chain);
  RUN_TEST(test_clock_confirmation_rejects_foreign_monthset_with_concrete_owner);
  RUN_TEST(test_clock_audit_append_uses_requested_local_month);
  RUN_TEST(test_sensor_fault_does_not_block_explicit_clock_recovery);
  RUN_TEST(test_backward_clock_change_requires_confirmation_without_mutation);
  RUN_TEST(test_healthy_clock_sync_inside_scan_window_is_blocked);
  RUN_TEST(test_confirmed_clock_transaction_sets_utc_clears_osf_and_replays_pair);
  RUN_TEST(test_full_rtc_write_reported_uncertain_is_reconciled_by_readback);
  RUN_TEST(test_partial_rtc_register_write_keeps_clock_untrusted_and_osf_set);
  RUN_TEST(test_valid_gap_prefix_without_clock_pair_stays_untrusted);
  RUN_TEST(test_complete_uncertain_log_append_is_reconciled_without_retry);
  RUN_TEST(test_torn_log_commit_fails_closed_and_latches_service);
  RUN_TEST(test_normal_rtc_second_ticks_are_accepted_with_elapsed_time_bound);
  RUN_TEST(test_rtc_advancement_beyond_monotonic_bound_is_rejected);
  RUN_TEST(test_monotonic_failure_after_audit_commit_prevents_rtc_write);
  RUN_TEST(test_monotonic_counter_regression_after_rtc_write_fails_closed);
  RUN_TEST(test_admin_clock_dispatch_requires_this_requests_token);
  RUN_TEST(test_admin_clock_dispatch_rejects_storage_fault_before_service);
  return UNITY_END();
}
