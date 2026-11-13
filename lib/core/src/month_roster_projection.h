#pragma once

#include "roster_snapshot.h"

namespace att {

enum MonthRosterCode : uint8_t {
  MRP_NOT_STARTED,
  MRP_READY,
  MRP_VALUE,
  MRP_END,
  MRP_BAD_RANGE,
  MRP_LOG_UNTRUSTED,
  MRP_REPLAY_FAILED,
  MRP_INTERVAL_INVALID,
  MRP_INTERVAL_OVERLAP,
  MRP_ID_WITHOUT_INTERVAL,
  MRP_PREFIX_CHANGED,
  MRP_SCRATCH_CHANGED,
  MRP_NOT_FOUND
};

struct MonthRosterStudent {
  uint16_t studentId;
  uint16_t overlappingIntervalCount;
  bool enrolledOnLastDay;
};

// Read-only report-month projection over a scratch roster reconstructed from
// one exact, audited log prefix. The caller must not mutate `scratch` while this
// reader is in use. Intervals are returned in their original, unclipped form.
// No new roster/history table is allocated.
class MonthRosterProjection {
 public:
  MonthRosterProjection(LogStore& log, const EventGroupAudit& audit,
                        const OpenInfo& opened, const RosterLogPrefix& prefix,
                        int16_t utcOffsetMin, RosterStore& scratch,
                        RosterTransitionAudit& transition,
                        int32_t monthStartDay, int32_t monthEndExclusive);

  MonthRosterCode begin();
  MonthRosterCode next(MonthRosterStudent& out);
  MonthRosterCode intervalAt(uint16_t studentId, size_t ordinal,
                             RosterIntervalView& out);
  bool current() const;
  MonthRosterCode status() const { return code_; }
  size_t studentCount() const { return studentCount_; }
  size_t intervalCount() const { return intervalCount_; }
  size_t enrolledOnLastDayCount() const { return enrolledOnLastDayCount_; }
  const RosterPrefixReplayResult& replayResult() const { return replay_; }

 private:
  bool validateIntervals_();
  MonthRosterCode summarizeStudent_(uint16_t studentId,
                                     MonthRosterStudent& out) const;
  bool prefixCurrent_() const;
  bool scratchMatchesReplay_() const;
  bool scratchFingerprint_(uint32_t& outHash) const;

  LogStore& log_;
  const EventGroupAudit& audit_;
  // These small identity descriptors are copied so caller-side mutation or a
  // temporary argument cannot change/dangle the projection's source identity.
  OpenInfo opened_;
  RosterLogPrefix prefix_;
  int16_t utcOffsetMin_;
  RosterStore& scratch_;
  RosterTransitionAudit& transition_;
  int32_t monthStartDay_;
  int32_t monthEndExclusive_;
  size_t nextKnownOrdinal_;
  size_t studentCount_;
  size_t intervalCount_;
  size_t enrolledOnLastDayCount_;
  RosterPrefixReplayResult replay_;
  MonthRosterCode code_;
  bool begun_;
  bool fingerprintSet_;
  uint32_t initialScratchFingerprint_;
};

}  // namespace att
