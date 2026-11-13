#include <unity.h>
#include <Arduino.h>
#include <string.h>
#include <type_traits>
#include <utility>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "as608_attendance_pipeline.h"
#include "as608_sensor_arbiter.h"
#include "as608_attendance_client.h"
#include "as608_runtime.h"
#include "as608_startup.h"
#include "as608_attendance_owner.h"
#include "as608_owner_config_runtime.h"
#include "application_attendance_gate.h"
#include "littlefs_mount_owner.h"
#include "config_boot.h"
#include "config_runtime_guard.h"
#include "closure_engine.h"

using namespace att;
static_assert(std::is_same<decltype(std::declval<As608AttendanceOwner&>().occupancy()),
                           const Occupancy&>::value,
              "Attendance owner must expose occupancy read-only");
static_assert(!std::is_constructible<As608AttendanceOwner, Stream&, LogStore&,
    RosterStore&, const Config&, const IAs608PageResolver&,
    IAttendanceAdmission&, uint32_t>::value,
    "preferred attendance owner must not accept an ungated admission");
static_assert(std::is_constructible<As608AttendanceOwner, Stream&, LogStore&,
    RosterStore&, const Config&, const IAs608PageResolver&,
    ApplicationAttendanceGate&, uint32_t>::value,
    "preferred attendance owner must require the application gate");
uint32_t gTestMillis = 0;

namespace {
const int32_t TEST_DAY = 1;
const uint32_t TEST_EPOCH =
    dayStartEpoch(TEST_DAY, 480) + 8u * 3600u + 5u * 60u;

class MemStorage : public IStorage {
 public:
  MemStorage() : length_(0), failAppend_(false), appendAllThenFail_(false) { memset(bytes_, 0, sizeof bytes_); }
  bool sizeChecked(size_t& out) override { out = length_; return true; }
  bool read(size_t offset, uint8_t* out, size_t count) override {
    if (!out || offset > length_ || count > length_ - offset) return false;
    memcpy(out, bytes_ + offset, count); return true;
  }
  bool append(const uint8_t* in, size_t count) override {
    if (failAppend_ || (!in && count) || count > sizeof bytes_ - length_) return false;
    if (count) memcpy(bytes_ + length_, in, count);
    length_ += count;
    if (appendAllThenFail_) { appendAllThenFail_ = false; return false; }
    return true;
  }
  bool truncate(size_t length) override {
    if (length > length_) return false;
    length_ = length;
    return true;
  }
  void failAppend(bool fail) { failAppend_ = fail; }
  void appendAllThenFailOnce() { appendAllThenFail_ = true; }
 private:
  uint8_t bytes_[REC_SIZE * 64];
  size_t length_;
  bool failAppend_, appendAllThenFail_;
};

class FakeStream : public Stream {
 public:
  FakeStream() : txLength_(0), rxLength_(0), rxPos_(0) {
    memset(tx_, 0, sizeof tx_); memset(rx_, 0, sizeof rx_);
  }
  size_t write(const uint8_t* data, size_t length) override {
    if (!data || length > sizeof tx_) return 0;
    memcpy(tx_, data, length); txLength_ = length; return length;
  }
  int available() override { return static_cast<int>(rxLength_ - rxPos_); }
  int read() override { return rxPos_ < rxLength_ ? rx_[rxPos_++] : -1; }
  uint8_t instruction() const { return txLength_ > 9 ? tx_[9] : 0; }
  void ackWith(uint8_t code, const uint8_t* data = nullptr, size_t dataLength = 0) {
    if (dataLength > 32 || (dataLength && !data)) return;
    uint8_t frame[64];
    const uint16_t packetLength = static_cast<uint16_t>(dataLength + 3);
    frame[0] = 0xEF; frame[1] = 0x01;
    frame[2] = frame[3] = frame[4] = frame[5] = 0xFF;
    frame[6] = 0x07; frame[7] = static_cast<uint8_t>(packetLength >> 8);
    frame[8] = static_cast<uint8_t>(packetLength); frame[9] = code;
    if (dataLength) memcpy(frame + 10, data, dataLength);
    uint16_t sum = static_cast<uint16_t>(0x07 + frame[7] + frame[8] + code);
    for (size_t i = 0; i < dataLength; ++i) sum = static_cast<uint16_t>(sum + data[i]);
    frame[10 + dataLength] = static_cast<uint8_t>(sum >> 8);
    frame[11 + dataLength] = static_cast<uint8_t>(sum);
    memcpy(rx_, frame, dataLength + 12); rxLength_ = dataLength + 12; rxPos_ = 0;
  }
 private:
  uint8_t tx_[64], rx_[64];
  size_t txLength_, rxLength_, rxPos_;
};

class FixedPageResolver : public IAs608PageResolver {
 public:
  FixedPageResolver(uint16_t page, uint16_t id, bool valid = true)
      : page_(page), id_(id), valid_(valid) {}
  bool studentIdForPage(uint16_t physicalPage, uint16_t& studentId) const override {
    if (!valid_ || physicalPage != page_) return false;
    studentId = id_; return true;
  }
 private:
  uint16_t page_, id_;
  bool valid_;
};

class TestConfigSlots : public IConfigSlotBackend {
 public:
  TestConfigSlots() : writes_(0) { memset(slots_, 0xFF, sizeof slots_); }
  bool readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) override {
    if (slot > 1 || !out) return false;
    memcpy(out, slots_[slot], CFG_SLOT_SIZE);
    return true;
  }
  bool writeSlot(uint8_t slot, const uint8_t data[CFG_SLOT_SIZE]) override {
    if (slot > 1 || !data) return false;
    memcpy(slots_[slot], data, CFG_SLOT_SIZE);
    ++writes_;
    return true;
  }
  bool seed(const Config& config, uint32_t generation) {
    return encodeConfig(config, generation, slots_[0]) != 0;
  }
  uint32_t writes() const { return writes_; }
 private:
  uint8_t slots_[2][CFG_SLOT_SIZE];
  uint32_t writes_;
};

