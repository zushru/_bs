# SF2-SHS development and critical audit — v0.2.18

Date: 2026-11-07  
Status: development snapshot; no production export or school-compliance claim.

## User-approved scope and rules

- Support either Regular SHS or Strengthened SHS, with one curriculum profile configured per device at a time.
- One section per device. Ask for curriculum, grade and section during setup.
- Generate learner rows from every learner whose section-enrollment period overlaps any part of the report month, including learners who leave before export day.
- Use the selected local AM/PM diagonal convention: full day = blank, AM only = upper-left shading, PM only = lower-right shading, `E` = excused, `x` = absent in both sessions.
- `E` counts as Present in monthly totals.
- Preserve internal Late records. The current reference model provisionally treats Late as attended when selecting the daily quadrant, records late-session observations and leaves the TARDY total deferred. This is not a teacher-confirmed rule.

These reporting choices do not change the signed device attendance model. Absent remains derived and requires trusted closure and coverage; missing records are never converted to `x`.

## Implemented in v0.2.18

### Browser calculation models

- `attendance_model.mjs`: AM/PM conversion, one-learner month summaries and month-overlap roster selection using stable student IDs and half-open enrollment intervals.
- `section_model.mjs`: requires complete roster-history/calendar/snapshot flags; checks exact expected attendance dates for every learner; rejects missing, duplicate, unexpected or unresolved records; calculates male/female and combined daily learner totals.
- `learner_layout.mjs`: creates a male-then-female semantic row sequence and paginates it using an explicit `learnerRowsPerPage` parameter. There is no default or hard-coded 4/45/300 row limit.

No API, browser dashboard, persistent reporting-profile store, XLSX writer or device firmware integration was added. No C++ production or test source was changed.

### Tests

- 29 attendance/roster-model registrations.
- 11 semantic row/pagination registrations.
- 15 section aggregation registrations.
- **55 JavaScript test registrations total; all intentionally unrun.**
- The user reported 318/318 PlatformIO native tests passed on v0.2.17. v0.2.18 does not alter those sources; this is not ESP32 or device verification.

## Uploaded workbook audit

The original uploaded workbook is preserved byte-for-byte as:

`docs/sf2/reference/UPLOADED_SHSF2_SOURCE_NOT_FINAL.xlsx`

SHA-256: `6894d7673fee924c88f09478d6473ec71194e3839c595dc0d97d29835f51fac5`

Observed structure:

- One worksheet: `SHSF-2`.
- Four learner placeholders: male rows 12–13, female rows 15–16; subtotals at 14 and 17, combined total at 18.
- 30 attendance-date slots in five Monday–Saturday blocks.
- 184 merged ranges.
- 25 defined names, 23 of which contain `#REF!`.
- One external workbook link referring to 17 sheet names.
- Zero formula cells; visible `18` values are literals.
- Two drawing anchors and two embedded media files.
- Landscape orientation, custom paper-size code 258, fixed 85% scale, 300-DPI metadata.
- Core workbook metadata dates to 2018.

The workbook's printed half-shading legend conflicts with the user's selected local AM/PM convention. Its current status as the latest school/LIS-approved file is unknown. This is a reference-only input, not a final exporter template.

## Critical architecture gaps

1. **Historical roster extraction (C24):** `RosterStore` keeps enrollment intervals privately; there is no interval enumeration API. `nameOf()` returns the latest name, and a new `RA_SET` replaces the prior stored name. A browser cannot reconstruct complete month-overlap rows or historical names from the current public roster interface.
2. **Reporting metadata (C21):** LRN, sex, full name components, transfers/NLS reasons and destinations, curriculum/section profile and school metadata are not present in the operational roster.
3. **No snapshot/API (C22):** there is no authenticated read-only endpoint or immutable revision-bound snapshot. The browser-side completeness flags are only inputs; a future server adapter must derive them from authoritative state and reject mixed revisions.
4. **Template not production-safe (C20):** broken names, external link, merged cells, images, fixed print scale and four learner placeholders make simple row insertion unsafe.
5. **Teacher-dependent policies (C23):** Late/TARDY totals, mixed Excused combinations, one-session no-class days, incomplete-coverage resolution and the `Semester` field remain unanswered.
6. **Acceptance of custom legend (C19):** the selected local convention is not confirmed as acceptable for official school submission.
7. **Historical timetable and term end:** C17/E2 and C18/E26 remain unresolved. The reporting layer must not infer historical absences across a schedule change or work around term-boundary ambiguity.

## Batch sizing and next work

A safe development increment can cover roughly 3–5 tightly related modules plus tests, documentation and a static audit when they share one stable contract. v0.2.18 already batches the daily resolver, month-overlap roster, section aggregator and semantic row paginator.

The next safe batch, without teacher answers or a device, is:

1. Add a strict browser snapshot/schema validator around the proposed `DATA_CONTRACT.md`.
2. Create synthetic fixture snapshots for mid-month enrollment, transfers, `E`, Late, complete absence, incomplete coverage, calendar exceptions and invalid revisions.
3. Add a preview/diagnostic model that identifies learner/date/session blockers without producing an XLSX.
4. Audit all snapshot fields against actual event encodings and roster interfaces; keep missing data as blockers.

Do not combine that batch with C++ roster persistence or a production XLSX writer. C24 needs a separately reviewed storage/replay design; the template needs authoritative-version confirmation and layout normalization first. Once the teacher confirms the remaining rules and the template is normalized, the XLSX writer, visual fixtures and print validation can be one dedicated batch. API/dashboard integration should follow only after the snapshot contract and security model are stable.

## Explicitly not verified

- None of the 55 JavaScript tests was run.
- No Node execution, browser execution, `npm install`, compiler, PlatformIO run or ESP32 build was performed for this development snapshot.
- No workbook was modified or written.
- No generated XLSX/PDF was opened in Excel/LibreOffice or print-tested.
- No teacher/school/LIS acceptance of the custom legend or uploaded template is established.
