# SF2 Batch D1 — specialized source audit (v0.2.23)

Date: 2026-11-07
Status: source review and archive-integrity audit only. Functional tests and C++ compilation were intentionally not run.

## Scope reviewed

- `MonthRosterProjection` C++ source and its caller-owned scratch `RosterStore` contract.
- Exact-prefix replay through `RosterLogPrefix`, `EventGroupAudit`, `OpenInfo` and `RosterTransitionAudit`.
- Browser `projectMonthRoster()` interval/name-history reconciliation.
- Month-overlap semantics, transfer-out inclusion, and E27 same-day lifecycle ambiguity.
- Source/test/documentation counts and the final archive manifest.

## E27 risk analysis

The existing roster store represents enrollment as local-day half-open intervals `[startDayInclusive, endDayExclusive)`. The event log retains source sequence and epoch, but the current `RosterStore` projection collapses the effective boundary to a local day. Therefore same-day lifecycle changes cannot safely be accepted as day-level intervals.

| Same-day sequence | Existing day-level consequence | Reporting risk |
|---|---|---|
| Initial SET/activation then DEACTIVATE on the same day | Can produce `[D,D)` after `RosterStore::close_()` | The learner may be omitted from the month roster even if present for part of the day. |
| DEACTIVATE then ACTIVATE on the same day | Can produce adjacent intervals `[start,D)` and `[D,open)` | The projection can make the learner appear enrolled for all of D despite an intraday gap. |
| ACTIVATE then DEACTIVATE after an earlier exit | Can produce a zero-length second interval | A learner may be incorrectly treated as never enrolled in the re-entry period. |
| Several changes on one local day | Source sequence orders actions but the date-only model cannot preserve their within-day boundaries | Attendance scans and CLOSE expected-ID bitmaps may disagree with the reconstructed roster. |
| RTC moved backwards | A later source sequence may have an earlier epoch/local day | Effective-date order and log order diverge; a chronological roster cannot be inferred without a clock-correction policy. |

**Disposition:** E27 remains OPEN. The existing transition audit continues to reject same-local-day ACTIVATE/DEACTIVATE and backwards effective-day transitions for reporting projection. No live `RosterStore` semantics, attendance policy, event format or P9 identity rule was changed.

The correct future design is not to relax the guard and accept day-level intervals. It is to preserve source event time and sequence in a separate time-granular reporting projection, then define session-level enrollment eligibility: whether a learner is enrolled at the relevant AM/PM session boundary (or for the whole session), and how a same-day transfer overlaps either session. RTC rollback and manual clock correction must be separately represented; source sequence alone must not silently become an effective-time policy.

## Source-level changes in this final D1 pass

1. `summarizeStudent_()` now has explicit result codes: `MRP_VALUE`, `MRP_NOT_FOUND`, or an error. A failed interval read/overflow is no longer conflated with “this learner has no interval overlapping the month,” so a row cannot be silently skipped on read failure.
2. Scratch fingerprint calculation now returns success/failure separately from the 32-bit hash value. A failed scan cannot accidentally compare equal to a numeric zero hash.
3. `begin()` clears prior fingerprint state before replay.
4. `OpenInfo` and `RosterLogPrefix` are copied into the projection. Caller-side mutation or temporary arguments cannot alter/dangle the source identity after construction.
5. The C++ test file includes `<stdint.h>` directly for the fixed-width integer constants it uses.
6. Added a regression for stale replay diagnostics when `begin()` is retried after the source prefix becomes stale.
7. Added a separate regression registration for initial SET followed by same-day DEACTIVATE. The pre-existing D1 case covers same-day DEACTIVATE followed by ACTIVATE. Both remain expected to fail closed under E27.

## D1 behavior retained

- One report-month row per stable device student ID with any reconstructed half-open enrollment interval intersecting the selected month.
- All overlapping intervals are preserved and streamed in original, unclipped form.
- End-of-month enrollment is reported separately from month-overlap membership.
- Historical display-name revisions are reconciled by the browser against the same exact source prefix.
- Missing names or multiple distinct names applying during the month block finalization rather than selecting one silently.
- P9 remains unchanged; learner ID remains the never-reused sensor slot. LRN remains a separate, not-yet-persisted reporting attribute.

## Limits and risks

- The C++ interval validation is O(n²) for at most 512 intervals; scratch mutation checks rescan up to 300 IDs and 512 intervals per returned row. No ESP32 timing or memory measurement exists.
- FNV-1a is a non-cryptographic accidental-mutation detector, not tamper protection or source authentication.
- Prefix tracking observes `LogStore` API changes; raw writes to the underlying storage driver can bypass it. A production coordinator must serialize all writes and reads.
- The projection is not installed in the production composition root or HTTP API.
- Operational display names are not legal name components. LRN, sex, section/curriculum profile, transfer reasons/destinations and reporting movement data still require the separate metadata workstream.
- Roster history is still day-granular. E27 remains a production reporting blocker for same-day lifecycle transitions.

## Verification record

- C++ source registrations: 376 across 23 suites.
- 318/318 passed is user-reported for v0.2.17 only.
- 58 later C++ registrations (25 v0.2.20, 17 v0.2.22 and 16 v0.2.23) remain unrun.
- SF2 Node registrations: 205, all unrun.
- Static `node --check` was performed for all SF2 `.mjs` source and test files. No Node functional tests were run.
- No C++ compiler, PlatformIO test, ESP32 target build, device test, or timing benchmark was performed.
- The release archive was extracted into a clean directory and its complete manifest was verified. `.git/`, `.pio/`, `node_modules/` and build caches are excluded.

## Next safe increment

Batch D2 should align the existing bitemporal reporting-metadata reference model with the month-roster output, separating operational display names from legal reporting identity and requiring explicit transfer/NLS metadata. It must not add persistence or expose learner PII until C26/C27/C29 and the storage/backup/security boundaries are resolved.