ConfigGateIn configGate(uint32_t generation = 7) {
  ConfigGateIn gate;
  gate.h = H_OK;
  gate.expectGen = generation;
  gate.curGen = 0;
  gate.windowOpen = false;
  gate.unclosedPast = false;
  return gate;
}

Config validConfig(int32_t termEnd = 10) {
  Config config = defaultConfig();
  config.termStart = 0; config.termEnd = termEnd;
  config.pinSet = 1; config.pinHash[0] = 1;
  strcpy(config.wifiPass, "test-passphrase");
  return config;
}

bool addStudent(LogStore& log, RosterStore& roster, uint16_t id) {
  Event events[5]; size_t count = 0;
  const char name[] = "Test Learner";
  if (roster.planSet(id, name, sizeof(name) - 1,
      dayStartEpoch(0, 480) + 12u * 3600u, events, count) != RE_OK) return false;
  return log.append(events, count);
}

class GateBootBackend : public IConfigBootBackend {
 public:
  explicit GateBootBackend(const Config& config, bool provisioned = true)
      : config_(config), provisioned_(provisioned) {}
  bool begin() override { return true; }
  bool load(Config& out, CfgPick& pick) override {
    if (!provisioned_) {
      pick = CfgPick{CB_UNPROVISIONED, -1, 0};
      return true;
    }
    out = config_;
    pick = CfgPick{CB_OK, 0, 1};
    return validateConfig(out);
  }
  bool readSlot(uint8_t, uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool writeSlot(uint8_t, const uint8_t[CFG_SLOT_SIZE]) override { return false; }
  bool readProvisionMarker(bool& present) override {
    present = provisioned_;
    return true;
  }
 private:
  Config config_;
  bool provisioned_;
};

class GateMountBackend : public ILittleFsMountBackend {
 public:
  bool mountWithoutFormatting(const char* mountpoint,
                              const char*) override {
    if (!mountpoint) return false;
    return mkdir(mountpoint, 0700) == 0 || errno == EEXIST;
  }
  bool unmountOwned() override { return true; }
};

struct GateFixture {
  char root[96];
  Config config;
  GateBootBackend bootBackend;
  ConfigBootCoordinator boot;
  GateMountBackend mountBackend;
  LittleFsStorageOwner* storage;
  MonthSet* months;
  LogStore* log;
  RosterStore* roster;
  LogAttendanceAdmission* admission;
  ClosureEngine* closure;
  HealthFsm health;
  ApplicationAttendanceGate* gate;
  bool built;

  explicit GateFixture(bool provisioned = true)
      : root{0}, config(validConfig()), bootBackend(config, provisioned),
        boot(bootBackend), mountBackend(), storage(nullptr),
        months(nullptr), log(nullptr), roster(nullptr), admission(nullptr),
        closure(nullptr), health(), gate(nullptr), built(false) {
    if (boot.start().state != (provisioned ? CFG_BOOT_READY : CFG_BOOT_NEEDS_SETUP)) return;
    // NEEDS_SETUP is a pre-runtime state. Do not construct storage/log/roster/
    // closure services until provisioning has transitioned boot to READY.
    if (!provisioned) return;
    storage = new LittleFsStorageOwner(mountBackend,
        (strcpy(root, "/tmp/att-c3-gate-XXXXXX"), mkdtemp(root) ? root : "/invalid"),
        "spiffs", 480);
    if (!storage || !storage->begin()) return;
    months = new MonthSet(*storage->fileSet());
    if (!months) return;
    months->setMonth(202611);
    log = new LogStore(*months);
    if (!log || !log->open().ok) return;
    roster = new RosterStore(480);
    if (!roster || !log->attach(roster)) return;
    admission = new LogAttendanceAdmission(*log, config.params);
    if (!admission || !admission->attached()) return;
    closure = new ClosureEngine(*log, config.params, config.tt, *roster,
                                MAX_ID, config.termStart, config.termEnd);
    if (!closure || !closure->ready()) return;
    setHealth(false, true, true);
    gate = new ApplicationAttendanceGate(boot, *storage, *months, *log,
                                         *roster, *admission, *closure, health);
    built = gate != nullptr;
  }

