#include <limits.h>
#include <unity.h>
#include <string.h>
#include <string>
#include <vector>
#include "api_logic.h"
#include "pin_auth.h"
using namespace att;

void setUp() {}
void tearDown() {}

struct MemStore : IStorage {
  std::vector<uint8_t> d;
  bool sizeChecked(size_t& out) override { out = d.size(); return true; }
  bool read(size_t o, uint8_t* b, size_t n) override { if (o + n > d.size()) return false; memcpy(b, d.data() + o, n); return true; }
  bool append(const uint8_t* b, size_t n) override { d.insert(d.end(), b, b + n); return true; }
  bool truncate(size_t n) override { d.resize(n); return true; }
};
static const HState STATES[6] = {H_OK, H_UNPROVISIONED, H_CLOCK_FAULT, H_STORAGE_FAULT, H_CONFIG_FAULT, H_SENSOR_FAULT};
static int32_t monday() { int32_t d = 200; while (weekdayIndex(d) != 0) d++; return d; }
static ApiCtx ctxAt(int32_t day, int minute) {
  ApiCtx c;
  c.p = Params{480, 30, 30};
  c.tt = defaultTimetable();
  c.termStart = 100; c.termEnd = 400; c.cap = 300;
  c.now = dayStartEpoch(day, 480) + (uint32_t)minute * 60;
  return c;
}
struct Roster {
  MemStore ms; LogStore ls; RosterStore r;
  Roster() : ls(ms), r(480) { ls.open(); ls.attach(&r); }
  void add(uint16_t id, const char* nm) {
    Event e[5]; size_t c;
    r.planSet(id, nm, strlen(nm), dayStartEpoch(150, 480) + 43200, e, c); ls.append(e, c);
  }
  void off(uint16_t id) {
    Event e[5]; size_t c;
    r.planDeactivate(id, dayStartEpoch(160, 480) + 43200, e, c); ls.append(e, c);
  }
};
static Config goodCfg() {
  Config c = defaultConfig();
  c.termStart = 100; c.termEnd = 400; c.pinSet = 1;
  memset(c.pinHash, 0xA5, 32); memset(c.pinSalt, 0x5A, 8);
  strcpy(c.wifiPass, "classroom-ap-77");
  return c;
}

