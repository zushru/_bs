# Specialized audit — v0.2.55 owner-bound config application

## Scope

- Whether the preferred `As608AttendanceOwner` can be used as the live attendance runtime behind `ConfigApplyCoordinator`.
- Whether config writes are refused before sensor startup and during an active scan.
- Whether post-commit updates reach the attendance pipeline/admission policy and `ClosureEngine`.
- Whether durable commit/read-back ordering remains controlled by the existing coordinator.
- Residual identity, lifetime, serialization, target-build and production-integration risks.

## Findings

### 1. The config adapter closes one composition gap

Added `As608OwnerConfigRuntime` in `lib/hal_esp32/src`. It implements `IConfigApplyRuntime` without exposing the owner's private pipeline or raw sensor client.

- `canApply()` delegates to `owner.canSetConfigAfterCommit()`.
- The owner requires startup READY and delegates pipeline validation, scan-idle checks, sensor-idle checks and admission parameter preflight.
- `applyAfterCommit()` calls `owner.setConfigAfterCommit()`; that updates the pipeline Config and admission Params. Only after that succeeds does the adapter call `ClosureEngine::setSchedule()`.
- This preserves the existing config transaction boundary: ConfigApplyCoordinator writes the inactive slot, reads it back and decodes/verifies it before invoking `applyAfterCommit()`.

### 2. The new integration test exercises the coordinator path

The added test in `test/test_as608_pipeline/test_main.cpp` uses:
- synthetic Stream ACKs and the existing attendance-only owner;
- an in-memory `IConfigSlotBackend`;
- a real `ConfigApplyCoordinator`;
- a real `ClosureEngine` attached to the same test LogStore.

Observed test behavior:
- Before owner READY, config apply returns `CFG_APPLY_BUSY`; no slot write occurs.
- During an active scan, config apply returns `CFG_APPLY_BUSY`; no slot write occurs.
- After the scan ends, config apply commits and verifies generation 8, then updates the owner pipeline/admission and ClosureEngine schedule.
- The new disabled timetable blocks subsequent attendance scans and closure ticks.
- Reapplying the same Config with the new generation returns `CFG_APPLY_UNCHANGED` and does not write another slot.
- A valid UTC-offset change from the roster's construction offset to UTC+0 is rejected as `CFG_APPLY_BUSY` before storage writes. This test was added after source inspection found that `RosterStore::onRecord()` converts event epochs with an immutable `off_`, while the pipeline's day calculations use the mutable Config offset. Without a safe full roster replay/rebuild, accepting an offset change could move the scan day without moving roster interval days.

Focused result: `pio test -e native -f 'test_as608_pipeline'` — **11/11 passed**, exit 0, 39.382 s. This is host-only evidence; UART and A/B storage are synthetic.

### 3. Ordering is sound only under the stated serialized-loop contract

The adapter does not perform storage writes itself. ConfigApplyCoordinator owns durable write, read-back verification, generation and active-slot selection. The adapter then updates pipeline/admission before ClosureEngine. No work may interleave between these updates and the next attendance/closure operation.

A race between `canApply()` and `applyAfterCommit()` is not prevented by the adapter. If another task starts a scan after preflight but before the persisted change is applied, `owner.setConfigAfterCommit()` can fail after the new Config is already durable. ConfigApplyCoordinator then latches `CFG_APPLY_RUNTIME_FAILED`; the caller must stop processing and reboot/reload. This is why the entire transaction must execute on the same serialized application loop. No mutex or rollback is added here.

### 4. Timezone changes are explicitly refused until roster replay exists

`RosterStore::onRecord()` converts each roster event's UTC epoch into a local effective day using its construction-time `off_`. The offset is immutable after construction. In contrast, the attendance pipeline calculates scan days and the ClosureEngine schedule from the current Config. A live Config UTC-offset change would therefore cause these components to disagree about effective-day boundaries.

The new `As608AttendancePipeline::canSetConfigAfterCommit()` guard requires `config.params.utcOffsetMin == roster_.utcOffsetMin()`. Because ConfigApplyCoordinator invokes runtime preflight before writing the inactive slot, the mismatch returns `CFG_APPLY_BUSY` with no durable write. `setConfigAfterCommit()` repeats the check. This guard also applies to the legacy pipeline-based config runtime because it delegates to this pipeline.

This is a deliberate temporary restriction, not a timezone migration implementation. To support a changed offset, the project needs a validated scratch replay of the durable roster history using the proposed offset, safe state replacement, and coordinated update of admission/closure state before scans resume. No such transaction is implemented here.

### 5. Dependency identity and lifetime are caller responsibilities

The adapter stores references to `As608AttendanceOwner` and `ClosureEngine`; neither is owned. The caller must keep them alive, construct both against the same selected boot Config, and ensure ClosureEngine observes the same LogStore/roster used by the owner. The type system cannot prove those references describe the same application state. A mismatched ClosureEngine could apply the new timetable to the wrong log or roster context.

### 6. C3 remains blocking

This adapter does not create the missing production composition:
- no production `main.cpp` or application root instantiates the owner, adapter, config coordinator and scheduler together;
- no HTTP/API route or admin UI applies configuration;
- duplicate owners, standalone low-level clients/transports and the legacy injected-map Runtime constructor remain possible;
- no mutex or global singleton enforcement exists;
- enrollment is still not exposed;
- E18/U5 remain unresolved.

Do not describe this as production-integrated.

### 7. Verification limits and decision integrity

- Static source inventory: **552 C++ registrations across 34 suites**; **541 not run** in this increment.
- The v0.2.54 gate suite result (34/34) is historical; it was not rerun.
- No full native suite, SF2 Node suite, exact final-source ESP32 compile/link, ELF inspection, flash, NVS operation, physical sensor test, power-cut test or hardware verification.
- The target smoke application does not instantiate this adapter; native host compilation is not ESP32 target evidence.
- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged. E18 remains DELEGATED.
- C3 remains blocking. This is a narrow config-runtime adapter, not a release-ready product composition.
