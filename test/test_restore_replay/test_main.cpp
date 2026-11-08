#include <unity.h>
#include <string.h>
#include <vector>
#include "restore_replay.h"

using namespace att;

namespace {
struct MemStore : IStorage {
  std::vector<uint8_t> bytes;
  bool failReads;
  MemStore() : failReads(false) {}
  size_t size() override { return bytes.size(); }
  bool read(size_t offset, uint8_t* out, size_t n) override {
    if (failReads || !out || offset > bytes.size() || n > bytes.size() - offset) return false;
    memcpy(out, bytes.data() + offset, n);
    return true;
  }
  bool append(const uint8_t* data, size_t n) override {
    if (!data && n) return false;
    bytes.insert(bytes.end(), data, data + n);
    return true;
  }
  bool truncate(size_t n) override {
    if (n > bytes.size()) return false;
    bytes.resize(n);
    return true;
  }
};

struct Buffer : IByteSink {
  std::vector<uint8_t> bytes;
  bool write(const uint8_t* data, size_t n) override {
    if (!data && n) return false;
    bytes.insert(bytes.end(), data, data + n);
    return true;
  }
};

Config validConfig() {
  Config c = defaultConfig();
  c.termStart = 100;
  c.termEnd = 400;
  c.pinSet = 1;
  memset(c.pinHash, 0xA5, sizeof c.pinHash);
  memset(c.pinSalt, 0x5A, sizeof c.pinSalt);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}

uint32_t rosterEpoch() {
  return dayStartEpoch(100, 480) + 12u * 3600u;
}

bool makeStagedRestore(RestoreValidator& validator,
                       bool invalidRosterName = false,
                       bool deactivateUnknown = false,
                       bool orphanPayload = false,
                       bool badCloseCount = false,
                       bool overflowingGap = false,
                       bool discontinuousGap = false) {
  MemStore source;
  LogStore log(source);
  if (!log.open().ok) return false;

  if (orphanPayload) {
    Event payload = makePayload(0x12345678u, 0, false);
    if (!log.append(&payload, 1)) return false;
  } else if (deactivateUnknown) {
    Event e = {EV_ROSTER, 0, 99, rosterEpoch(), 0, RA_DEACTIVATE};
    if (!log.append(&e, 1)) return false;
  } else {
    uint8_t name[ROSTER_NAME_MAX] = {0};
    name[0] = 'A'; name[1] = 'l'; name[2] = 'i'; name[3] = 'c'; name[4] = 'e';
    if (invalidRosterName) name[1] = 0x01;
    Event set[5];
    set[0] = Event{EV_ROSTER, 0, 1, rosterEpoch(), 0, RA_SET};
    for (uint8_t word = 0; word < 4; ++word) {
      uint64_t bits = 0;
      for (uint8_t i = 0; i < 8; ++i)
        bits |= (uint64_t)name[word * 8 + i] << (8u * i);
      set[word + 1] = makePayload(bits, 0, false);
    }
    if (!log.append(set, 5)) return false;
  }

  if (!orphanPayload) {
    const uint32_t gapStart = overflowingGap ? UINT32_MAX - 10u : rosterEpoch() - 600u;
    if (discontinuousGap) {
      Event gaps[2] = {
          Event{EV_GAP, 0, GAP_POWER, gapStart, 0, 5},
          Event{EV_GAP, 0, GAP_POWER, gapStart + 6u * 60u, 0, 5}};
      if (!log.append(gaps, 2)) return false;
    } else {
      Event gap = {EV_GAP, 0, GAP_POWER, gapStart, 0, 5};
      if (!log.append(&gap, 1)) return false;
    }

    uint64_t words[5] = {deactivateUnknown ? 0u : 1u, 0, 0, 0, 0};
    Event close[6];
    close[0] = Event{EV_CLOSE, 0,
                     (uint16_t)(badCloseCount ? 2 : (deactivateUnknown ? 0 : 1)),
                     rosterEpoch() + 5u * 3600u, 0, 100};
    for (uint8_t i = 0; i < 5; ++i) close[i + 1] = makePayload(words[i], 0, false);
    if (!log.append(close, 6)) return false;
  }

  Buffer inner;
  InnerBuilder builder(inner);
  const Config config = validConfig();
  const uint32_t count = (uint32_t)(source.bytes.size() / REC_SIZE);
  if (!builder.begin(config, 4, 3, count) ||
      !builder.records(source.bytes.data(), source.bytes.size()) || !builder.finish())
    return false;
  return validator.write(inner.bytes.data(), inner.bytes.size());
}
}

void setUp() {}
void tearDown() {}

