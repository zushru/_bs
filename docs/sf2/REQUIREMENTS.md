# SF2-SHS reporting requirements and decision record

Status: **design-stage only**. This reporting layer must not change the signed attendance core or become a second source of truth.

## Product boundary

- The ESP32 remains authoritative for roster, enrollment state, timetable, calendar/no-class events, attendance events, closure state, and coverage evidence.
- Browser assets perform snapshot validation, daily-cell conversion, monthly calculations, preview, and XLSX generation.
- The browser must work offline after assets are served/cached locally.
- The supplied XLSX is an unchanged reference input only; it is not yet approved as the production template or confirmed as the latest official file.
- The old `sf2_builder.html` is a logic reference only. Its embedded workbook, coordinates, and OOXML writer are not reused.

## Confirmed reporting decisions

| ID | Decision | Status / boundary |
|---|---|---|
| R1 | Support both Regular SHS and Strengthened SHS reporting profiles. | SIGNED by user, 2026-11-07. |
| R2 | One device manages one section only. | SIGNED by user, 2026-11-07. |
| R3 | Ask for the section during initial setup. | SIGNED by user, 2026-11-07. Curriculum and grade must accompany the section identity so a section name is not ambiguous. |
| R4 | Generate exactly one learner attendance row per learner in the reporting-month roster. | SIGNED by user, 2026-11-06/07 and reaffirmed 2026-11-11. Expand the workbook row structure as needed; do not impose the sample's four-row count or a 45-row ceiling. |
| R5 | Include every learner whose section enrollment overlaps any part of the reporting month, including learners who transferred out before export. | SIGNED by user, 2026-11-07. |
| R6 | Use the local AM/PM cell convention defined below. | SIGNED by user, 2026-11-07; detailed legend and totals reaffirmed 2026-11-11. This is a local adaptation, not proof of DepEd acceptance. |
| R7 | Blank and `E` count as Present; `X` and either diagonal mark count as Absent in monthly totals. | SIGNED by user, 2026-11-07 for `E`; exact daily-to-monthly totals reaffirmed 2026-11-11. |
| R8 | Preserve Late internally; project it as attended for cell selection but do not emit a separate Late/Tardy mark or tally in SF2. | SIGNED by user, 2026-11-07 and reaffirmed 2026-11-10/11. Original `ST_LATE` records remain unchanged. |

## Selected daily-cell conversion and monthly totals

The model resolves each learner's AM/PM result into one SF2 daily cell. `LATE` is treated as attended for cell selection but is not emitted as a distinct report mark; the source event remains `ST_LATE`.

| AM | PM | Local SF2 cell token | Monthly total |
|---|---|---|---|
| Present | Present | `BLANK` | Present |
| Present/Late | Present/Late | `BLANK` | Present |
| Present/Late | Confirmed Absent | `AM_ONLY` (upper-left shaded) | Absent |
| Excused | Confirmed Absent | `AM_ONLY` (upper-left shaded) | Absent |
| Confirmed Absent | Present/Late | `PM_ONLY` (lower-right shaded) | Absent |
| Confirmed Absent | Excused | `PM_ONLY` (lower-right shaded) | Absent |
| Confirmed Absent | Confirmed Absent | `X` | Absent |
| Excused | Excused | `E` | Present |
| Present/Late | Excused | `BLANK` | Present |
| Excused | Present/Late | `BLANK` | Present |

**Monthly total rule:** `BLANK` and `E` increment Present. `X`, `AM_ONLY`, and `PM_ONLY` increment Absent. Diagonal days also count as absent when evaluating a five-consecutive-absence run. `E` breaks an absence run because it counts as Present.

The uploaded workbook's two diagonal styles must be rendered as actual upper-left/lower-right cell shading; the tokens above are semantic model values, not literal text to print in the cell.

A full day split between Present/Late and Excused maps to `BLANK` and counts as Present, in either AM/PM order. `E` is reserved for both sessions Excused.

## Fail-closed conversion rules

- Only the device-derived `ABSENT_CONFIRMED` state may produce an absence. It must carry evidence that the session is closed, coverage is complete, the learner was expected, the log is healthy, and the report snapshot is consistent.
- Missing data, `CLEAR`, unresolved status, uncommitted Present/Late/Excused events, or weak absence evidence never become `X`.
- A whole-day non-class date is excluded from class-day calculations.
- A date with only one session not scheduled is unresolved until a specific AM/PM convention is approved for that case. It is not guessed as Present or Absent.
- `E` (both sessions Excused) counts as Present and breaks an absence streak. A Present/Late + Excused full day maps to `BLANK`, counts as Present, and also breaks an absence streak.
- `AM_ONLY`, `PM_ONLY`, and `X` all count as Absent and extend an absence streak.
- `Late` does not change the stored attendance status. It is projected as attended for cell selection, but SF2 emits no Tardy marker or tally.


## Section summary projections (preview-only)

The reference section model exposes two additional summaries:

