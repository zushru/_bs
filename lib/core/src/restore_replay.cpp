#include "restore_replay.h"

namespace att {

RestoreReplayAudit::RestoreReplayAudit(IStorage& staged,
                                       const RestoreValidator& validator)
    : staged_(staged), validator_(validator), config_(validator.config()),
      replayLog_(staged_), roster_(config_.params.utcOffsetMin),
      closure_(replayLog_, config_.params, config_.tt, roster_, MAX_ID,
               config_.termStart),
      groups_(), validatedAtConstruction_(validator.ok()), ran_(false),
      result_{RESTORE_REPLAY_NOT_VALIDATED, 0, 0, 0, false, false, false} {}

RestoreReplayResult RestoreReplayAudit::finish_(RestoreReplayCode code,
                                                const OpenInfo* opened) {
  result_.code = code;
  result_.malformedRoster = roster_.malformed();
  result_.rosterOverflow = roster_.overflow();
  result_.gapOverflow = closure_.index().gapOverflow();
  result_.closeOverflow = closure_.index().closeOverflow();
  if (opened) {
    result_.records = opened->records;
    result_.lastSeq = opened->lastSeq;
  }
  return result_;
}

RestoreReplayResult RestoreReplayAudit::run() {
  if (ran_) return result_;
  ran_ = true;

  if (!validatedAtConstruction_ || !validator_.ok())
    return finish_(RESTORE_REPLAY_NOT_VALIDATED);
  if (validator_.stagingStorage() != &staged_)
    return finish_(RESTORE_REPLAY_STORAGE_MISMATCH);
  if ((uint64_t)validator_.records() > (uint64_t)SIZE_MAX / REC_SIZE ||
      staged_.size() != (size_t)validator_.records() * REC_SIZE)
    return finish_(RESTORE_REPLAY_SIZE_MISMATCH);

  // Detect changes to the staged bytes after authenticated validation. This CRC is
  // an integrity cross-check against accidental mutation, not a cryptographic MAC.
  Crc32 stagedRecordsCrc;
  uint8_t record[REC_SIZE];
  for (uint32_t i = 0; i < validator_.records(); ++i) {
    if (!staged_.read((size_t)i * REC_SIZE, record, REC_SIZE))
      return finish_(RESTORE_REPLAY_STAGE_READ_FAILED);
    stagedRecordsCrc.add(record, REC_SIZE);
  }
  if (stagedRecordsCrc.value() != validator_.recordsCrc())
    return finish_(RESTORE_REPLAY_STAGE_CHANGED);

  // ClosureEngine attaches its LogIndex in its constructor. Attach the fresh roster
  // before open(), so both receive the same reset and complete ordered replay.
  if (!replayLog_.attach(&roster_) || !replayLog_.attach(&groups_))
    return finish_(RESTORE_REPLAY_ATTACH_FAILED);

  const OpenInfo opened = replayLog_.open();
  if (!opened.ok || opened.corruptAt >= 0 || opened.tornBytes != 0 ||
      opened.droppedGroup != 0)
    return finish_(RESTORE_REPLAY_LOG_INVALID, &opened);

  // The validator's authenticated record count/sequence are authoritative. LogStore
  // must reopen exactly that stream without repairing, dropping, or inventing records.
  if (opened.records != validator_.records() ||
      opened.lastSeq != validator_.lastSeq())
    return finish_(RESTORE_REPLAY_RECORD_MISMATCH, &opened);

  if (groups_.malformed() || groups_.pending() || groups_.sequenceError())
    return finish_(RESTORE_REPLAY_GROUP_MALFORMED, &opened);
  if (roster_.malformed() != 0)
    return finish_(RESTORE_REPLAY_ROSTER_MALFORMED, &opened);
  if (roster_.overflow())
    return finish_(RESTORE_REPLAY_ROSTER_OVERFLOW, &opened);
  if (closure_.index().gapOverflow())
    return finish_(RESTORE_REPLAY_GAP_OVERFLOW, &opened);
  if (closure_.index().closeOverflow())
    return finish_(RESTORE_REPLAY_CLOSE_OVERFLOW, &opened);

  return finish_(RESTORE_REPLAY_OK, &opened);
}

}  // namespace att
