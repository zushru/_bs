#include "api_logic.h"
#include <string.h>
namespace att {
namespace {
ApiResult res(ApiCode c, ApiReason r = R_NONE, uint32_t after = 0) { ApiResult x = {c, r, after}; return x; }
enum PermKind : uint8_t { P_NONE, P_MANUAL, P_ENROLL, P_READ, P_EXPORT };
inline uint16_t st(HState s) { return (uint16_t)(1u << s); }
const uint16_t ALL = 0x3F;
struct Rule { bool token, setup; uint16_t states; PermKind perm; };
const uint16_t S_OK = 1u << H_OK, S_UN = 1u << H_UNPROVISIONED, S_CLK = 1u << H_CLOCK_FAULT,
               S_STO = 1u << H_STORAGE_FAULT, S_CFG = 1u << H_CONFIG_FAULT, S_SEN = 1u << H_SENSOR_FAULT;
const Rule RULES[EP_COUNT] = {
  /* STATUS     */ {false, false, ALL, P_NONE},
  /* LOGIN      */ {false, false, (uint16_t)(S_OK | S_CLK | S_STO | S_SEN), P_NONE},   // needs a readable PIN hash
  /* LOGOUT     */ {true,  false, ALL, P_NONE},
  /* PIN_CHANGE */ {true,  false, (uint16_t)(S_OK | S_SEN), P_NONE},
  /* PIN_RESET  */ {false, true,  (uint16_t)(S_OK | S_CLK | S_SEN), P_NONE},
  /* DAY        */ {true,  false, ALL, P_READ},
  /* ROSTER_GET */ {true,  false, ALL, P_READ},
  /* ROSTER_SET */ {true,  false, ALL, P_MANUAL},
  /* MARK       */ {true,  false, ALL, P_MANUAL},
  /* NOCLASS    */ {true,  false, ALL, P_MANUAL},
  /* CONFIG_GET */ {true,  false, (uint16_t)(S_OK | S_CLK | S_STO | S_SEN), P_NONE},
  /* CONFIG_PUT */ {true,  false, (uint16_t)(S_OK | S_CLK | S_SEN), P_NONE},
  /* CLOCK      */ {true,  false, (uint16_t)(S_OK | S_CLK | S_SEN), P_NONE},
  /* ENROLL     */ {true,  false, ALL, P_ENROLL},
  /* EXPORT     */ {true,  false, ALL, P_EXPORT},
  /* BACKUP_ACK */ {true,  false, ALL, P_EXPORT},
  /* SETUP      */ {false, true,  S_UN, P_NONE},
  /* RESTORE    */ {false, true,  ALL, P_NONE},                       // physical presence (OLED code) is the proof
  /* ROLLOVER   */ {true,  true,  S_OK, P_NONE},
};
bool isSchedule(const Config& a, const Config& b) {
  return canApply(a, b, true, false) == CH_REFUSED_WINDOW_OPEN;     // refuses only when the schedule differs
}
bool secretsDiffer(const Config& a, const Config& b) {
  return a.pinSet != b.pinSet || memcmp(a.pinHash, b.pinHash, sizeof a.pinHash) != 0 ||
         memcmp(a.pinSalt, b.pinSalt, sizeof a.pinSalt) != 0 || memcmp(a.wifiPass, b.wifiPass, sizeof a.wifiPass) != 0;
}
ApiResult dayChecks(int32_t day, const ApiCtx& c, bool allowFuture, uint8_t session) {
  if (session > 1 || day < 0 || day > 65535) return res(A_INVALID, R_BAD_FIELD);
  if (day < c.termStart || day > c.termEnd) return res(A_INVALID, R_OUT_OF_TERM);
  int32_t today = dayNumber(c.now, c.p.utcOffsetMin);
  if (today < 0) return res(A_LOCKED, R_STATE);
  if (!allowFuture && day > today) return res(A_INVALID, R_FUTURE);
  if (weekdayIndex(day) > 4) return res(A_INVALID, R_NO_SUCH_SESSION);
  return res(A_OK);
}
}  // namespace

ApiResult authorize(Ep ep, HState h, bool tokenValid, bool setupCodeOk) {
  if (ep >= EP_COUNT || h > H_SENSOR_FAULT) return res(A_BAD, R_BAD_FIELD);
  const Rule& r = RULES[ep];
  if (r.token && !tokenValid) return res(A_UNAUTH, R_NO_TOKEN);
  if (r.setup && !setupCodeOk) return res(A_UNAUTH, R_BAD_SETUP);
  if (!(r.states & st(h))) return res(A_LOCKED, R_STATE);
  Perms p = HealthFsm::perms(h);
  bool okp = true;
  switch (r.perm) {
    case P_MANUAL: okp = p.manual; break;
    case P_ENROLL: okp = p.enroll; break;
    case P_READ: okp = p.readHistory; break;
    case P_EXPORT: okp = p.exportOk; break;
    default: break;
  }
  return okp ? res(A_OK) : res(A_LOCKED, R_PERM);
}

// ---------------- PinLockout ----------------
uint32_t PinLockout::lockSeconds(uint16_t fails) {
  if (fails == 0 || fails % 5 != 0) return 0;
  uint32_t s = 60;
  for (uint16_t k = fails / 5; k > 1 && s < 300; k--) s *= 2;
  return s > 300 ? 300 : s;
}
void PinLockout::boot(uint16_t persistedFails, uint32_t nowS) {
  fails_ = persistedFails;
  const uint32_t seconds = lockSeconds(fails_);
  locked_ = seconds != 0;
  until_ = locked_ ? nowS + seconds : 0;   // conservative: full lock again after a reboot
}
LockGate PinLockout::begin(uint32_t nowS) {
  LockGate g = {true, 0};
  if (locked_) {
    const uint32_t remaining = until_ - nowS;
    if (remaining != 0 && remaining < 0x80000000u) {
      g.allowed = false;
      g.retryAfterS = remaining;
      return g;
    }
    locked_ = false;  // exact deadline or a time after it
  }
  if (fails_ < 65535) fails_++;
  return g;
}
void PinLockout::fail(uint32_t nowS) {
  const uint32_t seconds = lockSeconds(fails_);
  if (seconds) {
    until_ = nowS + seconds;
    locked_ = true;
  }
}

// ---------------- TokenTable ----------------
void TokenTable::revokeAll() { memset(s_, 0, sizeof s_); evicted_ = false; }
bool TokenTable::canIssue(const uint8_t rnd[16]) const {
  if (rnd == nullptr) return false;
  uint8_t z = 0;
  for (int i = 0; i < 16; i++) z |= rnd[i];
  if (z == 0) return false;
  for (int i = 0; i < 2; i++) if (s_[i].used && memcmp(s_[i].t, rnd, 16) == 0) return false;
  return true;
}
bool TokenTable::issue(const uint8_t rnd[16], uint32_t nowS) {
  evicted_ = false;
  if (!canIssue(rnd)) return false;
  int slot = -1;
  for (int i = 0; i < 2 && slot < 0; i++) if (!live_(s_[i], nowS)) slot = i;
  if (slot < 0) {                                          // both live: displace the least recently used
    slot = (uint32_t)(nowS - s_[0].last) >= (uint32_t)(nowS - s_[1].last) ? 0 : 1;
    evicted_ = true;
  }
  memcpy(s_[slot].t, rnd, 16); s_[slot].last = nowS; s_[slot].used = true;
  return true;
}
bool TokenTable::check(const uint8_t tok[16], uint32_t nowS) {
  if (tok == nullptr) return false;
  int hit = -1;
  for (int i = 0; i < 2; i++) {
    uint8_t d = 0;
    for (int k = 0; k < 16; k++) d |= (uint8_t)(tok[k] ^ s_[i].t[k]);
    bool match = (d == 0) && live_(s_[i], nowS);
    if (match) hit = i;
  }
  if (hit < 0) return false;
  s_[hit].last = nowS;
  return true;
}
void TokenTable::revoke(const uint8_t tok[16]) {
  if (tok == nullptr) return;
  for (int i = 0; i < 2; i++) if (s_[i].used && ctEqual(s_[i].t, tok, 16)) memset(&s_[i], 0, sizeof s_[i]);
}
int TokenTable::active(uint32_t nowS) const { int n = 0; for (int i = 0; i < 2; i++) if (live_(s_[i], nowS)) n++; return n; }

// ---------------- ReqRing ----------------
bool reqFingerprint(uint8_t op, uint32_t a, uint32_t b, const uint8_t* s, size_t n, uint32_t& outFingerprint) {
  if (n != 0 && s == nullptr) return false;
  uint32_t h = 2166136261u;
  uint8_t hdr[9] = {op, (uint8_t)a, (uint8_t)(a >> 8), (uint8_t)(a >> 16), (uint8_t)(a >> 24),
                    (uint8_t)b, (uint8_t)(b >> 8), (uint8_t)(b >> 16), (uint8_t)(b >> 24)};
  for (int i = 0; i < 9; i++) { h ^= hdr[i]; h *= 16777619u; }
  for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 16777619u; }
  outFingerprint = h;
  return true;
}
ReqResult ReqRing::begin(uint32_t id, uint32_t fp, uint32_t nowS) {
  ReqResult r = {RQ_INVALID, A_BAD, 0, 0};
  if (id == 0) return r;
  for (int i = 0; i < 16; i++) if (e_[i].st && e_[i].id == id) {
    if (e_[i].fp != fp) { r.kind = RQ_MISMATCH; r.code = A_CONFLICT; return r; }
    if (e_[i].st == 1) { r.kind = RQ_INFLIGHT; r.code = A_CONFLICT; return r; }
    r.kind = RQ_REPLAY; r.code = (ApiCode)e_[i].code; r.aux = e_[i].aux; return r;
  }
  int slot = -1;
  for (int i = 0; i < 16 && slot < 0; i++) if (e_[i].st == 0) slot = i;
  if (slot < 0) {                                           // evict the oldest FINISHED entry; never a pending one
    for (int i = 0; i < 16; i++) if (e_[i].st == 2 && (slot < 0 || e_[i].order < e_[slot].order)) slot = i;
  }
  if (slot < 0) { r.kind = RQ_INFLIGHT; r.code = A_CONFLICT; return r; }
  ++nextTicket_;
  if (nextTicket_ == 0) ++nextTicket_;  // reserve zero for non-new results
  e_[slot].id = id; e_[slot].fp = fp; e_[slot].st = 1; e_[slot].t = nowS; e_[slot].order = ++seq_;
  e_[slot].ticket = nextTicket_; e_[slot].code = 0; e_[slot].aux = 0;
  r.kind = RQ_NEW; r.code = A_OK; r.ticket = e_[slot].ticket;
  return r;
}
bool ReqRing::finish(uint32_t id, uint64_t ticket, ApiCode code, uint32_t aux) {
  // ReqRing caches successful commits only. Failed/uncertain operations must
  // be aborted so a retry can execute the handler again.
  if (ticket == 0 || code != A_OK) return false;
  for (int i = 0; i < 16; i++) if (e_[i].st == 1 && e_[i].id == id && e_[i].ticket == ticket) {
    e_[i].st = 2; e_[i].code = code; e_[i].aux = aux; e_[i].order = ++seq_; return true;
  }
  return false;
}
void ReqRing::expire(uint32_t nowS, uint32_t maxAgeS) {
  for (int i = 0; i < 16; i++) if (e_[i].st == 1 && (uint32_t)(nowS - e_[i].t) > maxAgeS) { e_[i].st = 0; e_[i].ticket = 0; }
}
void ReqRing::abort(uint32_t id, uint64_t ticket) {
  if (ticket == 0) return;
  for (int i = 0; i < 16; i++) if (e_[i].st == 1 && e_[i].id == id && e_[i].ticket == ticket) { e_[i].st = 0; e_[i].ticket = 0; return; }
}