  ~GateFixture() {
    delete gate;
    delete closure;
    delete admission;
    delete roster;
    delete log;
    delete months;
    delete storage;
    const char* file = "/log/202611.bin";
    char path[160];
    const int n = snprintf(path, sizeof path, "%s%s", root, file);
    if (n > 0 && (size_t)n < sizeof path) unlink(path);
    if (root[0]) {
      snprintf(path, sizeof path, "%s/log", root);
      rmdir(path);
      rmdir(root);
    }
  }

  void setHealth(bool sensorOk, bool clockOk, bool storageOk) {
    HealthInputs inputs = {true, true, !clockOk, false, false,
                           storageOk, true, sensorOk, false, clockOk};
    updateHealthFromConfigBoot(health, boot, inputs);
  }
};

bool setupRuntime(As608StartupCoordinator& startup, FakeStream& stream,
                  bool includeStudentFour) {
  if (!startup.begin()) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.instruction() != 0x13) return false;
  stream.ackWith(0x00);  // enrollment-owner authentication
  if (startup.step() != AS608_STARTUP_PENDING) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.instruction() != 0x0F) return false;
  uint8_t params[16] = {0}; params[5] = 45;
  stream.ackWith(0x00, params, sizeof params);
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // capacity; begin index refresh
  if (startup.step() != AS608_STARTUP_PENDING || stream.instruction() != 0x1F) return false;
  uint8_t index[32] = {0};
  if (includeStudentFour) index[0] = 0x08;
  stream.ackWith(0x00, index, sizeof index);
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // bounded first RX chunk
  if (startup.step() != AS608_STARTUP_PENDING) return false;  // finish index; begin attendance setup
  if (startup.step() != AS608_STARTUP_PENDING || stream.instruction() != 0x13) return false;
  stream.ackWith(0x00);  // attendance-owner authentication
  if (startup.step() != AS608_STARTUP_PENDING) return false;
  if (startup.step() != AS608_STARTUP_PENDING || stream.instruction() != 0x0F) return false;
  stream.ackWith(0x00, params, sizeof params);
  return startup.step() == AS608_STARTUP_READY && startup.ready();
}

AttendancePipelineResult finishMatch(As608AttendancePipeline& pipeline,
    FakeStream& stream, uint16_t page, uint16_t score) {
  if (pipeline.step() != ATT_PIPE_PENDING || stream.instruction() != 0x01)
    return ATT_PIPE_REJECTED;
  stream.ackWith(0x00);
  if (pipeline.step() != ATT_PIPE_PENDING) return ATT_PIPE_REJECTED;
  if (pipeline.step() != ATT_PIPE_PENDING || stream.instruction() != 0x02)
    return ATT_PIPE_REJECTED;
  stream.ackWith(0x00);
  if (pipeline.step() != ATT_PIPE_PENDING) return ATT_PIPE_REJECTED;
  if (pipeline.step() != ATT_PIPE_PENDING || stream.instruction() != 0x04)
    return ATT_PIPE_REJECTED;
  const uint8_t match[4] = {static_cast<uint8_t>(page >> 8), static_cast<uint8_t>(page),
      static_cast<uint8_t>(score >> 8), static_cast<uint8_t>(score)};
  stream.ackWith(0x00, match, sizeof match);
  return pipeline.step();
}

struct Fixture {
  MemStorage storage;
  LogStore log;
  RosterStore roster;
  Config config;
  LogAttendanceAdmission admission;
  FakeStream stream;
  Occupancy occupancy;
  As608Runtime sensorRuntime;
  As608StartupCoordinator startup;
  FixedPageResolver pages;
  As608AttendancePipeline pipeline;
  explicit Fixture(int32_t termEnd = 10)
      : storage(), log(storage), roster(480), config(validConfig(termEnd)),
      admission(log, config.params), stream(), occupancy(),
      sensorRuntime(stream, roster, occupancy, 0), startup(sensorRuntime), pages(7, 4),
      pipeline(sensorRuntime, log, roster, config, pages, admission) {
    log.open(); log.attach(&roster);
  }
  bool ready(bool completeOccupancy = true) {
    if (!setupRuntime(startup, stream, completeOccupancy)) return false;
    if (!completeOccupancy) occupancy.begin();
    return addStudent(log, roster, 4);
  }
};
}  // namespace

void setUp() {}
void tearDown() {}

void test_match_is_resolved_checked_and_committed_once() {
  Fixture f;
  TEST_ASSERT_TRUE(f.ready()); TEST_ASSERT_TRUE(f.admission.attached());
  TEST_ASSERT_TRUE(validateConfig(f.config));
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH)); TEST_ASSERT_TRUE(f.pipeline.active());
  TEST_ASSERT_TRUE(f.sensorRuntime.attendance().ready());
  TEST_ASSERT_TRUE(f.sensorRuntime.attendance().held());
  const AttendancePipelineResult result = finishMatch(f.pipeline, f.stream, 7, 0x0123);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_SAVED_PRESENT, result);
  TEST_ASSERT_FALSE(f.pipeline.active()); TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
  TEST_ASSERT_EQUAL_UINT32(6, f.log.count());
  Event saved;
  TEST_ASSERT_TRUE(f.log.at(5, saved));
  TEST_ASSERT_EQUAL_UINT8(EV_SCAN, saved.type);
  TEST_ASSERT_EQUAL_UINT16(4, saved.a);
  TEST_ASSERT_EQUAL_UINT16(0x0123, saved.b);
  TEST_ASSERT_EQUAL_UINT32(TEST_EPOCH, saved.epoch);
  TEST_ASSERT_EQUAL_UINT8(ST_PRESENT, flagStatus(saved.flags));
  TEST_ASSERT_EQUAL_UINT8(0, flagSession(saved.flags));
  TEST_ASSERT_TRUE(f.admission.alreadyScanned(TEST_DAY, 0, 4));
}