// ---- authorize: independent oracle, full cross product ----
static bool oracle(Ep ep, HState h, bool tok, bool setup, ApiCode& code) {
  bool needTok = false, needSetup = false; uint8_t states = 0x3F;
  auto bit = [](HState s) { return (uint8_t)(1u << s); };
  uint8_t OK = bit(H_OK), UN = bit(H_UNPROVISIONED), CL = bit(H_CLOCK_FAULT), ST = bit(H_STORAGE_FAULT), SE = bit(H_SENSOR_FAULT);
  bool manual = false, enroll = false, read = false, exp = false;
  switch (ep) {
    case EP_STATUS: break;
    case EP_LOGIN: states = OK | CL | ST | SE; break;
    case EP_LOGOUT: needTok = true; break;
    case EP_PIN_CHANGE: needTok = true; states = OK | SE; break;
    case EP_PIN_RESET: needSetup = true; states = OK | CL | SE; break;
    case EP_DAY: case EP_ROSTER_GET: needTok = true; read = true; break;
    case EP_ROSTER_SET: case EP_MARK: case EP_NOCLASS: needTok = true; manual = true; break;
    case EP_CONFIG_GET: needTok = true; states = OK | CL | ST | SE; break;
    case EP_CONFIG_PUT: needTok = true; states = OK | CL | SE; break;
    case EP_CLOCK: needTok = true; states = OK | CL | SE; break;
    case EP_ENROLL: needTok = true; enroll = true; break;
    case EP_EXPORT: case EP_BACKUP_ACK: needTok = true; exp = true; break;
    case EP_SETUP: needSetup = true; states = UN; break;
    case EP_RESTORE: needSetup = true; break;
    case EP_ROLLOVER: needTok = true; needSetup = true; states = OK; break;
    default: return false;
  }
  if (needTok && !tok) { code = A_UNAUTH; return true; }
  if (needSetup && !setup) { code = A_UNAUTH; return true; }
  if (!(states & bit(h))) { code = A_LOCKED; return true; }
  Perms p = HealthFsm::perms(h);
  if ((manual && !p.manual) || (enroll && !p.enroll) || (read && !p.readHistory) || (exp && !p.exportOk)) { code = A_LOCKED; return true; }
  code = A_OK; return true;
}
void test_authorize_matches_oracle_everywhere() {
  for (int e = 0; e < EP_COUNT; e++)
    for (int s = 0; s < 6; s++)
      for (int t = 0; t < 2; t++)
        for (int u = 0; u < 2; u++) {
          ApiCode want;
          TEST_ASSERT_TRUE(oracle((Ep)e, STATES[s], t, u, want));
          ApiResult got = authorize((Ep)e, STATES[s], t, u);
          TEST_ASSERT_EQUAL_INT(want, got.code);
        }
  TEST_ASSERT_EQUAL_INT(A_BAD, authorize(EP_COUNT, H_OK, true, true).code);
  TEST_ASSERT_EQUAL_INT(A_BAD, authorize(EP_MARK, (HState)9, true, true).code);
}
void test_unauthenticated_learns_nothing() {
  for (int s = 0; s < 6; s++) {                         // 401 always wins over 423 for protected endpoints
    TEST_ASSERT_EQUAL_INT(A_UNAUTH, authorize(EP_MARK, STATES[s], false, false).code);
    TEST_ASSERT_EQUAL_INT(A_UNAUTH, authorize(EP_CONFIG_PUT, STATES[s], false, true).code);
    TEST_ASSERT_EQUAL_INT(A_UNAUTH, authorize(EP_RESTORE, STATES[s], true, false).code);   // a token is not a setup code
  }
  TEST_ASSERT_EQUAL_INT(A_OK, authorize(EP_STATUS, H_STORAGE_FAULT, false, false).code);
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, authorize(EP_ROLLOVER, H_OK, true, false).code);     // needs BOTH
  TEST_ASSERT_EQUAL_INT(A_UNAUTH, authorize(EP_ROLLOVER, H_OK, false, true).code);
}
// ---- lockout ----
void test_lockout_schedule_and_cap() {
  PinLockout l; uint32_t now = 1000;
  uint32_t expect[4] = {60, 120, 240, 300};
  for (int round = 0; round < 4; round++) {
    for (int i = 0; i < 5; i++) {
      LockGate g = l.begin(now);
      TEST_ASSERT_TRUE(g.allowed);
      l.fail(now);
    }
    LockGate g = l.begin(now);
    TEST_ASSERT_FALSE(g.allowed);
    TEST_ASSERT_EQUAL_UINT32(expect[round], g.retryAfterS);
    TEST_ASSERT_FALSE(l.begin(now + expect[round] - 1).allowed);
    now += expect[round];
  }
  for (int round = 0; round < 3; round++) {              // stays at 300 s
    for (int i = 0; i < 5; i++) { TEST_ASSERT_TRUE(l.begin(now).allowed); l.fail(now); }
    TEST_ASSERT_EQUAL_UINT32(300, l.begin(now).retryAfterS);
    now += 300;
  }
}
void test_lockout_success_resets_and_refusals_do_not_count() {
  PinLockout l;
  for (int i = 0; i < 4; i++) { l.begin(10); l.fail(10); }
  TEST_ASSERT_EQUAL_UINT16(4, l.fails());
  TEST_ASSERT_TRUE(l.begin(10).allowed);                 // 5th attempt succeeds
  l.success();
  TEST_ASSERT_EQUAL_UINT16(0, l.fails());
  for (int i = 0; i < 5; i++) { l.begin(20); l.fail(20); }
  uint16_t f = l.fails();
  for (int i = 0; i < 50; i++) TEST_ASSERT_FALSE(l.begin(21).allowed);   // hammering while locked
  TEST_ASSERT_EQUAL_UINT16(f, l.fails());                // does not escalate the counter
}
void test_lockout_counter_is_persisted_before_verify() {
  PinLockout l;
  for (int i = 0; i < 4; i++) { l.begin(0); l.fail(0); }
  LockGate g = l.begin(0);                               // attempt 5 starts...
  TEST_ASSERT_TRUE(g.allowed);
  TEST_ASSERT_EQUAL_UINT16(5, l.fails());                // ...and is already counted before any verify result
  PinLockout after;                                      // power cut right here: only fails() was persisted
  after.boot(l.fails(), 500);
  TEST_ASSERT_FALSE(after.begin(500).allowed);
  TEST_ASSERT_EQUAL_UINT32(60, after.begin(500).retryAfterS);
  TEST_ASSERT_TRUE(after.begin(560).allowed);
}
void test_lockout_boot_restore_and_saturation() {
  PinLockout a; a.boot(7, 100);
  TEST_ASSERT_TRUE(a.begin(100).allowed);                // 7 is not a multiple of 5: not locked
  PinLockout b; b.boot(10, 100);
  TEST_ASSERT_EQUAL_UINT32(120, b.begin(100).retryAfterS);
  PinLockout c; c.boot(65535, 0);
  TEST_ASSERT_EQUAL_UINT32(300, PinLockout::lockSeconds(65535));
  PinLockout d; d.boot(65530, 5);
  TEST_ASSERT_TRUE(d.begin(1000).allowed);
  for (int i = 0; i < 20; i++) { d.begin(1000); d.fail(1000); }
  TEST_ASSERT_EQUAL_UINT16(65535, d.fails());            // saturates, never wraps to 0
  TEST_ASSERT_EQUAL_UINT32(0, PinLockout::lockSeconds(0));
  TEST_ASSERT_EQUAL_UINT32(0, PinLockout::lockSeconds(3));
}
void test_lockout_deadline_wrapping_to_zero_does_not_skip_lockout() {
  const uint32_t now = 0xFFFFFFFFu - 59u;  // a 60-second deadline wraps exactly to zero

  PinLockout afterFailure;
  afterFailure.boot(4, now);
  TEST_ASSERT_TRUE(afterFailure.begin(now).allowed);
  TEST_ASSERT_EQUAL_UINT16(5, afterFailure.fails());
  afterFailure.fail(now);
  LockGate stillLocked = afterFailure.begin(now + 1u);
  TEST_ASSERT_FALSE(stillLocked.allowed);
  TEST_ASSERT_EQUAL_UINT32(59, stillLocked.retryAfterS);
  TEST_ASSERT_TRUE(afterFailure.begin(0u).allowed);  // exact deadline: lock expires

  PinLockout afterReboot;
  afterReboot.boot(5, now);
  LockGate rebootLocked = afterReboot.begin(now + 1u);
  TEST_ASSERT_FALSE(rebootLocked.allowed);
  TEST_ASSERT_EQUAL_UINT32(59, rebootLocked.retryAfterS);
  TEST_ASSERT_TRUE(afterReboot.begin(0u).allowed);
}
// ---- tokens ----
static void tk(uint8_t* t, uint8_t seed) { for (int i = 0; i < 16; i++) t[i] = (uint8_t)(seed + i * 7); }
void test_token_basics_and_expiry_boundary() {
  TokenTable t; uint8_t a[16];
  tk(a, 1);
  TEST_ASSERT_TRUE(t.issue(a, 100));
  TEST_ASSERT_TRUE(t.check(a, 100 + TOKEN_IDLE_S));       // exactly 900 s idle: still valid
  TEST_ASSERT_TRUE(t.check(a, 100 + TOKEN_IDLE_S + TOKEN_IDLE_S));   // refreshed by the previous check
  TEST_ASSERT_FALSE(t.check(a, 100 + 2 * TOKEN_IDLE_S + TOKEN_IDLE_S + 1));   // 901 s idle: expired
  TEST_ASSERT_EQUAL_INT(0, t.active(5000));
  TokenTable u; u.issue(a, 50);
  TEST_ASSERT_FALSE(u.check(a, 49));                      // time going backwards is treated as expired
}
void test_token_rejects_wrong_zero_and_neighbours() {
  TokenTable t; uint8_t a[16], z[16] = {0};
  TEST_ASSERT_FALSE(t.check(z, 0));                       // empty table must not accept an all-zero token
  tk(a, 9);
  t.issue(a, 10);
  TEST_ASSERT_FALSE(t.check(z, 11));
  for (int byte = 0; byte < 16; byte++)
    for (int bit = 0; bit < 8; bit++) { uint8_t b[16]; memcpy(b, a, 16); b[byte] ^= (uint8_t)(1u << bit); TEST_ASSERT_FALSE(t.check(b, 11)); }
  TEST_ASSERT_TRUE(t.check(a, 11));
  TEST_ASSERT_FALSE(t.issue(z, 12));                      // RNG failure never creates a session
  TEST_ASSERT_FALSE(t.issue(a, 12));                      // duplicate token
}
void test_token_can_issue_rejects_zero_and_duplicate_without_mutation() {
  TokenTable t; uint8_t zero[16] = {0}; uint8_t a[16]; tk(a, 41);
  TEST_ASSERT_FALSE(t.canIssue(nullptr));
  TEST_ASSERT_FALSE(t.canIssue(zero));
  TEST_ASSERT_EQUAL_INT(0, t.active(10));
  TEST_ASSERT_TRUE(t.issue(a, 10));
  TEST_ASSERT_FALSE(t.canIssue(a));
  TEST_ASSERT_EQUAL_INT(1, t.active(11));
  TEST_ASSERT_TRUE(t.check(a, 11));
}
void test_token_can_issue_accepts_distinct_token_without_mutating_lru() {
  TokenTable t; uint8_t a[16], b[16], c[16];
  tk(a, 1); tk(b, 2); tk(c, 3);
  TEST_ASSERT_TRUE(t.issue(a, 1));
  TEST_ASSERT_TRUE(t.issue(b, 2));
  TEST_ASSERT_TRUE(t.check(a, 3));  // a is MRU; b is LRU
  TEST_ASSERT_TRUE(t.canIssue(c));  // preflight must not change LRU order
  TEST_ASSERT_EQUAL_INT(2, t.active(4));
  TEST_ASSERT_TRUE(t.issue(c, 4));  // b must still be the displaced session
  TEST_ASSERT_TRUE(t.lastIssueEvicted());
  TEST_ASSERT_TRUE(t.check(a, 5));
  TEST_ASSERT_FALSE(t.check(b, 5));
  TEST_ASSERT_TRUE(t.check(c, 5));
}
void test_token_two_sessions_and_lru_eviction() {
  TokenTable t; uint8_t a[16], b[16], c[16];
  tk(a, 1); tk(b, 2); tk(c, 3);
  TEST_ASSERT_TRUE(t.issue(a, 0));
  TEST_ASSERT_TRUE(t.issue(b, 10));
  TEST_ASSERT_FALSE(t.lastIssueEvicted());
  TEST_ASSERT_EQUAL_INT(2, t.active(10));
  TEST_ASSERT_TRUE(t.check(a, 20));                       // a used more recently than b
  TEST_ASSERT_TRUE(t.issue(c, 30));                       // evicts the least recently used: b
  TEST_ASSERT_TRUE(t.lastIssueEvicted());
  TEST_ASSERT_TRUE(t.check(a, 31));
  TEST_ASSERT_FALSE(t.check(b, 31));
  TEST_ASSERT_TRUE(t.check(c, 31));
  t.revoke(a);
  TEST_ASSERT_FALSE(t.check(a, 32));
  t.revokeAll();
  TEST_ASSERT_EQUAL_INT(0, t.active(32));
  TokenTable e; e.issue(a, 0); e.issue(b, 1000);          // an expired session is reused, not counted as an eviction
  TEST_ASSERT_TRUE(e.issue(c, 1000 + 1));
  TEST_ASSERT_FALSE(e.lastIssueEvicted());
}
void test_token_null_inputs_are_rejected_without_session_mutation() {
  TokenTable t;
  uint8_t a[16], b[16], c[16];
  tk(a, 77); tk(b, 78); tk(c, 79);
  TEST_ASSERT_TRUE(t.issue(a, 100));
  TEST_ASSERT_TRUE(t.issue(b, 101));
  TEST_ASSERT_TRUE(t.check(a, 102));  // b is now least recently used
  TEST_ASSERT_FALSE(t.check(nullptr, 103));
  t.revoke(nullptr);
  TEST_ASSERT_EQUAL_INT(2, t.active(103));
  TEST_ASSERT_TRUE(t.issue(c, 104));
  TEST_ASSERT_TRUE(t.lastIssueEvicted());
  TEST_ASSERT_TRUE(t.check(a, 105));   // null operations did not alter LRU order
  TEST_ASSERT_FALSE(t.check(b, 105));
  TEST_ASSERT_TRUE(t.check(c, 105));
  TEST_ASSERT_FALSE(ctEqual(nullptr, a, sizeof a));
  TEST_ASSERT_FALSE(ctEqual(a, nullptr, sizeof a));
  TEST_ASSERT_TRUE(ctEqual(nullptr, nullptr, 0));
}
// ---- request ring ----
void test_reqring_replay_mismatch_inflight() {
  ReqRing r;
  TEST_ASSERT_EQUAL_INT(RQ_INVALID, r.begin(0, 1, 0).kind);
  ReqResult first = r.begin(7, 111, 0);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, first.kind);
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, r.begin(7, 111, 1).kind);
  TEST_ASSERT_EQUAL_INT(RQ_MISMATCH, r.begin(7, 222, 1).kind);
  r.finish(7, first.ticket, A_OK, 42);
  ReqResult x = r.begin(7, 111, 2);
  TEST_ASSERT_EQUAL_INT(RQ_REPLAY, x.kind);
  TEST_ASSERT_EQUAL_INT(A_OK, x.code);
  TEST_ASSERT_EQUAL_UINT32(42, x.aux);
  TEST_ASSERT_EQUAL_INT(RQ_MISMATCH, r.begin(7, 999, 2).kind);     // same id, different request: never replay it
  ReqResult retry = r.begin(8, 5, 2);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, retry.kind);
  r.abort(8, retry.ticket);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, r.begin(8, 5, 3).kind);            // a failed request can be retried and re-run
}
void test_reqring_evicted_pending_ticket_cannot_finish_or_abort_reused_id() {
  ReqRing r;
  ReqResult oldRequest = r.begin(77, 1001, 0);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, oldRequest.kind);
  TEST_ASSERT_TRUE(oldRequest.ticket != 0);

  r.expire(31, 30);  // handler timed out; its completion may still arrive late
  ReqResult newRequest = r.begin(77, 1001, 32);  // same request ID and fingerprint, new execution attempt
  TEST_ASSERT_EQUAL_INT(RQ_NEW, newRequest.kind);
  TEST_ASSERT_TRUE(newRequest.ticket != 0);
  TEST_ASSERT_TRUE(oldRequest.ticket != newRequest.ticket);

  r.finish(77, oldRequest.ticket, A_OK, 111);
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, r.begin(77, 1001, 33).kind);
  r.abort(77, oldRequest.ticket);
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, r.begin(77, 1001, 34).kind);

  r.finish(77, newRequest.ticket, A_OK, 222);
  ReqResult replay = r.begin(77, 1001, 35);
  TEST_ASSERT_EQUAL_INT(RQ_REPLAY, replay.kind);
  TEST_ASSERT_EQUAL_UINT32(222, replay.aux);
}

