#include "roster_event_writer.h"
#include <string.h>
namespace att {
RosterEventWriter::RosterEventWriter(LogStore& log, RosterStore& roster)
    : log_(log), roster_(roster), generation_(log.openGeneration()), faulted_(false) {}
bool RosterEventWriter::ready_() const {
  return !faulted_ && generation_ != 0 && log_.openGeneration() == generation_ &&
      !log_.faulted() && log_.hasObserver(&roster_) && roster_.historyComplete();
}
RosterWriteOutcome RosterEventWriter::set(uint16_t id, const char* name,
    size_t nameLen, uint32_t epoch) {
  if (faulted_) return RosterWriteOutcome{RWS_UNCERTAIN, RE_UNTRUSTED};
  if (!ready_()) return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  const size_t beforeCount = log_.count();
  const uint32_t beforeLastSeq = log_.lastSeq();
  const bool wasKnown = roster_.known(id);
  const bool wasActive = roster_.active(id);
  Event events[5]; size_t count = 0;
  const RosterErr error = roster_.planSet(id, name, nameLen, epoch, events, count);
  if (error == RE_NOOP) return RosterWriteOutcome{RWS_NOOP, error};
  if (error != RE_OK) return RosterWriteOutcome{RWS_REJECTED, error};
  char expectedName[ROSTER_NAME_MAX];
  memcpy(expectedName, name, nameLen);  // planSet validated pointer and length
  return commit_(id, events, count, expectedName, nameLen,
      wasKnown ? wasActive : true, beforeCount, beforeLastSeq);
}
RosterWriteOutcome RosterEventWriter::deactivate(uint16_t id, uint32_t epoch) {
  if (faulted_) return RosterWriteOutcome{RWS_UNCERTAIN, RE_UNTRUSTED};
  if (!ready_()) return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  const size_t beforeCount = log_.count();
  const uint32_t beforeLastSeq = log_.lastSeq();
  char expectedName[ROSTER_NAME_MAX + 1] = {0};
  const size_t expectedNameLen = roster_.nameOf(id, expectedName);
  Event events[5]; size_t count = 0;
  const RosterErr error = roster_.planDeactivate(id, epoch, events, count);
  if (error == RE_NOOP) return RosterWriteOutcome{RWS_NOOP, error};
  if (error != RE_OK) return RosterWriteOutcome{RWS_REJECTED, error};
  return commit_(id, events, count, expectedName, expectedNameLen, false,
      beforeCount, beforeLastSeq);
}
RosterWriteOutcome RosterEventWriter::activate(uint16_t id, uint32_t epoch) {
  if (faulted_) return RosterWriteOutcome{RWS_UNCERTAIN, RE_UNTRUSTED};
  if (!ready_()) return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  const size_t beforeCount = log_.count();
  const uint32_t beforeLastSeq = log_.lastSeq();
  char expectedName[ROSTER_NAME_MAX + 1] = {0};
  const size_t expectedNameLen = roster_.nameOf(id, expectedName);
  Event events[5]; size_t count = 0;
  const RosterErr error = roster_.planActivate(id, epoch, events, count);
  if (error == RE_NOOP) return RosterWriteOutcome{RWS_NOOP, error};
  if (error != RE_OK) return RosterWriteOutcome{RWS_REJECTED, error};
  return commit_(id, events, count, expectedName, expectedNameLen, true,
      beforeCount, beforeLastSeq);
}
RosterWriteOutcome RosterEventWriter::commit_(uint16_t id, const Event* events,
    size_t count, const char* expectedName, size_t expectedNameLen,
    bool expectedActive, size_t beforeCount, uint32_t beforeLastSeq) {
  if (!ready_() || log_.count() != beforeCount || log_.lastSeq() != beforeLastSeq) {
    faulted_ = true;
    return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  }
  if (!events || count == 0 || count > 5 || !expectedName ||
      expectedNameLen == 0 || expectedNameLen > ROSTER_NAME_MAX ||
      count > static_cast<size_t>(-1) - beforeCount) {
    faulted_ = true;
    return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  }
  // LogStore reserves UINT32_MAX as the exhausted next-sequence sentinel.
  const uint64_t nextSeq = static_cast<uint64_t>(beforeLastSeq) + 1u;
  if (nextSeq == 0 || nextSeq > UINT32_MAX ||
      static_cast<uint64_t>(count) > static_cast<uint64_t>(UINT32_MAX) - nextSeq)
    return RosterWriteOutcome{RWS_BLOCKED, RE_UNTRUSTED};
  if (!log_.append(events, count)) {
    faulted_ = true;
    return RosterWriteOutcome{RWS_UNCERTAIN, RE_UNTRUSTED};
  }
  char actualName[ROSTER_NAME_MAX + 1] = {0};
  const size_t actualNameLen = roster_.nameOf(id, actualName);
  const uint64_t wantedLastSeq = static_cast<uint64_t>(beforeLastSeq) + count;
  const size_t wantedCount = beforeCount + count;
  const bool postcondition = !log_.faulted() &&
      log_.openGeneration() == generation_ && log_.hasObserver(&roster_) &&
      log_.count() == wantedCount &&
      static_cast<uint64_t>(log_.lastSeq()) == wantedLastSeq &&
      roster_.historyComplete() && roster_.known(id) &&
      roster_.active(id) == expectedActive && actualNameLen == expectedNameLen &&
      memcmp(actualName, expectedName, expectedNameLen) == 0;
  if (!postcondition) {
    faulted_ = true;
    return RosterWriteOutcome{RWS_UNCERTAIN, RE_UNTRUSTED};
  }
  return RosterWriteOutcome{RWS_COMMITTED, RE_OK};
}
}  // namespace att
