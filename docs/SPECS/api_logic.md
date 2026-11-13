# SPEC: api_logic (lib/core/src/api_logic.h/.cpp)

Status: DELEGATED core API design. `api_logic.*` remains pure decision logic. v0.2.31 added `pin_auth.*`; v0.2.34 added the NVS attempt store/runtime; v0.2.38 adds `pin_auth_session.*`, the `TokenSessionSuccessAction` bridge and the RF-gated `Esp32SecureRandom` HAL adapter. The user's 413/413 native run covers the v0.2.36 baseline only; 18 new v0.2.38 native registrations are not run. No production firmware/API wiring or ESP32 target build exists.

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
5 failures = 60 s, doubling per further 5 failures, capped at 300 s. `PinLockout` remains the timing/counter primitive. v0.2.31 adds `PinAuthCoordinator`, which enforces the required order when used:

1. `boot()` loads the persisted failure counter; any read failure latches the coordinator.
2. For a syntactically valid six-digit candidate, `begin()` checks the lock and increments the in-memory count.
3. The coordinator persists the incremented counter before calling the verifier. The store contract requires durable commit plus read-back confirmation.
4. A mismatch leaves that persisted increment in place and starts the in-memory delay. Verifier errors latch the coordinator.
5. A match resets and durably persists the counter before invoking the required success action (e.g. session issuance or a setup operation).
6. If either persistence step fails, no verifier/action progression occurs after that failure and the coordinator latches until reboot/reload.

Use one long-lived coordinator per lockout-counter domain; do not construct one per request. The module is synchronous and serialized-main-loop only; a multi-task adapter must hold one mutex across the complete operation. Which routes share a counter remains a composition/policy concern and is not silently changed here.

## TokenTable
Two sessions, 16-byte random tokens, RAM only, 900 s idle expiry (monotonic seconds), constant-time comparison, least-recently-used eviction. An all-zero or duplicate token is rejected. `check(nullptr, nowS)` returns false without refreshing either session; `revoke(nullptr)` is a no-op. `ctEqual()` returns false for a null operand with nonzero length and true for any zero-length comparison. v0.2.38 adds read-only `canIssue()` plus `TokenSessionSuccessAction`: it requests token bytes from `ISecureRandomSource`, preflights without changing active sessions, and inserts only after the HTTP handler calls `confirmResponseSent(monotonicNowS)` following the complete response write. Aborted/abandoned responses never enter the table and therefore cannot evict an existing LRU session. The route must serialize TokenTable mutation between preflight and confirmation; if that contract cannot be guaranteed, add an explicit reservation/commit layer before integration. The handler must erase its temporary response copy. `Esp32SecureRandom` wraps ESP-IDF `esp_fill_random()` and requires a caller notification after Wi-Fi RF starts; the production Wi-Fi owner and route are absent.

## ReqRing (roster and enroll idempotency)
16 RAM-only entries keyed by req_id plus a request fingerprint (FNV-1a over the body, 32-bit). `reqFingerprint()` returns bool and writes through an output reference only on success; null body with nonzero length returns false without modifying the output, while a null pointer with zero length hashes the empty body header. No invalid-input hash sentinel is used. Replay returns the stored result. Same ID with a different fingerprint -> 409. A new reservation returns a nonzero 64-bit ticket; `finish()` and `abort()` require both req_id and that ticket, so a late handler cannot complete or clear a newer reservation after expiry and ID reuse. Non-new results carry ticket zero. `finish()` returns true only when the matching pending reservation is committed with `A_OK`; non-success codes, stale tickets and zero tickets return false and leave a matching reservation pending. The caller must explicitly `abort()` unsuccessful or uncertain operations before retrying. Pending entries expire after a caller-given age to release abandoned reservations. **Expiry does not cancel the old handler or undo its side effects**: the caller must ensure timed-out work has stopped, or serialize/reconcile it before allowing a retry, otherwise duplicate side effects remain possible. A finished entry can be evicted after 16 newer requests, so an old req_id can run again (known limit). This RAM ring is not durable exactly-once protection.

## Builders and gates
- buildMark: status 0..4; id 1..cap and known; day in term and not in the future; session enabled; Present/Late only after the window opens (now >= start - scanLead) and only for today or earlier. Deactivated students can be corrected. Null event output returns A_INVALID/R_BAD_FIELD with count zero.
- buildNoClass: future days allowed; disabled session refused unless whole-day. Null event output returns A_INVALID/R_BAD_FIELD with count zero.
- rosterSet / rosterDeactivate / rosterActivate: map RosterStore plan results to codes; null event output maps appended RE_BAD_OUTPUT to A_INVALID/R_BAD_FIELD, count zero.
- gateEnroll: refuses a taken slot unless replacing; refuses unknown ids for replace; never overwrites silently.
- gateConfigPut: order = stale generation (409) -> secret fields (422, must use their own endpoints) -> validateConfig (422) -> clock fault blocks schedule changes (423) -> canApply (409).
- gateRestore: a healthy device needs typed confirmation; recovery states do not.
- validPin (6 digits), weakPin (same digit or straight run), ctEqual.

## Enforced core boundary and remaining caller duties
1. Route LOGIN and setup-code checks through `PinAuthCoordinator::authenticateAndRun()`; the success action is mandatory and must not execute before a durable counter reset.
2. Route PIN_CHANGE through `PinAuthCoordinator::changeAdminPin()`. It validates the new PIN and verifies the CURRENT PIN before the injected writer is called. The writer must derive/store the salted hash, commit it durably and verify read-back; raw PIN storage is forbidden.
3. Supply a persistent `IPinAttemptStore`. `persistFailureCount()` may return true only after durable commit and read-back. The current tree has no concrete adapter.
4. Supply a PBKDF2-SHA256 `IPinVerifier` using the configured salt/hash. KDF parameters and target performance are not invented by this module.
5. Use monotonic seconds for sessions and lockouts, never the RTC. A faulted coordinator must cause the production adapter to reject all protected routes and revoke/disable sessions; this integration is not present.
6. Use a cryptographically secure random source for 16-byte tokens and ensure tokens are issued only inside the successful PIN action. v0.2.38 adds the action and RF-gated ESP32 RNG adapter, but its 18 new host tests are unrun, the Wi-Fi lifecycle does not call the adapter, no ESP32 build is pinned, and no HTTP route is present.
7. After a schedule change passes `validateConfig()` and `canApply()`, persist and verify the config, then call the existing `ClosureEngine::setSchedule()` in the serialized engine context (C1). Do not destroy/recreate the engine: its `LogIndex` is a registered `LogStore` observer.

## Known gaps
- `PinAuthCoordinator` is a core helper only. A future adapter can bypass it unless all secret-check routes are deliberately wired through it. C6 remains open.
- GET /status is approved as public only for basic non-sensitive status fields. The exact field list must be constrained by the HTTP adapter; sensitive/admin data must not be included.
- The adapter does not exist. No HTTP server, JSON, or routing.
