#pragma once
#include "roster_store.h"
namespace att {
// Student ID n lives in sensor page n-1: the AS608 addresses pages 0..capacity-1, so "ID = slot" with ID 1..300
// must shift by one or ID 300 would be an invalid page. The adapter must use these two helpers everywhere.
inline uint16_t idToPage(uint16_t id) { return (uint16_t)(id - 1); }   // id >= 1
inline uint16_t pageToId(uint16_t page) { return (uint16_t)(page + 1); }

// Which student IDs currently have a fingerprint template in the sensor. Starts EMPTY AND INVALID: until a
// complete read of the sensor's index finishes, nobody counts as enrolled.
struct Occupancy {
  uint32_t w[10];
  bool valid;
  Occupancy() { clear(); }
  void clear() { for (int i = 0; i < 10; i++) w[i] = 0; valid = false; }
  void begin() { clear(); }                       // start a refresh
  void finish() { valid = true; }                 // call only after every needed page was added
  void setPresent(uint16_t id, bool v);
  bool has(uint16_t id) const;
  size_t count() const;
  // AS608 ReadIndexTable page p (0..3): 32 bytes, bit j of byte i set => slot p*256+i*8+j holds a template
  // [UNVERIFIED: bit order and page size on the real module]. Slots >= capacity are ignored.
  void addIndexPage(uint8_t page, const uint8_t b[32], uint16_t capacity);
  static uint8_t pagesNeeded(uint16_t capacity) { return (uint8_t)((capacity + 255) / 256); }
};

// A student without a template can never scan, so counting them as expected would produce false Absent marks
// (after a restore onto a blank sensor, a lost template, a swapped sensor). They are excluded instead, which is
// visible (see reconcile) and fail-closed. An unreadable or unfinished sensor index excludes everybody.
class EnrolledRoster : public IRoster {
 public:
  EnrolledRoster(RosterStore& r, const Occupancy& o) : r_(r), o_(o) {}
  bool expected(uint16_t id, int32_t day) override { return o_.valid && o_.has(id) && r_.expected(id, day); }
 private:
  RosterStore& r_; const Occupancy& o_;
};

struct Reconcile {
  bool valid;                 // false: occupancy not trustworthy, all counts are 0
  uint16_t matched;           // active students with a template
  uint16_t orphans;           // active students WITHOUT a template: need (re-)enrolling, not expected until then
  uint16_t strays;            // templates whose ID is not a known student: never deleted automatically
  uint16_t firstOrphan, firstStray;   // 0 if none
};
Reconcile reconcile(const RosterStore& r, const Occupancy& o);
uint16_t nextToEnroll(const RosterStore& r, const Occupancy& o, uint16_t after);     // next active student lacking a template; 0 = none
uint16_t firstFreeSlot(const RosterStore& r, const Occupancy& o, uint16_t capacity);  // lowest ID neither known nor occupied; 0 = full
}
