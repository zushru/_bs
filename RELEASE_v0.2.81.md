# Release v0.2.81: section summary projections

Date: 2026-11-11  
Baseline: v0.2.80  
Baseline archive SHA-256: `0cbb7499bdca1db2fa73ae8500958159ff15c99b902df5b093eacbfcf142a7a0`  
Status: development snapshot; focused SF2 reference-model host tests only; no XLSX writer, firmware build, or hardware verification.

## Increment

### Month-end registered learners

`calculateSectionMonth()` now returns `summary.monthEndRegistration` with male/female/total counts. It counts a learner once when a validated half-open enrollment interval contains the report month's final calendar date. This is intentionally separate from the month-overlap learner roster, which still includes learners who transferred out before month-end. The adapter test verifies the aggregate survives snapshot-to-preview projection. Leap-year February 29 and the end-exclusive March 1 boundary are covered.

### Five-consecutive-absence learners

`calculateSectionMonth()` now returns `summary.fiveConsecutiveAbsenceLearners` with male/female/total distinct-learner counts. A learner qualifies when a run reaches at least five absent confirmed class dates within the report month. `X` and both diagonal outcomes extend the run; `BLANK` and `E` reset it. A confirmed class date outside the learner's enrollment intervals resets the run, so absences before transfer-out and after re-entry are not joined.

The definition remains provisional: the adviser must confirm class dates versus calendar days and whether a run beginning in the prior month carries into this month's count. The current preview is report-month scoped and does not include prior-month carry-in.

## Critical boundaries retained

- O8 is unchanged. No reporting PII persistence, production API, or authoritative export path was added.
- No XLSX workbook cells are written. The new counts are summary model fields only.
- Average daily attendance and attendance percentage remain unimplemented because their denominators and one-session-day treatment are not approved.
- One-session no-class days remain fail-closed.
- The supplied workbook remains a working reference, not a verified official template.
- The new absence count is not an adviser-approved official SF2 formula.

## Files changed

- `web/sf2/section_model.mjs`
- `web/sf2/test/section_model.test.mjs`
- `web/sf2/test/snapshot_adapter.test.mjs`
- `docs/sf2/REQUIREMENTS.md`
- `docs/sf2/UPLOAD_TEMPLATE_AUDIT_2026-11-10.md`
- `docs/sf2/README.md`
- `docs/sf2/NEXT_STEPS.md`
- `docs/sf2/TEACHER_QUESTIONS.md`
- `web/sf2/README.md`
- `RELEASE.md`
- `RELEASE_v0.2.81.md`

## Verification

Exact command:

```sh
cd web/sf2
node --test test/section_model.test.mjs test/snapshot_adapter.test.mjs test/preview_model.test.mjs
```

Result: **47/47 passed**, exit status **0**. The suite covers month-end interval boundaries, re-entry deduplication, leap-year handling, sex-grouped five-class-date counts, non-consecutive absence runs, enrollment-gap resets, and snapshot-adapter propagation. The test-first run failed the new assertions before implementation, including the false five-day run across a re-enrollment gap.

## Not run

- Complete SF2 Node suite or full project regression.
- Browser/dashboard integration or XLSX generation/round-trip/print validation.
- PlatformIO native tests, firmware build/link, ESP32 target verification, or hardware tests.
- Official workbook/adviser approval.

The working tree was extracted from the v0.2.80 archive, which has no Git metadata. Baseline integrity was checked against the archive's 399-entry manifest and the SHA-256 above; no `git status` or commit ancestry is claimed.
