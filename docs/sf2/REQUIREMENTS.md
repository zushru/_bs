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
| R4 | Generate learner rows automatically. | SIGNED by user, 2026-11-06/07. Row count is derived from the reporting-month roster, not a hard-coded four- or 45-row ceiling. |
| R5 | Include every learner whose section enrollment overlaps any part of the reporting month, including learners who transferred out before export. | SIGNED by user, 2026-11-07. |
| R6 | Use the HTML builder's AM/PM diagonal-cell convention as the intended local report convention (option 2). | SIGNED by user, 2026-11-07. This is a local adaptation, not proof of DepEd acceptance. |
| R7 | `E` counts as Present in monthly attendance totals. | SIGNED by user, 2026-11-07. |
| R8 | Preserve Late marks in source attendance. | SIGNED by user, 2026-11-07. This does not authorize dropping Late records or changing their stored status. |

## Selected daily-cell conversion

The legacy builder's `mk(am, pm)` function is the source for the matrix below. The new module uses stable status tokens rather than learner names or CSV position.

| AM | PM | Local SF2 cell token | Monthly day total |
|---|---|---|---|
| Present | Present | `BLANK` | Present |
| Present | Confirmed Absent | `AM_ONLY` (upper-left shading) | Present |
| Confirmed Absent | Present | `PM_ONLY` (lower-right shading) | Present |
| Confirmed Absent | Confirmed Absent | `X` | Absent |
| Excused | Excused | `E` | Present |
| Excused | Confirmed Absent | `E` | Present |
| Confirmed Absent | Excused | `E` | Present |
| Present | Excused | `BLANK` | Present |
| Excused | Present | `BLANK` | Present |

The initial pure model treats `LATE` as an attended session for quadrant selection and preserves a separate list of late sessions. This is a provisional bridge, not a teacher-confirmed reporting rule. It does **not** calculate or populate the template's monthly TARDY total yet. The TARDY total remains explicitly deferred; it must not be written as zero merely because it is not implemented.

## Fail-closed conversion rules

- Only the device-derived `ABSENT_CONFIRMED` state may produce an absence. It must carry evidence that the session is closed, coverage is complete, the learner was expected, the log is healthy, and the report snapshot is consistent.
- Missing data, `CLEAR`, unresolved status, uncommitted Present/Late/Excused events, or weak absence evidence never become `X`.
- A whole-day non-class date is excluded from class-day calculations.
- A date with only one session not scheduled is unresolved until a specific AM/PM convention is approved for that case. It is not guessed as Present or Absent.
- `E` counts as Present and breaks an absence streak under R7.
- `Late` does not change the stored attendance status. The new model records late-session observations but does not yet create a TARDY monthly number.

## Single-section and curriculum semantics

Both curricula are supported by configuration, but the signed one-section rule means one running device is configured for **one section and one curriculum profile at a time**. It does not manage a Regular SHS section and a Strengthened SHS section simultaneously. The configured section identity must be associated with its school year/term so later setup changes cannot relabel earlier attendance.

## Still open; not silently decided

1. Teacher/school acceptance of the custom AM/PM legend against the required submitted SF2 form.
2. Tardy daily/monthly computation and the exact printed TARDY field behaviour.
3. Whether `E` represents only a fully excused day, or also any day containing a qualifying excused session; the legacy conversion currently emits `E` only when no attended session is paired with the `E`.
4. Mapping for a day when one session is non-class and the other is scheduled.
5. Meaning of the workbook's `Semester` field under the applicable school-year calendar.
6. Whether the supplied XLSX is the latest school/LIS-approved template and whether the custom legend is accepted.
7. Transfer/dropped/shifting reason and destination details needed by the NLS/remarks section.
8. Enrollment end-date semantics and historical section-profile changes.
9. How a school-authorized administrator may resolve incomplete coverage, and what audit evidence that action requires.
10. Historical data before the device began recording: no synthetic attendance may be created. Any import process must be separately approved.

## Scope of v0.2.18/v0.2.19 code

`web/sf2/attendance_model.mjs` is a pure, DOM-free reference model for daily conversion, per-learner month summaries and month-overlap roster selection. `section_model.mjs` cross-checks every learner's expected class dates against a complete calendar, roster-history completeness and a consistent snapshot before generating daily sex-group/combined totals. `learner_layout.mjs` plans male-then-female learner rows and paginates only when supplied a separately measured template capacity. None of these modules fetch API data, store school metadata, edit an XLSX, or integrate into the production dashboard. The 55 tests are authored but were intentionally not run. No existing `lib/core` files or signed attendance rules were changed.


## Scope added in v0.2.19

The v0.2.19 browser-only increment adds a proposed revision-bound snapshot validator, adapter into the pure section calculation model, and a record-oriented diagnostic preview. It requires the complete report-month calendar, month-overlap learner intervals, exact attendance/session coverage, source event references, movement rows, and source revision agreement. For each closed session it requires exact close-group provenance: one `EV_CLOSE` sequence and five contiguous `EV_PAYLOAD` sequence references for the 300-ID bitmap, with matching close coverage flag/count and no sequence collisions.

A synthetic fixture and 89 additional Node test registrations were added (67 schema, 12 adapter, 10 preview). Together with the 55 existing v0.2.18 registrations, 144 SF2 JavaScript tests are registered. They remain intentionally unrun. JavaScript syntax-only checking was performed; no functional test, compiler, API, XLSX, ESP32, or hardware verification is claimed.

The browser validator cannot establish that a future ESP32 service decoded original event bytes correctly. C25 remains blocking: normal `LogStore::open()` does not invoke the staged-restore `RestoreGroupAudit`, and that audit currently accepts fewer than five close bitmap payloads even though the 300-ID closure writer emits five. A live snapshot builder needs a strict read-only event-group audit before it can assert trusted coverage or absence. C24 (historical roster enumeration/name history) and C17/E2 (historical timetable revisions) also prevent a complete authoritative snapshot today.


## v0.2.23 month-roster projection

The C++ `MonthRosterProjection` now rebuilds a scratch roster from one exact audited prefix and emits each learner whose enrollment interval intersects the selected month. It preserves every overlapping interval, emits an end-of-month-enrollment flag, and checks the source prefix and scratch-roster fingerprint during reads. The browser `projectMonthRoster()` reconciles the full name-history stream from that same prefix. Conflicting distinct names during the month block finalization; the model never silently picks a name. An empty month roster is represented as a valid zero-row selection. E27 is DELEGATED: same-day lifecycle changes continue to fail closed for reporting projection only; operational events remain unchanged. See `MONTH_ROSTER_PROJECTION_v0.5.md`.