void test_configured_term_end_day_is_inclusive_for_scans() {
  Fixture f(TEST_DAY);
  TEST_ASSERT_TRUE(f.ready());
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_SAVED_PRESENT,
      finishMatch(f.pipeline, f.stream, 7, 321));
  Event saved;
  TEST_ASSERT_TRUE(f.log.at(5, saved));
  TEST_ASSERT_EQUAL_UINT8(EV_SCAN, saved.type);
  TEST_ASSERT_EQUAL_UINT16(TEST_DAY, saved.b);
  const size_t afterFinalDayScan = f.log.count();
  TEST_ASSERT_FALSE(f.pipeline.begin(TEST_EPOCH + 86400u));
  TEST_ASSERT_EQUAL_UINT32(afterFinalDayScan, f.log.count());
}

void test_late_match_is_committed_as_late() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  const uint32_t lateEpoch = TEST_EPOCH + 40u * 60u;
  TEST_ASSERT_TRUE(f.pipeline.begin(lateEpoch));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_SAVED_LATE,
      finishMatch(f.pipeline, f.stream, 7, 222));
  Event saved;
  TEST_ASSERT_TRUE(f.log.at(5, saved));
  TEST_ASSERT_EQUAL_UINT8(ST_LATE, flagStatus(saved.flags));
  TEST_ASSERT_EQUAL_UINT32(lateEpoch, saved.epoch);
}

void test_duplicate_scan_does_not_append_a_second_attendance_event() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_SAVED_PRESENT, finishMatch(f.pipeline, f.stream, 7, 100));
  const size_t countAfterFirst = f.log.count();
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH + 60));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_ALREADY_RECORDED,
      finishMatch(f.pipeline, f.stream, 7, 110));
  TEST_ASSERT_EQUAL_UINT32(countAfterFirst, f.log.count());
}

void test_unresolved_page_mapping_fails_without_creating_a_scan() {
  MemStorage storage; LogStore log(storage); RosterStore roster(480);
  Config config = validConfig();
  TEST_ASSERT_TRUE(log.open().ok); TEST_ASSERT_TRUE(log.attach(&roster));
  TEST_ASSERT_TRUE(addStudent(log, roster, 4));
  LogAttendanceAdmission admission(log, config.params);
  FakeStream stream; Occupancy occupancy;
  As608Runtime sensorRuntime(stream, roster, occupancy, 0);
  As608StartupCoordinator startup(sensorRuntime);
  FixedPageResolver unresolved(7, 4, false);
  As608AttendancePipeline pipeline(sensorRuntime, log, roster, config, unresolved, admission);
  TEST_ASSERT_TRUE(setupRuntime(startup, stream, true));
  TEST_ASSERT_TRUE(pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_NOT_ACTIVE, finishMatch(pipeline, stream, 7, 90));
  TEST_ASSERT_EQUAL_UINT32(5, log.count());
  TEST_ASSERT_FALSE(admission.alreadyScanned(TEST_DAY, 0, 4));
}

void test_log_write_failure_reports_uncertain_commit() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  f.storage.failAppend(true);
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_COMMIT_UNCERTAIN, finishMatch(f.pipeline, f.stream, 7, 120));
  TEST_ASSERT_TRUE(f.log.faulted());
  TEST_ASSERT_FALSE(f.pipeline.active()); TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
  TEST_ASSERT_FALSE(f.admission.allowScan(TEST_DAY, 0));
}

