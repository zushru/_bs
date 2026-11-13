# Release v0.2.78 — 30-minute scan lead and Present-only SF2 projection

Date: 2026-11-10  
Baseline: v0.2.77  
Baseline archive SHA-256: `19fb1ad5b84b7f0e5fbf41a3eda61f3acc5ac0e469513eb04a5ebc271a03d3c5`  
Status: development snapshot; not production firmware; not firmware-test-verified; not hardware-verified.

## Scope

- Change the scan-window default from 15 to 30 minutes before the scheduled session.
- Treat stored schema-v1 `scanLeadMin == 15` as the former default and normalize it to 30 at decode; preserve other configured values under the existing delegated configuration design.
- Keep internal `ST_LATE` event/status behavior. In the SF2 reference projection only, map `LATE` to `PRESENT` and omit explicit late/tardy markers and tallies.
- Pin and audit the user-uploaded SHSF-2 working template. No XLSX writer or authoritative export is enabled.

## Code changes

- `lib/core/src/config_store.cpp`: default scan lead is 30; legacy stored value 15 normalizes to 30; same-generation A/B slots must match their encoded bytes as well as the normalized config so migration cannot hide differing source bytes.
- `web/sf2/attendance_model.mjs`: source `LATE` maps to `PRESENT` in daily report fields; late-session and monthly tardy outputs are removed.
- `web/sf2/section_model.mjs`: section aggregation no longer emits late/tardy counters or placeholders.
- Relevant C++ and Node test sources were updated/extended. C++ changes were not executable in this environment because PlatformIO is unavailable.

## Configuration migration risk

Schema v1 does not record whether a stored value of 15 was a default or an explicit user choice. This implementation treats every stored 15 as the former default and changes it to 30. Other non-15 values remain configurable. The decoded config is revalidated after normalization. A previously valid custom timetable with less than a 30-minute inter-session gap, or a session starting before minute 30, can fail validation and prevent normal config startup. Such schedules must be reviewed before upgrading; this is not proven safe for every existing device configuration.

## Uploaded template

The workbook is pinned at `docs/sf2/reference/UPLOADED_SHSF2_WORKING_TEMPLATE_2026-11-10.xlsx`. Its 26 marked `In` cells and the structural/calculation blockers are documented in `docs/sf2/UPLOAD_TEMPLATE_AUDIT_2026-11-10.md`.

Critical limitations:

- Four learner rows do not satisfy the 45-learner product limit.
- Weekday columns require actual calendar alignment.
- Month-overlap roster counts cannot be reused as month-end registered-learner counts.
- Average daily attendance, attendance percentage, and five-consecutive-absence section counts are not fully implemented/defined.
- The static Tardy legend conflicts with the Present-only SF2 rule.
- The NLS/movement area remains blank under the current “only marked fields plus attendance” input rule unless separately specified.
- O8 remains a hard gate: authoritative export with identity/reporting PII is disabled until an explicitly approved O8 amendment or companion backup/recovery workflow.
- No XLSX writer or production SF2 endpoint exists; `finalExportReady` remains false.

## Verification

### SF2 reference-model tests

Exact command:

```sh
cd /mnt/data/att_v0.2.77_inspect/web/sf2 && node --test test/attendance_model.test.mjs test/section_model.test.mjs test/snapshot_adapter.test.mjs
```

Result: **57/57 passed**, exit status **0**.

These are the three directly affected Node suites only. They verify the SF2 projection and totals shape; they do not verify an XLSX writer, browser integration, the ESP32 snapshot API, or official DepEd form correctness.

### Firmware test attempt

Exact command:

```sh
cd /mnt/data/att_v0.2.77_inspect && pio test -e native -f test_config -f test_core -f test_api -f test_attendance_admission
```

Result: **not run**; shell returned `pio: command not found`, exit status **127**. No C++/Unity assertion was executed.

### Not run

- No full native regression.
- No ESP32 compile/link.
- No firmware test pass claim.
- No browser UI / dashboard / HTTP integration test.
- No XLSX writer test or round-trip/open-in-Excel verification.
- No physical hardware, scan timing, power, or power-cut verification.
- No flash, NVS erase, partition change, filesystem format, or physical-device operation.

## Source-of-truth and evidence

The supplied v0.2.77 archive's `MANIFEST.sha256` was verified before editing (all entries passed). It contains no `.git` metadata, so no Git working-tree status is available. The uploaded workbook SHA-256 is recorded in the template audit. The bundled source files and manifest are regenerated for this snapshot.

This is a development increment only. It must not be described as a fully tested firmware release or as an operational SF2 export.
