#pragma once

#include "backup_format.h"
#include "closure_engine.h"
#include "roster_store.h"
#include "event_group_audit.h"

namespace att {

enum RestoreReplayCode : uint8_t {
  RESTORE_REPLAY_OK,
  RESTORE_REPLAY_NOT_VALIDATED,
  RESTORE_REPLAY_STAGE_SIZE_FAILED,
  RESTORE_REPLAY_SIZE_MISMATCH,
  RESTORE_REPLAY_STAGE_READ_FAILED,
  RESTORE_REPLAY_STAGE_CHANGED,
  RESTORE_REPLAY_STORAGE_MISMATCH,
  RESTORE_REPLAY_ATTACH_FAILED,
  RESTORE_REPLAY_LOG_INVALID,
  RESTORE_REPLAY_RECORD_MISMATCH,
  RESTORE_REPLAY_GROUP_MALFORMED,
  RESTORE_REPLAY_ROSTER_MALFORMED,
  RESTORE_REPLAY_ROSTER_OVERFLOW,
  RESTORE_REPLAY_GAP_OVERFLOW,
  RESTORE_REPLAY_CLOSE_OVERFLOW
};

struct RestoreReplayResult {
  RestoreReplayCode code;
  uint32_t records;
  uint32_t lastSeq;
  uint32_t malformedRoster;
  bool rosterOverflow;
  bool gapOverflow;
  bool closeOverflow;
};

typedef EventGroupAudit RestoreGroupAudit;

// Construct only after validator.ok(): this object snapshots the validated Config.
// Replays a successfully authenticated RestoreValidator staging stream through fresh
// production observers. This validates roster semantics and rebuilds the LogIndex
// owned by ClosureEngine. It does not modify the live log and does not perform a
// storage swap. The object is intentionally one-shot because LogStore::open() may
// truncate a bad staged tail while reporting the replay result. The staged record CRC
// is compared with the bytes accepted by RestoreValidator before replay, detecting
// accidental mutation between validation and audit (not an adversarial MAC).
//
// Large objects: RosterStore and ClosureEngine contain fixed-size tables. Allocate
// this coordinator statically or on the heap, never as a local on a small task stack.
class RestoreReplayAudit {
 public:
  RestoreReplayAudit(IStorage& staged, const RestoreValidator& validator);
  RestoreReplayAudit(const RestoreReplayAudit&) = delete;
  RestoreReplayAudit& operator=(const RestoreReplayAudit&) = delete;

  RestoreReplayResult run();
  bool ran() const { return ran_; }
  const RosterStore& roster() const { return roster_; }
  const LogIndex& index() const { return closure_.index(); }

 private:
  RestoreReplayResult finish_(RestoreReplayCode code, const OpenInfo* opened = nullptr);

  IStorage& staged_;
  const RestoreValidator& validator_;
  Config config_;
  LogStore replayLog_;
  RosterStore roster_;
  ClosureEngine closure_;
  RestoreGroupAudit groups_;
  bool validatedAtConstruction_;
  bool ran_;
  RestoreReplayResult result_;
};

}  // namespace att