void test_complete_attendance_write_reported_failed_is_not_duplicated_after_reopen() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  f.storage.appendAllThenFailOnce();
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_COMMIT_UNCERTAIN,
      finishMatch(f.pipeline, f.stream, 7, 120));
  TEST_ASSERT_TRUE(f.log.faulted());
  TEST_ASSERT_EQUAL_UINT32(5, f.log.count());  // failed instance must not claim the event

  // Simulate restart/reconstruction over the exact same bytes. A new LogStore
  // replays the complete record that the failed append may have persisted.
  LogStore recovered(f.storage);
  OpenInfo opened = recovered.open();
  TEST_ASSERT_TRUE(opened.ok);
  TEST_ASSERT_EQUAL_UINT32(6, opened.records);
  TEST_ASSERT_EQUAL_UINT32(6, opened.lastSeq);
  RosterStore recoveredRoster(480);
  TEST_ASSERT_TRUE(recovered.attach(&recoveredRoster));
  LogAttendanceAdmission recoveredAdmission(recovered, f.config.params);
  TEST_ASSERT_TRUE(recoveredAdmission.attached());
  TEST_ASSERT_TRUE(recoveredAdmission.alreadyScanned(TEST_DAY, 0, 4));

  // A subsequent physical retry for the same student/session must be rejected
  // by the replayed first-scan-wins admission policy, without another log append.
  FakeStream retryStream;
  Occupancy retryOccupancy;
  As608Runtime retryRuntime(retryStream, recoveredRoster, retryOccupancy, 0);
  As608StartupCoordinator retryStartup(retryRuntime);
  FixedPageResolver retryPages(7, 4);
  As608AttendancePipeline retryPipeline(retryRuntime, recovered, recoveredRoster,
      f.config, retryPages, recoveredAdmission);
  TEST_ASSERT_TRUE(setupRuntime(retryStartup, retryStream, true));
  TEST_ASSERT_TRUE(retryPipeline.begin(TEST_EPOCH + 60u));
  const size_t countBeforeRetry = recovered.count();
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_ALREADY_RECORDED,
      finishMatch(retryPipeline, retryStream, 7, 121));
  TEST_ASSERT_EQUAL_UINT32(countBeforeRetry, recovered.count());
  TEST_ASSERT_FALSE(recovered.faulted());
}

void test_pipeline_fails_closed_when_roster_history_is_incomplete() {
  {
    Fixture f; TEST_ASSERT_TRUE(f.ready());
    f.roster.onRecord(Event{EV_ROSTER, FL_MORE, 4, TEST_EPOCH, 0, RA_SET});
    f.roster.onRecord(makePayload(0x0000000000000041ull, 0, true));
    TEST_ASSERT_EQUAL_UINT32(0, f.roster.malformed());
    TEST_ASSERT_FALSE(f.roster.historyComplete());
    TEST_ASSERT_FALSE(f.pipeline.begin(TEST_EPOCH));
    TEST_ASSERT_FALSE(f.pipeline.active());
    TEST_ASSERT_EQUAL_UINT32(5, f.log.count());
  }
  {
    Fixture f; TEST_ASSERT_TRUE(f.ready());
    TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
    TEST_ASSERT_EQUAL_UINT8(0x01, f.stream.instruction());
    f.stream.ackWith(0x00);
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
    TEST_ASSERT_EQUAL_UINT8(0x02, f.stream.instruction());
    f.stream.ackWith(0x00);
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
    TEST_ASSERT_EQUAL_UINT8(0x04, f.stream.instruction());

    Event malformed = {EV_ROSTER, 0, 4, TEST_EPOCH, 0, 99};
    TEST_ASSERT_TRUE(f.log.append(&malformed, 1));
    TEST_ASSERT_FALSE(f.roster.historyComplete());
    const uint8_t match[4] = {0, 7, 0, 99};
    f.stream.ackWith(0x00, match, sizeof match);
    TEST_ASSERT_EQUAL_INT(ATT_PIPE_REJECTED, f.pipeline.step());
    TEST_ASSERT_FALSE(f.pipeline.active());
    TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
    TEST_ASSERT_EQUAL_UINT32(6, f.log.count());  // malformed record only; no EV_SCAN
  }
}

void test_pipeline_refuses_scan_when_occupancy_is_not_valid() {
  Fixture f;
  TEST_ASSERT_TRUE(f.ready(false));
  TEST_ASSERT_FALSE(f.occupancy.valid);
  TEST_ASSERT_TRUE(f.sensorRuntime.attendance().ready());
  const uint8_t lastInstruction = f.stream.instruction();
  TEST_ASSERT_FALSE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_FALSE(f.pipeline.active());
  TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
  TEST_ASSERT_EQUAL_UINT8(lastInstruction, f.stream.instruction());
  TEST_ASSERT_EQUAL_UINT32(5, f.log.count());
}

void test_pipeline_rechecks_occupancy_before_committing_a_match() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_UINT8(0x01, f.stream.instruction());
  f.stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_UINT8(0x02, f.stream.instruction());
  f.stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_UINT8(0x04, f.stream.instruction());
  f.occupancy.begin();  // invalidate after scan start, before durable event append
  const uint8_t match[4] = {0, 7, 0, 99};
  f.stream.ackWith(0x00, match, sizeof match);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_REJECTED, f.pipeline.step());
  TEST_ASSERT_FALSE(f.pipeline.active());
  TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
  TEST_ASSERT_EQUAL_UINT32(5, f.log.count());
}

void test_no_finger_result_is_propagated_and_sensor_lease_released() {
  Fixture f; TEST_ASSERT_TRUE(f.ready());
  TEST_ASSERT_TRUE(f.pipeline.begin(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, f.pipeline.step());
  TEST_ASSERT_EQUAL_UINT8(0x01, f.stream.instruction());
  f.stream.ackWith(0x02);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_NO_FINGER, f.pipeline.step());
  TEST_ASSERT_FALSE(f.pipeline.active()); TEST_ASSERT_FALSE(f.sensorRuntime.attendance().held());
}

