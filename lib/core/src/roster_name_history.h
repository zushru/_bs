#pragma once

#include "event_group_audit.h"
#include "roster_snapshot.h"

namespace att {

enum RosterNameReadCode : uint8_t {
  ROSTER_NAME_VALUE,
  ROSTER_NAME_END,
  ROSTER_NAME_LOG_UNTRUSTED,
  ROSTER_NAME_LOG_READ_FAILED,
  ROSTER_NAME_BAD_SET,
  ROSTER_NAME_SNAPSHOT_CHANGED
};

struct RosterNameVersion {
  uint16_t studentId;
  int32_t effectiveDay;
  uint32_t sourceSeq;
  uint8_t length;
  char name[ROSTER_NAME_MAX + 1];
};

// Streams every historical RA_SET name directly from the authoritative event log.
// Returned names are the exact 32-byte roster display names, not parsed learner
// name components. The caller must consume the entire stream successfully before
// using any partial output as a report snapshot.
class RosterNameHistoryReader {
 public:
  RosterNameHistoryReader(LogStore& log, const EventGroupAudit& audit,
                          const OpenInfo& opened, int16_t utcOffsetMin);
  // Explicit prefix overload for snapshot coordinators. It is intentionally
  // strict: any append/reopen after prefix capture invalidates the reader.
  RosterNameHistoryReader(LogStore& log, const EventGroupAudit& audit,
                          const OpenInfo& opened, const RosterLogPrefix& prefix,
                          int16_t utcOffsetMin);
  RosterNameReadCode next(RosterNameVersion& out);

 private:
  LogStore& log_;
  const EventGroupAudit& audit_;
  int16_t offsetMin_;
  size_t cursor_;
  OpenInfo opened_;
  RosterLogPrefix prefix_;
  bool trustedAtStart_;
  bool failed_;
  bool ended_;
  RosterNameReadCode failureCode_;
};

}  // namespace att
