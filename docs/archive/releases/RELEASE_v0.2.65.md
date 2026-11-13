# Release v0.2.65 — null-safe token API boundaries

Date: 2026-11-09
Baseline: v0.2.64
Project root: /root/att

## Change

A public API inconsistency allowed null pointer input to crash the process:
- TokenTable::canIssue(nullptr) already returned false.
- TokenTable::check(nullptr, nowS) dereferenced the pointer while comparing 16 bytes.
- TokenTable::revoke(nullptr) called ctEqual() with a null operand whenever a session slot was active.
- The public ctEqual() helper also dereferenced either null operand for a nonzero comparison length.

The patch:
- Makes check(nullptr, nowS) return false before inspecting or refreshing session entries.
- Makes revoke(nullptr) a no-op.
- Makes ctEqual() return false if either operand is null and n > 0.
- Preserves ctEqual()'s existing n == 0 result (true), including null operands.

No public signature, token format, session capacity, expiry policy, LRU policy, persisted format, product decision, or production composition was changed. The checks are defensive behavior at the existing API boundary. For valid non-null buffers, ctEqual() retains its bytewise constant-time accumulator loop.

## Test-first evidence

The new last-registered regression was executed before implementation. The old source completed the 37 existing test_api cases, then terminated with:
- Program received signal SIGSEGV (Segmentation fault)
- native:test_api ERRORED
- 38 test cases: 37 succeeded

The final regression uses two live sessions. It verifies:
- null check returns false;
- null revoke leaves both sessions active;
- issuing a third token still evicts the previously least-recently-used session, proving null calls do not alter LRU state;
- ctEqual() rejects null with nonzero length and preserves zero-length equality.

## Final verification

Exact final command:
- pio test -e native -f 'test_api' -f 'test_pin_auth_session' — 49/49 passed, exit 0; test_api 38/38, test_pin_auth_session 11/11; aggregate suite time 22.267 seconds (remote process runtime 26.42 seconds).

Current inventory: 607 C++ RUN_TEST registrations across 35 suites. Selected verification: 49/49 across 2 suites. The other 558 registrations were not run.

## Not run / limits

- Full native suite and SF2 Node suite.
- ESP32 target build, final ELF inspection, flash and physical-device test.
- Production HTTP/session boundary, route serialization, real token source/RNG, NVS runtime, power-loss, brownout and soak testing.

C6 remains blocking: the production HTTP/boot owner and protected routes do not exist. This patch prevents these invalid pointer inputs from crashing the core session API; it does not prove that all production callers validate request buffers or serialize TokenTable operations. No signed or delegated decision was altered. HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
