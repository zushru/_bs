#include "roster_name_history.h"
#include "time_rules.h"
#include <string.h>

namespace att {

RosterNameHistoryReader::RosterNameHistoryReader(
    LogStore& log, const EventGroupAudit& audit, const OpenInfo& opened,
    int16_t utcOffsetMin)
    : log_(log), audit_(audit), offsetMin_(utcOffsetMin), cursor_(0),
      opened_(opened), prefix_{0, 0, 0, 0}, trustedAtStart_(false),
      failed_(false), ended_(false), failureCode_(ROSTER_NAME_LOG_UNTRUSTED) {
  trustedAtStart_ = captureRosterLogPrefix(log_, audit_, opened_, prefix_);
  if (!trustedAtStart_) failed_ = true;
}

RosterNameHistoryReader::RosterNameHistoryReader(
    LogStore& log, const EventGroupAudit& audit, const OpenInfo& opened,
    const RosterLogPrefix& prefix, int16_t utcOffsetMin)
    : log_(log), audit_(audit), offsetMin_(utcOffsetMin), cursor_(0),
      opened_(opened), prefix_(prefix),
      trustedAtStart_(rosterLogPrefixCurrent(log_, audit_, opened_, prefix_)),
      failed_(!trustedAtStart_), ended_(false),
      failureCode_(ROSTER_NAME_LOG_UNTRUSTED) {}

RosterNameReadCode RosterNameHistoryReader::next(RosterNameVersion& out) {
  if (failed_) return failureCode_;
  if (ended_) return ROSTER_NAME_END;
  if (!trustedAtStart_) {
    failed_ = true; failureCode_ = ROSTER_NAME_LOG_UNTRUSTED; return failureCode_;
  }
  if (!rosterLogPrefixCurrent(log_, audit_, opened_, prefix_)) {
    failed_ = true; failureCode_ = ROSTER_NAME_SNAPSHOT_CHANGED; return failureCode_;
  }

  while (cursor_ < prefix_.recordCount) {
    Event first;
    if (!log_.at(cursor_, first)) {
      failed_ = true; failureCode_ = ROSTER_NAME_LOG_READ_FAILED; return failureCode_;
    }
    const size_t firstIndex = cursor_++;
    if (first.type != EV_ROSTER || first.b != RA_SET) continue;

    if (first.a < 1 || first.a > MAX_ID ||
        (first.flags & (uint8_t)~FL_MORE) != 0 || !flagMore(first.flags) ||
        firstIndex > prefix_.recordCount - 1 || prefix_.recordCount - firstIndex < 5) {
      failed_ = true; failureCode_ = ROSTER_NAME_BAD_SET; return failureCode_;
    }

    uint8_t bytes[ROSTER_NAME_MAX];
    for (size_t word = 0; word < 4; ++word) {
      Event payload;
      if (!log_.at(firstIndex + word + 1, payload)) {
        failed_ = true; failureCode_ = ROSTER_NAME_LOG_READ_FAILED; return failureCode_;
      }
      if (payload.type != EV_PAYLOAD ||
          payload.seq != first.seq + (uint32_t)word + 1u ||
          (payload.flags & (uint8_t)~FL_MORE) != 0 ||
          flagMore(payload.flags) != (word < 3)) {
        failed_ = true; failureCode_ = ROSTER_NAME_BAD_SET; return failureCode_;
      }
      const uint64_t bits = payloadBits(payload);
      for (size_t byte = 0; byte < 8; ++byte)
        bytes[word * 8 + byte] = (uint8_t)(bits >> (byte * 8));
    }

    size_t length = 0;
    while (length < ROSTER_NAME_MAX && bytes[length] != 0) ++length;
    for (size_t i = length; i < ROSTER_NAME_MAX; ++i) {
      if (bytes[i] != 0) {
        failed_ = true; failureCode_ = ROSTER_NAME_BAD_SET; return failureCode_;
      }
    }
    if (!RosterStore::validName((const char*)bytes, length)) {
      failed_ = true; failureCode_ = ROSTER_NAME_BAD_SET; return failureCode_;
    }
    const int32_t day = dayNumber(first.epoch, offsetMin_);
    if (day < 0) {
      failed_ = true; failureCode_ = ROSTER_NAME_BAD_SET; return failureCode_;
    }

    out.studentId = first.a;
    out.effectiveDay = day;
    out.sourceSeq = first.seq;
    out.length = (uint8_t)length;
    memcpy(out.name, bytes, length);
    out.name[length] = 0;
    cursor_ = firstIndex + 5;
    return ROSTER_NAME_VALUE;
  }
  ended_ = true;
  return ROSTER_NAME_END;
}

}  // namespace att