// ---------------- builders / gates ----------------
ApiResult buildMark(const MarkReq& r, const ApiCtx& c, const RosterStore& roster, Event* out, size_t& cnt) {
  cnt = 0;
  if (out == nullptr) return res(A_INVALID, R_BAD_FIELD);
  if (r.status > ST_EXCUSED) return res(A_INVALID, R_BAD_FIELD);
  if (r.id < 1 || r.id > c.cap || r.id > MAX_ID) return res(A_INVALID, R_BAD_FIELD);
  if (!roster.historyComplete()) return res(A_LOCKED, R_STATE);
  if (!roster.known(r.id)) return res(A_NOTFOUND, R_UNKNOWN_ID);
  ApiResult d = dayChecks(r.day, c, false, r.session);
  if (d.code != A_OK) return d;
  const Slot& s = c.tt.day[weekdayIndex(r.day)][r.session];
  if (!s.enabled) return res(A_INVALID, R_NO_SUCH_SESSION);
  if (r.day == dayNumber(c.now, c.p.utcOffsetMin) && (r.status == ST_PRESENT || r.status == ST_LATE)) {
    uint16_t m = minuteOfDay(c.now, c.p.utcOffsetMin);
    if (m + c.p.scanLeadMin < s.start) return res(A_INVALID, R_FUTURE);     // cannot mark Present before the window opens
  }
  out[0] = Event{EV_MARK, makeFlags(r.session, r.status, false, false, false), r.id, c.now, 0, r.day};
  cnt = 1;
  return res(A_OK);
}
ApiResult buildNoClass(const NoClassReq& r, const ApiCtx& c, Event* out, size_t& cnt) {
  cnt = 0;
  if (out == nullptr) return res(A_INVALID, R_BAD_FIELD);
  ApiResult d = dayChecks(r.day, c, true, r.session);   // holidays are known in advance: future days allowed
  if (d.code != A_OK) return d;
  if (!r.wholeDay && !c.tt.day[weekdayIndex(r.day)][r.session].enabled) return res(A_INVALID, R_NO_SUCH_SESSION);
  out[0] = Event{r.set ? EV_NOCLASS_SET : EV_NOCLASS_CLEAR, makeFlags(r.session, 0, false, false, r.wholeDay), 0, c.now, 0, r.day};
  cnt = 1;
  return res(A_OK);
}
static ApiResult mapRoster(RosterErr e) {
  switch (e) {
    case RE_OK: case RE_NOOP: return res(A_OK);
    case RE_BAD_ID: return res(A_INVALID, R_BAD_FIELD);
    case RE_BAD_NAME: case RE_BAD_OUTPUT: return res(A_INVALID, R_BAD_FIELD);
    case RE_UNKNOWN: return res(A_NOTFOUND, R_UNKNOWN_ID);
    case RE_UNTRUSTED: return res(A_LOCKED, R_STATE);
    default: return res(A_CONFLICT, R_FULL);
  }
}
ApiResult rosterSet(const RosterStore& r, uint16_t id, const char* name, size_t n, uint32_t now, Event* out, size_t& cnt) {
  return mapRoster(r.planSet(id, name, n, now, out, cnt));
}
ApiResult rosterDeactivate(const RosterStore& r, uint16_t id, uint32_t now, Event* out, size_t& cnt) {
  return mapRoster(r.planDeactivate(id, now, out, cnt));
}
ApiResult rosterActivate(const RosterStore& r, uint16_t id, uint32_t now, Event* out, size_t& cnt) {
  return mapRoster(r.planActivate(id, now, out, cnt));
}
ApiResult gateEnroll(const RosterStore& r, uint16_t id, bool replace, uint16_t cap) {
  if (id < 1 || id > cap || id > MAX_ID) return res(A_INVALID, R_BAD_FIELD);
  if (!r.historyComplete()) return res(A_LOCKED, R_STATE);
  if (!replace && r.known(id)) return res(A_CONFLICT, R_SLOT_TAKEN);   // enroll is not idempotent: never overwrite silently
  if (replace && !r.known(id)) return res(A_NOTFOUND, R_UNKNOWN_ID);
  return res(A_OK);
}
ApiResult gateConfigPut(const Config& cur, const Config& next, const ConfigGateIn& in) {
  if (in.expectGen != in.curGen) return res(A_CONFLICT, R_GEN_MISMATCH);   // stale client learns first
  if (secretsDiffer(cur, next)) return res(A_INVALID, R_SECRET_FIELD);     // PIN and Wi-Fi have their own endpoints
  if (!validateConfig(next)) return res(A_INVALID, R_BAD_FIELD);
  bool sched = isSchedule(cur, next);
  if (sched && in.h == H_CLOCK_FAULT) return res(A_LOCKED, R_CLOCK_UNSAFE);   // window/closure state is not trustworthy
  CfgChange ch = canApply(cur, next, in.windowOpen, in.unclosedPast);
  if (ch == CH_REFUSED_WINDOW_OPEN) return res(A_CONFLICT, R_WINDOW_OPEN);
  if (ch == CH_REFUSED_UNCLOSED) return res(A_CONFLICT, R_UNCLOSED);
  return res(A_OK);
}
ApiResult gateRestore(HState h, bool typedConfirm) {
  bool recovery = h == H_UNPROVISIONED || h == H_CONFIG_FAULT || h == H_STORAGE_FAULT;
  if (!recovery && !typedConfirm) return res(A_INVALID, R_NEED_CONFIRM);   // replacing a healthy device needs intent
  return res(A_OK);
}
bool windowOpenNow(const Timetable& t, const Params& p, uint32_t nowEpoch) { return sessionAt(t, p, nowEpoch) >= 0; }

bool validPin(const char* s, size_t n) {
  if (!s || n != 6) return false;
  for (size_t i = 0; i < 6; i++) if (s[i] < '0' || s[i] > '9') return false;
  return true;
}
bool weakPin(const char* s, size_t n) {
  if (!validPin(s, n)) return true;
  bool same = true, up = true, down = true;
  for (int i = 1; i < 6; i++) {
    if (s[i] != s[0]) same = false;
    if (s[i] != s[i - 1] + 1) up = false;
    if (s[i] != s[i - 1] - 1) down = false;
  }
  return same || up || down;
}
bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n) {
  if (n == 0) return true;
  if (a == nullptr || b == nullptr) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}
}
