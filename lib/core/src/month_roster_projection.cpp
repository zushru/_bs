#include "month_roster_projection.h"
#include <limits.h>
#include <stdint.h>

namespace att {

MonthRosterProjection::MonthRosterProjection(
    LogStore& log, const EventGroupAudit& audit, const OpenInfo& opened,
    const RosterLogPrefix& prefix, int16_t utcOffsetMin, RosterStore& scratch,
    RosterTransitionAudit& transition, int32_t monthStartDay,
    int32_t monthEndExclusive)
    : log_(log), audit_(audit), opened_(opened), prefix_(prefix),
      utcOffsetMin_(utcOffsetMin), scratch_(scratch), transition_(transition),
      monthStartDay_(monthStartDay), monthEndExclusive_(monthEndExclusive),
      nextKnownOrdinal_(0), studentCount_(0), intervalCount_(0),
      enrolledOnLastDayCount_(0),
      replay_{RPR_LOG_UNTRUSTED, 0, 0, 0,
              RosterTransitionFinding{RTA_OK, 0, 0, -1, -1}},
      code_(MRP_NOT_STARTED), begun_(false), fingerprintSet_(false),
      initialScratchFingerprint_(0) {}

bool MonthRosterProjection::prefixCurrent_() const {
  return rosterLogPrefixCurrent(log_, audit_, opened_, prefix_);
}

bool MonthRosterProjection::scratchFingerprint_(uint32_t& outHash) const {
  outHash = 0;
  uint32_t hash = 2166136261u;
  const auto mix = [&hash](uint8_t byte) { hash = (hash ^ byte) * 16777619u; };
  const auto mix32 = [&mix](uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) mix((uint8_t)(value >> (8u * i)));
  };
  mix32((uint32_t)scratch_.utcOffsetMin());
  mix32((uint32_t)scratch_.knownCount());
  mix32((uint32_t)scratch_.intervalCount());
  for (size_t ordinal = 0; ordinal < scratch_.knownCount(); ++ordinal) {
    uint16_t id = 0;
    if (!scratch_.knownIdAt(ordinal, id)) return false;
    char name[ROSTER_NAME_MAX + 1];
    const size_t length = scratch_.nameOf(id, name);
    mix((uint8_t)id); mix((uint8_t)(id >> 8)); mix((uint8_t)length);
    for (size_t i = 0; i < length; ++i) mix((uint8_t)name[i]);
  }
  for (size_t i = 0; i < scratch_.intervalCount(); ++i) {
    RosterIntervalView interval;
    if (!scratch_.intervalAt(i, interval)) return false;
    mix((uint8_t)interval.studentId); mix((uint8_t)(interval.studentId >> 8));
    mix32((uint32_t)interval.startDayInclusive);
    mix32((uint32_t)interval.endDayExclusive);
  }
  outHash = hash;
  return true;
}

bool MonthRosterProjection::scratchMatchesReplay_() const {
  if (!scratch_.historyComplete() ||
      scratch_.intervalCount() != replay_.intervals ||
      scratch_.knownCount() != replay_.knownStudents ||
      scratch_.utcOffsetMin() != utcOffsetMin_) return false;
  if (!fingerprintSet_) return true;
  uint32_t currentHash = 0;
  return scratchFingerprint_(currentHash) && currentHash == initialScratchFingerprint_;
}

bool MonthRosterProjection::current() const {
  return begun_ && code_ != MRP_PREFIX_CHANGED && prefixCurrent_() &&
      scratchMatchesReplay_();
}

