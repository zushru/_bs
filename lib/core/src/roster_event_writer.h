#pragma once
#include <stddef.h>
#include <stdint.h>
#include "log_store.h"
#include "roster_store.h"
namespace att {
enum RosterWriteState { RWS_COMMITTED, RWS_NOOP, RWS_REJECTED, RWS_BLOCKED, RWS_UNCERTAIN };
struct RosterWriteOutcome { RosterWriteState state; RosterErr rosterError; };
// Construct after a successful LogStore::open() and attachment/replay of this
// exact RosterStore. The roster must not be attached to another LogStore.
// All access and all other log appends must be serialized by one owner.
// This helper cannot prevent external callers from bypassing it via public
// LogStore::append(); production composition must prohibit alternate writers.
class RosterEventWriter {
 public:
  RosterEventWriter(LogStore& log, RosterStore& roster);
  RosterEventWriter(const RosterEventWriter&) = delete;
  RosterEventWriter& operator=(const RosterEventWriter&) = delete;
  RosterWriteOutcome set(uint16_t id, const char* name, size_t nameLen, uint32_t epoch);
  RosterWriteOutcome deactivate(uint16_t id, uint32_t epoch);
  RosterWriteOutcome activate(uint16_t id, uint32_t epoch);
  // Failed/indeterminate writes latch this instance. Reopen/replay/reconcile
  // before constructing a new writer; never retry the same logical operation.
  bool faulted() const { return faulted_; }
 private:
  bool ready_() const;
  RosterWriteOutcome commit_(uint16_t id, const Event* events, size_t count,
      const char* expectedName, size_t expectedNameLen, bool expectedActive,
      size_t beforeCount, uint32_t beforeLastSeq);
  LogStore& log_;
  RosterStore& roster_;
  uint64_t generation_;
  bool faulted_;
};
}  // namespace att
