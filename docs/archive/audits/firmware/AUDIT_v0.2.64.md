# Audit v0.2.64 — PIN lockout deadline wrap

Date: 2026-11-09
Baseline: v0.2.63
Root: /root/att

## Baseline / scope

- README identified v0.2.63; Git metadata is absent in this archive-based tree.
- v0.2.63 archive checksum and source manifest were verified before editing.
- HANDOFF_FINAL.md and docs/DECISIONS.md were hash-checked and remained unchanged.
- Scope is limited to PinLockout's in-memory deadline/active-state representation and one regression in test_api.

## Finding

The previous implementation treated until_ == 0 as the unlocked sentinel. But nowS + lockSeconds(fails_) is uint32 arithmetic. A lock starting at 0xFFFFFFC4 with a 60-second duration calculates an exact deadline of zero. The previous begin() then considered the lock inactive and allowed another attempt before the required deadline.

This affects both a lock created after the fifth failure and a lock reconstructed by boot(5, nowS) from the persisted attempt count.

## Patch

- Added locked_ independent of until_.
- boot() sets locked_ when the persisted failure count maps to a nonzero lock duration, even when the computed deadline is zero.
- fail() marks the newly calculated deadline active.
- begin() uses unsigned remaining-time arithmetic and the existing half-range ordering rule; it blocks until the exact deadline and clears the state at the deadline.
- success() clears both pieces of state.
- NVS key, persisted type/value, thresholds, durations and saturation behavior are unchanged.

The existing half-range comparison assumes timestamps are compared within less than 2^31 seconds of each other. The API still has no production caller; do not infer that a real monotonic clock integration has been implemented.

## Evidence

Test-first:
- New regression added and registered before implementation.
- Initial exact command: pio test -e native -f 'test_api'.
- Expected failure observed at test_lockout_deadline_wrapping_to_zero_does_not_skip_lockout: expected lock denial, but old code allowed the attempt. The run exited 1; remaining test assertions shown in the output passed.

After patch:
- pio test -e native -f 'test_api' — 37/37 passed, exit 0.
- pio test -e native -f 'test_pin_auth_session' — 11/11 passed, exit 0.
- ./tools/verify_v0.2.64.sh targeted — **48/48 passed across test_api and test_pin_auth_session**, exit 0; aggregate suite time 40.381 seconds (remote process runtime 47.69 seconds).

The API suite is the owner of PinLockout; the session-auth suite was included as a dependent compile/runtime check because the changed class is in the shared auth header. No unrelated suite was run.

## Critical review / residuals

- No production caller for PinLockout; C6 and U80 remain open.
- No target build or check of the real monotonic-second source was done. The half-range assumption must be preserved by any future caller.
- The persisted counter still counts an attempt before verification, as designed.
- No persistence format or signed/delegated decision changed.
- No flash or device operation was performed.
- Full native, SF2, ESP32 build/ELF and physical hardware were not tested.