void test_reqring_does_not_cache_failed_results() {
  ReqRing r;
  ReqResult first = r.begin(91, 0xAABBCCDDu, 10);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, first.kind);
  TEST_ASSERT_FALSE(r.finish(91, first.ticket, A_INVALID, 123));
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, r.begin(91, 0xAABBCCDDu, 11).kind);

  r.abort(91, first.ticket);
  ReqResult retry = r.begin(91, 0xAABBCCDDu, 12);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, retry.kind);
  TEST_ASSERT_TRUE(r.finish(91, retry.ticket, A_OK, 456));

  ReqResult replay = r.begin(91, 0xAABBCCDDu, 13);
  TEST_ASSERT_EQUAL_INT(RQ_REPLAY, replay.kind);
  TEST_ASSERT_EQUAL_INT(A_OK, replay.code);
  TEST_ASSERT_EQUAL_UINT32(456, replay.aux);
}

void test_reqring_eviction_and_leak_guard() {
  ReqRing r;
  for (uint32_t i = 1; i <= 16; i++) { ReqResult x = r.begin(i, i, 0); TEST_ASSERT_EQUAL_INT(RQ_NEW, x.kind); r.finish(i, x.ticket, A_OK, i); }
  TEST_ASSERT_EQUAL_INT(RQ_REPLAY, r.begin(1, 1, 0).kind);
  ReqResult seventeenth = r.begin(100, 100, 0);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, seventeenth.kind);                  // 17th id evicts the OLDEST finished entry (1)
  r.finish(100, seventeenth.ticket, A_OK, 0);
  ReqResult oneAgain = r.begin(1, 1, 0);                          // documented limit: an evicted id would run again
  TEST_ASSERT_EQUAL_INT(RQ_NEW, oneAgain.kind);
  r.abort(1, oneAgain.ticket);
  ReqRing p;
  for (uint32_t i = 1; i <= 16; i++) p.begin(i, i, 0);              // 16 handlers that never finished
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, p.begin(99, 1, 5).kind);       // ring is jammed: refuse rather than evict a pending op
  p.expire(100, 30);
  TEST_ASSERT_EQUAL_INT(RQ_NEW, p.begin(99, 1, 100).kind);
  ReqRing q; q.begin(1, 1, 0);
  q.expire(10, 30);
  TEST_ASSERT_EQUAL_INT(RQ_INFLIGHT, q.begin(1, 1, 11).kind);       // young pending entries survive
  uint32_t fpAb = 0, fpAc = 0, fpOtherOp = 0;
  TEST_ASSERT_TRUE(reqFingerprint(1, 2, 3, (const uint8_t*)"ab", 2, fpAb));
  TEST_ASSERT_TRUE(reqFingerprint(1, 2, 3, (const uint8_t*)"ac", 2, fpAc));
  TEST_ASSERT_TRUE(reqFingerprint(2, 2, 3, nullptr, 0, fpOtherOp));
  TEST_ASSERT_TRUE(fpAb != fpAc);
  TEST_ASSERT_TRUE(fpAb != fpOtherOp);
}
// ---- marks ----
void test_mark_valid_and_event_shape() {
  Roster g; g.add(5, "Ana");
  int32_t mon = monday();
  ApiCtx c = ctxAt(mon + 1, 17 * 60);                      // Tuesday evening
  MarkReq m = {5, (uint16_t)mon, 1, ST_EXCUSED};           // Monday PM, yesterday
  Event ev[1]; size_t n = 9;
  ApiResult r = buildMark(m, c, g.r, ev, n);
  TEST_ASSERT_EQUAL_INT(A_OK, r.code);
  TEST_ASSERT_EQUAL_UINT32(1, n);
  TEST_ASSERT_EQUAL_INT(EV_MARK, ev[0].type);
  TEST_ASSERT_EQUAL_UINT16(5, ev[0].a);
  TEST_ASSERT_EQUAL_UINT16(mon, ev[0].b);
  TEST_ASSERT_EQUAL_UINT8(1, flagSession(ev[0].flags));
  TEST_ASSERT_EQUAL_UINT8(ST_EXCUSED, flagStatus(ev[0].flags));
  TEST_ASSERT_EQUAL_UINT32(c.now, ev[0].epoch);
  TEST_ASSERT_FALSE(flagMore(ev[0].flags));
}
void test_mark_rejections() {
  Roster g; g.add(5, "Ana"); g.add(6, "Ben"); g.off(6);
  int32_t mon = monday();
  Event ev[1]; size_t n;
  ApiCtx c = ctxAt(mon + 2, 10 * 60);                      // Wednesday 10:00
  MarkReq ok = {5, (uint16_t)(mon + 1), 0, ST_PRESENT};
  TEST_ASSERT_EQUAL_INT(A_OK, buildMark(ok, c, g.r, ev, n).code);
  MarkReq m = ok; m.status = 4;           TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(m, c, g.r, ev, n).code);
  m = ok; m.session = 2;                  TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(m, c, g.r, ev, n).code);
  m = ok; m.id = 0;                       TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(m, c, g.r, ev, n).code);
  m = ok; m.id = 301;                     TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(m, c, g.r, ev, n).code);
  m = ok; m.id = 9;                       { ApiResult r = buildMark(m, c, g.r, ev, n); TEST_ASSERT_EQUAL_INT(A_NOTFOUND, r.code); TEST_ASSERT_EQUAL_UINT32(0, n); }
  m = ok; m.day = (uint16_t)(mon + 3);    { ApiResult r = buildMark(m, c, g.r, ev, n); TEST_ASSERT_EQUAL_INT(R_FUTURE, r.reason); }
  m = ok; m.day = 99;                     { ApiResult r = buildMark(m, c, g.r, ev, n); TEST_ASSERT_EQUAL_INT(R_OUT_OF_TERM, r.reason); }
  { ApiCtx late = ctxAt(401, 600); late.termEnd = 400; m = ok; m.day = 401; TEST_ASSERT_EQUAL_INT(R_OUT_OF_TERM, buildMark(m, late, g.r, ev, n).reason); }
  m = ok; m.day = (uint16_t)(mon + 5);    TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(m, ctxAt(mon + 9, 600), g.r, ev, n).code);   // Saturday
  { ApiCtx nopm = c; nopm.tt.day[1][1].enabled = false; m = ok; m.day = (uint16_t)(mon + 1); m.session = 1; TEST_ASSERT_EQUAL_INT(R_NO_SUCH_SESSION, buildMark(m, nopm, g.r, ev, n).reason); }
  m = ok; m.id = 6;                       TEST_ASSERT_EQUAL_INT(A_OK, buildMark(m, c, g.r, ev, n).code);     // deactivated students can still be corrected
  ApiCtx clockLost = c; clockLost.now = 5;                 // RTC before 2020
  TEST_ASSERT_EQUAL_INT(A_LOCKED, buildMark(ok, clockLost, g.r, ev, n).code);
}
void test_mark_today_window_rule() {
  Roster g; g.add(5, "Ana");
  int32_t mon = monday();
  Event ev[1]; size_t n;
  MarkReq pm = {5, (uint16_t)mon, 1, ST_PRESENT};          // today's PM session, window opens 13:00
  TEST_ASSERT_EQUAL_INT(R_FUTURE, buildMark(pm, ctxAt(mon, 12 * 60 + 59), g.r, ev, n).reason);
  TEST_ASSERT_EQUAL_INT(A_OK, buildMark(pm, ctxAt(mon, 13 * 60), g.r, ev, n).code);
  MarkReq late = pm; late.status = ST_LATE;
  TEST_ASSERT_EQUAL_INT(R_FUTURE, buildMark(late, ctxAt(mon, 9 * 60), g.r, ev, n).reason);
  MarkReq ex = pm; ex.status = ST_EXCUSED;                 // Excused and Clear are allowed before the window opens
  TEST_ASSERT_EQUAL_INT(A_OK, buildMark(ex, ctxAt(mon, 9 * 60), g.r, ev, n).code);
  ex.status = ST_CLEAR;
  TEST_ASSERT_EQUAL_INT(A_OK, buildMark(ex, ctxAt(mon, 9 * 60), g.r, ev, n).code);
  pm.day = (uint16_t)(mon + 1);
  TEST_ASSERT_EQUAL_INT(R_FUTURE, buildMark(pm, ctxAt(mon, 23 * 60), g.r, ev, n).reason);   // tomorrow never
}
// ---- no-class ----
void test_noclass_rules_and_shape() {
  int32_t mon = monday();
  Event ev[1]; size_t n;
  ApiCtx c = ctxAt(mon, 600);
  NoClassReq r = {(uint16_t)(mon + 2), 0, true, true};      // future holiday: allowed
  TEST_ASSERT_EQUAL_INT(A_OK, buildNoClass(r, c, ev, n).code);
  TEST_ASSERT_EQUAL_INT(EV_NOCLASS_SET, ev[0].type);
  TEST_ASSERT_TRUE(flagWholeDay(ev[0].flags));
  TEST_ASSERT_EQUAL_UINT16(mon + 2, ev[0].b);
  r.set = false; r.wholeDay = false; r.session = 1;
  TEST_ASSERT_EQUAL_INT(A_OK, buildNoClass(r, c, ev, n).code);
  TEST_ASSERT_EQUAL_INT(EV_NOCLASS_CLEAR, ev[0].type);
  TEST_ASSERT_FALSE(flagWholeDay(ev[0].flags));
  TEST_ASSERT_EQUAL_UINT8(1, flagSession(ev[0].flags));
  NoClassReq bad = {(uint16_t)(mon + 5), 0, true, true};    // Saturday
  TEST_ASSERT_EQUAL_INT(A_INVALID, buildNoClass(bad, c, ev, n).code);
  bad = NoClassReq{450, 0, true, true};                     // after term end
  TEST_ASSERT_EQUAL_INT(R_OUT_OF_TERM, buildNoClass(bad, c, ev, n).reason);
  ApiCtx dis = c; dis.tt.day[2][1].enabled = false;
  NoClassReq pm = {(uint16_t)(mon + 2), 1, false, true};
  TEST_ASSERT_EQUAL_INT(R_NO_SUCH_SESSION, buildNoClass(pm, dis, ev, n).reason);
  pm.wholeDay = true;                                        // whole-day ignores a disabled session
  TEST_ASSERT_EQUAL_INT(A_OK, buildNoClass(pm, dis, ev, n).code);
  TEST_ASSERT_EQUAL_UINT32(1, n);
}
// ---- roster / enroll ----
void test_roster_mapping_and_idempotency() {
  Roster g;
  Event ev[5]; size_t n;
  TEST_ASSERT_EQUAL_INT(A_OK, rosterSet(g.r, 1, "Ana", 3, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_UINT32(5, n);
  g.add(1, "Ana");
  ApiResult same = rosterSet(g.r, 1, "Ana", 3, 5000, ev, n);
  TEST_ASSERT_EQUAL_INT(A_OK, same.code);
  TEST_ASSERT_EQUAL_UINT32(0, n);                            // replay of the same op writes nothing
  TEST_ASSERT_EQUAL_INT(A_INVALID, rosterSet(g.r, 0, "Ana", 3, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_INT(A_INVALID, rosterSet(g.r, 2, " bad", 4, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_INT(A_NOTFOUND, rosterDeactivate(g.r, 9, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_INT(A_OK, rosterDeactivate(g.r, 1, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_UINT32(1, n);
  g.off(1);
  TEST_ASSERT_EQUAL_INT(A_OK, rosterDeactivate(g.r, 1, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_UINT32(0, n);
  TEST_ASSERT_EQUAL_INT(A_OK, rosterActivate(g.r, 1, 5000, ev, n).code);
  TEST_ASSERT_EQUAL_UINT32(1, n);
}
void test_roster_and_manual_mark_gates_fail_closed_on_incomplete_history() {
  Roster g; g.add(1, "Ana");
  ApiCtx c = ctxAt(monday() + 1, 17 * 60);
  g.r.onRecord(Event{EV_ROSTER, FL_MORE, 1, c.now, 0, RA_SET});
  g.r.onRecord(makePayload(0x0000000000000041ull, 0, true));
  TEST_ASSERT_EQUAL_UINT32(0, g.r.malformed());
  TEST_ASSERT_FALSE(g.r.historyComplete());

  Event events[5]; size_t count = 99;
  TEST_ASSERT_EQUAL_INT(A_LOCKED,
      buildMark(MarkReq{1, (uint16_t)monday(), 0, ST_EXCUSED}, c,
                g.r, events, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  count = 99;
  TEST_ASSERT_EQUAL_INT(A_LOCKED, rosterSet(g.r, 2, "Ben", 3, c.now, events, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  count = 99;
  TEST_ASSERT_EQUAL_INT(A_LOCKED, rosterDeactivate(g.r, 1, c.now, events, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  count = 99;
  TEST_ASSERT_EQUAL_INT(A_LOCKED, rosterActivate(g.r, 1, c.now, events, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  TEST_ASSERT_EQUAL_INT(A_LOCKED, gateEnroll(g.r, 2, false, 300).code);
  TEST_ASSERT_EQUAL_UINT32(5, g.ls.count());
}

void test_enroll_gate() {
  Roster g; g.add(5, "Ana");
  TEST_ASSERT_EQUAL_INT(A_OK, gateEnroll(g.r, 6, false, 300).code);
  TEST_ASSERT_EQUAL_INT(R_SLOT_TAKEN, gateEnroll(g.r, 5, false, 300).reason);   // a replayed enroll must not overwrite a finger
  TEST_ASSERT_EQUAL_INT(A_OK, gateEnroll(g.r, 5, true, 300).code);
  TEST_ASSERT_EQUAL_INT(A_NOTFOUND, gateEnroll(g.r, 6, true, 300).code);
  TEST_ASSERT_EQUAL_INT(A_INVALID, gateEnroll(g.r, 0, false, 300).code);
  TEST_ASSERT_EQUAL_INT(A_INVALID, gateEnroll(g.r, 61, false, 60).code);        // beyond the sensor capacity in use
  TEST_ASSERT_EQUAL_INT(A_INVALID, gateEnroll(g.r, 301, false, 400).code);
}
// ---- config ----
static ConfigGateIn gin(HState h = H_OK, bool win = false, bool unc = false, uint32_t exp = 4, uint32_t cur = 4) {
  ConfigGateIn i; i.h = h; i.expectGen = exp; i.curGen = cur; i.windowOpen = win; i.unclosedPast = unc; return i;
}
void test_config_gate_order_and_rules() {
  Config cur = goodCfg(), nx = goodCfg();
  TEST_ASSERT_EQUAL_INT(A_OK, gateConfigPut(cur, nx, gin()).code);                              // no change
  nx.soundMode = 2;
  TEST_ASSERT_EQUAL_INT(A_OK, gateConfigPut(cur, nx, gin(H_OK, true, true)).code);              // non-schedule: always fine
  Config bad = nx; bad.soundMode = 9;
  ApiResult r = gateConfigPut(cur, bad, gin(H_OK, false, false, 3, 4));
  TEST_ASSERT_EQUAL_INT(A_CONFLICT, r.code);                                                    // stale generation wins over 422
  TEST_ASSERT_EQUAL_INT(R_GEN_MISMATCH, r.reason);
  TEST_ASSERT_EQUAL_INT(A_INVALID, gateConfigPut(cur, bad, gin()).code);
  Config s = goodCfg(); s.pinHash[0] ^= 1;
  TEST_ASSERT_EQUAL_INT(R_SECRET_FIELD, gateConfigPut(cur, s, gin()).reason);
  s = goodCfg(); strcpy(s.wifiPass, "another-passphrase");
  TEST_ASSERT_EQUAL_INT(R_SECRET_FIELD, gateConfigPut(cur, s, gin()).reason);
  s = goodCfg(); memset(s.pinHash, 0, 32);                                                      // adapter forgot to copy secrets
  TEST_ASSERT_EQUAL_INT(A_INVALID, gateConfigPut(cur, s, gin()).code);
  Config t = goodCfg(); t.params.lateMin = 25;
  TEST_ASSERT_EQUAL_INT(R_WINDOW_OPEN, gateConfigPut(cur, t, gin(H_OK, true, false)).reason);
  TEST_ASSERT_EQUAL_INT(R_UNCLOSED, gateConfigPut(cur, t, gin(H_OK, false, true)).reason);
  TEST_ASSERT_EQUAL_INT(A_OK, gateConfigPut(cur, t, gin(H_OK, false, false)).code);
  Config u = goodCfg(); u.tt.day[0][0].end += 5;
  TEST_ASSERT_EQUAL_INT(A_CONFLICT, gateConfigPut(cur, u, gin(H_SENSOR_FAULT, true, false)).code);
  TEST_ASSERT_EQUAL_INT(A_LOCKED, gateConfigPut(cur, t, gin(H_CLOCK_FAULT)).code);               // schedule change with an untrusted clock
  TEST_ASSERT_EQUAL_INT(R_CLOCK_UNSAFE, gateConfigPut(cur, t, gin(H_CLOCK_FAULT)).reason);
  TEST_ASSERT_EQUAL_INT(A_OK, gateConfigPut(cur, nx, gin(H_CLOCK_FAULT)).code);                  // sound change is fine
}
void test_restore_gate_and_helpers() {
  TEST_ASSERT_EQUAL_INT(A_OK, gateRestore(H_UNPROVISIONED, false).code);
  TEST_ASSERT_EQUAL_INT(A_OK, gateRestore(H_CONFIG_FAULT, false).code);
  TEST_ASSERT_EQUAL_INT(A_OK, gateRestore(H_STORAGE_FAULT, false).code);
  TEST_ASSERT_EQUAL_INT(R_NEED_CONFIRM, gateRestore(H_OK, false).reason);
  TEST_ASSERT_EQUAL_INT(R_NEED_CONFIRM, gateRestore(H_SENSOR_FAULT, false).reason);
  TEST_ASSERT_EQUAL_INT(A_OK, gateRestore(H_OK, true).code);
  TEST_ASSERT_TRUE(validPin("483920", 6));
  TEST_ASSERT_FALSE(validPin("48392", 5));
  TEST_ASSERT_FALSE(validPin("4839201", 7));
  TEST_ASSERT_FALSE(validPin("48392a", 6));
  TEST_ASSERT_FALSE(validPin(nullptr, 6));
  TEST_ASSERT_TRUE(weakPin("111111", 6));
  TEST_ASSERT_TRUE(weakPin("123456", 6));
  TEST_ASSERT_TRUE(weakPin("987654", 6));
  TEST_ASSERT_TRUE(weakPin("12a456", 6));
  TEST_ASSERT_FALSE(weakPin("135792", 6));
  TEST_ASSERT_FALSE(weakPin("483920", 6));
  uint8_t a[6] = {'1', '2', '3', '4', '5', '6'}, b[6] = {'1', '2', '3', '4', '5', '7'};
  TEST_ASSERT_TRUE(ctEqual(a, a, 6));
  TEST_ASSERT_FALSE(ctEqual(a, b, 6));
  TEST_ASSERT_TRUE(ctEqual(a, b, 5));
  TEST_ASSERT_TRUE(ctEqual(a, b, 0));
}
void test_window_open_now() {
  int32_t mon = monday();
  Timetable t = defaultTimetable(); Params p = {480, 30, 30};
  TEST_ASSERT_FALSE(windowOpenNow(t, p, dayStartEpoch(mon, 480) + (7 * 60 + 29) * 60));
  TEST_ASSERT_TRUE(windowOpenNow(t, p, dayStartEpoch(mon, 480) + (7 * 60 + 30) * 60));
  TEST_ASSERT_TRUE(windowOpenNow(t, p, dayStartEpoch(mon, 480) + (12 * 60 + 29) * 60));
  TEST_ASSERT_FALSE(windowOpenNow(t, p, dayStartEpoch(mon, 480) + (12 * 60 + 30) * 60));
  TEST_ASSERT_FALSE(windowOpenNow(t, p, dayStartEpoch(mon + 5, 480) + 9 * 3600));          // Saturday
}


// ---- enforced PIN-attempt sequencing (new adapter boundary) ----
struct FakePinStore : IPinAttemptStore {
  uint16_t persisted;
  bool loadOk;
  int failPersistAt;
  int persistCalls;
  std::vector<char>* order;
  FakePinStore() : persisted(0), loadOk(true), failPersistAt(-1), persistCalls(0), order(nullptr) {}
  bool loadFailureCount(uint16_t& out) override {
    if (order) order->push_back('l');
    if (!loadOk) return false;
    out = persisted;
    return true;
  }
  bool persistFailureCount(uint16_t value) override {
    if (order) order->push_back('p');
    ++persistCalls;
    if (persistCalls == failPersistAt) return false;
    persisted = value;
    return true;
  }
};
struct FakePinVerifier : IPinVerifier {
  PinVerifyOutcome outcome;
  std::vector<char>* order;
  FakePinStore* observeStore;
  uint16_t observedPersisted;
  int calls;
  FakePinVerifier() : outcome(PV_MATCH), order(nullptr), observeStore(nullptr), observedPersisted(0), calls(0) {}
  PinVerifyOutcome verify(const char*, size_t) override {
    if (order) order->push_back('v');
    if (observeStore) observedPersisted = observeStore->persisted;
    ++calls;
    return outcome;
  }
};
struct FakePinAction : IPinSuccessAction {
  PinActionOutcome outcome;
  std::vector<char>* order;
  FakePinStore* observeStore;
  uint16_t observedPersisted;
  int calls;
  FakePinAction() : outcome(PA_DONE), order(nullptr), observeStore(nullptr), observedPersisted(0), calls(0) {}
  PinActionOutcome run() override {
    if (order) order->push_back('a');
    if (observeStore) observedPersisted = observeStore->persisted;
    ++calls;
    return outcome;
  }
};
struct FakePinWriter : IAdminPinWriter {
  PinActionOutcome outcome;
  std::vector<char>* order;
  FakePinStore* observeStore;
  uint16_t observedPersisted;
  int calls;
  std::string committed;
  FakePinWriter() : outcome(PA_DONE), order(nullptr), observeStore(nullptr), observedPersisted(0), calls(0) {}
  PinActionOutcome commitNewPin(const char* pin, size_t n) override {
    if (order) order->push_back('w');
    if (observeStore) observedPersisted = observeStore->persisted;
    ++calls;
    committed.assign(pin, n);
    return outcome;
  }
};
void test_pin_auth_boot_fails_closed_when_counter_unreadable() {
  FakePinStore store; store.loadOk = false;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_FALSE(auth.boot(100));
  TEST_ASSERT_TRUE(auth.faulted());
  FakePinVerifier verifier; FakePinAction action;
  TEST_ASSERT_EQUAL_INT(PAC_NOT_READY, auth.authenticateAndRun("483920", 6, 100, verifier, action).code);
  TEST_ASSERT_EQUAL_INT(0, verifier.calls);
  TEST_ASSERT_EQUAL_INT(0, action.calls);
}
void test_pin_auth_persists_attempt_and_reset_before_action() {
  std::vector<char> order;
  FakePinStore store; store.order = &order;
  FakePinVerifier verifier; verifier.order = &order; verifier.observeStore = &store;
  FakePinAction action; action.order = &order; action.observeStore = &store;
  PinAuthCoordinator auth(store);
  TEST_ASSERT_TRUE(auth.boot(10));
  PinAuthResult r = auth.authenticateAndRun("483920", 6, 11, verifier, action);
  TEST_ASSERT_EQUAL_INT(PAC_OK, r.code);
  TEST_ASSERT_EQUAL_UINT16(0, store.persisted);
  TEST_ASSERT_EQUAL_INT(1, verifier.calls);
  TEST_ASSERT_EQUAL_INT(1, action.calls);
  TEST_ASSERT_EQUAL_UINT16(1, verifier.observedPersisted); // verifier sees the increment already durable
  TEST_ASSERT_EQUAL_UINT16(0, action.observedPersisted);    // action sees the durable reset
  TEST_ASSERT_EQUAL_UINT32(5, order.size());
  TEST_ASSERT_EQUAL_INT('l', order[0]);
  TEST_ASSERT_EQUAL_INT('p', order[1]);
  TEST_ASSERT_EQUAL_INT('v', order[2]);
  TEST_ASSERT_EQUAL_INT('p', order[3]);
  TEST_ASSERT_EQUAL_INT('a', order[4]);
}
void test_pin_auth_bad_secret_is_persisted_and_action_never_runs() {
  FakePinStore store; FakePinVerifier verifier; FakePinAction action;
  verifier.outcome = PV_MISMATCH;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  PinAuthResult r = auth.authenticateAndRun("483920", 6, 1, verifier, action);
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, r.code);
  TEST_ASSERT_EQUAL_UINT16(1, store.persisted);
  TEST_ASSERT_EQUAL_INT(0, action.calls);
  TEST_ASSERT_FALSE(auth.faulted());
}
void test_pin_auth_lockout_and_reboot_restore() {
  FakePinStore store; store.persisted = 4;
  FakePinVerifier verifier; verifier.outcome = PV_MISMATCH;
  FakePinAction action;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(1000));
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, auth.authenticateAndRun("483920", 6, 1000, verifier, action).code);
  TEST_ASSERT_EQUAL_UINT16(5, store.persisted);
  PinAuthResult locked = auth.authenticateAndRun("483920", 6, 1001, verifier, action);
  TEST_ASSERT_EQUAL_INT(PAC_LOCKED, locked.code);
  TEST_ASSERT_EQUAL_UINT32(59, locked.retryAfterS);
  TEST_ASSERT_EQUAL_INT(1, verifier.calls);
  FakePinStore rebootStore; rebootStore.persisted = store.persisted;
  PinAuthCoordinator rebooted(rebootStore); TEST_ASSERT_TRUE(rebooted.boot(2000));
  TEST_ASSERT_EQUAL_INT(PAC_LOCKED, rebooted.authenticateAndRun("483920", 6, 2000, verifier, action).code);
  TEST_ASSERT_EQUAL_UINT32(60, rebooted.authenticateAndRun("483920", 6, 2000, verifier, action).retryAfterS);
}
void test_pin_auth_attempt_persist_failure_latches_before_verify() {
  FakePinStore store; store.failPersistAt = 1;
  FakePinVerifier verifier; FakePinAction action;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_STORAGE_ERROR, auth.authenticateAndRun("483920", 6, 1, verifier, action).code);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_INT(0, verifier.calls);
  TEST_ASSERT_EQUAL_INT(0, action.calls);
}
void test_pin_auth_reset_persist_failure_blocks_action() {
  FakePinStore store; store.failPersistAt = 2;
  FakePinVerifier verifier; FakePinAction action;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_STORAGE_ERROR, auth.authenticateAndRun("483920", 6, 1, verifier, action).code);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_INT(1, verifier.calls);
  TEST_ASSERT_EQUAL_INT(0, action.calls);
}
void test_pin_auth_verifier_error_latches_and_preserves_attempt() {
  FakePinStore store; FakePinVerifier verifier; FakePinAction action;
  verifier.outcome = PV_ERROR;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_VERIFY_ERROR, auth.authenticateAndRun("483920", 6, 1, verifier, action).code);
  TEST_ASSERT_TRUE(auth.faulted());
  TEST_ASSERT_EQUAL_UINT16(1, store.persisted);
  TEST_ASSERT_EQUAL_INT(0, action.calls);
}
void test_pin_auth_invalid_pin_does_not_consume_attempt() {
  FakePinStore store; FakePinVerifier verifier; FakePinAction action;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_BAD_INPUT, auth.authenticateAndRun("12a456", 6, 1, verifier, action).code);
  TEST_ASSERT_EQUAL_INT(0, store.persistCalls);
  TEST_ASSERT_EQUAL_INT(0, verifier.calls);
}
void test_pin_auth_indeterminate_action_latches() {
  FakePinStore store; FakePinVerifier verifier; FakePinAction action;
  action.outcome = PA_INDETERMINATE;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_INDETERMINATE, auth.authenticateAndRun("483920", 6, 1, verifier, action).code);
  TEST_ASSERT_TRUE(auth.faulted());
}
void test_pin_change_requires_old_pin_and_commits_after_reset() {
  std::vector<char> order;
  FakePinStore store; store.order = &order;
  FakePinVerifier verifier; verifier.order = &order; verifier.observeStore = &store;
  FakePinWriter writer; writer.order = &order; writer.observeStore = &store;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_OK, auth.changeAdminPin("483920", 6, "627194", 6, 1, verifier, writer).code);
  TEST_ASSERT_EQUAL_INT(1, verifier.calls);
  TEST_ASSERT_EQUAL_INT(1, writer.calls);
  TEST_ASSERT_EQUAL_UINT16(1, verifier.observedPersisted);
  TEST_ASSERT_EQUAL_UINT16(0, writer.observedPersisted);
  TEST_ASSERT_EQUAL_STRING("627194", writer.committed.c_str());
  TEST_ASSERT_EQUAL_UINT32(5, order.size());
  TEST_ASSERT_EQUAL_INT('p', order[1]);
  TEST_ASSERT_EQUAL_INT('v', order[2]);
  TEST_ASSERT_EQUAL_INT('p', order[3]);
  TEST_ASSERT_EQUAL_INT('w', order[4]);
}
void test_pin_change_rejects_weak_new_pin_before_old_pin_attempt() {
  FakePinStore store; FakePinVerifier verifier; FakePinWriter writer;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_WEAK_NEW_PIN, auth.changeAdminPin("483920", 6, "123456", 6, 1, verifier, writer).code);
  TEST_ASSERT_EQUAL_INT(0, store.persistCalls);
  TEST_ASSERT_EQUAL_INT(0, verifier.calls);
  TEST_ASSERT_EQUAL_INT(0, writer.calls);
}
void test_pin_change_wrong_old_pin_never_writes_new_pin() {
  FakePinStore store; FakePinVerifier verifier; FakePinWriter writer;
  verifier.outcome = PV_MISMATCH;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_BAD_SECRET, auth.changeAdminPin("111111", 6, "627194", 6, 1, verifier, writer).code);
  TEST_ASSERT_EQUAL_INT(0, writer.calls);
  TEST_ASSERT_EQUAL_UINT16(1, store.persisted);
}
void test_pin_change_indeterminate_write_latches() {
  FakePinStore store; FakePinVerifier verifier; FakePinWriter writer;
  writer.outcome = PA_INDETERMINATE;
  PinAuthCoordinator auth(store); TEST_ASSERT_TRUE(auth.boot(0));
  TEST_ASSERT_EQUAL_INT(PAC_ACTION_INDETERMINATE, auth.changeAdminPin("483920", 6, "627194", 6, 1, verifier, writer).code);
  TEST_ASSERT_TRUE(auth.faulted());
}

void test_api_pointer_inputs_fail_closed_without_mutation() {
  uint32_t fingerprint = 0xA5A5A5A5u;
  TEST_ASSERT_FALSE(reqFingerprint(1, 2, 3, nullptr, 1, fingerprint));
  TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fingerprint);
  uint32_t emptyFingerprint = 0;
  TEST_ASSERT_TRUE(reqFingerprint(1, 2, 3, nullptr, 0, emptyFingerprint));
  uint32_t ab = 0, ac = 0;
  TEST_ASSERT_TRUE(reqFingerprint(1, 2, 3, (const uint8_t*)"ab", 2, ab));
  TEST_ASSERT_TRUE(reqFingerprint(1, 2, 3, (const uint8_t*)"ac", 2, ac));
  TEST_ASSERT_TRUE(ab != ac);

  Roster g; g.add(5, "Ana"); g.add(6, "Ben"); g.off(6);
  int32_t mon = monday();
  ApiCtx c = ctxAt(mon + 1, 17 * 60);
  MarkReq mark = {5, (uint16_t)mon, 1, ST_EXCUSED};
  size_t count = 99;
  TEST_ASSERT_EQUAL_INT(A_INVALID, buildMark(mark, c, g.r, nullptr, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);

  count = 99;
  NoClassReq holiday = {(uint16_t)(mon + 1), 0, true, true};
  TEST_ASSERT_EQUAL_INT(A_INVALID, buildNoClass(holiday, c, nullptr, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);

  count = 99;
  TEST_ASSERT_EQUAL_INT(A_INVALID, rosterSet(g.r, 7, "Cal", 3, c.now, nullptr, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  count = 99;
  TEST_ASSERT_EQUAL_INT(A_INVALID, rosterDeactivate(g.r, 5, c.now, nullptr, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);
  count = 99;
  TEST_ASSERT_EQUAL_INT(A_INVALID, rosterActivate(g.r, 6, c.now, nullptr, count).code);
  TEST_ASSERT_EQUAL_UINT32(0, count);

  TEST_ASSERT_TRUE(g.r.known(5));
  TEST_ASSERT_TRUE(g.r.known(6));
  TEST_ASSERT_TRUE(g.r.active(5));
  TEST_ASSERT_FALSE(g.r.active(6));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_authorize_matches_oracle_everywhere);
  RUN_TEST(test_unauthenticated_learns_nothing);
  RUN_TEST(test_lockout_schedule_and_cap);
  RUN_TEST(test_lockout_success_resets_and_refusals_do_not_count);
  RUN_TEST(test_lockout_counter_is_persisted_before_verify);
  RUN_TEST(test_lockout_boot_restore_and_saturation);
  RUN_TEST(test_lockout_deadline_wrapping_to_zero_does_not_skip_lockout);
  RUN_TEST(test_token_basics_and_expiry_boundary);
  RUN_TEST(test_token_rejects_wrong_zero_and_neighbours);
  RUN_TEST(test_token_can_issue_rejects_zero_and_duplicate_without_mutation);
  RUN_TEST(test_token_can_issue_accepts_distinct_token_without_mutating_lru);
  RUN_TEST(test_token_two_sessions_and_lru_eviction);
  RUN_TEST(test_reqring_replay_mismatch_inflight);
  RUN_TEST(test_reqring_evicted_pending_ticket_cannot_finish_or_abort_reused_id);
  RUN_TEST(test_reqring_does_not_cache_failed_results);
  RUN_TEST(test_reqring_eviction_and_leak_guard);
  RUN_TEST(test_mark_valid_and_event_shape);
  RUN_TEST(test_mark_rejections);
  RUN_TEST(test_mark_today_window_rule);
  RUN_TEST(test_noclass_rules_and_shape);
  RUN_TEST(test_roster_mapping_and_idempotency);
  RUN_TEST(test_roster_and_manual_mark_gates_fail_closed_on_incomplete_history);
  RUN_TEST(test_enroll_gate);
  RUN_TEST(test_config_gate_order_and_rules);
  RUN_TEST(test_restore_gate_and_helpers);
  RUN_TEST(test_window_open_now);
  RUN_TEST(test_pin_auth_boot_fails_closed_when_counter_unreadable);
  RUN_TEST(test_pin_auth_persists_attempt_and_reset_before_action);
  RUN_TEST(test_pin_auth_bad_secret_is_persisted_and_action_never_runs);
  RUN_TEST(test_pin_auth_lockout_and_reboot_restore);
  RUN_TEST(test_pin_auth_attempt_persist_failure_latches_before_verify);
  RUN_TEST(test_pin_auth_reset_persist_failure_blocks_action);
  RUN_TEST(test_pin_auth_verifier_error_latches_and_preserves_attempt);
  RUN_TEST(test_pin_auth_invalid_pin_does_not_consume_attempt);
  RUN_TEST(test_pin_auth_indeterminate_action_latches);
  RUN_TEST(test_pin_change_requires_old_pin_and_commits_after_reset);
  RUN_TEST(test_pin_change_rejects_weak_new_pin_before_old_pin_attempt);
  RUN_TEST(test_pin_change_wrong_old_pin_never_writes_new_pin);
  RUN_TEST(test_pin_change_indeterminate_write_latches);
  RUN_TEST(test_token_null_inputs_are_rejected_without_session_mutation);
  RUN_TEST(test_api_pointer_inputs_fail_closed_without_mutation);
  return UNITY_END();
}
