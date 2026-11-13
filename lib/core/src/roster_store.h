#pragma once
#include "closure_engine.h"
#include "day_view.h"
namespace att {
const size_t MAX_INTERVALS = 512;   // active spans over all IDs; overflow = nobody is "expected"
const size_t ROSTER_NAME_MAX = 32;
enum RosterAction : uint16_t { RA_SET = 1, RA_DEACTIVATE = 2, RA_ACTIVATE = 3 };
enum RosterErr { RE_OK, RE_BAD_ID, RE_BAD_NAME, RE_UNKNOWN, RE_NOOP, RE_FULL, RE_UNTRUSTED, RE_BAD_OUTPUT };
struct RosterIntervalView {
  uint16_t studentId;
  int32_t startDayInclusive;
  int32_t endDayExclusive;
};

// Roster is event-sourced from the log: EV_ROSTER(a=id, b=action) [+ 4 PAYLOAD = 32-byte name for SET].
// ~16 KB object: allocate statically or on the heap, never on a task stack.
class RosterStore : public LogObserver, public IRoster {
 public:
  explicit RosterStore(int16_t utcOffsetMin) : off_(utcOffsetMin) { reset_(); }
  void onReset() override { reset_(); }
  void onRecord(const Event& e) override;
  bool expected(uint16_t id, int32_t day) override;   // untrusted history/overflow => false (never a false Absent)
  bool known(uint16_t id) const { return id >= 1 && id <= MAX_ID && n_[id].len > 0; }
  bool active(uint16_t id) const;
  size_t nameOf(uint16_t id, char out[ROSTER_NAME_MAX + 1]) const;   // NUL-terminated; 0 = unknown or null output
  size_t knownCount() const { return nKnown_; }
  size_t activeCount() const;
  size_t intervalCount() const { return nIv_; }
  bool intervalAt(size_t index, RosterIntervalView& out) const;
  bool knownIdAt(size_t ordinal, uint16_t& outId) const;
  // The parser must be between complete records/groups: a pending RA_SET payload
  // group is untrusted even before a following record proves it malformed.
  bool historyComplete() const override { return !overflow_ && bad_ == 0 && !col_; }
  int16_t utcOffsetMin() const { return off_; }
  bool overflow() const { return overflow_; }
  uint32_t malformed() const { return bad_; }    // replayed roster records that were rejected
  // Writer side: build a commit group (out has room for 5 events); log.append() assigns seq.
  RosterErr planSet(uint16_t id, const char* name, size_t n, uint32_t epoch, Event* out, size_t& cnt) const;
  RosterErr planDeactivate(uint16_t id, uint32_t epoch, Event* out, size_t& cnt) const;
  RosterErr planActivate(uint16_t id, uint32_t epoch, Event* out, size_t& cnt) const;
  static bool validName(const char* s, size_t n);     // 1..32 B, valid UTF-8, no controls, no edge spaces
 private:
  struct Iv { uint16_t id; int32_t from, to; };
  struct Nm { uint8_t len; char s[ROSTER_NAME_MAX]; };
  void reset_();
  bool openIv_(uint16_t id) const;
  void open_(uint16_t id, int32_t day);
  void close_(uint16_t id, int32_t day);
  void applySet_();
  int16_t off_;
  Iv iv_[MAX_INTERVALS];
  Nm n_[MAX_ID + 1];
  size_t nIv_, nKnown_;
  bool overflow_, col_;
  uint32_t bad_;
  uint8_t got_;
  uint16_t pid_;
  int32_t pday_;
  uint64_t w_[4];
};
}
