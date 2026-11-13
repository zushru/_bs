# Uploaded SHSF-2 workbook audit

Audit target (preserved byte-for-byte at `docs/sf2/reference/UPLOADED_SHSF2_SOURCE_NOT_FINAL.xlsx`):

`School Form 2 Daily Attendance Report of Learners  for Senior High School (SF2-SHS).xlsx`

SHA-256: `6894d7673fee924c88f09478d6473ec71194e3839c595dc0d97d29835f51fac5`

This is an OOXML structural audit of the uploaded workbook, not certification that it is the latest DepEd/LIS-approved file.

## Workbook structure

| Property | Observed value |
|---|---|
| Worksheets | 1: `SHSF-2` |
| Worksheet dimension | `A1:AZ46` |
| Print area | `A1:AY46` (AZ is outside print area) |
| Print titles | Rows 9:11 |
| Visible learner placeholders | 4: male rows 12-13; female rows 15-16 |
| Subtotal rows | Male 14; female 17; combined 18 |
| Date slots | **30**, five blocks of Monday-Saturday; not 31 |
| Attendance marks totals | `ABSENT` and `TARDY` columns are present |
| Merged ranges | 184 |
| Defined names | 25 total; 23 contain `#REF!`; print-area and print-title names are valid |
| Formula cells | 0 |
| External workbook links | 1 external-link part referencing 17 sheet names |
| Drawing anchors / media | 2 drawing anchors; 2 media files (JPEG and PNG) |
| Hidden rows/columns | None declared hidden |
| Manual row/column page breaks | None |
| Conditional formatting / data validation | None |
| Print orientation | Landscape |
| Paper size code | `258` (custom/other; not the standard A4 code) |
| Print scaling | Fixed 85%; `fitToWidth=0`, `fitToHeight=0` |
| Print resolution metadata | 300 DPI horizontal/vertical |
| Margins | left 0.3543 in, right 0.1575 in, top/bottom 0.2756 in; header/footer 0 |
| Core metadata timestamps | Created 2018-08-11 13:22:00Z; modified 2018-08-11 13:22:27Z |

The 30 date-header cells are `F11,H11,I11,J11,K11,L11,M11,O11,P11,Q11,R11,S11,T11,V11,W11,X11,Z11,AB11,AC11,AE11,AF11,AG11,AH11,AI11,AJ11,AK11,AM11,AN11,AO11,AQ11`. They form five six-day blocks (M/T/W/TH/F/S). The signed attendance product schedules Monday-Friday; Saturday columns must remain unused unless the product rules change.

## Structural defects / risks

1. **Only four learner placeholders.** This cannot represent the device's current 45-active-learner capacity, much less a reporting month with turnover where distinct learners may exceed 45.
2. **23 broken defined names.** They must be reconciled or removed with a precise impact analysis before release.
3. **External workbook link remains embedded.** It references 17 original workbook sheet names. The production workbook must not depend on that external source.
4. **No formulas.** All visible `18` values are literal placeholders. Summary calculations must be implemented in the browser and/or written as verified values/formulas by the template adapter.
5. **Fixed 85% custom-paper print setup.** Paper size, fit, and printer behavior require review. The current setting does not prove an A4-printable layout.
6. **184 merged ranges and drawing anchors.** Inserting learner rows requires updating merges, drawing anchors, print area, repeated titles, summary placement and print pages; simple cell writes or row copies are unsafe.
7. **Date capacity is 30 slots, not 31.** The five Mon-Sat blocks do not align exactly with the device's Mon-Fri timetable. The weekday/day-number mapping must be generated from the selected report month, not copied from fixed header positions.
8. **NLS and learner movement fields are materially richer than the current roster model.** The current roster does not hold all reasons/destination/track-shift data needed to complete remarks and NLS summaries.
9. **Workbook age / official version unknown.** Embedded metadata is from 2018. This audit did not establish that it is the current official submission version.

## Recommended dynamic layout strategy

- Learner row count is based on the report-month roster: every learner whose section enrollment overlaps at least one day in the selected month, including those who exit before export day.
- The month roster must be backed by enrollment history, not only today's active roster.
- Do not cap the report at 45 rows. 45 is a current active-roster limit, not a guaranteed upper bound on unique learners during one month when students transfer or are replaced.
- Split male/female rows only after validating that every learner has an approved sex value for reporting; never guess a missing value.
- Use repeatable learner-row blocks and continuation pages. Recalculate every summary and page count from the complete month roster.
- Preserve a visible page identifier, report month, section, and continuation sequence on every page. Signature/certification areas must appear in the correct place on the final page and/or as required by the approved form.
- Never silently truncate learners or dates. If workbook layout/print rendering cannot fit, block export and report the actual required page count.

## Template approval gate

Before using this XLSX as the production template: compare it with the exact current file downloaded/approved by the school/LIS, remove unsafe external links and invalid names, resolve A4/print settings, define dynamic row blocks, produce realistic sample workbooks, open/round-trip them in actual spreadsheet software, and inspect printed/PDF output at all target learner counts (0, 1, 2, 4, 45 and more than 45 month-overlap learners). This workbook remains reference-only until those checks are completed.

## Official LIS acquisition check (2026-11-07)

The DepEd LIS Support page currently lists **Senior High School Forms (SHSF-1 to SHSF-7, SF8-SHS)** in its Downloads section and states that official School Forms are exclusively available to schools through LIS/EBEIS; a school LIS/EBEIS account is required. The page also warns that unauthorized electronic forms may be considered invalid for official transactions.

Official source: https://support.lis.deped.gov.ph/support/ (Downloads section; accessed 2026-11-07). The page's linked XLSX endpoint was identified as `Senior High School Forms.xlsx`, but this work environment could not retrieve the XLSX payload (the browser tool rejected its spreadsheet MIME type, and the container download failed). Therefore **no current official workbook was inspected or compared** in this release.

Consequences:
- The supplied 2018-era workbook remains reference-only.
- The official workbook must be downloaded by an authorized school/LIS account holder and supplied as a file before element-level comparison.
- Do not infer that the supplied workbook is the current official template, or that an independently generated workbook is LIS-authorized merely because it resembles the form.
- Both Regular SHS and Strengthened SHS are in the product's signed scope, but the exact SF2-SHS workbook for each applicable program must be confirmed against the school's LIS output.