namespace {
bool finishOwnedOwnerStartup(As608AttendanceOwner& owner, FakeStream& stream) {
  if (owner.stepStartup() != AS608_STARTUP_PENDING || stream.instruction() != 0x13)
    return false;
  stream.ackWith(0x00);
  if (owner.stepStartup() != AS608_STARTUP_PENDING) return false;
  if (owner.stepStartup() != AS608_STARTUP_PENDING || stream.instruction() != 0x0F)
    return false;
  uint8_t params[16] = {0}; params[5] = 45;
  stream.ackWith(0x00, params, sizeof params);
  if (owner.stepStartup() != AS608_STARTUP_PENDING) return false;
  if (owner.stepStartup() != AS608_STARTUP_PENDING || stream.instruction() != 0x1F)
    return false;
  uint8_t index[32] = {0}; index[0] = 0x08;  // synthetic student ID 4 template
  stream.ackWith(0x00, index, sizeof index);
  if (owner.stepStartup() != AS608_STARTUP_PENDING) return false;
  if (owner.stepStartup() != AS608_STARTUP_PENDING) return false;
  if (owner.stepStartup() != AS608_STARTUP_PENDING || stream.instruction() != 0x13)
    return false;
  stream.ackWith(0x00);
  if (owner.stepStartup() != AS608_STARTUP_PENDING) return false;
  if (owner.stepStartup() != AS608_STARTUP_PENDING || stream.instruction() != 0x0F)
    return false;
  stream.ackWith(0x00, params, sizeof params);
  return owner.stepStartup() == AS608_STARTUP_READY && owner.startupReady();
}

AttendancePipelineResult finishOwnedOwnerMatch(As608AttendanceOwner& owner,
    FakeStream& stream, uint16_t page, uint16_t score) {
  if (owner.stepScan() != ATT_PIPE_PENDING || stream.instruction() != 0x01)
    return ATT_PIPE_REJECTED;
  stream.ackWith(0x00);
  if (owner.stepScan() != ATT_PIPE_PENDING) return ATT_PIPE_REJECTED;
  if (owner.stepScan() != ATT_PIPE_PENDING || stream.instruction() != 0x02)
    return ATT_PIPE_REJECTED;
  stream.ackWith(0x00);
  if (owner.stepScan() != ATT_PIPE_PENDING) return ATT_PIPE_REJECTED;
  if (owner.stepScan() != ATT_PIPE_PENDING || stream.instruction() != 0x04)
    return ATT_PIPE_REJECTED;
  const uint8_t match[4] = {static_cast<uint8_t>(page >> 8), static_cast<uint8_t>(page),
      static_cast<uint8_t>(score >> 8), static_cast<uint8_t>(score)};
  stream.ackWith(0x00, match, sizeof match);
  return owner.stepScan();
}
}

void test_attendance_owner_remains_closed_after_startup_authentication_fault() {
  GateFixture app;
  TEST_ASSERT_TRUE(app.built);
  FakeStream stream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner owner(stream, *app.log, *app.roster,
                             app.config, pages, *app.gate, 0);

  TEST_ASSERT_TRUE(owner.beginStartup());
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_PENDING, owner.stepStartup());
  TEST_ASSERT_EQUAL_UINT8(0x13, stream.instruction());
  stream.ackWith(0x13);  // wrong password: startup must fail closed
  TEST_ASSERT_EQUAL_INT(AS608_STARTUP_FAULT, owner.stepStartup());
  TEST_ASSERT_TRUE(owner.startupFaulted());
  TEST_ASSERT_FALSE(owner.startupReady());
  TEST_ASSERT_FALSE(owner.occupancy().valid);
  TEST_ASSERT_FALSE(owner.beginScan(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_REJECTED, owner.stepScan());
  TEST_ASSERT_FALSE(owner.beginStartup());  // failed owner is terminal
}

void test_attendance_owner_hides_raw_sensor_and_commits_only_through_pipeline() {
  GateFixture app;
  TEST_ASSERT_TRUE(app.built);
  TEST_ASSERT_TRUE(addStudent(*app.log, *app.roster, 4));
  FakeStream stream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner owner(stream, *app.log, *app.roster,
                             app.config, pages, *app.gate, 0);

  TEST_ASSERT_FALSE(owner.beginScan(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_REJECTED, owner.stepScan());
  TEST_ASSERT_TRUE(owner.beginStartup());
  TEST_ASSERT_FALSE(owner.beginScan(TEST_EPOCH));
  TEST_ASSERT_TRUE(finishOwnedOwnerStartup(owner, stream));
  TEST_ASSERT_TRUE(owner.startupReady());
  TEST_ASSERT_FALSE(owner.startupFaulted());
  TEST_ASSERT_EQUAL_UINT16(45, owner.sensorCapacity());
  TEST_ASSERT_TRUE(owner.occupancy().valid);
  TEST_ASSERT_TRUE(owner.occupancy().has(4));

  // The only health fault was the sensor's pre-startup state. Clear it only
  // after the complete occupancy/capacity startup has succeeded.
  app.setHealth(true, true, true);
  TEST_ASSERT_TRUE(app.gate->readyForAttendance());
  TEST_ASSERT_TRUE(owner.beginScan(TEST_EPOCH));
  TEST_ASSERT_TRUE(owner.scanActive());
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_SAVED_PRESENT,
      finishOwnedOwnerMatch(owner, stream, 7, 0x0456));
  TEST_ASSERT_FALSE(owner.scanActive());
  TEST_ASSERT_EQUAL_UINT32(6, app.log->count());
  Event saved;
  TEST_ASSERT_TRUE(app.log->at(5, saved));
  TEST_ASSERT_EQUAL_UINT8(EV_SCAN, saved.type);
  TEST_ASSERT_EQUAL_UINT16(4, saved.a);
  TEST_ASSERT_EQUAL_UINT16(0x0456, saved.b);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_IDLE, owner.stepScan());
}

