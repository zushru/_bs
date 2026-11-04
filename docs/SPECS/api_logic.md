# SPEC: api_logic (lib/core/src/api_logic.h/.cpp)

Status: DELEGATED design, device-run 142 total including this suite (20 tests). Pure decision logic: no JSON, no HTTP, no I/O. The adapter parses requests, calls these, and executes the returned events or config through the single store task.

## Health states used for gating
H_OK, H_UNPROVISIONED, H_CLOCK_FAULT, H_STORAGE_FAULT, H_CONFIG_FAULT, H_SENSOR_FAULT (health_fsm.h). Per-state permissions come from HealthFsm::perms.

## Endpoint classes (authorize(ep, state, tokenValid, setupCodeOk))
| Endpoint | Token | Setup code | Allowed states | Extra permission |
|---|---|---|---|---|
| STATUS | no | no | all | - |
| LOGIN | no | no | OK, CLOCK, STORAGE, SENSOR | - |
| LOGOUT | yes | no | all | - |
| PIN_CHANGE | yes | no | OK, SENSOR | - |
| PIN_RESET | no | yes | OK, CLOCK, SENSOR | - |
| DAY, ROSTER_GET | yes | no | all | readHistory |
| ROSTER_SET, MARK, NOCLASS | yes | no | all | manual |
| CONFIG_GET | yes | no | OK, CLOCK, STORAGE, SENSOR | - |
| CONFIG_PUT, CLOCK | yes | no | OK, CLOCK, SENSOR | - |
| ENROLL | yes | no | all | enroll |
| EXPORT, BACKUP_ACK | yes | no | all | exportOk |
| SETUP | no | yes | UNPROVISIONED | - |
| RESTORE | no | yes | all | - |
| ROLLOVER | yes | yes | OK | - |

Order: token, then setup code (both give 401, so an unauthenticated caller learns nothing), then state (423), then permission (423).

## PinLockout (setup code and PIN)
5 failures = 60 s, doubling per further 5 failures, capped at 300 s. Contract: begin(); if allowed, persist fails() BEFORE verifying; then success() or fail(). Refused attempts do not count. boot(persisted) restores a full lock if the persisted count is a multiple of 5.

## TokenTable
Two sessions, 16-byte random tokens (adapter supplies randomness), RAM only, 900 s idle expiry (monotonic seconds), constant-time comparison, least-recently-used eviction. An all-zero token is never accepted.

## ReqRing (roster and enroll idempotency)
16 entries keyed by req_id plus a request fingerprint (FNV-1a over the body, 32-bit). Replay returns the stored result. Same id with a different fingerprint -> 409. Pending entries expire after a caller-given age so a crashed handler cannot jam the ring. A finished entry can be evicted after 16 newer requests, so an old req_id can run again (known limit).

## Builders and gates
- buildMark: status 0..4; id 1..cap and known; day in term and not in the future; session enabled; Present/Late only after the window opens (now >= start - scanLead) and only for today or earlier. Deactivated students can be corrected.
- buildNoClass: future days allowed; disabled session refused unless whole-day.
- rosterSet / rosterDeactivate / rosterActivate: map RosterStore plan results to codes.
- gateEnroll: refuses a taken slot unless replacing; refuses unknown ids for replace; never overwrites silently.
- gateConfigPut: order = stale generation (409) -> secret fields (422, must use their own endpoints) -> validateConfig (422) -> clock fault blocks schedule changes (423) -> canApply (409).
- gateRestore: a healthy device needs typed confirmation; recovery states do not.
- validPin (6 digits), weakPin (same digit or straight run), ctEqual.

## Caller duties (not enforced by the core)
1. Run LOGIN, PIN_CHANGE (old PIN required) and setup-code checks through PinLockout.
2. PIN_CHANGE must verify the CURRENT PIN.
3. Use monotonic seconds for sessions and lockouts, never the RTC.
4. Persist lockout counters before verifying.
5. Rebuild the closure engine after any schedule change (C1).

## Known gaps
- GET /status is public by default. Not explicitly approved by you; it exposes health and time.
- The adapter does not exist. No HTTP server, JSON, or routing.