- `monthEndRegistration`: count each month-overlap learner once when at least one validated half-open enrollment interval contains the report month's final calendar date (`startInclusive <= finalDate < endExclusive`, or no end). Group by the validated M/F classification. This is deliberately different from `monthRosterCount`: a learner who transferred out before month-end remains in the attendance rows but is not registered on the final date. A learner whose end-exclusive date is the final date is not counted; an end-exclusive date on the following day is counted.
- `fiveConsecutiveAbsenceLearners`: count distinct learners whose longest run reaches at least five absent **confirmed class dates**, grouped by M/F. `X` and either diagonal extend the run; `BLANK` and `E` reset it. A class date outside the learner's enrollment intervals resets the run. Weekends and confirmed non-class dates are not entries in the class-date sequence.

These are reference/preview calculations only. The absence-run count currently evaluates only confirmed class dates inside the report month; it does not carry a run in from the previous month. The adviser must confirm whether "5 consecutive days" means class dates or calendar dates and whether a run may cross a month boundary. The snapshot contract and source-history projection are not deployed, and O8 still prohibits authoritative reporting persistence/export.

## Single-section and curriculum semantics

Both curricula are supported by configuration, but the signed one-section rule means one running device is configured for **one section and one curriculum profile at a time**. It does not manage a Regular SHS section and a Strengthened SHS section simultaneously. The configured section identity must be associated with its school year/term so later setup changes cannot relabel earlier attendance.

## Still open; not silently decided

1. Teacher/school acceptance of the custom AM/PM legend against the required submitted SF2 form.
2. During template polish, remove or repurpose the static Tardy legend/field and confirm the school accepts a report with no separate Tardy mark or tally.
3. Adviser/school acceptance of the full-day Present/Late + Excused mapping to `BLANK` (now user-selected locally; formal template acceptance remains open).
4. Mapping for a day when one session is non-class and the other is scheduled.
5. Meaning of the workbook's `Semester` field under the applicable school-year calendar.
6. Whether the supplied XLSX is the latest school/LIS-approved template and whether the custom legend is accepted.
7. Transfer/dropped/shifting reason and destination details needed by the NLS/remarks section.
8. Enrollment end-date semantics and historical section-profile changes.
9. How a school-authorized administrator may resolve incomplete coverage, and what audit evidence that action requires.
10. Historical data before the device began recording: no synthetic attendance may be created. Any import process must be separately approved.
11. Confirm whether the five-consecutive-absence summary means five consecutive confirmed class dates or five consecutive calendar days.
12. Confirm whether the five-consecutive-absence summary carries an absence run into a month when the run began in the previous month. The current preview calculation evaluates only class dates inside the report month.

## Scope of v0.2.18/v0.2.19 code

`web/sf2/attendance_model.mjs` is a pure, DOM-free reference model for daily conversion, per-learner month summaries and month-overlap roster selection. `section_model.mjs` cross-checks every learner's expected class dates against a complete calendar, roster-history completeness and a consistent snapshot before generating daily sex-group/combined totals. `learner_layout.mjs` plans male-then-female learner rows and paginates only when supplied a separately measured template capacity. None of these modules fetch API data, store school metadata, edit an XLSX, or integrate into the production dashboard. The 55 tests are authored but were intentionally not run. No existing `lib/core` files or signed attendance rules were changed.


## Scope added in v0.2.19

The v0.2.19 browser-only increment adds a proposed revision-bound snapshot validator, adapter into the pure section calculation model, and a record-oriented diagnostic preview. It requires the complete report-month calendar, month-overlap learner intervals, exact attendance/session coverage, source event references, movement rows, and source revision agreement. For each closed session it requires exact close-group provenance: one `EV_CLOSE` sequence and five contiguous `EV_PAYLOAD` sequence references for the 300-ID bitmap, with matching close coverage flag/count and no sequence collisions.

A synthetic fixture and 89 additional Node test registrations were added (67 schema, 12 adapter, 10 preview). Together with the 55 existing v0.2.18 registrations, 144 SF2 JavaScript tests are registered. They remain intentionally unrun. JavaScript syntax-only checking was performed; no functional test, compiler, API, XLSX, ESP32, or hardware verification is claimed.

The browser validator cannot establish that a future ESP32 service decoded original event bytes correctly. C25 remains blocking: normal `LogStore::open()` does not invoke the staged-restore `RestoreGroupAudit`, and that audit currently accepts fewer than five close bitmap payloads even though the 300-ID closure writer emits five. A live snapshot builder needs a strict read-only event-group audit before it can assert trusted coverage or absence. C24 (historical roster enumeration/name history) and C17/E2 (historical timetable revisions) also prevent a complete authoritative snapshot today.


## v0.2.23 month-roster projection

The C++ `MonthRosterProjection` now rebuilds a scratch roster from one exact audited prefix and emits each learner whose enrollment interval intersects the selected month. It preserves every overlapping interval, emits an end-of-month-enrollment flag, and checks the source prefix and scratch-roster fingerprint during reads. The browser `projectMonthRoster()` reconciles the full name-history stream from that same prefix. Conflicting distinct names during the month block finalization; the model never silently picks a name. An empty month roster is represented as a valid zero-row selection. E27 is DELEGATED: same-day lifecycle changes continue to fail closed for reporting projection only; operational events remain unchanged. See `MONTH_ROSTER_PROJECTION_v0.5.md`.
