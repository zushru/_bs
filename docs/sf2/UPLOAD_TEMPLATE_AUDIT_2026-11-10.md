# Uploaded SHSF-2 working-template audit — 2026-11-10

Status: **working design reference; not verified as the current official DepEd/LIS template; not yet approved for authoritative export**.

## Source file

- Uploaded filename: `PR II RV Mock Biometric Attendance Excel Output.xlsx`
- Pinned project copy: `docs/sf2/reference/UPLOADED_SHSF2_WORKING_TEMPLATE_2026-11-10.xlsx`
- SHA-256: `73769d444263919e8f7117a3015dc6f54d87a1fb74de4c4320d57f4f01a8f4eb`
- Workbook: one sheet, `SHSF-2`; used range `A1:AY46`.
- The user designates this as the current working export template and expects further polish. This audit does not claim school/LIS approval.

## Explicit `In` markers

These 26 cells are the workbook's marked input locations. Populate a marker only when its value is known and applicable; preserve the static template text. `W7` is conditional on TVL.

| Group | Cell | Label / meaning |
|---|---|---|
| School profile | `F3` | School Name |
| School profile | `V3` | School ID |
| School profile | `AF3` | District |
| School profile | `AR3` | Division |
| School profile | `AX3` | Region |
| School profile | `F5` | Term |
| School profile | `V5` | School Year |
| School profile | `AH5` | Grade Level |
| School profile | `AV5` | Track and Strand |
| School profile | `F7` | Section |
| School profile | `W7` | Course/s (TVL only; otherwise leave blank) |
| Month | `AV7` | Month of |
| Month | `AR21` | Month |
| Class calendar | `AU21` | Number of days of classes |
| Month-end registration | `AW24` | Male registered learners as of end of month |
| Month-end registration | `AX24` | Female registered learners as of end of month |
| Month-end registration | `AY24` | Total registered learners as of end of month |
| Attendance summary | `AW26` | Male average daily attendance |
| Attendance summary | `AX26` | Female average daily attendance |
| Attendance summary | `AY26` | Total average daily attendance |
| Attendance summary | `AW27` | Male percentage of attendance for month |
| Attendance summary | `AX27` | Female percentage of attendance for month |
| Attendance summary | `AY27` | Total percentage of attendance for month |
| Consecutive absence | `AW28` | Male count absent for 5 consecutive days |
| Consecutive absence | `AX28` | Female count absent for 5 consecutive days |
| Consecutive absence | `AY28` | Total count absent for 5 consecutive days |

## Attendance-grid interpretation

The actual learner attendance grid must also be populated, even though its daily cells are not marked `In`. The current project model consolidates AM and PM into one daily SF2 result:

- `BLANK`: Present in both sessions.
- `X`: Absent in both sessions.
- `E`: Excused in both sessions.
- `BLANK`: also applies when one session is Present/Late and the other is Excused; that full day counts as Present.
- Upper-left shaded: Present/Excused AM, confirmed Absent PM.
- Lower-right shaded: confirmed Absent AM, Present/Excused PM.

Monthly totals count `BLANK` and `E` as Present; `X` and either diagonal as Absent. Diagonals therefore also count toward a five-consecutive-absence run. A full day split between Present/Late and Excused maps to `BLANK` and counts as Present in either session order. `LATE` is treated as attended when selecting the daily cell, but no separate Tardy mark or tally is emitted; the original internal `ST_LATE` event remains unchanged.

The template's static legend currently includes a Tardy explanation and does not show the full `E`/diagonal/monthly-total rules. It must be revised during template polishing.

## Structural and calculation blockers

