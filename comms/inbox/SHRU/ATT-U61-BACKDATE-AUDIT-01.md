# ATT-U61-BACKDATE-AUDIT-01 — SHRU read-only audit

**Status:** COMPLETE — findings submitted; no implementation authorized  
**Baseline:** v0.2.95 development snapshot in canonical workspace `/root/prototype/att`  
**Integrity:** `MANIFEST.sha256` is not valid integrity evidence; the assignment records 66 failed entries in the latest read-only audit.  
**Workspace:** Canonical shared tree accessed successfully through RDC.  
**Scope:** Read-only trace of roster planning, API builders, durable append/observer notification, live `RosterStore`, replay transition audit and monthly projection.

## Executive finding

**Earlier-local-day roster events are not rejected consistently before append.** The three `RosterStore::plan*` functions do not compare a candidate event's local day with the learner's prior effective day. The API roster builders simply pass the supplied epoch into those planners. Therefore an earlier-day rename, deactivation or activation can be planned and appended.

The live `RosterStore` observer does not enforce chronological order. It applies the records (with a clamp on a backwards deactivation) and does not mark history incomplete solely because the effective day moved backwards. The separate `RosterTransitionAudit` detects that condition only when `replayRosterPrefix()` is later invoked, currently by `MonthRosterProjection::begin()`. Thus monthly projection fails closed, but live operational consumers that rely on `RosterStore::historyComplete()` can still treat the temporally invalid roster cache as trusted.

This is a **confirmed source-level consistency gap**, not evidence of a presently reachable production endpoint: the documented production boot shell still has no roster mutation route/writer. Same-day opposite lifecycle events are a separate, intentional E27 case and must not be conflated with strictly earlier-day events.

## Trace and evidence

### 1. Planner and API-builder boundary

- `RosterStore::planSet()` (`lib/core/src/roster_store.cpp:121-140`) validates output, ID, name and current history. For a known ID it returns `RE_NOOP` only for an identical name; otherwise it emits an `RA_SET` carrying the supplied epoch. It has no comparison against a previous day.
- `planDeactivate()` (`roster_store.cpp:142-151`) and `planActivate()` (`roster_store.cpp:153-163`) check ID, current history, known/active state and interval capacity, but not chronology. A valid state transition can therefore be planned with an epoch earlier than the learner's last roster event.
- `rosterSet()`, `rosterDeactivate()` and `rosterActivate()` (`lib/core/src/api_logic.cpp:255-272`) only map the planner result. They do not run an effective-day ordering check. The separate `dayChecks()` helper (`api_logic.cpp:40-47`) is not called by these roster builders. The builders accept a `now` epoch supplied by their caller; they do not accept a distinct client-provided effective-day field.
- This distinction matters: the pure API builder does not demonstrate that a client can directly submit an arbitrary past date. However, it also does not protect against a caller-supplied stale/backward epoch, RTC rollback, or another writer using the planner directly. No production HTTP caller is present to establish the actual caller's time source or serialization.

### 2. Live observer and durable append

