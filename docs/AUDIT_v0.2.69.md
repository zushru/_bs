# Critical audit — v0.2.69 roster chronology correction

Date: 2026-11-10  
Baseline: v0.2.68

## Finding

The original `RosterTransitionAudit::State::lastDay` mixed two independent concepts:
1. chronological ordering of every roster event for an ID; and
2. the day of the last enrollment-state transition.

For an existing ID, `RA_SET` is a name revision. Updating `lastDay` for this event is required to reject a later backdated roster event, but treating it as the prior lifecycle boundary is wrong. It produced false rejections for:
- SET day 100, SET/rename day 110, DEACTIVATE day 110;
- SET day 100, DEACTIVATE day 110, SET/rename + ACTIVATE day 120.

The former is valid under current day-granular semantics: initial enrollment is day 100, and deactivation occurs on day 110. The latter is also valid: reactivation is day 120, strictly later than deactivation on day 110. A name revision is not an enrollment-state boundary.

## Correction and guardrails

The audit now tracks:
- `lastDay`: most recent effective day across all roster events for this ID;
- `lastLifecycleDay`: most recent effective day at which enrollment state changed.

Every roster event must be nondecreasing relative to `lastDay`. ACTIVATE/DEACTIVATE must additionally occur on a day strictly later than `lastLifecycleDay`, then update both values. Initial SET initializes both.

The extra `int32_t` state field adds approximately 1,204 bytes to the fixed audit object on a conventional 4-byte-aligned ABI (301 entries). Exact target object size, task placement and runtime heap/stack impact were not measured; this is a RAM cost that must be rechecked during target integration.

This preserves the important rejection cases:
- same-day initial SET then DEACTIVATE remains invalid;
- same-day DEACTIVATE then ACTIVATE remains invalid;
- duplicate DEACTIVATE and redundant ACTIVATE remain invalid;
- an event backdated before a later rename remains invalid;
- pre-epoch events, bad IDs, unknown actions, and invalid state transitions remain invalid.

The change is restricted to the read-only `RosterTransitionAudit` used by exact-prefix replay and month projection. `RosterStore`, live event writers, record encoding, attendance eligibility, and persisted data are unchanged.

## Test evidence

The pre-implementation run failed the two intended regression cases at runtime. After implementation the final focused suites passed:
- `test_roster_snapshot`: 26/26;
- `test_month_roster`: 17/17;
- combined: 43/43, exit 0 through `tools/verify_v0.2.69.sh targeted` (manifest and inventory checks included).

Coverage includes all current `RosterTransitionCode` failure paths either directly or through replay, the positive same-day rename/lifecycle cases, the backdated event control, and both E27 same-day lifecycle controls.

## Remaining uncertainty / blockers

- U61 remains open. No semantic audit is enforced by `RosterStore::historyComplete()` or every production write/consumer boundary.
- The audit trusts event epochs and the supplied timezone offset. It cannot distinguish a legitimate event chronology from an incorrect forward RTC adjustment that rewrites effective local days into a monotonic sequence.
- No real anonymized log history, actual RTC correction trace, or physical-device evidence was available.
- The operational roster interval representation remains local-day granular. Intraday session eligibility cannot be reconstructed from it; E27 remains DELEGATED and unchanged.
- No production main/scheduler, HTTP snapshot route, or report integration is created by this patch.

No delegated decision was changed. `HANDOFF_FINAL.md` and `docs/DECISIONS.md` remain unchanged.
