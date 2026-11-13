#include "roster_snapshot.h"
#include "time_rules.h"
#include <limits.h>

namespace att {

bool captureRosterLogPrefix(LogStore& log, const EventGroupAudit& audit,
                            const OpenInfo& opened, RosterLogPrefix& out) {
  out = RosterLogPrefix{0, 0, 0, 0};
  if (!audit.readyFor(log, opened) || log.count() > UINT32_MAX ||
      log.openGeneration() == 0 || log.lastSeq() != (uint32_t)log.count() ||
      audit.recordsSeen() != log.count() || audit.lastSeq() != log.lastSeq()) return false;
  out.openGeneration = log.openGeneration();
  out.recordCount = (uint32_t)log.count();
  out.lastSeq = log.lastSeq();
  out.auditGeneration = audit.generation();
  return true;
}

bool rosterLogPrefixCurrent(const LogStore& log, const EventGroupAudit& audit,
                            const OpenInfo& opened, const RosterLogPrefix& prefix) {
  return prefix.openGeneration != 0 &&
      prefix.openGeneration == log.openGeneration() &&
      prefix.recordCount == log.count() && prefix.lastSeq == log.lastSeq() &&
      prefix.auditGeneration == audit.generation() &&
      prefix.lastSeq == prefix.recordCount && audit.readyFor(log, opened) &&
      audit.recordsSeen() == prefix.recordCount && audit.lastSeq() == prefix.lastSeq;
}

RosterTransitionAudit::RosterTransitionAudit() { reset(); }

void RosterTransitionAudit::reset() {
  for (size_t i = 0; i <= MAX_ID; ++i) {
    state_[i].lastDay = -1;
    state_[i].lastLifecycleDay = -1;
    state_[i].known = false;
    state_[i].active = false;
  }
  finding_ = RosterTransitionFinding{RTA_OK, 0, 0, -1, -1};
}

bool RosterTransitionAudit::fail_(RosterTransitionCode code, uint16_t id,
                                  const Event& event, int32_t day,
                                  int32_t previousDay) {
  if (finding_.code == RTA_OK) {
    finding_ = RosterTransitionFinding{code, id, event.seq, day, previousDay};
  }
  return false;
}

bool RosterTransitionAudit::consume(const Event& event, int16_t utcOffsetMin) {
  if (!valid()) return false;
  if (event.type != EV_ROSTER) return true;
  if (event.a < 1 || event.a > MAX_ID) {
    return fail_(RTA_BAD_ID, event.a, event, -1, -1);
  }
  const int32_t day = dayNumber(event.epoch, utcOffsetMin);
  if (day < 0) return fail_(RTA_PRE_EPOCH, event.a, event, day, state_[event.a].lastDay);

  State& state = state_[event.a];
  const uint8_t baseFlags = (uint8_t)(event.flags & (uint8_t)~FL_MORE);
  if (event.b == RA_SET) {
    if (!flagMore(event.flags) || baseFlags != 0) {
      return fail_(RTA_BAD_ACTION, event.a, event, day, state.lastDay);
    }
    if (!state.known) {
      state.known = true;
      state.active = true;
      state.lastDay = day;
      state.lastLifecycleDay = day;
      return true;
    }
    if (day < state.lastDay) {
      return fail_(RTA_SET_BACKWARD_DAY, event.a, event, day, state.lastDay);
    }
    if (day > state.lastDay) state.lastDay = day;
    return true;  // rename; it does not change enrollment state
  }

  if (flagMore(event.flags) || baseFlags != 0) {
    return fail_(RTA_BAD_ACTION, event.a, event, day, state.lastDay);
  }
  if (event.b == RA_DEACTIVATE) {
    if (!state.known) return fail_(RTA_DEACTIVATE_UNKNOWN, event.a, event, day, -1);
    if (!state.active) return fail_(RTA_DEACTIVATE_INACTIVE, event.a, event, day, state.lastDay);
    if (day < state.lastDay || day <= state.lastLifecycleDay) {
      return fail_(RTA_DEACTIVATE_NOT_FORWARD, event.a, event, day, state.lastDay);
    }
    state.active = false;
    state.lastDay = day;
    state.lastLifecycleDay = day;
    return true;
  }
  if (event.b == RA_ACTIVATE) {
    if (!state.known) return fail_(RTA_ACTIVATE_UNKNOWN, event.a, event, day, -1);
    if (state.active) return fail_(RTA_ACTIVATE_ACTIVE, event.a, event, day, state.lastDay);
    if (day < state.lastDay || day <= state.lastLifecycleDay) {
      return fail_(RTA_ACTIVATE_NOT_FORWARD, event.a, event, day, state.lastDay);
    }
    state.active = true;
    state.lastDay = day;
    state.lastLifecycleDay = day;
    return true;
  }
  return fail_(RTA_BAD_ACTION, event.a, event, day, state.lastDay);
}

bool RosterTransitionAudit::matches(const RosterStore& roster) const {
  if (!valid() || !roster.historyComplete()) return false;
  for (uint16_t id = 1; id <= MAX_ID; ++id) {
    if (state_[id].known != roster.known(id) ||
        state_[id].active != roster.active(id)) return false;
  }
  return true;
}

RosterPrefixReplayResult replayRosterPrefix(LogStore& log,
                                             const EventGroupAudit& audit,
                                             const OpenInfo& opened,
                                             const RosterLogPrefix& prefix,
                                             int16_t utcOffsetMin,
                                             RosterStore& scratch,
                                             RosterTransitionAudit& transition) {
  RosterPrefixReplayResult result = {RPR_LOG_UNTRUSTED, 0, 0, 0,
      RosterTransitionFinding{RTA_OK, 0, 0, -1, -1}};
  if (!audit.readyFor(log, opened)) return result;
  if (prefix.openGeneration == 0 || prefix.openGeneration != opened.generation ||
      prefix.recordCount != prefix.lastSeq) {
    result.code = RPR_PREFIX_INVALID;
    return result;
  }
  if (!rosterLogPrefixCurrent(log, audit, opened, prefix)) {
    result.code = RPR_SNAPSHOT_CHANGED;
    return result;
  }
  if (scratch.utcOffsetMin() != utcOffsetMin) {
    result.code = RPR_OFFSET_MISMATCH;
    return result;
  }

  scratch.onReset();
  transition.reset();
  for (uint32_t index = 0; index < prefix.recordCount; ++index) {
    if ((index & 0x0Fu) == 0 &&
        !rosterLogPrefixCurrent(log, audit, opened, prefix)) {
      result.code = RPR_SNAPSHOT_CHANGED;
      return result;
    }
    Event event;
    if (!log.at(index, event)) {
      result.code = RPR_LOG_READ_FAILED;
      return result;
    }
    scratch.onRecord(event);
    if (event.type == EV_ROSTER && !transition.consume(event, utcOffsetMin)) {
      result.code = RPR_TRANSITION_INVALID;
      result.transition = transition.finding();
      return result;
    }
    ++result.recordsReplayed;
  }

  if (!rosterLogPrefixCurrent(log, audit, opened, prefix)) {
    result.code = RPR_SNAPSHOT_CHANGED;
    return result;
  }
  if (!scratch.historyComplete()) {
    result.code = RPR_ROSTER_INCOMPLETE;
    return result;
  }
  if (!transition.matches(scratch)) {
    result.code = RPR_REPLAY_MISMATCH;
    result.transition = transition.finding();
    return result;
  }
  result.code = RPR_OK;
  result.intervals = scratch.intervalCount();
  result.knownStudents = scratch.knownCount();
  result.transition = transition.finding();
  return result;
}

}  // namespace att
