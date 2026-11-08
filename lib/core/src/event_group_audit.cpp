#include "event_group_audit.h"
#include "event_codec.h"
#include "day_view.h"
#include "roster_store.h"

namespace att {
namespace {
uint16_t bitCount64(uint64_t value) {
  uint16_t count = 0;
  while (value) {
    value &= value - 1;
    ++count;
  }
  return count;
}
}  // namespace

EventGroupAudit::EventGroupAudit()
    : count_(0), malformed_(false), sequenceError_(false), attached_(false),
      recordsSeen_(0), lastSeq_(0), generation_(0) {}

bool EventGroupAudit::attach(LogStore& log) {
  if (attached_ || log.faulted()) return false;
  if (!log.attach(this)) return false;
  attached_ = true;
  return true;
}

bool EventGroupAudit::readyFor(const LogStore& log, const OpenInfo& opened) const {
  return attached_ && opened.ok && opened.generation != 0 &&
      opened.generation == log.openGeneration() && opened.corruptAt < 0 &&
      opened.tornBytes == 0 && opened.droppedGroup == 0 && !log.faulted() &&
      !malformed_ && !pending() && !sequenceError_ &&
      recordsSeen_ == log.count() && lastSeq_ == log.lastSeq();
}

void EventGroupAudit::onReset() {
  count_ = 0;
  malformed_ = false;
  sequenceError_ = false;
  recordsSeen_ = 0;
  lastSeq_ = 0;
  ++generation_;
}

void EventGroupAudit::onRecord(const Event& event) {
  if (malformed_) return;
  if ((recordsSeen_ == 0 && event.seq != 1) ||
      (recordsSeen_ != 0 && (lastSeq_ == UINT32_MAX || event.seq != lastSeq_ + 1u)) ||
      recordsSeen_ == UINT32_MAX) {
    sequenceError_ = true;
    malformed_ = true;
    count_ = 0;
    return;
  }
  ++recordsSeen_;
  lastSeq_ = event.seq;

  if (count_ >= MAX_GROUP) {
    malformed_ = true;
    count_ = 0;
    return;
  }
  group_[count_++] = event;
  if (!flagMore(event.flags)) {
    if (!validGroup_()) malformed_ = true;
    count_ = 0;
  }
}

bool EventGroupAudit::validGroup_() const {
  if (count_ == 0) return false;
  const Event& first = group_[0];
  for (size_t i = 1; i < count_; ++i) {
    if (group_[i].seq != group_[0].seq + (uint32_t)i) return false;
  }
  const uint8_t firstBase = (uint8_t)(first.flags & (uint8_t)~FL_MORE);

  if (first.type == EV_ROSTER) {
    if (firstBase != 0 || first.a < 1 || first.a > MAX_ID) return false;
    if (first.b == RA_SET) {
      if (count_ != 5 || !flagMore(first.flags)) return false;
      for (size_t i = 1; i < count_; ++i) {
        if (group_[i].type != EV_PAYLOAD ||
            (group_[i].flags & (uint8_t)~FL_MORE) != 0 ||
            flagMore(group_[i].flags) != (i + 1 < count_)) return false;
      }
      return true;
    }
    return count_ == 1 && !flagMore(first.flags) &&
        (first.b == RA_DEACTIVATE || first.b == RA_ACTIVATE);
  }

  if (first.type == EV_CLOSE) {
    // The signed product capacity is 300 IDs. The current writer always emits
    // exactly ceil(300/64) = five bitmap payload records, even for count zero.
    const uint8_t closeFlags = (uint8_t)(FL_SESSION | FL_COVERAGE | FL_MORE);
    if (count_ != 6 || first.a > MAX_ID ||
        (first.flags & (uint8_t)~closeFlags) != 0 || !flagMore(first.flags)) return false;
    uint16_t bitmapCount = 0;
    for (size_t i = 1; i < count_; ++i) {
      if (group_[i].type != EV_PAYLOAD ||
          (group_[i].flags & (uint8_t)~FL_MORE) != 0 ||
          flagMore(group_[i].flags) != (i + 1 < count_)) return false;
      const uint64_t bits = payloadBits(group_[i]);
      const size_t firstIdOffset = (i - 1) * 64;
      const size_t validBits = MAX_ID > firstIdOffset ?
          ((MAX_ID - firstIdOffset) < 64 ? (MAX_ID - firstIdOffset) : 64) : 0;
      if (validBits < 64 && (bits >> validBits) != 0) return false;
      bitmapCount = (uint16_t)(bitmapCount + bitCount64(bits));
    }
    return bitmapCount == first.a;
  }

  if (first.type == EV_GAP) {
    const bool hasClockPair = count_ >= 2 &&
        group_[count_ - 2].type == EV_CLOCK_SET &&
        group_[count_ - 1].type == EV_CLOCK_SET;
    const size_t gapCount = hasClockPair ? count_ - 2 : count_;
    if (gapCount == 0) return false;
    const uint16_t cause = group_[0].a;
    if (cause < GAP_POWER || cause > GAP_CLOCK) return false;
    for (size_t i = 0; i < gapCount; ++i) {
      const Event& e = group_[i];
      const uint64_t gapEnd = (uint64_t)e.epoch + (uint64_t)e.b * 60u;
      if (e.type != EV_GAP || e.a != cause || e.b == 0 ||
          gapEnd > UINT32_MAX ||
          (e.flags & (uint8_t)~FL_MORE) != 0 ||
          flagMore(e.flags) != (hasClockPair || i + 1 < gapCount)) return false;
      if (i + 1 < gapCount && gapEnd != group_[i + 1].epoch) return false;
    }
    if (hasClockPair) {
      const Event& a = group_[count_ - 2];
      const Event& b = group_[count_ - 1];
      return a.a == b.a && a.a <= SRC_WIZARD && a.b == 0 && b.b == 0 &&
          (a.flags & (uint8_t)~FL_MORE) == 0 &&
          (b.flags & (uint8_t)~FL_MORE) == 0 &&
          flagMore(a.flags) && !flagMore(b.flags);
    }
    return true;
  }

  if (first.type == EV_CLOCK_SET) {
    if (count_ != 2) return false;
    const Event& second = group_[1];
    return second.type == EV_CLOCK_SET && first.a == second.a &&
        first.a <= SRC_WIZARD && first.b == 0 && second.b == 0 &&
        (first.flags & (uint8_t)~FL_MORE) == 0 &&
        (second.flags & (uint8_t)~FL_MORE) == 0 &&
        flagMore(first.flags) && !flagMore(second.flags);
  }

  // PAYLOAD must be owned by a ROSTER SET or CLOSE group. Validate the current
  // single-record attendance writers as well as their commit-group shape.
  if (count_ != 1 || first.type == EV_PAYLOAD || flagMore(first.flags) ||
      (first.flags & 0xC0u) != 0) return false;

  const uint8_t sessionStatusFlags = (uint8_t)(FL_SESSION | 0x06u);
  if (first.type == EV_SCAN) {
    const uint8_t status = flagStatus(first.flags);
    return first.a >= 1 && first.a <= MAX_ID && first.epoch != 0 &&
        (first.flags & (uint8_t)~sessionStatusFlags) == 0 &&
        (status == ST_PRESENT || status == ST_LATE);
  }
  if (first.type == EV_MARK) {
    return first.a >= 1 && first.a <= MAX_ID && first.epoch != 0 &&
        (first.flags & (uint8_t)~sessionStatusFlags) == 0 &&
        flagStatus(first.flags) <= ST_EXCUSED;
  }
  if (first.type == EV_NOCLASS_SET || first.type == EV_NOCLASS_CLEAR) {
    const uint8_t noClassFlags = (uint8_t)(FL_SESSION | FL_WHOLEDAY);
    return first.a == 0 && first.epoch != 0 &&
        (first.flags & (uint8_t)~noClassFlags) == 0;
  }
  // EV_SETTING, EV_BOOT, EV_REPAIR and any future event types have no current
  // writer/schema in this release. Refuse them until a type-specific validator
  // is implemented rather than treating an unknown semantic record as safe.
  return false;
}

}  // namespace att
