# Release v0.2.79 — SF2 diagonal marks count as absent

Date: 2026-11-11  
Baseline: v0.2.78  
Baseline archive SHA-256: `3a0fab89dd933900dcde096e68e6f617f23c54a03d03271eeee7c635a81b75b4`  
Status: development snapshot; reference-model host tests only; no XLSX writer, firmware build, or hardware verification.

## User-confirmed SF2 rules

- `BLANK`: Present in both AM and PM; monthly total = Present.
- `X`: Absent in both AM and PM; monthly total = Absent.
- `E`: Excused in both AM and PM; monthly total = Present.
- Upper-left shaded: AM is Present/Excused and PM is Confirmed Absent; monthly total = Absent.
- Lower-right shaded: AM is Confirmed Absent and PM is Present/Excused; monthly total = Absent.
- Late remains an internal attendance status. It is treated as attended for selecting the daily mark, but SF2 emits no separate Tardy mark or tally.
- One semantic learner row is generated per learner in the reporting-month roster. The current workbook's four placeholder rows are not a data limit.

A full day split between Present/Late and Excused has no mark in the supplied legend. The model fails closed and blocks this case pending an adviser-approved rule; it does not guess `BLANK` or `E`.

## Code changes

- `web/sf2/attendance_model.mjs`: diagonal marks now set `countsAsAbsentDay`; only `BLANK` and `E` set `countsAsPresentDay`.
- An Excused+Confirmed-Absent pair now produces the corresponding diagonal, not `E`; `E` is reserved for both sessions Excused.
- Consecutive-absence streaks now include `AM_ONLY` and `PM_ONLY` in addition to `X`.
- `web/sf2/section_model.mjs` already aggregates using the learner-level present/absent flags; no duplicated section logic was added.
- `learner_layout.mjs` already derives row count from roster size. A 60-learner regression confirms the semantic plan has no four-row cap. No workbook row insertion implementation was added.
- Requirements, template audit, adviser questions and next-step docs were aligned to the new semantics.

## Verification

Exact command:

```sh
cd web/sf2
node --test test/attendance_model.test.mjs test/section_model.test.mjs test/learner_layout.test.mjs test/snapshot_adapter.test.mjs test/preview_model.test.mjs
```

Result: **83/83 passed**, exit status **0**.

The test-first run against the unmodified implementation exposed six failures in the changed attendance semantics. The first dependent run also exposed two stale snapshot-adapter expectations; those fixtures/assertions were corrected. The final five-suite run passed all 83 registrations.

## Not run

- No full SF2 Node suite.
- No browser/dashboard integration.
- No XLSX writer, workbook expansion, formula, styles, print/PDF, or Excel round-trip test.
- No official DepEd/LIS template acceptance check.
- No PlatformIO/native firmware tests, firmware compile/link, physical device, or hardware verification.
- O8 remains unchanged. Authoritative reporting export remains disabled pending the approved PII companion-recovery path or explicit O8 amendment.

## Remaining limits

- A real XLSX writer must expand the male/female learner blocks to exactly match the semantic row plan and move subtotal/combined-total rows safely.
- Monthly registered-as-of-month-end counts remain distinct from month-overlap roster rows.
- Average daily attendance, attendance percentage and aggregate count of learners absent five consecutive class days still need defined formulas/data.
- One-session non-class day handling, NLS/movement records, mixed Present+Excused full-day mapping, official template acceptance and O8 remain open.