bool MonthRosterProjection::validateIntervals_() {
  const size_t count = scratch_.intervalCount();
  if (count != replay_.intervals || scratch_.knownCount() != replay_.knownStudents ||
      !scratch_.historyComplete()) return false;

  // Validate every interval and ensure that the interval store cannot describe
  // overlapping enrollment periods for the same never-reused device ID.
  for (size_t i = 0; i < count; ++i) {
    RosterIntervalView a;
    if (!scratch_.intervalAt(i, a) || a.studentId < 1 || a.studentId > MAX_ID ||
        !scratch_.known(a.studentId) || a.startDayInclusive < 0 ||
        a.startDayInclusive == INT32_MAX ||
        (a.endDayExclusive != INT32_MAX &&
         a.endDayExclusive <= a.startDayInclusive)) return false;

    for (size_t j = i + 1; j < count; ++j) {
      RosterIntervalView b;
      if (!scratch_.intervalAt(j, b)) return false;
      if (a.studentId != b.studentId) continue;
      if (a.startDayInclusive < b.endDayExclusive &&
          b.startDayInclusive < a.endDayExclusive) {
        code_ = MRP_INTERVAL_OVERLAP;
        return false;
      }
    }
  }

  // Every known ID must have at least one valid enrollment interval.
  uint16_t previousId = 0;
  for (size_t ordinal = 0; ordinal < scratch_.knownCount(); ++ordinal) {
    uint16_t id = 0;
    if (!scratch_.knownIdAt(ordinal, id) || id <= previousId || id > MAX_ID) return false;
    previousId = id;
    size_t found = 0;
    for (size_t i = 0; i < count; ++i) {
      RosterIntervalView interval;
      if (!scratch_.intervalAt(i, interval)) return false;
      if (interval.studentId == id) ++found;
    }
    if (found == 0) {
      code_ = MRP_ID_WITHOUT_INTERVAL;
      return false;
    }
  }

  studentCount_ = 0;
  intervalCount_ = 0;
  enrolledOnLastDayCount_ = 0;
  for (size_t ordinal = 0; ordinal < scratch_.knownCount(); ++ordinal) {
    uint16_t id = 0;
    if (!scratch_.knownIdAt(ordinal, id)) return false;
    MonthRosterStudent student;
    const MonthRosterCode summary = summarizeStudent_(id, student);
    if (summary == MRP_NOT_FOUND) continue;
    if (summary != MRP_VALUE) {
      code_ = summary;
      return false;
    }
    ++studentCount_;
    intervalCount_ += student.overlappingIntervalCount;
    if (student.enrolledOnLastDay) ++enrolledOnLastDayCount_;
  }
  return true;
}

MonthRosterCode MonthRosterProjection::summarizeStudent_(
    uint16_t studentId, MonthRosterStudent& out) const {
  out = MonthRosterStudent{studentId, 0, false};
  const int32_t lastDay = monthEndExclusive_ - 1;
  for (size_t i = 0; i < scratch_.intervalCount(); ++i) {
    RosterIntervalView interval;
    if (!scratch_.intervalAt(i, interval)) return MRP_INTERVAL_INVALID;
    if (interval.studentId != studentId) continue;
    const bool overlaps = interval.startDayInclusive < monthEndExclusive_ &&
        interval.endDayExclusive > monthStartDay_;
    if (overlaps) {
      if (out.overlappingIntervalCount == UINT16_MAX) return MRP_INTERVAL_INVALID;
      ++out.overlappingIntervalCount;
    }
    if (interval.startDayInclusive <= lastDay && lastDay < interval.endDayExclusive)
      out.enrolledOnLastDay = true;
  }
  return out.overlappingIntervalCount != 0 ? MRP_VALUE : MRP_NOT_FOUND;
}

