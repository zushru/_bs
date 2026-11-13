# Release v0.2.62 — roster trust gates and explicit clock commit status

Date: 2026-11-09

## Included increments

### v0.2.61 — complete the roster-history integrity WIP

- Added IRoster::historyComplete() as an explicit projection-integrity contract. RosterStore returns true only when its interval table has not overflowed, no roster record was rejected, and no RA_SET payload group is pending.
- Added RE_UNTRUSTED and ES_BEGIN_ROSTER_UNTRUSTED without renumbering existing enum values.
- Fail-closed gates now cover roster expected/planning operations, API manual-mark and enrollment gates, enrollment queue creation and pre-store/refresh resume, attendance scan start and match commit, EnrolledRoster/reconcile helpers, and ClosureEngine CLOSE snapshot creation.
- Complete roster SET/rename/activate/deactivate writes that returned false are checked after a fresh LogStore reopen/replay. If the current replayed state already reflects the requested operation, the planner returns RE_NOOP and writes nothing.
- Strengthened consumer regressions to use a pending RA_SET header plus only its first payload (bad_ remains zero) rather than relying only on a malformed-record counter. Added a ClosureEngine test through EnrolledRoster, proving the adapter forwards the underlying roster-history trust state.

### v0.2.62 — clock commit result and decision enforcement

- Replaced bool applyClockSet() with ClockApplyResult: REJECTED, COMMITTED, COMMIT_UNCERTAIN and LOG_FAULTED.
- The write boundary now rejects every decision whose action is not CA_APPLY, unknown source values, and reversed GAP bounds before appending.
- A pre-faulted LogStore is distinguished from an attempted append failure.
- Any false append reports COMMIT_UNCERTAIN, including the final commit group after earlier GAP chunks were already written. Reopening a test log proves that an accepted GAP prefix remains and the final CLOCK_SET pair is absent; another test proves a complete CLOCK_SET pair may be present after an append returned false.
- Persistent record encoding and event format are unchanged.

## Verification evidence

Current inventory: **601 C++ RUN_TEST registrations across 35 suites**.

The final focused set contains **242 registrations across 12 suites**, all passed in the current source tree:
- test_roster (14)
- test_roster_history (7)
- test_api (36)
- test_enroll (40)
- test_as608_pipeline (13)
- test_coreb (43)
- test_coreb2 (14)
- test_roster_snapshot (17)
- test_month_roster (16)
- test_restore_replay (15)
- test_hardening (15)
- test_attendance_admission (12)

Exact executed command batches:
- WIP initial batch: pio test -e native -f 'test_roster' -f 'test_roster_history' -f 'test_api' -f 'test_enroll' -f 'test_as608_pipeline' -f 'test_coreb' -f 'test_coreb2' — 163/163 passed.
- Projection dependencies: pio test -e native -f 'test_roster_snapshot' -f 'test_month_roster' — 33/33 passed.
- After strengthening pending-RA_SET consumer tests: pio test -e native -f 'test_api' -f 'test_enroll' -f 'test_as608_pipeline' -f 'test_roster' — 103/103 passed.
- EnrolledRoster-to-ClosureEngine adapter regression: pio test -e native -f 'test_roster' — 14/14 passed.
- Clock and restore-replay batch: pio test -e native -f 'test_coreb' -f 'test_restore_replay' — 58/58 passed.
- Clock dependent hardening suite: pio test -e native -f 'test_hardening' — 15/15 passed.
- Attendance admission dependency: pio test -e native -f 'test_attendance_admission' — 12/12 passed.

The clock API test-first attempt first failed compilation because the new result symbols did not yet exist. The first implementation run then exposed a faulty test expectation: the long-gap prefix had already advanced the original LogStore count to 14; the test was corrected to assert that successful prefix rather than expect zero. The final clock suite and its dependent suites passed.

The 242 figure is the sum of unique registrations in the selected suites, not one monolithic PlatformIO invocation. The other **359/601 registrations were not run**. No full native suite, SF2 Node suite, ESP32 target build, ELF inspection, physical device, flash, VFS fault injection or power-cut test was performed.

## Residual risks and limits

- Roster history completeness is parser/projection integrity only. Duplicate/no-op lifecycle transitions and non-monotonic effective dates remain U61; the semantic transition audit is not wired into operational RosterStore.
- Roster retry no-op behavior is based on current replayed state, not a durable request ID. An old identical request after intervening state changes may execute again. No generic exactly-once behavior is claimed.
- ClockApplyResult has no production caller yet (C3). The future caller must handle all four results and stop/reopen/reconcile after CLOCK_APPLY_COMMIT_UNCERTAIN.
- A false append can still leave a partial tail. LittleFsFileSet refuses truncate/quarantine; C13 remains a service-availability blocker.
- No production firmware composition, HTTP/API routes, UI, enrollment service, scheduler, global owner or RTOS synchronization exists (C3).
- E18 and U5 remain unresolved/delegated. No product or persistence decisions were changed.
- HANDOFF_FINAL.md and docs/DECISIONS.md are unchanged.
