# Audit v0.2.62 — explicit clock commit outcome

Date: 2026-11-09
Project root: /root/att

## Finding

The prior applyClockSet() accepted any ClockDecision action. A caller could pass CA_NONE, a confirmation-pending decision or CA_BLOCKED_WINDOW and still append a CLOCK_SET pair. Its bool return also combined rejected/no-write, pre-faulted log, successful commit and an indeterminate storage failure.

## Change

- Replaced bool applyClockSet() with ClockApplyResult.
- CLOCK_APPLY_REJECTED means the decision is not CA_APPLY, its source is outside SRC_MANUAL..SRC_WIZARD, the gap bounds are reversed, or generation cannot make progress before any append. No append was attempted.
- CLOCK_APPLY_LOG_FAULTED means the LogStore was already faulted; no append was attempted.
- CLOCK_APPLY_COMMITTED means every required earlier GAP chunk and the final group containing remaining GAP chunks (if any) plus the two CLOCK_SET records was accepted by LogStore.
- CLOCK_APPLY_COMMIT_UNCERTAIN means an append returned false, or a long-gap operation could not proceed after a GAP prefix was already appended. The caller must stop, reopen/replay and reconcile before retrying.
- The writer itself requires CA_APPLY, a currently valid source enum, and gapTo >= gapFrom. Existing event encoding and record layout are unchanged.
- Test storage was extended to fail a selected append before writing or write the complete append and then report failure.

## Test-first and failure handling

- Before implementation, the new coreb assertions failed compilation because the four CLOCK_APPLY_* result names did not yet exist. This is the expected API-red state, not a runtime test pass.
- The first implementation test run found one bad test assumption: it expected LogStore::count() to remain zero after a successful first GAP-prefix append and a failure of the final group. The first 14 records had succeeded and the count correctly became 14. The test was corrected to assert that prefix count and independently reopen the bytes to prove no CLOCK_SET pair existed.
- The final coreb suite passed 43/43. The combined coreb + restore replay run passed 58/58; test_hardening passed 15/15; test_attendance_admission passed 12/12.

## Coverage

- Rejection: CA_NONE, CA_NEED_CONFIRM_BACK, CA_NEED_CONFIRM_JUMP, CA_BLOCKED_WINDOW, reversed GAP bounds and invalid source.
- Pre-faulted LogStore: result distinguishes no attempt from append uncertainty.
- Failed-before-write: append false yields COMMIT_UNCERTAIN and latches the current LogStore.
- Complete-write-then-false: reopen recovers the two CLOCK_SET records and the new baseline epoch.
- Multi-group long GAP: 14 GAP records are committed as a prefix; the next append fails before mutation. Result is COMMIT_UNCERTAIN; reopened log contains those 14 GAP records and no CLOCK_SET pair.
- Existing torn-group regression still verifies that a partial final commit group is handled by the current host storage/reopen behavior.

## Limitations

There is no production clock caller/HTTP route (C3). It must be added and must handle all four outcomes; the enum alone does not stop a defective future caller from continuing after uncertainty. There is no durable request ID or exactly-once retry. GAP records can be duplicated by retry, and partial-tail repair remains blocked by C13. This does not resolve C17/C18 or prove RTC correctness. All tests use in-memory host storage. No ESP32 target build, actual VFS fault injection or power-cut test was performed.