void test_owner_config_runtime_gates_until_ready_blocks_active_scan_and_updates_schedule() {
  GateFixture app;
  TEST_ASSERT_TRUE(app.built);
  FakeStream stream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner owner(stream, *app.log, *app.roster,
                             app.config, pages, *app.gate, 0);
  As608OwnerConfigRuntime configRuntime(owner, *app.closure);
  TestConfigSlots slots;
  TEST_ASSERT_TRUE(slots.seed(app.config, 7));
  const CfgPick selected = {CB_OK, 0, 7};
  ConfigApplyCoordinator coordinator(slots, configRuntime, app.config, selected);

  Config next = app.config;
  for (int day = 0; day < 5; ++day)
    for (int session = 0; session < 2; ++session)
      next.tt.day[day][session].enabled = false;
  TEST_ASSERT_TRUE(validateConfig(next));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_BUSY, coordinator.apply(next, configGate()).code);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());  // sensor health not ready: no durable mutation

  TEST_ASSERT_TRUE(owner.beginStartup());
  TEST_ASSERT_TRUE(finishOwnedOwnerStartup(owner, stream));
  app.setHealth(true, true, true);
  TEST_ASSERT_TRUE(app.closure->onTick(TEST_EPOCH));  // original timetable is active

  // RosterStore derives effective days using its immutable boot UTC offset.
  // A timezone change must not be persisted until a safe roster replay exists.
  Config changedZone = app.config;
  changedZone.params.utcOffsetMin = 0;
  TEST_ASSERT_TRUE(validateConfig(changedZone));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_BUSY,
      coordinator.apply(changedZone, configGate()).code);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());

  TEST_ASSERT_TRUE(owner.beginScan(TEST_EPOCH));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_BUSY, coordinator.apply(next, configGate()).code);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());  // don't persist a change mid-scan
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());
  stream.ackWith(0x02);  // no finger; release the scan lease
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_NO_FINGER, owner.stepScan());

  ConfigApplyResult applied = coordinator.apply(next, configGate());
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_OK, applied.code);
  TEST_ASSERT_EQUAL_UINT32(8, coordinator.generation());
  TEST_ASSERT_EQUAL_UINT32(1, slots.writes());
  TEST_ASSERT_FALSE(app.closure->onTick(TEST_EPOCH + 600));  // closure uses the new disabled timetable
  TEST_ASSERT_FALSE(owner.beginScan(TEST_EPOCH));             // pipeline uses the same new timetable

  TEST_ASSERT_EQUAL_INT(CFG_APPLY_UNCHANGED,
      coordinator.apply(next, configGate(8)).code);
  TEST_ASSERT_EQUAL_UINT32(1, slots.writes());  // unchanged config does not rewrite a slot
}

void test_owner_config_runtime_rejects_unbound_closure() {
  GateFixture app;
  GateFixture other;
  TEST_ASSERT_TRUE(app.built);
  TEST_ASSERT_TRUE(other.built);

  FakeStream stream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner owner(stream, *app.log, *app.roster,
                             app.config, pages, *app.gate, 0);
  TEST_ASSERT_TRUE(owner.beginStartup());
  TEST_ASSERT_TRUE(finishOwnedOwnerStartup(owner, stream));
  app.setHealth(true, true, true);

  // A different, otherwise-ready closure must not receive config changes for
  // the attendance owner whose admission gate is bound to app.closure.
  As608OwnerConfigRuntime wrongRuntime(owner, *other.closure);
  TestConfigSlots slots;
  TEST_ASSERT_TRUE(slots.seed(app.config, 7));
  const CfgPick selected = {CB_OK, 0, 7};
  ConfigApplyCoordinator coordinator(slots, wrongRuntime, app.config, selected);

  Config next = app.config;
  for (int day = 0; day < 5; ++day)
    for (int session = 0; session < 2; ++session)
      next.tt.day[day][session].enabled = false;
  TEST_ASSERT_TRUE(validateConfig(next));
  TEST_ASSERT_EQUAL_INT(CFG_APPLY_BUSY,
      coordinator.apply(next, configGate()).code);
  TEST_ASSERT_EQUAL_UINT32(0, slots.writes());
}

