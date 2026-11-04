#include "roster_store.h"
#include <string.h>
namespace att {
void RosterStore::reset_() {
  memset(n_, 0, sizeof n_);
  nIv_ = 0; nKnown_ = 0; overflow_ = false; bad_ = 0; col_ = false; got_ = 0; pid_ = 0; pday_ = 0;
  for (int i = 0; i < 4; i++) w_[i] = 0;
}
bool RosterStore::openIv_(uint16_t id) const {
  for (size_t i = 0; i < nIv_; i++) if (iv_[i].id == id && iv_[i].to == INT32_MAX) return true;
  return false;
}
bool RosterStore::active(uint16_t id) const { return known(id) && openIv_(id); }
size_t RosterStore::activeCount() const {
  size_t c = 0;
  for (uint16_t id = 1; id <= MAX_ID; id++) if (active(id)) c++;
  return c;
}
size_t RosterStore::nameOf(uint16_t id, char out[ROSTER_NAME_MAX + 1]) const {
  if (!known(id)) return 0;
  memcpy(out, n_[id].s, n_[id].len);
  out[n_[id].len] = 0;
  return n_[id].len;
}
void RosterStore::open_(uint16_t id, int32_t day) {
  if (openIv_(id)) return;
  if (nIv_ >= MAX_INTERVALS) { overflow_ = true; return; }
  iv_[nIv_].id = id; iv_[nIv_].from = day; iv_[nIv_].to = INT32_MAX; nIv_++;
}
void RosterStore::close_(uint16_t id, int32_t day) {
  for (size_t i = 0; i < nIv_; i++)
    if (iv_[i].id == id && iv_[i].to == INT32_MAX) { iv_[i].to = day < iv_[i].from ? iv_[i].from : day; return; }
}
void RosterStore::applySet_() {
  uint8_t b[ROSTER_NAME_MAX];
  for (int i = 0; i < (int)ROSTER_NAME_MAX; i++) b[i] = (uint8_t)(w_[i / 8] >> (8 * (i % 8)));
  size_t len = 0;
  while (len < ROSTER_NAME_MAX && b[len] != 0) len++;
  for (size_t i = len; i < ROSTER_NAME_MAX; i++) if (b[i] != 0) { bad_++; return; }   // non-canonical padding
  if (!validName((const char*)b, len)) { bad_++; return; }                           // same rules as the writer
  bool fresh = n_[pid_].len == 0;
  memset(n_[pid_].s, 0, ROSTER_NAME_MAX);
  memcpy(n_[pid_].s, b, len);
  n_[pid_].len = (uint8_t)len;
  if (fresh) { nKnown_++; open_(pid_, pday_); }
}
void RosterStore::onRecord(const Event& e) {
  // Commit-group shape is enforced from the MORE flags: SET = ROSTER(more) + 3 PAYLOAD(more) + 1 PAYLOAD(last);
  // DEACTIVATE/ACTIVATE = a lone record. Anything else is rejected whole (fail-closed: nobody becomes "expected").
  if (e.type == EV_PAYLOAD) {
    if (!col_) return;                                   // payloads of other records (e.g. CLOSE bitmaps)
    if (flagMore(e.flags) != (got_ < 3)) { col_ = false; bad_++; return; }
    w_[got_++] = payloadBits(e);
    if (got_ == 4) { applySet_(); col_ = false; }
    return;
  }
  if (col_) { col_ = false; bad_++; }                    // group cut short
  if (e.type != EV_ROSTER) return;
  int32_t day = dayNumber(e.epoch, off_);
  bool more = flagMore(e.flags);
  if (e.a < 1 || e.a > MAX_ID || day < 0) { bad_++; return; }
  switch (e.b) {
    case RA_SET:
      if (!more) { bad_++; return; }
      col_ = true; got_ = 0; pid_ = e.a; pday_ = day; break;
    case RA_DEACTIVATE:
      if (more || !known(e.a)) { bad_++; return; }
      close_(e.a, day); break;
    case RA_ACTIVATE:
      if (more || !known(e.a)) { bad_++; return; }
      open_(e.a, day); break;
    default: bad_++; break;
  }
}
bool RosterStore::expected(uint16_t id, int32_t day) {
  if (overflow_) return false;
  for (size_t i = 0; i < nIv_; i++)
    if (iv_[i].id == id && iv_[i].from <= day && day < iv_[i].to) return true;
  return false;
}
bool RosterStore::validName(const char* s, size_t n) {
  if (!s || n == 0 || n > ROSTER_NAME_MAX) return false;
  const uint8_t* p = (const uint8_t*)s;
  if (p[0] == ' ' || p[n - 1] == ' ') return false;
  size_t i = 0;
  while (i < n) {
    uint8_t c = p[i];
    if (c < 0x20 || c == 0x7F) return false;
    if (c < 0x80) { i++; continue; }
    size_t len; uint32_t cp;
    if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1Fu; }
    else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0Fu; }
    else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07u; }
    else return false;
    if (i + len > n) return false;
    for (size_t k = 1; k < len; k++) {
      if ((p[i + k] & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (p[i + k] & 0x3Fu);
    }
    if ((len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) || cp > 0x10FFFF ||
        (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    i += len;
  }
  return true;
}
RosterErr RosterStore::planSet(uint16_t id, const char* name, size_t n, uint32_t epoch, Event* out, size_t& cnt) const {
  cnt = 0;
  if (id < 1 || id > MAX_ID) return RE_BAD_ID;
  if (!validName(name, n)) return RE_BAD_NAME;
  if (known(id)) {
    if (n_[id].len == n && memcmp(n_[id].s, name, n) == 0) return RE_NOOP;
  } else if (nIv_ >= MAX_INTERVALS || overflow_) return RE_FULL;
  uint8_t b[ROSTER_NAME_MAX];
  memset(b, 0, sizeof b);
  memcpy(b, name, n);
  out[0] = Event{EV_ROSTER, 0, id, epoch, 0, RA_SET};
  for (int w = 0; w < 4; w++) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)b[w * 8 + i] << (8 * i);
    out[1 + w] = makePayload(v, 0, false);
  }
  cnt = 5;
  return RE_OK;
}
RosterErr RosterStore::planDeactivate(uint16_t id, uint32_t epoch, Event* out, size_t& cnt) const {
  cnt = 0;
  if (id < 1 || id > MAX_ID) return RE_BAD_ID;
  if (!known(id)) return RE_UNKNOWN;
  if (!openIv_(id)) return RE_NOOP;
  out[0] = Event{EV_ROSTER, 0, id, epoch, 0, RA_DEACTIVATE};
  cnt = 1;
  return RE_OK;
}
RosterErr RosterStore::planActivate(uint16_t id, uint32_t epoch, Event* out, size_t& cnt) const {
  cnt = 0;
  if (id < 1 || id > MAX_ID) return RE_BAD_ID;
  if (!known(id)) return RE_UNKNOWN;
  if (openIv_(id)) return RE_NOOP;
  if (nIv_ >= MAX_INTERVALS || overflow_) return RE_FULL;
  out[0] = Event{EV_ROSTER, 0, id, epoch, 0, RA_ACTIVATE};
  cnt = 1;
  return RE_OK;
}
}
