# Release v0.2.69 — separate roster effective-day and lifecycle-day tracking

Date: 2026-11-10  
Baseline: v0.2.68  
Project root: /root/att

## Change

`RosterTransitionAudit` previously used one `lastDay` value for both chronology of all roster events and lifecycle boundaries. A rename (`RA_SET` on an existing ID) advanced that value, even though a rename does not open or close an enrollment interval. This incorrectly rejected a lifecycle action later on the same local day as a rename, even when the last actual lifecycle boundary was on an earlier day.

The audit now tracks two values per student:
- `lastDay`: latest effective local day across all roster events. No roster event may move this backwards.
- `lastLifecycleDay`: latest effective local day that changed enrollment state. A lifecycle event must be strictly later than this boundary.

Memory cost: the new `int32_t` field adds approximately 1,204 bytes to the audit's per-ID state array on a conventional 4-byte-aligned ABI (301 entries). The actual ESP32 target object size and total RAM impact were not measured in this increment.

A rename can share a day with a lifecycle event, provided that day is still later than the preceding lifecycle boundary. A rename does not advance the lifecycle boundary. The first SET remains the initial active boundary. Same-day lifecycle transitions remain rejected.

Changed files:
- `lib/core/src/roster_snapshot.h`
- `lib/core/src/roster_snapshot.cpp`
- `test/test_roster_snapshot/test_main.cpp`
- `test/test_month_roster/test_main.cpp`
- `README.md`, `RELEASE.md`, current context, changelog, tests, critical/unverified records, and roster-prefix projection documentation
- `RELEASE_v0.2.69.md`, `docs/AUDIT_v0.2.69.md`, `tools/verify_v0.2.69.sh`

No persistent event format, live `RosterStore` behavior, product rule, or decision status changed. `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged. E27 remains DELEGATED and unchanged.

## Test-first evidence

Four new replay regressions were added before the source change. The pre-fix command:

```sh
pio test -e native -f 'test_roster_snapshot'
```

reached runtime assertions and failed the two intended same-day rename/lifecycle cases: both returned `RPR_TRANSITION_INVALID`. The backwards-date and E27 same-day lifecycle controls passed. The runner ended with exit 1. This was a runtime red phase, not a compile failure.

After implementation:
- `pio test -e native -f 'test_roster_snapshot' -f 'test_month_roster'` — 38/38 passed (snapshot 21, month 17), exit 0.
- Five additional direct tests were then added for zero ID, pre-epoch date, unknown deactivation, unknown action, and backdated name-revision diagnostics.
- Final expanded snapshot suite: `pio test -e native -f 'test_roster_snapshot'` — 26/26 passed, exit 0.
- Final release verifier reruns both affected suites with manifest/inventory validation.

Inventory at release: 621 C++ registrations across 35 suites. The 578 registrations outside the two focused suites were not run. No full native or SF2 suite was run.

## Critical limits

This increment repairs a false rejection in the existing read-only prefix/reporting audit. It does **not** close U61:
- `RosterStore::historyComplete()` remains a parser-integrity predicate, not a semantic lifecycle validator.
- The audit is still not enforced at all operational writers/consumers or wired to a production snapshot endpoint.
- The effective-day order is based on stored event epochs and a caller-supplied UTC offset; it does not prove true time, RTC provenance, or correct handling of real forward clock corrections.
- No anonymized production roster history was available for comparison.
- Same-day ACTIVATE/DEACTIVATE remains fail-closed under delegated E27. No session-level time-granular eligibility model was introduced.

No SF2 Node tests, final-source ESP32 target build, ELF inspection, flash, or physical-device test was performed. The other existing blockers (including C3 production integration, C6 route security, C13 storage recovery, and U112 request idempotency) remain.
