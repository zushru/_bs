# Historical roster prefix projection — design v0.4

Status: **host-test-ready source implementation; tests intentionally unrun; not production-wired**.

## 1. Purpose

A month-overlap report needs historical enrollment intervals and historical display-name revisions from one exact event-log prefix. The current live `RosterStore` changes as new events append, so a report must not read its intervals from one moment and its name history from another.

v0.2.22 adds a read-only prefix identity, transition audit and scratch replay. It does not change roster event encoding or the live roster rules.

## 2. Prefix identity

`RosterLogPrefix` captures:

- `openGeneration`: exact `LogStore::open()` generation;
- `recordCount` and `lastSeq`: the same committed prefix length;
- `auditGeneration`: exact `EventGroupAudit` replay/reset generation.

Capture succeeds only when `EventGroupAudit::readyFor(log, opened)` succeeds, the log is healthy, the open result is clean, the sequence starts at 1 and is contiguous, and the count/last sequence fit the current `uint32_t` event sequence model.

`rosterLogPrefixCurrent()` requires exact equality of all four prefix identity values and a still-ready audit. Any append, reopen, tail repair, read fault or audit reset invalidates the prefix. This is intentionally stricter than a read-only cursor that silently ignores later appends.

The existing `RosterNameHistoryReader` constructor keeps its strict changed-snapshot behavior. A second constructor accepts an explicit captured `RosterLogPrefix`; it does not relax the check. The reader only traverses records below that prefix count and aborts if the exact prefix ceases to be current.

## 3. Transition semantic audit

`RosterTransitionAudit` checks the lifecycle semantics that `RosterStore::historyComplete()` does not prove:

- a first `RA_SET` introduces a known, active ID;
- later `RA_SET` events are name revisions and do not change active state;
- a name revision may share a local day with another name revision, with source sequence resolving order, but may not move to an earlier local day;
- `RA_DEACTIVATE` requires a known, active ID and a strictly later local day;
- `RA_ACTIVATE` requires a known, inactive ID and a strictly later local day;
- invalid/pre-2020 local dates and unknown actions fail closed.

The replay also checks that the caller-supplied UTC offset is exactly the offset used to construct the scratch `RosterStore`; otherwise the same event prefix could produce different enrollment days. A mismatch is rejected before `scratch.onReset()`. This guard has an authored regression but is not yet functionally verified.

The strict later-day rule exists because roster intervals are day-granular `[startInclusive,endExclusive)` intervals. The existing format cannot represent a partial-day enrollment accurately. A deactivate/reactivate on the same local day is therefore rejected by this reporting audit rather than guessed.

**E27 is DELEGATED:** on 2026-11-07 the user accepted assistant judgment to keep same-local-day ACTIVATE/DEACTIVATE fail-closed for authoritative report-month projection. This is a reporting restriction only; it does not reject, edit, or rewrite committed operational roster events. Do not relax until time-granular session eligibility and RTC rollback semantics are designed.

Backward RTC corrections are not normalized or guessed. A roster action whose local effective day moves backwards for that student causes the audit to fail. This is conservative; it may require an approved correction/migration procedure if such records exist.

## 4. Scratch replay

`replayRosterPrefix()`:

1. verifies the exact prefix and `EventGroupAudit` readiness;
2. requires `scratch.utcOffsetMin()` to equal the explicit `utcOffsetMin` argument, rejecting a mismatch before changing scratch state;
3. resets a caller-owned scratch `RosterStore`;
4. reads exactly the captured record count in sequence order;
5. replays every event into the scratch roster;
6. feeds each `EV_ROSTER` leader record into `RosterTransitionAudit`;
7. checks that the prefix is still unchanged;
8. requires `scratch.historyComplete()` and agreement between semantic transition state and scratch known/active state.

Only `RPR_OK` makes the scratch roster eligible for further read-only projection. On any other return code, the caller must discard the scratch object and all partially read names/intervals.

The live `RosterStore` is never mutated. The caller owns the scratch object's memory; its approximately 16 KiB size must be statically or heap allocated, not placed on a small task stack.

## 5. Limits

This is not a complete event-domain semantic validator, cryptographic integrity verifier, PII store, or production HTTP endpoint. `EventGroupAudit` remains the structural/current-writer gate and rejects event types without a registered validator. Raw `IStorage` writes can bypass `LogStore` counters and are outside this in-memory revision model; the eventual firmware must enforce one serialized writer and forbid direct storage mutation while a snapshot is active.

The projection still exposes only the operational display name and active intervals. It does not provide legal name components, LRN, sex, transfer reasons/destinations, school profile, or curriculum metadata. C21/C24 remain blockers for complete SF2.