1. **Four learner placeholders only.** The sheet has two male learner rows (12–13), a male subtotal (14), two female learner rows (15–16), a female subtotal (17), and a combined total (18). The semantic row planner already creates exactly one row per month-roster learner and has no four-row or 45-row ceiling; a 60-learner row-plan regression now covers this. The future XLSX writer must actually expand the workbook's male/female learner blocks to match those rows, then reposition subtotal/combined-total rows and preserve styles, formulas, merges, print area, repeated headers and print behavior. The workbook itself does not yet expand dynamically.
2. **Calendar alignment.** The weekday header has 31 repeated weekday positions beginning with Monday and does not provide a separate day-of-month number row. A writer must map each actual calendar date to its actual weekday and handle month length and starting weekday. It must not assume the first day of every month is Monday.
3. **Month-end registration is a separate count.** The preview model now exposes `monthEndRegistration` by male/female/total, derived from validated half-open enrollment intervals containing the final calendar date. It counts each learner once, including re-entry histories. It deliberately differs from the month-overlap attendance roster: transfers out before the final date remain in attendance rows but are excluded from the month-end count. This has model and snapshot-adapter coverage; it is not yet written into workbook cells.
4. **Average daily attendance is not yet emitted.** The current section model exposes daily totals and cumulative learner-day counts, but does not produce the workbook's average-daily-attendance cells. The averaging denominator and treatment of one-session days must be confirmed before implementing the formula.
5. **Attendance percentage is not yet emitted.** The model does not currently return the workbook's percentage cells. The denominator must be explicitly defined (for example, enrolled learner-days versus another school-approved basis) rather than guessed.
6. **Five-consecutive-absence counts are previewed, with an unresolved definition.** The section model now exposes distinct male/female/total learner counts whose longest run reaches five absent confirmed class dates. `X` and either diagonal extend a run; `BLANK` and `E` reset it. A confirmed class date outside a learner's enrollment intervals resets the run, preventing absences on either side of a transfer-out/re-entry gap from being joined. The calculation is report-month scoped and does not carry a run in from the previous month. Adviser confirmation is still required on class dates versus calendar days and cross-month carry-in. The count is not yet written into workbook cells.
7. **NLS/movement table.** No `In` markers occur in this section, and the current attendance totals do not provide sufficient school-approved movement reasons/destinations. Per the present instruction, leave this section blank rather than inventing entries; confirm separately whether a complete official form requires it.
8. **Template status and identity data.** School profile fields, learner names, sex, and movement history are not all available from the device's currently approved storage/API contract. Do not fill them with guessed or synthetic values in an authoritative workbook.

## O8 implementation gate

O8 remains signed: ordinary device backup is config + roster + attendance log. The reporting design explicitly prohibits persisting learner legal names, LRN, sex, source categories, review records, or movement details on the ESP32, in ordinary logs/NVS/browser storage, via HTTP, or in XLSX. Authoritative SF2 export remains disabled until the user explicitly approves either an O8 amendment or a separately specified companion backup/recovery workflow that preserves reporting metadata and attendance as one recoverable reporting set. A volatile preview may be developed only with a clear data-loss warning and must not be represented as a usable reporting record.

## Current implementation state

- The browser-side SF2 code is a pure reference/preview model.
- Daily AM/PM consolidation and `LATE` -> attended cell projection are implemented. `BLANK`/`E` count Present; `X`/both diagonals count Absent, including for consecutive-absence streaks.
- The section preview exposes a separate month-end registration count and sex-grouped counts of learners with at least five absent confirmed class dates. Streaks reset on confirmed class dates outside a learner's enrollment intervals. These fields are calculations only; the workbook is not populated.
- There is no production snapshot API, dashboard integration, or XLSX writer.
- The model always keeps `finalExportReady` false; do not describe this increment as an implemented SF2 export.

## Work required before an XLSX writer

- Polish and approve the workbook's labels, calendar columns, dynamic learner-row insertion, formulas, print settings, and Tardy legend (remove/rewrite the Tardy entry to match the no-Tardy SF2 rule).
- Confirm the summary formulas and one-session-day handling with the adviser.
- Obtain adviser confirmation for the consecutive-absence definition, including calendar-day versus class-day interpretation and cross-month carry-in. Do not treat the provisional preview aggregate as an approved workbook formula.
- Define the required movement/NLS policy and data source.
- Resolve O8 using an explicit approved companion backup/recovery design or O8 amendment before enabling authoritative export.
