# AS608 attendance event pipeline and admission policy

## Scope
`As608AttendancePipeline` turns a completed `As608AttendanceClient` scan into a durable `EV_SCAN` event. `LogAttendanceAdmission` is the concrete log-backed admission policy. These are main-loop components, not a firmware scheduler or HTTP adapter.

## Contracts
- One serialized application loop owns the sensor client, config updates, admission policy and log writes.
- `IAs608PageResolver` is mandatory. The pipeline does not infer student IDs from physical sensor pages. E18 remains DELEGATED and unresolved.
- `LogAttendanceAdmission` must attach to the same `LogStore` used by the pipeline. Attachment failure, log-open failure, storage fault, or day-view replay failure blocks scans.
- Admission rejects a no-class day, a no-class session, or an already-closed session. It checks the log-backed view before starting the scan and again immediately before writing.
- The admission observer applies new durable log events incrementally while its cached target day/session is valid. Log reset or config timezone change invalidates the view and forces a full replay before another scan is allowed.
- Duplicate EV_SCAN records for the same student and session are suppressed through `alreadyScanned()`. The first durable scan remains authoritative; a later scan cannot change Present to Late or create repeated log records.
- `canSetConfigAfterCommit()` is a side-effect-free preflight: pipeline and sensor must be idle, config must validate, and the admission observer must be attached/healthy and accept the new Params. `setConfigAfterCommit()` applies the Params to admission and then updates the pipeline config. `AttendanceConfigRuntime` calls this first, then `ClosureEngine::setSchedule()`, in one serialized operation. No scan or closure step may run between these updates. If a post-commit apply unexpectedly fails, persistent config may be newer than RAM; latch `H_CONFIG_FAULT` and stop normal processing until reboot reloads the A/B slots.
- RosterStore::historyComplete() must be true when a scan begins and again immediately before an EV_SCAN append. This rejects malformed roster history, interval overflow, and a parser still inside an RA_SET name-payload group; the latter is untrusted even when malformed() == 0.
- The roster must know the resolved ID, the student must be active, and RosterStore::expected(id, localDay) must be true.
- Scan time is captured at `begin()`. Classification and event epoch use that captured time, not the later UART completion time.
- The timetable window is `[start - scanLeadMin, end)`; the current default scan lead is 30 minutes, while the existing delegated configuration field remains adjustable (0..60). A scan in the lead window is Present unless it is strictly later than `start + lateMin`; exact threshold minute remains Present, matching `isLate()`.
- `minScore` is intentionally not applied. C4 has no measured threshold data; sensor match acceptance remains the sensor's current search behavior.
- `EV_SCAN.a` is the resolved student ID, flags contain session and Present/Late, `epoch` is the captured scan epoch, and `b` stores the sensor score.
- A `LogStore::append()` false result returns `ATT_PIPE_COMMIT_UNCERTAIN`, not `ATT_PIPE_NOT_SAVED`: storage may contain none, part, or all of the event. The current LogStore is faulted and scans are blocked. The caller must stop and reopen/replay before offering another action. If the complete EV_SCAN is present after replay, `LogAttendanceAdmission::alreadyScanned()` applies first-scan-wins and a repeated physical scan returns `ATT_PIPE_ALREADY_RECORDED` without another append. If the append left a partial tail or incomplete group, recovery depends on a safe repair-capable backend; current LittleFsFileSet refuses destructive repair (C13). `ATT_PIPE_NOT_SAVED` remains for failures before a log append is attempted, such as a fatal sensor result.

## Configuration transaction bridge (v0.2.12)
- `ConfigApplyCoordinator` owns the active A/B slot and generation. It overwrites caller-supplied `curGen`, runs `gateConfigPut`, checks runtime preflight, writes only the inactive slot, reads and decodes it back, verifies sequence/content, and only then invokes `AttendanceConfigRuntime`.
- A false backend write return is indeterminate; the coordinator latches and requires reboot/reload. Read-back mismatch and runtime apply failure also latch. The production caller must translate these states into `H_CONFIG_FAULT` and stop the loop.
- `IConfigSlotBackend` is implemented by `Esp32NvsConfigBackend` in v0.2.13. No exact target build or project-specific power-loss guarantee is included. Gate facts `windowOpen` and `unclosedPast` still come from the production caller and must be computed from trusted current state.

## Deliberate exclusions
- No page-to-ID convention is selected here.
- O6 remains DELEGATED. This policy only blocks no-class records already present for the scanned local day/session; it does not define how future-day no-class entries should be scheduled or applied.
- No score threshold is applied until C4 is resolved with device measurements.
- No display, buzzer, HTTP, scheduler, RTC trust, or production firmware integration is included.

## Authored regression coverage (not executed)
1. Empty, unopened and faulted log admission behavior.
2. Whole-day and session-only no-class behavior.
3. No-class clear behavior.
4. Closed-session rejection.
5. Incremental duplicate scan detection.
6. Config timezone update/replay behavior.
7. Storage failure and observer-capacity fail-closed behavior.
8. Config-parameter preflight is rejected after the log faults (added in v0.2.12).

## Required pipeline tests before release
1. Resolver maps page and rejects unmapped/out-of-range IDs.
2. Unknown, inactive and not-expected roster IDs do not append.
3. Session boundary and scan-lead boundary classification.
4. Exactly `start + lateMin` is Present; one minute later is Late.
5. Scan epoch remains the begin epoch if UART completes later.
6. Admission is checked both before scan and before append.
7. A false log append reports ATT_PIPE_COMMIT_UNCERTAIN; callers stop and reopen/replay before another logical action.
8. Config update is rejected during an active scan and on invalid config.
9. Duplicate records are suppressed on the first-scan-wins rule.
10. `minScore` does not silently filter matches.
11. E18 mapping is supplied by test resolver; no implicit `page+1` assumption.
