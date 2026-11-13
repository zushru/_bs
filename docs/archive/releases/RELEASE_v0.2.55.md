# Release v0.2.55 — owner-bound configuration application

Date: 2026-11-09  
Base: complete v0.2.54 archive extraction  
Base archive SHA-256: `dafa973da0cfe04746488f923c79a939b38128ba072e8ad2d94f60c62cb8a45c`  
Project root during development: /root/att  
Release status: development snapshot; C3 remains blocking

## Scope

This increment connects the preferred attendance-only owner to the existing durable A/B configuration transaction:

1. Added `As608OwnerConfigRuntime`, an `IConfigApplyRuntime` adapter that uses the public config-after-commit boundary of `As608AttendanceOwner` and updates the external `ClosureEngine`.
2. Preflight delegates to `As608AttendanceOwner::canSetConfigAfterCommit()`. Configuration changes are refused until AS608 startup is READY, while a scan is active, while the attendance sensor client is active, or when the new Config/admission parameters are invalid.
3. Post-commit application updates the owner pipeline and admission policy first, then calls `ClosureEngine::setSchedule()`. It is intended to be called only by `ConfigApplyCoordinator`, after the inactive slot is durably written and read back successfully.
4. Added an integration regression through the real `ConfigApplyCoordinator` with an in-memory A/B backend and synthetic AS608 UART. It verifies zero writes before startup READY, during a scan, and for a live UTC-offset change; a successful generation-8 timetable commit after the scan ends; synchronized attendance/closure timetable changes; and no redundant write for an unchanged Config.
5. Added a fail-closed offset invariant to `As608AttendancePipeline`: the configured UTC offset must equal `RosterStore::utcOffsetMin()`. `RosterStore` converts roster event epochs to effective local days using its construction-time offset and has no safe live replay/rebuild operation. A timezone change is therefore refused by runtime preflight before the A/B slot is written.

This does not add production firmware, a scheduler, HTTP routes, admin UI, enrollment or a real NVS/storage implementation. The caller must construct the owner and ClosureEngine against the same log and boot Config, keep all references alive, and serialize all operations on one loop.

## Implementation

### New adapter and offset guard

- `lib/hal_esp32/src/as608_owner_config_runtime.h/.cpp`
- `lib/as608/src/as608_attendance_pipeline.cpp` — config preflight refuses UTC-offset changes until roster intervals can be safely replayed and replaced under a new offset.
  - `As608OwnerConfigRuntime` implements `IConfigApplyRuntime`.
  - `canApply(next)` delegates to the owner's readiness/scan/config preflight.
  - `applyAfterCommit(next)` calls the owner's config-after-commit update and then updates the ClosureEngine schedule.
  - The adapter holds non-owning references to both objects and adds no task synchronization.

### Regression

- `test/test_as608_pipeline/test_main.cpp`
  - New test drives ConfigApplyCoordinator, not just the adapter directly.
  - A config change is rejected as busy while startup is not READY; the fake backend records zero writes.
  - The same change is rejected while a scan lease is active; the fake backend still records zero writes.
  - After a synthetic no-finger terminal result releases the scan, the coordinator writes/verifies the inactive slot and applies generation 8.
  - The updated disabled timetable is observed by both ClosureEngine and owner scan admission.
  - A valid UTC-offset change is rejected as busy before any A/B backend write because RosterStore's effective-day offset is immutable after construction.
  - Reapplying the same Config at generation 8 returns unchanged and does not write again.

## Verification evidence

Exact final command:

```sh
pio test -e native -f 'test_as608_pipeline'
```

Result: exit 0; **11/11 passed**; final suite duration **39.382 s** (remote command runtime 47.96 s). The final test includes 11 registrations. The static inventory is **552 C++ registrations across 34 suites**; the other **541 were not run**.

Test-first notes: before the adapter existed, the new test failed to compile on the missing header, as expected. The first full-coordinator run exposed a stale expected-generation value in the unchanged-config fixture; that helper was corrected to send generation 8. The subsequent timezone regression then failed because a valid UTC-offset change was accepted and persisted. The pipeline preflight was tightened to require the configured offset to match the immutable roster offset. The final focused run passed all 11 cases.

The v0.2.54 gate suite's 34/34 result is historical and was not rerun. No full native suite, SF2 Node suite, ESP32 target compile/link, ELF inspection, flash, physical sensor test, NVS operation, power-cut test or device verification was performed. The new adapter has host compile/link coverage only.

## Critical limitations

- C3 remains blocking: there is no production firmware root/scheduler, HTTP/API route, admin UI or enrollment service.
- The owner and adapter do not enforce one global sensor owner, a mutex, or task serialization. Production must use exactly one owner and serialize config/closure/attendance work.
- The adapter cannot prove the supplied ClosureEngine shares the owner's LogStore, RosterStore or boot Config. The caller must ensure those identities and lifetimes.
- This adapter does not derive trusted `ConfigGateIn` facts such as `windowOpen` or `unclosedPast`; the request/config application layer must supply them accurately. C17 still prevents safely deriving historical unclosed-session state from schedule-effective history. The test's false/false gate values are synthetic fixtures, not proof this policy is wired in production.
- Runtime UTC-offset changes are deliberately refused because the live `RosterStore` still uses its boot-time offset. A future supported offset change needs a fully validated replay/rebuild of roster intervals from the durable log before any scan or closure work resumes.
- If owner runtime application fails after storage commit/read-back, ConfigApplyCoordinator latches a runtime fault because persistent state is newer than RAM. The application must stop normal processing and reboot/reload; no rollback protocol is added.
- No final-source ESP32 build or physical hardware behavior is verified.
- E18 remains DELEGATED; no student-ID/page mapping or enrollment workflow was changed.
- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged.
