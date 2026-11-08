#pragma once

#include "event_group_audit.h"
#include "roster_store.h"

namespace att {

// Immutable identity for one clean, currently-readable LogStore prefix.
// Appends, reopen/repair, a different open generation, or audit reset invalidate it.
struct RosterLogPrefix {
  uint64_t openGeneration;
  uint32_t recordCount;
  uint32_t lastSeq;
  uint32_t auditGeneration;
};

bool captureRosterLogPrefix(LogStore& log, const EventGroupAudit& audit,
                            const OpenInfo& opened, RosterLogPrefix& out);
bool rosterLogPrefixCurrent(const LogStore& log, const EventGroupAudit& audit,
                            const OpenInfo& opened, const RosterLogPrefix& prefix);

enum RosterTransitionCode : uint8_t {
  RTA_OK,
  RTA_BAD_ID,
  RTA_PRE_EPOCH,
  RTA_BAD_ACTION,
  RTA_SET_BACKWARD_DAY,
  RTA_DEACTIVATE_UNKNOWN,
  RTA_DEACTIVATE_INACTIVE,
  RTA_DEACTIVATE_NOT_FORWARD,
  RTA_ACTIVATE_UNKNOWN,
  RTA_ACTIVATE_ACTIVE,
  RTA_ACTIVATE_NOT_FORWARD
};

struct RosterTransitionFinding {
  RosterTransitionCode code;
  uint16_t studentId;
  uint32_t sourceSeq;
  int32_t effectiveDay;
  int32_t previousDay;
};

// Read-only semantic audit of roster lifecycle transitions. It does not mutate
// the source RosterStore and does not alter the event log. Roster SET on an
// existing ID is a rename; it may share a local day with another SET, but may
// not move backwards. ACTIVATE/DEACTIVATE must change state on a strictly later
// local day because the roster model has day granularity, not intraday intervals.
class RosterTransitionAudit {
 public:
  RosterTransitionAudit();
  void reset();
  bool consume(const Event& event, int16_t utcOffsetMin);
  bool valid() const { return finding_.code == RTA_OK; }
  const RosterTransitionFinding& finding() const { return finding_; }
  bool matches(const RosterStore& roster) const;

 private:
  struct State {
    int32_t lastDay;
    bool known;
    bool active;
  };
  bool fail_(RosterTransitionCode code, uint16_t id, const Event& event,
             int32_t day, int32_t previousDay);
  State state_[MAX_ID + 1];
  RosterTransitionFinding finding_;
};

enum RosterPrefixReplayCode : uint8_t {
  RPR_OK,
  RPR_LOG_UNTRUSTED,
  RPR_PREFIX_INVALID,
  RPR_OFFSET_MISMATCH,
  RPR_SNAPSHOT_CHANGED,
  RPR_LOG_READ_FAILED,
  RPR_ROSTER_INCOMPLETE,
  RPR_TRANSITION_INVALID,
  RPR_REPLAY_MISMATCH
};

struct RosterPrefixReplayResult {
  RosterPrefixReplayCode code;
  size_t recordsReplayed;
  size_t intervals;
  size_t knownStudents;
  RosterTransitionFinding transition;
};

// Rebuilds a caller-owned scratch RosterStore from exactly the captured prefix.
// The live RosterStore is never changed. The caller must discard `scratch` unless
// code == RPR_OK. Log changes before, during, or after replay invalidate the result.
RosterPrefixReplayResult replayRosterPrefix(LogStore& log,
                                             const EventGroupAudit& audit,
                                             const OpenInfo& opened,
                                             const RosterLogPrefix& prefix,
                                             int16_t utcOffsetMin,
                                             RosterStore& scratch,
                                             RosterTransitionAudit& transition);

}  // namespace att