- `RosterStore` is a `LogObserver` (`roster_store.h:17-21`). Its `historyComplete()` predicate is only `!overflow_ && bad_ == 0 && !col_` (`roster_store.h:31-36); it has no semantic chronology state.
- `RosterStore::onRecord()` (`roster_store.cpp:62-88`) validates record shape, identity and current lifecycle state, but does not compare event day with the preceding event day. `RA_SET` captures the new event day in `pday_`; a completed rename updates the current name without moving the enrollment interval (`roster_store.cpp:49-60`).
- A backwards `RA_DEACTIVATE` reaches `close_()`; `close_()` clamps the end to `max(interval.startDay, event.day)` (`roster_store.cpp:45-48`). If the requested day predates the open interval, this produces a zero-length interval instead of rejecting the record.
- A backwards `RA_ACTIVATE` reaches `open_()` at the supplied earlier day (`roster_store.cpp:40-43, 84-86`). After a prior deactivation this can create an interval that overlaps the preceding closed interval. In both cases, the event is applied to the live cache and chronology alone does not increment `bad_`; `historyComplete()` may remain true.
- `LogObserver::onRecord()` returns `void` (`lib/core/src/log_store.h:15-20`). `LogStore::append()` first calls `IStorage::append()`, then tracks and notifies observers, and finally advances its count/sequence and returns true (`log_store.cpp:118-136`). An observer cannot veto or roll back an already-durable append.
- Related, directly demonstrated observer behavior: the duplicate-lifecycle tests append an invalid same-state action and assert `LogStore::append()` returns true while the live `RosterStore` increments `malformed()` and marks history incomplete (`test/test_roster/test_main.cpp:263-281`). This is not the backdate path—the live observer currently does not reject backdated chronology—but it proves the post-commit observer contract can report a rejected record after append has succeeded. With concurrent/stale planners, duplicate state actions are a plausible case; the repository has no production writer composition proving such concurrency is or is not possible.

### 3. Replay transition audit and monthly projection

- `RosterTransitionAudit::consume()` in `lib/core/src/roster_snapshot.cpp:51-105` computes each roster event's local day with `dayNumber(event.epoch, utcOffsetMin)`.
  - First `RA_SET` for an ID establishes its initial day; it has no earlier per-ID day to compare.
  - A later `RA_SET` (rename) fails with `RTA_SET_BACKWARD_DAY` when `day < state.lastDay`; same-day renames remain accepted and source-sequence ordered (`73-78`).
  - `RA_DEACTIVATE` fails if `day < state.lastDay` or `day <= state.lastLifecycleDay` (`83-92`).
  - `RA_ACTIVATE` uses the corresponding check (`94-103`). Lifecycle boundaries must therefore be on strictly later local days; same-day opposite transitions are rejected by this reporting audit.
- `replayRosterPrefix()` replays each record into a scratch `RosterStore`, then consumes roster records through `RosterTransitionAudit`; a transition failure returns `RPR_TRANSITION_INVALID` (`roster_snapshot.cpp:141-160`). The header explicitly says the scratch store must be discarded unless replay returns `RPR_OK` (`roster_snapshot.h:92-101`).
- `MonthRosterProjection::begin()` rejects any non-`RPR_OK` replay as `MRP_REPLAY_FAILED` (`month_roster_projection.cpp:176-180`). This is a reporting fail-closed boundary, not pre-append admission or a live-cache guard.
- Existing regression `test_month_projection_rejects_backward_effective_day_history()` proves the asymmetry for rename: after day 110 SET, `planSet()` for day 105 returns `RE_OK`, `log.append()` returns true, and projection subsequently fails with `RPR_TRANSITION_INVALID` (`test/test_month_roster/test_main.cpp:271-282`). No equivalent backdated DEACTIVATE/ACTIVATE regression was found in the inspected `test_roster` or `test_month_roster` files. All tests were **NOT RUN** for this audit.

## Action-by-action outcome

| Action submitted for a previously known learner | Planner/API before append | Live `RosterStore` after append | Monthly replay/projection |
|---|---|---|---|
| Backdated rename (`RA_SET`) | Accepted unless same name/other input error | Name is replaced; interval unchanged; `historyComplete()` can remain true | Rejects as `RTA_SET_BACKWARD_DAY` |
| Backdated deactivate (`RA_DEACTIVATE`) | Accepted when currently active | Closes at the clamped start day if event day is earlier; may create a zero-length interval; chronology does not mark history incomplete | Rejects as `RTA_DEACTIVATE_NOT_FORWARD` |
| Backdated activate (`RA_ACTIVATE`) | Accepted when currently inactive | Opens at the supplied earlier day; can overlap a prior closed interval; chronology does not mark history incomplete | Rejects as `RTA_ACTIVATE_NOT_FORWARD` |
| Same-day opposite lifecycle | Accepted operationally when current state permits | Applied in sequence order | Rejected by reporting audit under E27; intentional reporting-only restriction |

For a brand-new ID, the first `RA_SET` establishes the replay audit's initial day and is not classified as backwards relative to that ID. The inspected specifications do not define whether first enrollment may be given a historical effective date; this is a separate policy boundary from changing an existing learner's history.

## Operational consequences and limits

1. A backdated rename can update the live name while making monthly roster projection unavailable. A backdated deactivation can collapse an interval; a backdated activation can create overlapping intervals. Since live `historyComplete()` does not include `RosterTransitionAudit`, helpers such as `EnrolledRoster.expected()`, `reconcile()`, `nextToEnroll()` and `firstFreeSlot()` can continue consuming the cache instead of automatically failing closed (`enrolled_roster.cpp:25-51`; `enrolled_roster.h:13-21`).
2. The reporting replay and live operational cache are not governed by one shared chronology acceptance decision. This can result in apparently available operational roster answers alongside a failed authoritative monthly projection.
3. Log sequence is assigned in append order (`log_store.cpp:118-136`), while the transition audit compares local calendar days derived from event epochs. The signed handoff's clock section says ordering is by sequence number and backward clock sets require explicit confirmation (`HANDOFF_FINAL.md:34-40`); E27 also notes that event epoch/sequence order exists while the roster projection is day-granular (`docs/DECISIONS.md:E27`). The transition audit does not consume `EV_CLOCK_SET` semantics; it only compares roster-event local days. A valid confirmed RTC rollback can therefore make subsequent sequence-ordered roster events appear to move backwards by local day. Whether those events should be blocked, accepted but reporting-untrusted, or interpreted under a clock-epoch/generation rule is not settled by the inspected authority.
4. The production boot shell currently lacks an integrated roster writer/consumer route. Findings describe reusable core behavior and a concrete source-level test seam, not proof of a remotely reachable production defect.

## Smallest policy decision required before implementation

The project owner needs to decide the admission contract for an existing learner's roster events when the candidate's local day is **strictly earlier** than that learner's last accepted roster-event day. Keep E27's same-day opposite lifecycle behavior separate; it is already explicitly reporting-only.

Alternatives for consideration (not selected by SHRU):

- **A — Reject strictly earlier-day changes before commit.** Define per-ID local-day monotonicity as an operational admission rule for rename/deactivate/activate, with lifecycle changes still subject to E27's same-day reporting restriction. Specify what happens during/after an explicitly confirmed RTC rollback (for example, whether roster writes remain blocked until time catches up or a separately approved correction procedure exists). Candidate validation must happen before durable append against the same serialized prefix that will be committed.
- **B — Preserve operational acceptance, fail closed for reporting.** Explicitly extend E27-like reporting-only treatment to strictly earlier-day events, but then make every operational consumer use a shared semantic-trust result; the present `RosterStore::historyComplete()` alone is insufficient. Also specify the operator-facing behavior after a backdated event has been committed.
- **C — Make append sequence the temporal authority.** Define how local-day intervals are derived when event epochs move backwards (including RTC rollback and day/session eligibility). The current interval model and `RosterTransitionAudit` do not implement that rule; this is not a small local patch.

The minimum decision is **A versus B**, plus the RTC rollback rule. The initial-enrollment effective date should be explicitly included or excluded from that rule. Until decided, do not silently add a planner rejection or relax the replay audit.

## Verification

- Canonical shared tree read access: **SUCCESS**.
- Source/spec/authority/test inspection: **DONE** for the exact assignment paths.
- Tests, compilers, builds, PlatformIO, flashing and hardware: **NOT RUN / NOT AUTHORIZED**.
- No project source, test, spec, decision, coordination or working-memory file was modified.
