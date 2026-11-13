#include "enrolled_roster.h"
namespace att {
void Occupancy::setPresent(uint16_t id, bool v) {
  if (id < 1 || id > MAX_ID) return;
  if (v) w[id / 32] |= (1u << (id % 32)); else w[id / 32] &= ~(1u << (id % 32));
}
bool Occupancy::has(uint16_t id) const {
  if (id < 1 || id > MAX_ID) return false;
  return (w[id / 32] >> (id % 32)) & 1u;
}
size_t Occupancy::count() const {
  size_t n = 0;
  for (uint16_t id = 1; id <= MAX_ID; id++) if (has(id)) n++;
  return n;
}
void Occupancy::addIndexPage(uint8_t page, const uint8_t b[32], uint16_t capacity) {
  if (page > 3) return;
  for (int i = 0; i < 32; i++)
    for (int j = 0; j < 8; j++)
      if ((b[i] >> j) & 1u) {
        uint32_t slot = (uint32_t)page * 256u + (uint32_t)i * 8u + (uint32_t)j;
        if (slot < capacity) setPresent(pageToId((uint16_t)slot), true);
      }
}
Reconcile reconcile(const RosterStore& r, const Occupancy& o) {
  Reconcile x = {o.valid && r.historyComplete(), 0, 0, 0, 0, 0};
  if (!x.valid) { return x; }
  for (uint16_t id = 1; id <= MAX_ID; id++) {
    bool occ = o.has(id);
    if (r.active(id)) {
      if (occ) x.matched++;
      else { if (!x.orphans) x.firstOrphan = id; x.orphans++; }
    } else if (occ && !r.known(id)) {
      if (!x.strays) x.firstStray = id;
      x.strays++;
    }
  }
  return x;
}
uint16_t nextToEnroll(const RosterStore& r, const Occupancy& o, uint16_t after) {
  if (!o.valid || !r.historyComplete()) return 0;
  for (uint16_t id = (uint16_t)(after + 1); id >= 1 && id <= MAX_ID; id++)
    if (r.active(id) && !o.has(id)) return id;
  return 0;
}
uint16_t firstFreeSlot(const RosterStore& r, const Occupancy& o, uint16_t capacity) {
  if (!o.valid || !r.historyComplete()) return 0;
  for (uint16_t id = 1; id <= capacity && id <= MAX_ID; id++)
    if (!r.known(id) && !o.has(id)) return id;       // IDs are never reused: known (even inactive) means taken
  return 0;
}
}
