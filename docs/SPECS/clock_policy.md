# Clock decision and commit contract

## Scope

ClockPolicy separates the pure decision (decideClock) from the event-log write (applyClockSet). The decision must be reviewed by its caller before it is applied. applyClockSet now enforces the CA_APPLY action itself rather than trusting every caller to remember that precondition.

## ClockApplyResult (v0.2.62)

applyClockSet(log, decision, oldEpoch, newEpoch) returns one of four results:

- CLOCK_APPLY_REJECTED: the decision is not CA_APPLY, its source is outside SRC_MANUAL..SRC_WIZARD, the gap bounds are reversed or the rounded whole-minute endpoint would exceed UINT32_MAX. No append was attempted.
- CLOCK_APPLY_LOG_FAULTED: the supplied LogStore was already faulted. No append was attempted.
- CLOCK_APPLY_COMMITTED: every required earlier GAP chunk and the final group containing the remaining GAP chunks (if any) plus the two CLOCK_SET records was accepted by LogStore.
- CLOCK_APPLY_COMMIT_UNCERTAIN: an append returned false, or a long-gap operation could not continue after one or more earlier GAP chunks had already been appended. The log may contain no bytes, a partial group, a complete group, or a committed GAP prefix without the final CLOCK_SET pair. The caller must stop writes, reopen/replay, and reconcile before retrying.

A zero-length gap (gapFrom == gapTo) is valid and writes the two-record CLOCK_SET group. The final clock pair remains one commit group. Long GAP spans are emitted in bounded groups; earlier GAP chunks may become durable before the final clock-setting group.

## Safety requirements

- Only CA_APPLY can write. Confirmation-pending, blocked-window, no-op and unknown action values are rejected by the writer even if the caller is defective.
- Clock source is checked against the currently defined source enum before any log mutation.
- Reversed or unrepresentable gap bounds (including a rounded endpoint beyond UINT32_MAX) are rejected before any append by the shared gap-span preflight.
- An already-faulted LogStore is distinguished from an attempted write failure.
- A false append is never treated as definite non-commit. The original LogStore faults; the caller must not continue using it.
- A long-gap append can persist a prefix of GAP records and then fail before the final group. Reopening exposes that prefix but does not invent the missing CLOCK_SET pair.
- Persistent event encoding and event-group layout are unchanged.

## Explicit limitations

There is no production clock HTTP route or firmware caller in this tree (C3). A future caller must explicitly handle all four results and stop normal writes/reopen after CLOCK_APPLY_COMMIT_UNCERTAIN; merely checking for CLOCK_APPLY_COMMITTED is not enough to restore the clock. The result enum does not provide a durable request ID, exactly-once retry, or automatic recovery. Repeating a logical clock request after an uncertain write can duplicate GAP records. Partial-tail recovery remains blocked by C13.

The clock decision logic still depends on trusted RTC, current log reference, confirmation policy and window facts supplied by the application. This change does not resolve C17 schedule-effective history or C18 term-end semantics.

## Regression coverage

test_coreb covers rejected actions, invalid source/range, already-faulted log, normal commit, torn group, failed-before-write, complete-write-then-false replay, and a long-gap case where the first 14 GAP records persist but the final GAP+CLOCK_SET group does not. test_hardening retains the long-gap and appendGaps regressions. All evidence is host-native with in-memory storage; no target VFS or power-cut claim is made.


## Clock-health application owner follow-through

The HealthInputs contract includes explicit clockTrusted. For provisioned devices, HealthFsm asserts HB_CLOCK whenever clockTrusted is false or RTC presence, OSF, epoch-floor or replayed-log-baseline checks fail. Before initial provisioning, calendar trust is not an attendance prerequisite.

HealthFsm::update(inputs, clockReverified) adds a separate one-shot release path for HB_CLOCK. It clears no storage/configuration latch and only clears the clock latch when the current clock facts are healthy. The existing HealthInputs::reverified flag remains the broader all-domain release signal.

ClockHealthOwner binds to the dispatcher service's exact HealthFsm and ConfigBootCoordinator instances. Both public refresh and dispatch paths force the broad reverified input false and refresh HealthFsm from current caller-supplied sensor/storage/config/RTC facts using the owner's own volatile clock-trust state. A newly observed RTC/epoch fault therefore revokes old trust before authorization, and a storage/config fault is reflected before EP_CLOCK authorization. The clock owner cannot clear storage/configuration latches; those require a separate full-domain verification owner. It invokes the request-bound dispatcher itself; it does not accept a caller-provided success boolean or arbitrary dispatch result. It consumes clock trust only after ACDC_CONFIRMATION_INVOKED, A_OK, CCF_OK and clockTrusted=true. The CCF_OK path supplies the verified RTC-presence/OSF/floor/baseline facts, sets the one-shot clock reverify argument, and forcibly keeps the broad reverified flag false. Storage/config/sensor facts are preserved. A service-latched failure revokes volatile clock trust; preflight or confirmation-required responses that do not latch the service leave previous trust unchanged.

Clock trust is RAM-only and resets on owner reconstruction/reboot. The reusable owner is not part of target/esp32_attendance and no production route invokes it. Its methods must run on the same serialized application task as all competing writers. Its regressions are source-only and NOT RUN.
