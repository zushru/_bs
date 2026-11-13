# Release v0.2.80 — mixed Present/Excused full day counts as Present

Date: 2026-11-11  
Baseline: v0.2.79  
Baseline archive SHA-256: `7be73490bb8ba8a5d247f76cddab01cbae5bab660d3d3ab73d04ff1738473535`  
Status: development snapshot; focused SF2 reference-model host tests only; no XLSX writer, firmware build, or hardware verification.

## User-confirmed SF2 rule

A full day with no confirmed absence maps as follows:

- Present + Present (including Late normalized to attended): `BLANK`, monthly total = Present.
- Excused + Excused: `E`, monthly total = Present.
- Present/Late + Excused, in either AM/PM order: `BLANK`, monthly total = Present.
- Any attended/Excused session paired with a confirmed absence: corresponding diagonal, monthly total = Absent.
- Confirmed Absent + Confirmed Absent: `X`, monthly total = Absent.

Late remains an internal attendance status. SF2 treats it as attended for cell selection and does not emit a separate Tardy mark or tally. The original internal `ST_LATE` record is unchanged.

## Code and document changes

- `web/sf2/attendance_model.mjs`: both sessions attended-or-excused resolve to `E` only when both are Excused; otherwise the result is `BLANK`. This covers Present+Excused and Late+Excused in either order.
- `web/sf2/test/attendance_model.test.mjs`: added both-order conversion checks, Late+Excused coverage, and monthly-total verification.
- `web/sf2/test/section_model.test.mjs`: added a section-level regression asserting Present+Excused contributes to Present, not Absent.
- `docs/sf2/REQUIREMENTS.md`, `TEACHER_QUESTIONS.md`, `UPLOAD_TEMPLATE_AUDIT_2026-11-10.md`, `FIXTURE_MATRIX_v0.2.md`, `NEXT_STEPS.md` and `README.md`: aligned active requirements, audit text, fixture expected totals and adviser questions. Historical release notes were not rewritten.

## Verification

Exact command:

```sh
cd web/sf2
node --test test/attendance_model.test.mjs test/section_model.test.mjs test/learner_layout.test.mjs test/snapshot_adapter.test.mjs test/preview_model.test.mjs
```

Result: **85/85 passed**, exit status **0**.

The test-first run against the unmodified conversion failed the new Present/Excused daily and monthly assertions as expected. After the implementation change, the final five-suite run passed all 85 registrations.

## Not run

- No full SF2 Node suite.
- No browser/dashboard integration.
- No XLSX writer, workbook expansion, formula, style, print/PDF, or Excel round-trip test.
- No official DepEd/LIS template acceptance check.
- No PlatformIO/native firmware tests, firmware compile/link, physical device, or hardware verification.

## Remaining limits

- This increment changes the reference-model projection only; it does not implement or enable authoritative XLSX export.
- O8 remains unchanged. Authoritative reporting export remains disabled pending the approved PII companion-recovery path or explicit O8 amendment.
- One-session non-class day handling, month-end registration counts, average daily attendance, attendance percentage, section-level five-consecutive-absence counts, NLS/movement records and official template acceptance remain open.