MonthRosterCode MonthRosterProjection::begin() {
  begun_ = false;
  fingerprintSet_ = false;
  initialScratchFingerprint_ = 0;
  replay_ = RosterPrefixReplayResult{RPR_LOG_UNTRUSTED, 0, 0, 0,
      RosterTransitionFinding{RTA_OK, 0, 0, -1, -1}};
  code_ = MRP_NOT_STARTED;
  nextKnownOrdinal_ = 0;
  studentCount_ = 0;
  intervalCount_ = 0;
  enrolledOnLastDayCount_ = 0;

  if (monthStartDay_ < 0 || monthEndExclusive_ <= monthStartDay_) {
    code_ = MRP_BAD_RANGE;
    return code_;
  }
  if (!prefixCurrent_()) {
    code_ = MRP_LOG_UNTRUSTED;
    return code_;
  }

  replay_ = replayRosterPrefix(log_, audit_, opened_, prefix_, utcOffsetMin_,
                               scratch_, transition_);
  if (replay_.code != RPR_OK) {
    code_ = MRP_REPLAY_FAILED;
    return code_;
  }
  if (!prefixCurrent_()) {
    code_ = MRP_PREFIX_CHANGED;
    return code_;
  }
  if (scratch_.utcOffsetMin() != utcOffsetMin_ || !scratchMatchesReplay_()) {
    code_ = MRP_INTERVAL_INVALID;
    return code_;
  }
  if (!scratchFingerprint_(initialScratchFingerprint_)) {
    code_ = MRP_INTERVAL_INVALID;
    return code_;
  }
  fingerprintSet_ = true;
  if (!validateIntervals_()) {
    if (code_ == MRP_NOT_STARTED) code_ = MRP_INTERVAL_INVALID;
    return code_;
  }
  if (!prefixCurrent_()) {
    code_ = MRP_PREFIX_CHANGED;
    return code_;
  }
  begun_ = true;
  code_ = MRP_READY;
  return code_;
}

MonthRosterCode MonthRosterProjection::next(MonthRosterStudent& out) {
  out = MonthRosterStudent{0, 0, false};
  if (!begun_ || (code_ != MRP_READY && code_ != MRP_VALUE)) return code_;
  if (!prefixCurrent_()) {
    code_ = MRP_PREFIX_CHANGED;
    return code_;
  }
  if (!scratchMatchesReplay_()) {
    code_ = MRP_SCRATCH_CHANGED;
    return code_;
  }
  while (nextKnownOrdinal_ < scratch_.knownCount()) {
    uint16_t id = 0;
    if (!scratch_.knownIdAt(nextKnownOrdinal_++, id)) {
      code_ = MRP_INTERVAL_INVALID;
      return code_;
    }
    const MonthRosterCode summary = summarizeStudent_(id, out);
    if (summary == MRP_VALUE) {
      code_ = MRP_VALUE;
      return code_;
    }
    if (summary != MRP_NOT_FOUND) {
      code_ = summary;
      return code_;
    }
  }
  code_ = MRP_END;
  return code_;
}

MonthRosterCode MonthRosterProjection::intervalAt(uint16_t studentId,
                                                   size_t ordinal,
                                                   RosterIntervalView& out) {
  out = RosterIntervalView{0, 0, 0};
  if (!begun_ || (code_ != MRP_READY && code_ != MRP_VALUE && code_ != MRP_END))
    return code_;
  if (!prefixCurrent_()) {
    code_ = MRP_PREFIX_CHANGED;
    return code_;
  }
  if (!scratchMatchesReplay_()) {
    code_ = MRP_SCRATCH_CHANGED;
    return code_;
  }
  if (studentId < 1 || studentId > MAX_ID || !scratch_.known(studentId))
    return MRP_NOT_FOUND;
  size_t found = 0;
  for (size_t i = 0; i < scratch_.intervalCount(); ++i) {
    RosterIntervalView interval;
    if (!scratch_.intervalAt(i, interval)) {
      code_ = MRP_INTERVAL_INVALID;
      return code_;
    }
    if (interval.studentId != studentId ||
        interval.startDayInclusive >= monthEndExclusive_ ||
        interval.endDayExclusive <= monthStartDay_) continue;
    if (found++ == ordinal) {
      out = interval;  // preserve original half-open interval; do not clip it
      return MRP_VALUE;
    }
  }
  return MRP_NOT_FOUND;
}

}  // namespace att
