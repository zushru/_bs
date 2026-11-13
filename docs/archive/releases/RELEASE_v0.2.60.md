# Release v0.2.60 — explicit uncertain attendance commit result

Date: 2026-11-09  
Base: complete v0.2.59 archive extraction  
Base archive SHA-256: `4e5d4970d1c430f536f053894dcc8633c24b2b9f5c94b69c4e10cdd15f5704d1`  
Project root during development: `/root/att`  
Release status: development snapshot; C3 and C13 remain blocking

## Scope

1. Added `ATT_PIPE_COMMIT_UNCERTAIN` to `AttendancePipelineResult`. The new value is appended after existing enumerators so previous numeric result values are preserved.
2. `As608AttendancePipeline::saveMatch_()` now returns `ATT_PIPE_COMMIT_UNCERTAIN` when `LogStore::append()` returns false. This is required because the storage interface cannot prove that false means no bytes or records were written.
3. `ATT_PIPE_NOT_SAVED` remains for the sensor-fatal path where no log append was attempted. A caller must not interpret `ATT_PIPE_COMMIT_UNCERTAIN` as definitely absent; the current LogStore is faulted and the caller must stop writes and reopen/replay before offering another action.
4. Added a fake-storage regression that writes the complete EV_SCAN record but returns false. A fresh LogStore reopens and replays the record; rebuilt LogAttendanceAdmission recognizes the same student/session as already scanned; the next physical scan returns `ATT_PIPE_ALREADY_RECORDED` without another append.
5. The storage format, EV_SCAN record encoding, event sequence semantics, first-scan-wins rule, signed rules and delegated decisions are unchanged.

## Verification

Test-first command: `pio test -e native -f 'test_as608_pipeline'`  
Initial result after adding the new result expectation but before mapping append failure: both append-failure assertions failed (expected `ATT_PIPE_COMMIT_UNCERTAIN` value 13, observed `ATT_PIPE_NOT_SAVED` value 11). This reproduced the misleading result contract.

Final command:

```sh
pio test -e native -f 'test_as608_pipeline' -f 'test_attendance_admission'
```

Result: **24/24 passed**, exit 0; `test_as608_pipeline` 12/12 and `test_attendance_admission` 12/12; 61.349 seconds aggregate suite time.

Source inventory: **585 C++ registrations across 35 suites**; **561 were not run** in this increment. This is targeted host-test evidence only.

## Remaining limits

- This verifies first-scan-wins recovery only when the complete EV_SCAN record is present after reopen. It does not provide generic or request-level exactly-once semantics.
- A partial append or incomplete commit group may prevent reopen. The current `LittleFsFileSet` adapter refuses truncate/quarantine, so safe repair remains blocked (C13).
- Clock changes, roster writes, GAP writes and future API writes still need durable request identity/reconciliation and separate retry tests (U112).
- No production firmware root, scheduler, HTTP route or UI exists (C3). No ESP32 target build, ELF inspection, physical device, flash, power-cut/brownout test, full native suite or SF2 suite was run.

## Changed files

- `lib/as608/src/as608_attendance_pipeline.h`
- `lib/as608/src/as608_attendance_pipeline.cpp`
- `test/test_as608_pipeline/test_main.cpp`
- `README.md`, `RELEASE.md`, `RELEASE_v0.2.60.md`
- `docs/AUDIT_v0.2.60.md`, `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/SPECS/as608_attendance_pipeline.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`
- `tools/verify_v0.2.60.sh`
- `MANIFEST.sha256`

`HANDOFF_FINAL.md` and `docs/DECISIONS.md` were not changed. No signed or delegated decision was altered. No firmware was flashed.
