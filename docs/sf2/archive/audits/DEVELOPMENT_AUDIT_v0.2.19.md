# SF2-SHS Development Audit — v0.2.19

Date: 2026-11-07  
Status: development snapshot; no production exporter or API

## Scope

v0.2.19 adds a browser-side snapshot contract, adapter into the existing pure section calculator, and a diagnostic preview model. It does not change attendance-core behavior, signed product rules, C++ implementation, the production dashboard, HTTP handlers, persistent storage, or XLSX generation.

## Implemented source

- `web/sf2/snapshot_schema.mjs`: validates the proposed one-section/month snapshot, revision tuple, month calendar, reporting profile, school metadata, month-overlap roster, enrollment intervals, event provenance, coverage, close-group references, movement records and completeness claims.
- `web/sf2/snapshot_adapter.mjs`: passes a structurally valid/incomplete snapshot through the existing section calculation model. It does not perform I/O or write source data.
- `web/sf2/preview_model.mjs`: creates learner/date/session diagnostics for absent or unresolved pairs, missing results, coverage gaps and the unapproved one-session-day policy. It keeps final export disabled.
- `web/sf2/fixtures/snapshot_fixtures.mjs`: synthetic November 2026 data for enrollments and transfers within the month, AM/PM statuses, and close-group provenance.

## Source-level reconciliation

Reviewed against `event_codec.h`, `closure_engine.cpp`, `log_store.cpp`, `day_view.cpp`, `roster_store.*`, and `restore_replay.*`.

1. `Event.seq` is `uint32_t`; the proposed browser contract limits event references to 1..`0xFFFFFFFF` and not beyond `attendanceLastSeq`.
2. For the current capacity `MAX_ID=300`, `ClosureEngine::closeSession()` creates `uint64_t words[5]` and calls `payloadCount(cap_)`, writing five `EV_PAYLOAD` records after each `EV_CLOSE`. The expected-student bitmap is the historical roster snapshot for that close; it must not be reconstructed from the current roster alone.
3. The browser contract requires one `EV_CLOSE` sequence plus exactly five immediately following `EV_PAYLOAD` sequences, all within the snapshot sequence boundary. It checks that the close coverage flag matches `coverageComplete`, the expected count matches the decoded ID array, IDs are unique and within 1..300, close groups do not reuse sequences, and attendance event references do not collide with a close group.
4. The browser schema cross-checks the projected expected-ID set against `[startInclusive,endExclusive)` enrollment history. This is an internal consistency check only; it does not decode the raw log.

## Critical finding C25

`LogStore::open()` validates record-level CRC/sequence and tail/group framing, then replays observers. Normal `open()` does not attach or invoke `RestoreGroupAudit`. `DayView` consumes `EV_CLOSE` and the following payload records under the assumption that the event group is valid.

A second, stricter finding: `RestoreGroupAudit::validGroup_()` currently allows a CLOSE group with fewer than five bitmap payloads. It permits a one-record CLOSE when `EV_CLOSE.a == 0`, and other short payload groups when the bitmap popcount matches `a`. That does not enforce the current 300-ID writer's five-word shape. Its current padding-bit and popcount checks are useful, but its CLOSE group length is not strict enough for a trustworthy live reporting snapshot.

Before the ESP32 may project trusted absence/coverage, a future read-only authoritative-log audit must validate:

- exactly one `EV_CLOSE` plus five `EV_PAYLOAD` records for `MAX_ID=300`;
- correct `MORE` flag progression and no orphan/interleaved payload;
- contiguous increasing event sequence, valid record CRC, and no torn/dropped group;
- valid session flag and day matching the coverage row;
- zero unused bits above ID 300 in the final bitmap word;
- bitmap popcount exactly equal to `EV_CLOSE.a`;
- no malformed roster/gap/clock groups, roster malformed/overflow state, or closure-index gap/close overflow;
- attendance source references that match the actual decoded event's student ID, day, session, event type and effective status.

Until that audit exists, `attendanceSnapshotConsistent` and `coverageComplete` cannot be asserted merely because `LogStore::open()` succeeded. This is recorded as C25/U56. No C++ patch was made in v0.2.19.

## Other blockers

- **C24:** `RosterStore` has no public complete interval enumeration and does not retain all historical name versions. A month-overlap roster cannot yet be assembled truthfully from the live roster alone.
- **C17/E2:** historical timetable-effective revisions are not persisted; full historical calendar completeness cannot be certified after timetable changes.
- **C21/C22:** reporting identity/profile stores and the authenticated immutable snapshot endpoint do not exist.
- **C19/C20:** the AM/PM legend is a user-selected local convention but is not school-confirmed; the uploaded 2018 workbook is structurally unsafe and its current official status is unknown.
- **C23:** TARDY computation, Excused combinations and one-session class-day handling remain teacher-dependent.

## Test and verification status

- SF2 Node registrations: **144 total** (55 from v0.2.18; 89 added in v0.2.19: 67 schema, 12 adapter, 10 preview).
- Tests were intentionally not executed. They are registrations, not pass results.
- All `.mjs` files under `web/sf2` passed `node --check`. This is syntax-only and does not establish functional behavior.
- C++ source/test files were not modified. The last user-reported native result remains **318/318 on v0.2.17**; no later PlatformIO run is claimed.
- No ESP32 target build, device verification, HTTP integration, browser UI integration, XLSX writer, workbook round-trip, or print validation was performed.

## Verdict

The snapshot contract is materially stricter and now reflects the actual CLOSE bitmap shape at the browser boundary. It is still only a proposed contract. **Do not build the live endpoint or allow final export until C24/C25 and the schedule-history blocker are addressed, and until the workbook/legend are accepted.**
