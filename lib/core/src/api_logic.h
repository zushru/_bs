#pragma once
#include "config_store.h"
#include "health_fsm.h"
#include "roster_store.h"
namespace att {
// Pure decision layer for /api/v1. No JSON, no I/O: the adapter parses a request into these structs, calls
// the gate, and executes the returned events/config through the single store task.
// All "now" values named nowS are MONOTONIC seconds since boot (never the RTC), so a clock change can neither
// extend a session nor shorten a lockout.
enum ApiCode : uint16_t {
  A_OK = 200, A_BAD = 400, A_UNAUTH = 401, A_NOTFOUND = 404, A_CONFLICT = 409,
  A_INVALID = 422, A_LOCKED = 423, A_LOCKOUT = 429
};
enum ApiReason : uint8_t {
  R_NONE = 0, R_NO_TOKEN, R_BAD_SETUP, R_STATE, R_PERM, R_GEN_MISMATCH, R_WINDOW_OPEN, R_UNCLOSED,
  R_SECRET_FIELD, R_BAD_FIELD, R_CLOCK_UNSAFE, R_FULL, R_SLOT_TAKEN, R_UNKNOWN_ID, R_NO_SUCH_SESSION,
  R_OUT_OF_TERM, R_FUTURE, R_NEED_CONFIRM, R_WEAK_PIN, R_REQ_MISMATCH, R_REQ_INFLIGHT, R_LOCKOUT
};
struct ApiResult { ApiCode code; ApiReason reason; uint32_t retryAfterS; };

enum Ep : uint8_t {
  EP_STATUS, EP_LOGIN, EP_LOGOUT, EP_PIN_CHANGE, EP_PIN_RESET, EP_DAY, EP_ROSTER_GET, EP_ROSTER_SET,
  EP_MARK, EP_NOCLASS, EP_CONFIG_GET, EP_CONFIG_PUT, EP_CLOCK, EP_ENROLL, EP_EXPORT, EP_BACKUP_ACK,
  EP_SETUP, EP_RESTORE, EP_ROLLOVER, EP_COUNT
};
// Callers MUST also: (1) run EP_LOGIN, EP_PIN_CHANGE (old PIN) and every setup-code check through a PinLockout,
// otherwise a 6-digit code can be brute-forced over Wi-Fi; (2) require the CURRENT PIN for EP_PIN_CHANGE, because a
// sniffed token alone must not be able to take over the admin account.
// Order of checks: 401 (token, then setup code) before 423 (health/permission), so an unauthenticated
// client learns nothing beyond the public /status. Lockout (429) is applied by the caller before verifying.
ApiResult authorize(Ep ep, HState h, bool tokenValid, bool setupCodeOk);

// ---- PIN / setup-code lockout: 5 failures = 60 s, doubling per further 5, capped at 300 s.
// Contract: call begin(); if allowed, PERSIST fails() BEFORE verifying; then call success() or fail().
// A power cut between persist and verify therefore still counts as a failure.
struct LockGate { bool allowed; uint32_t retryAfterS; };
class PinLockout {
 public:
  PinLockout() : fails_(0), until_(0), locked_(false) {}
  void boot(uint16_t persistedFails, uint32_t nowS);   // multiple of 5 => locked again for its full duration
  LockGate begin(uint32_t nowS);
  void fail(uint32_t nowS);
  void success() { fails_ = 0; until_ = 0; locked_ = false; }
  uint16_t fails() const { return fails_; }
  static uint32_t lockSeconds(uint16_t fails);         // 0 unless fails is a positive multiple of 5
 private:
  uint16_t fails_; uint32_t until_; bool locked_;
};

// ---- Sessions: 128-bit random tokens, RAM only, 15 min idle expiry, max 2.
const uint32_t TOKEN_IDLE_S = 900;
class TokenTable {
 public:
  TokenTable() { revokeAll(); }
  bool canIssue(const uint8_t rnd[16]) const;           // read-only preflight; no slot or LRU mutation
  bool issue(const uint8_t rnd[16], uint32_t nowS);    // false: all-zero (RNG failure) or duplicate token
  bool lastIssueEvicted() const { return evicted_; }   // a live session was displaced (least recently used)
  bool check(const uint8_t tok[16], uint32_t nowS);    // false for null; constant-time compare otherwise; refreshes idle timer on success
  void revoke(const uint8_t tok[16]);                 // null is a no-op
  void revokeAll();
  int active(uint32_t nowS) const;
 private:
  struct S { uint8_t t[16]; uint32_t last; bool used; };
  bool live_(const S& s, uint32_t nowS) const { return s.used && (uint32_t)(nowS - s.last) <= TOKEN_IDLE_S; }
  S s_[2]; bool evicted_;
};

// ---- Idempotency for roster/enroll: 16-entry ring keyed by client req_id + request fingerprint.
enum ReqKind { RQ_NEW, RQ_REPLAY, RQ_MISMATCH, RQ_INFLIGHT, RQ_INVALID };
struct ReqResult { ReqKind kind; ApiCode code; uint32_t aux; uint64_t ticket; };
bool reqFingerprint(uint8_t op, uint32_t a, uint32_t b, const uint8_t* s, size_t n, uint32_t& outFingerprint); // false for null/nonzero input; output unchanged
class ReqRing {
 public:
  ReqRing() { for (int i = 0; i < 16; i++) { e_[i].st = 0; e_[i].ticket = 0; } seq_ = 0; nextTicket_ = 0; }
  ReqResult begin(uint32_t reqId, uint32_t fp, uint32_t nowS);   // reserves the id as pending when new; ticket identifies this reservation
  void expire(uint32_t nowS, uint32_t maxAgeS);          // frees pending entries whose handler never finished (leak guard)
  bool finish(uint32_t reqId, uint64_t ticket, ApiCode code, uint32_t aux); // only A_OK may be cached; false leaves the reservation pending
  void abort(uint32_t reqId, uint64_t ticket);           // only the matching live reservation can abort
 private:
  struct E { uint32_t id, fp, aux, order, t; uint64_t ticket; uint16_t code; uint8_t st; }; // st 0 free, 1 pending, 2 done
  E e_[16]; uint32_t seq_; uint64_t nextTicket_;
};

// ---- Request validation. Each returns A_OK and fills events, or an error and leaves cnt = 0.
struct ApiCtx { uint32_t now; Params p; Timetable tt; int32_t termStart, termEnd; uint16_t cap; };
struct MarkReq { uint16_t id; uint16_t day; uint8_t session; uint8_t status; };
struct NoClassReq { uint16_t day; uint8_t session; bool wholeDay; bool set; };
// Shared boundary check for mutations and future read/day-view routes.
// Both configured endpoints are inclusive local day numbers in [0, 65535].
bool apiDayInInclusiveTerm(int32_t day, int32_t termStart, int32_t termEnd);
ApiResult buildMark(const MarkReq& r, const ApiCtx& c, const RosterStore& roster, Event* out, size_t& cnt);   // out >= 1
ApiResult buildNoClass(const NoClassReq& r, const ApiCtx& c, Event* out, size_t& cnt);                     // out >= 1
ApiResult rosterSet(const RosterStore& r, uint16_t id, const char* name, size_t n, uint32_t now, Event* out, size_t& cnt);   // out >= 5
ApiResult rosterDeactivate(const RosterStore& r, uint16_t id, uint32_t now, Event* out, size_t& cnt);
ApiResult rosterActivate(const RosterStore& r, uint16_t id, uint32_t now, Event* out, size_t& cnt);
ApiResult gateEnroll(const RosterStore& r, uint16_t id, bool replace, uint16_t cap);
struct ConfigGateIn { HState h; uint32_t expectGen, curGen; bool windowOpen, unclosedPast; };
ApiResult gateConfigPut(const Config& cur, const Config& next, const ConfigGateIn& in);
ApiResult gateRestore(HState h, bool typedConfirm);
bool windowOpenNow(const Timetable& t, const Params& p, uint32_t nowEpoch);   // any scan window open (adapter passes to gateConfigPut)

bool validPin(const char* s, size_t n);          // exactly 6 ASCII digits
bool weakPin(const char* s, size_t n);           // all same digit, or a straight ascending/descending run
bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n);   // zero length is equal; null with nonzero length is false; constant time for valid buffers
}
