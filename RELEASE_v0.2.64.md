# Release v0.2.64 — wrap-safe PIN lockout deadline state

Date: 2026-11-09
Project root: /root/att

## Change

PinLockout used until_ == 0 to mean “unlocked”. A valid wraparound calculation can also produce a deadline of zero. In that state, the old implementation skipped the lockout even though the persisted failure count had reached a lockout threshold.

The fix adds an independent locked_ state bit:
- boot() derives lock activity from the persisted failure count, then computes the deadline with unsigned wraparound.
- fail() sets the deadline and marks the lock active.
- begin() computes the remaining time modulo uint32. It refuses attempts while the deadline is still ahead within the supported half-range, and clears the active flag at or after the exact deadline.
- success() clears both the deadline and active flag.

Persisted NVS data is unchanged: only the uint16 failure count is stored. The 5-failure schedule remains 60, 120, 240, then 300 seconds capped. The new member changes the in-memory class layout only. No product decision, lockout policy, storage format or production caller was changed.

## Test-first and verification

The new test_lockout_deadline_wrapping_to_zero_does_not_skip_lockout regression was run before the implementation. It failed against the prior source with Expected FALSE Was TRUE, reproducing the lockout bypass when the calculated deadline was exactly zero.

Final exact commands:
- pio test -e native -f 'test_api' — 37/37 passed, exit 0.
- pio test -e native -f 'test_pin_auth_session' — 11/11 passed, exit 0.
- ./tools/verify_v0.2.64.sh targeted — **48/48 passed across 2 suites**, exit 0; aggregate suite time 40.381 seconds (remote process runtime 47.69 seconds).

Current inventory: 606 C++ RUN_TEST registrations across 35 native suites. Selected verification: 48/48 across 2 suites. The remaining 558 registrations were not run.

## Not run / limits

- Full native suite and SF2 Node suite.
- ESP32 target build, final ELF inspection, flash and physical-device test.
- Production HTTP/auth integration, real monotonic-time source validation, NVS runtime, power-loss, brownout and uptime/soak testing.

C6 remains blocking: no production HTTP/boot owner, PIN verifier/writer or protected routes exist. This change repairs the core arithmetic sentinel only; it does not make PIN lockout operationally enforced end-to-end. nowS must continue to be supplied as a wrap-safe monotonic seconds counter by a future production caller. E12 remains DELEGATED. HANDOFF_FINAL.md and docs/DECISIONS.md are unchanged.
