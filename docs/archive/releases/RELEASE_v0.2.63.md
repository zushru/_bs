# Release v0.2.63 — overflow-safe GAP encoding

Date: 2026-11-09
Project root: /root/att

## Included incremental work

This release contains the completed v0.2.61 roster-history integrity gates, the v0.2.62 typed clock-commit result, and the v0.2.63 GAP arithmetic/preflight hardening.

### v0.2.63 change

- Replaced overflow-prone (to - cur + 59) / 60 rounding with division plus remainder. The old addition could wrap for large uint32 spans.
- Each EV_GAP chunk is bounded by the whole minutes that still fit before UINT32_MAX. The helper never emits a derived end beyond UINT32_MAX or a zero-minute record.
- Added gapSpanEncodable(from, to) using uint64 arithmetic. It rejects reversed spans and spans whose required rounded whole-minute exclusive endpoint cannot be encoded.
- appendGaps() and applyClockSet() run the shared range preflight before any append. An unrepresentable interval is rejected without persisting an otherwise misleading partial prefix.
- The lower-level makeGapEvents() can still return a bounded representable prefix through its next output when the caller explicitly handles partial progress.
- No event field, record size, persistent format, or GAP coalescing policy changed.

### Why the preflight is necessary

EV_GAP stores an epoch plus a whole-minute duration in a 32-bit epoch domain. Rounding a final partial minute upward can move the derived endpoint beyond UINT32_MAX, even when the requested to itself is UINT32_MAX. Clamping individual chunks prevents wraparound, but callers that promise to cover the complete interval must validate the entire rounded span before their first append.

## Verification

Current inventory: **605 C++ RUN_TEST registrations across 35 suites**.

The final release verifier runs the following 12 directly affected suites, totalling **246 registrations**:
- test_roster (14)
- test_roster_history (7)
- test_api (36)
- test_enroll (40)
- test_as608_pipeline (13)
- test_coreb (44)
- test_coreb2 (17)
- test_roster_snapshot (17)
- test_month_roster (16)
- test_restore_replay (15)
- test_hardening (15)
- test_attendance_admission (12)

Latest pre-release verification:
- Latest final-source dependent batch: pio test -e native -f test_coreb2 -f test_coreb -f test_hardening -f test_restore_replay — **91/91 passed**, exit 0. This includes the final no-write tests for unrepresentable intervals.
- Exact final release command: ./tools/verify_v0.2.63.sh targeted — **246/246 passed across all 12 selected suites**, exit 0, runtime 3:40.440.
- Earlier separate WIP/clock runs are historical supporting evidence only; the 246/246 result is the final-tree verifier run.

The remaining **359/605 C++ registrations are not run** in this release. This is not a full native regression. SF2 Node tests, final ESP32 target build, ELF inspection, hardware, actual VFS fault injection and power-cut tests were not run.

## Residual risks

- The roster completeness predicate is parser/projection integrity only; chronological lifecycle semantics remain unresolved (U61).
- Roster write reconciliation is state-based, not durable-operation-ID idempotency.
- ClockApplyResult has no production caller yet. C3 remains blocking; a future caller must stop writes and reopen/reconcile after uncertainty.
- GAP/clock writes still lack durable operation identity. Duplicate retry records can consume append-only storage (U112).
- Partial writes can prevent reopen because LittleFsFileSet refuses truncate/quarantine (C13). This change does not solve torn-tail repair or prove target durability.
- No production firmware root, scheduler, HTTP route, admin UI or enrollment service exists (C3).
- E18 and U5 remain delegated/unverified. No signed or delegated decision was changed.
- HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
