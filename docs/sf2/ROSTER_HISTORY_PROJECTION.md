# Roster history projection — v0.2.20

Status: **source helper only**. It is not a production snapshot API. The signed P9 mapping (`student ID = never-reused sensor slot`) is unchanged.

## Current projection

`RosterStore` remains the existing event-sourced roster. v0.2.20 adds read-only accessors:

- `intervalCount()` returns the number of stored enrollment intervals.
- `intervalAt(index, out)` returns the indexed interval in replay/insertion order, with `studentId`, `startDayInclusive` and `endDayExclusive`. The interval is half-open: `[startDayInclusive,endDayExclusive)`. The open interval uses `INT32_MAX` as its end.
- `knownIdAt(ordinal, outId)` enumerates currently known IDs in ascending numeric order. IDs remain stable and are never reused under P9.
- `historyComplete()` is true only when the existing fixed interval table did not overflow and roster replay did not reject a roster record at the parser level (`!overflow_ && bad_ == 0`). It must be checked before using the interval projection. It does **not** prove that every activation/deactivation is chronologically sensible or that an inactive deactivation/active activation was impossible.

The existing table has `MAX_INTERVALS=512`. The API does not allocate a new roster history table. If more than 512 intervals are needed, the roster projection becomes incomplete and must not be used to claim full month coverage.

## Historical display names

`RosterNameHistoryReader` streams historical `RA_SET` name records from `LogStore`, returning:

- `studentId` — existing device ID / sensor slot.
- `effectiveDay` — local day derived from the event epoch using the supplied UTC offset.
- `sourceSeq` — original event sequence.
- `name` and `length` — exact validated display-name string stored by the device (maximum 32 bytes).

It holds only a single result and a 32-byte decode buffer; it does not materialize an unbounded history table. It requires a ready `EventGroupAudit` at construction, including the exact current `OpenInfo.generation`, and rejects reads if the `LogStore` faults, its count/last sequence changes, or the audit generation changes during streaming. A malformed/noncanonical name group latches an error; callers must discard partial output and must consume the stream to `ROSTER_NAME_END` to establish successful completion.

## What this does not provide

- It does not split a display name into first, middle and last name components.
- It does not provide LRN, sex, curriculum, section, transfer/withdrawal reason, destination, or NLS status. Those remain C21 reporting metadata.
- It does not resolve historical names when the RTC moved backwards and event sequence order differs from effective-day order. The downstream reporting profile must not sort solely by `effectiveDay` and assume it recreates the name active on a historical date.
- It does not establish section-level history, report-month totals or attendance coverage.
- It does not validate all enrollment lifecycle semantics. The current `RosterStore` replay tolerates duplicate/no-op activate/deactivate events and does not enforce monotonic effective dates after an RTC correction; a separate semantic transition audit is required before authoritative month-overlap reporting.
- It does not make `RosterStore` overflow impossible.
- It does not protect against a storage driver being modified directly outside `LogStore`; the future snapshot coordinator must serialize all reads/writes and own the source revision.

## Required snapshot preconditions

Before exporting a report-month roster, a future coordinator must require all of the following:

1. The authoritative `LogStore` is open and healthy.
2. One long-lived `EventGroupAudit` is attached after open, and `readyFor(log, exactOpenInfo)` is true.
3. `RosterStore::historyComplete()` is true.
4. The full `RosterNameHistoryReader` stream reaches `ROSTER_NAME_END` without error.
5. The coordinator freezes the log revision while deriving intervals/names and builds the report roster using the signed month-overlap rule.
6. A separate versioned reporting-identity store supplies LRN, sex, parsed name components and movement metadata.
7. The timetable/calendar coverage is independently proven; this roster projection cannot clear C17/E2 or C18/E26.

`EventGroupAudit` validates current event-group structure and selected current-writer record fields. It is neither a cryptographic log verifier nor a full validator for every future event type. Unsupported event types fail closed until their schema is implemented.