void test_replay_audit_rebuilds_roster_gap_and_close_index() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());

  RestoreReplayAudit audit(staging, validator);
  RestoreReplayResult result = audit.run();
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_OK, result.code);
  TEST_ASSERT_EQUAL_UINT32(12, result.records);
  TEST_ASSERT_EQUAL_UINT32(12, result.lastSeq);
  TEST_ASSERT_EQUAL_UINT32(0, result.malformedRoster);
  TEST_ASSERT_FALSE(result.rosterOverflow);
  TEST_ASSERT_EQUAL_UINT32(1, audit.roster().knownCount());
  TEST_ASSERT_TRUE(audit.roster().active(1));
  TEST_ASSERT_EQUAL_UINT32(1, audit.index().gapCount());
  TEST_ASSERT_EQUAL_UINT32(1, audit.index().closeCount());
}

void test_replay_audit_rejects_unfinished_or_failed_validator() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_NOT_VALIDATED, audit.run().code);
}


void test_replay_audit_must_be_constructed_after_validation() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  RestoreReplayAudit audit(staging, validator);  // captures default config too early
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_NOT_VALIDATED, audit.run().code);
}

void test_replay_audit_requires_same_staging_storage() {
  MemStore actualStage, unrelatedStage;
  RestoreValidator validator(&actualStage, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(unrelatedStage, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_STORAGE_MISMATCH, audit.run().code);
}

void test_replay_audit_rejects_malformed_roster_name() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, true, false));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  RestoreReplayResult result = audit.run();
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_ROSTER_MALFORMED, result.code);
  TEST_ASSERT_TRUE(result.malformedRoster > 0);
}

void test_replay_audit_rejects_roster_operation_for_unknown_id() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, false, true));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  RestoreReplayResult result = audit.run();
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_ROSTER_MALFORMED, result.code);
  TEST_ASSERT_TRUE(result.malformedRoster > 0);
}

void test_replay_audit_rejects_post_validation_stage_corruption() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());
  staging.bytes[REC_SIZE - 1] ^= 0x01;
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_STAGE_CHANGED, audit.run().code);
}



void test_replay_audit_rejects_staging_read_failure() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());
  staging.failReads = true;
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_STAGE_READ_FAILED, audit.run().code);
}

void test_replay_audit_rejects_orphan_payload_group() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, false, false, true));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_GROUP_MALFORMED, audit.run().code);
}

void test_replay_audit_detects_valid_record_mutation_after_validation() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());

  Event changed;
  uint8_t encoded[REC_SIZE];
  TEST_ASSERT_TRUE(decode(staging.bytes.data(), changed));
  changed.epoch += 60u;
  encode(changed, encoded);  // keep the per-record CRC valid after changing the event
  memcpy(staging.bytes.data(), encoded, REC_SIZE);

  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_STAGE_CHANGED, audit.run().code);
}


void test_replay_audit_rejects_close_bitmap_count_mismatch() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, false, false, false, true));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_GROUP_MALFORMED, audit.run().code);
}

void test_replay_audit_rejects_gap_epoch_overflow() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, false, false, false, false, true));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_GROUP_MALFORMED, audit.run().code);
}


void test_replay_audit_rejects_discontinuous_gap_chunks() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator, false, false, false,
                                     false, false, true));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_GROUP_MALFORMED, audit.run().code);
}

void test_replay_audit_is_one_shot_and_does_not_reopen_stage() {
  MemStore staging;
  RestoreValidator validator(&staging, 3);
  TEST_ASSERT_TRUE(makeStagedRestore(validator));
  TEST_ASSERT_TRUE(validator.ok());
  RestoreReplayAudit audit(staging, validator);
  RestoreReplayResult first = audit.run();
  staging.bytes.clear();
  RestoreReplayResult second = audit.run();
  TEST_ASSERT_EQUAL_INT(RESTORE_REPLAY_OK, first.code);
  TEST_ASSERT_EQUAL_INT(first.code, second.code);
  TEST_ASSERT_EQUAL_UINT32(first.records, second.records);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_replay_audit_rebuilds_roster_gap_and_close_index);
  RUN_TEST(test_replay_audit_rejects_unfinished_or_failed_validator);
  RUN_TEST(test_replay_audit_must_be_constructed_after_validation);
  RUN_TEST(test_replay_audit_requires_same_staging_storage);
  RUN_TEST(test_replay_audit_rejects_malformed_roster_name);
  RUN_TEST(test_replay_audit_rejects_roster_operation_for_unknown_id);
  RUN_TEST(test_replay_audit_rejects_post_validation_stage_corruption);
  RUN_TEST(test_replay_audit_rejects_staging_read_failure);
  RUN_TEST(test_replay_audit_rejects_orphan_payload_group);
  RUN_TEST(test_replay_audit_detects_valid_record_mutation_after_validation);
  RUN_TEST(test_replay_audit_rejects_close_bitmap_count_mismatch);
  RUN_TEST(test_replay_audit_rejects_gap_epoch_overflow);
  RUN_TEST(test_replay_audit_rejects_discontinuous_gap_chunks);
  RUN_TEST(test_replay_audit_is_one_shot_and_does_not_reopen_stage);
  return UNITY_END();
}
