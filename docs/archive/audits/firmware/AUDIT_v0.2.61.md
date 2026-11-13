# Audit v0.2.61 — roster-history integrity WIP completion

Date: 2026-11-09
Base release archive: v0.2.60, SHA-256 96eeacef35103f5e9ba76c5ffb036fb926d9c8aa33bb2f8dec71681206aa2fae
Project root: /root/att

## Source changes preserved and completed

- IRoster exposes historyComplete() as a required const query; every in-tree implementation and test fake was updated.
- RosterStore reports incomplete history for interval overflow, parser-rejected roster records, and a pending RA_SET name-payload group. The pending condition is important because malformed() can still be zero while the parser awaits the remaining payloads.
- RE_UNTRUSTED and ES_BEGIN_ROSTER_UNTRUSTED were appended to their enums so existing numeric values are preserved.
- Roster planning and expected queries refuse incomplete history. API manual mark/enrollment checks return a locked state. Enrollment queue creation, pre-store rechecks and occupancy-refresh resume refuse untrusted history.
- Attendance scan begin and pre-commit match validation check roster history.
- EnrolledRoster forwards the underlying roster-history integrity status; reconcile and enrollment helper functions refuse to derive decisions from incomplete roster state.
- ClosureEngine refuses to create a CLOSE snapshot from incomplete roster history.
- Complete SET/rename/activate/deactivate append failures can be reconciled after reopen/replay when the requested final state is already present. The planner then returns RE_NOOP without another append.

## Critical review findings

1. historyComplete() is a parser/projection-integrity predicate, not a semantic lifecycle audit. Duplicate/no-op transitions and non-monotonic effective dates remain unresolved under U61.
2. The roster retry behavior is state-based, not operation-ID-based. A repeated old request after intervening changes may apply again. It is not generic exactly-once handling.
3. Partial append repair remains unavailable. An incomplete multi-record group can prevent reopen because LittleFsFileSet refuses truncate/quarantine (C13).
4. No production API handler, main loop, enrollment service or scheduler consumes these core results yet (C3). The host tests prove the reusable-core boundary only.
5. Roster state is shared across projections. The added check is fail-closed, but all production consumers must continue to use the same authoritative RosterStore and serialized log lifecycle.

## Regression evidence

- Initial seven-suite WIP batch: test_roster, test_roster_history, test_api, test_enroll, test_as608_pipeline, test_coreb, test_coreb2 — 163/163 passed.
- Projection dependencies: test_roster_snapshot + test_month_roster — 33/33 passed.
- After replacing malformed-record fixtures with a pending RA_SET header plus only its first payload (malformed counter remains zero): test_api + test_enroll + test_as608_pipeline + test_roster — 103/103 passed.
- EnrolledRoster-to-ClosureEngine trust propagation: test_roster — 14/14 passed.
- Related restore replay and clock regression later ran together: test_restore_replay + test_coreb — 58/58 passed.
- Additional adjacent dependency: test_attendance_admission — 12/12 passed.

These commands overlap; the release's unique selected-suite count is documented in RELEASE_v0.2.62.md. All evidence is PlatformIO native host testing with fake storage/UART. No target/hardware evidence is implied.
