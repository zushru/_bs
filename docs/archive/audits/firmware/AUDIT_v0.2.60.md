# Audit v0.2.60 — uncertain attendance append outcome

Date: 2026-11-09  
Base: v0.2.59 archive SHA-256 `4e5d4970d1c430f536f053894dcc8633c24b2b9f5c94b69c4e10cdd15f5704d1`  
Scope: attendance pipeline result contract and complete-write/reopen retry behavior  
Result: the attendance-specific result contract is corrected; generic U112 remains unresolved

## Finding

`LogStore::append()` returns false when the backend does not establish its success condition. That false result does not prove storage is unchanged: a backend may have written no bytes, a partial record, or the complete EV_SCAN record before a later failure. The attendance pipeline previously mapped every false append result to `ATT_PIPE_NOT_SAVED`, a stronger claim than the storage interface supports. A UI/caller could therefore incorrectly tell an operator that the mark was definitely absent and invite a duplicate retry.

## Change

- Added `ATT_PIPE_COMMIT_UNCERTAIN` at the end of `AttendancePipelineResult`, preserving the numeric values of existing enum members.
- Changed only the `LogStore::append()` failure mapping in `As608AttendancePipeline::saveMatch_()` to return the new result.
- Kept `ATT_PIPE_NOT_SAVED` for `AS608_ATT_FATAL` when no log append was attempted. `ATT_PIPE_SENSOR_FAULT` remains the result when the sensor has latched its own fault.
- Documented that the caller must stop and reconstruct/replay the log before offering another logical action. The current LogStore has already latched `faulted()` and refuses further writes in that instance.

## Regression and test-first evidence

The fake storage in `test_as608_pipeline` gained a one-shot mode that copies the entire append buffer into its backing bytes and then returns false.

The integration regression verifies:

1. Initial fixture has one student roster group and no attendance scan.
2. The sensor returns a valid match; fake storage writes the complete EV_SCAN but reports failure.
3. The pipeline returns `ATT_PIPE_COMMIT_UNCERTAIN`; the original LogStore faults and keeps its prior in-memory record count.
4. A new LogStore over the same bytes opens successfully with the EV_SCAN included.
5. A fresh RosterStore and LogAttendanceAdmission replay the recovered log and identify the student as already scanned for that day/session.
6. A newly started sensor runtime completes the same logical student scan; the pipeline returns `ATT_PIPE_ALREADY_RECORDED`, does not append another record and leaves the recovered LogStore healthy.

Test-first result: after the test expectation was changed to the new enumerator but before the implementation mapping changed, both append-failure assertions failed: expected enum value 13, observed 11. After the narrow mapping change, the final test suite passed.

## Verification

Exact final command:

```sh
pio test -e native -f 'test_as608_pipeline' -f 'test_attendance_admission'
```

Result: **24/24 passed**, exit 0; `test_as608_pipeline` 12/12 and `test_attendance_admission` 12/12; 61.349 seconds aggregate suite time. These suites exercise the real core/runtime pipeline and log-backed admission policy with synthetic UART and storage.

The current source inventory is **585 C++ registrations across 35 suites**; **561 were not run** in this increment. No full native regression is claimed.

## Critical limits

- The test simulates a complete write followed by false; it does not prove any actual LittleFS/VFS durability or close/sync result on ESP32.
- A partial write or incomplete group may prevent LogStore reopen. `LittleFsFileSet` still refuses truncate/quarantine, so C13 remains a recovery blocker.
- Only attendance EV_SCAN first-scan-wins is covered. Clock-set, roster and GAP writes still lack durable idempotency keys/reconciliation. U112 remains unresolved generally.
- The application has no production HTTP routes, UI or scheduler (C3). This enum is not yet translated into an operator-facing workflow.
- No target build, ELF inspection, physical hardware test, flash, power-cut, brownout, full native suite or SF2 suite was run.
- No signed/delegated decision, `HANDOFF_FINAL.md` or `docs/DECISIONS.md` changed.