void test_application_gate_rechecks_clock_health_before_match_commit() {
  GateFixture app;
  TEST_ASSERT_TRUE(app.built);
  TEST_ASSERT_TRUE(addStudent(*app.log, *app.roster, 4));
  FakeStream stream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner owner(stream, *app.log, *app.roster,
                             app.config, pages, *app.gate, 0);
  TEST_ASSERT_TRUE(app.gate->readyForSensorStartup());
  TEST_ASSERT_FALSE(app.gate->readyForAttendance());
  TEST_ASSERT_TRUE(owner.beginStartup());
  TEST_ASSERT_TRUE(finishOwnedOwnerStartup(owner, stream));
  app.setHealth(true, true, true);
  TEST_ASSERT_TRUE(owner.beginScan(TEST_EPOCH));

  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());
  stream.ackWith(0x00);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_PENDING, owner.stepScan());

  // The clock becomes untrusted while the sensor transaction is outstanding.
  // Drain the sensor response, but do not turn that match into an EV_SCAN commit.
  app.setHealth(true, false, true);
  const uint8_t match[4] = {0, 7, 0, 99};
  stream.ackWith(0x00, match, sizeof match);
  TEST_ASSERT_EQUAL_INT(ATT_PIPE_REJECTED, owner.stepScan());
  TEST_ASSERT_FALSE(owner.scanActive());
  TEST_ASSERT_FALSE(app.log->faulted());
  TEST_ASSERT_EQUAL_UINT32(5, app.log->count());
}

void test_application_gate_rejects_incomplete_boot_or_mismatched_owner_graph() {
  GateFixture setupRequired(false);
  TEST_ASSERT_TRUE(setupRequired.boot.needsSetup());

  GateFixture ready;
  GateFixture other;
  TEST_ASSERT_TRUE(ready.built);
  TEST_ASSERT_TRUE(other.built);
  TEST_ASSERT_TRUE(ready.gate->matches(*ready.log, *ready.roster));

  // Reuse an already-built dependency graph only as a test fixture; the
  // NEEDS_SETUP coordinator itself has constructed no runtime services.
  ApplicationAttendanceGate setupGate(setupRequired.boot, *ready.storage,
      *ready.months, *ready.log, *ready.roster, *ready.admission,
      *ready.closure, ready.health);
  TEST_ASSERT_FALSE(setupGate.readyForSensorStartup());
  TEST_ASSERT_FALSE(setupGate.readyForAttendance());

  TEST_ASSERT_FALSE(ready.gate->matches(*other.log, *other.roster));
  FakeStream wrongGraphStream;
  FixedPageResolver pages(7, 4);
  As608AttendanceOwner wrongGraphOwner(wrongGraphStream, *other.log,
      *other.roster, ready.config, pages, *ready.gate, 0);
  TEST_ASSERT_FALSE(wrongGraphOwner.beginStartup());

  Config mismatchedConfig = ready.config;
  mismatchedConfig.params.utcOffsetMin = 0;
  TEST_ASSERT_TRUE(validateConfig(mismatchedConfig));
  FakeStream wrongConfigStream;
  As608AttendanceOwner wrongConfigOwner(wrongConfigStream, *ready.log,
      *ready.roster, mismatchedConfig, pages, *ready.gate, 0);
  TEST_ASSERT_FALSE(wrongConfigOwner.beginStartup());

  ready.setHealth(true, true, true);
  TEST_ASSERT_TRUE(ready.gate->readyForAttendance());
  ready.setHealth(true, false, true);
  TEST_ASSERT_FALSE(ready.gate->readyForSensorStartup());
  TEST_ASSERT_FALSE(ready.gate->readyForAttendance());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_owner_config_runtime_gates_until_ready_blocks_active_scan_and_updates_schedule);
  RUN_TEST(test_owner_config_runtime_rejects_unbound_closure);
  RUN_TEST(test_attendance_owner_remains_closed_after_startup_authentication_fault);
  RUN_TEST(test_attendance_owner_hides_raw_sensor_and_commits_only_through_pipeline);
  RUN_TEST(test_application_gate_rechecks_clock_health_before_match_commit);
  RUN_TEST(test_application_gate_rejects_incomplete_boot_or_mismatched_owner_graph);
  RUN_TEST(test_match_is_resolved_checked_and_committed_once);
  RUN_TEST(test_configured_term_end_day_is_inclusive_for_scans);
  RUN_TEST(test_late_match_is_committed_as_late);
  RUN_TEST(test_duplicate_scan_does_not_append_a_second_attendance_event);
  RUN_TEST(test_unresolved_page_mapping_fails_without_creating_a_scan);
  RUN_TEST(test_log_write_failure_reports_uncertain_commit);
  RUN_TEST(test_complete_attendance_write_reported_failed_is_not_duplicated_after_reopen);
  RUN_TEST(test_pipeline_fails_closed_when_roster_history_is_incomplete);
  RUN_TEST(test_pipeline_refuses_scan_when_occupancy_is_not_valid);
  RUN_TEST(test_pipeline_rechecks_occupancy_before_committing_a_match);
  RUN_TEST(test_no_finger_result_is_propagated_and_sensor_lease_released);
  return UNITY_END();
}
