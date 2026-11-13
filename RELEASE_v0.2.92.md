# v0.2.92 — C3 attendance admission and owner-graph gate

**Status: development-only. C3 remains BLOCKING. This is not production firmware and the ESP32 adapter-link image must never be flashed.**

## Yield

The preferred AS608 attendance owner can no longer be constructed with an arbitrary admission policy. It now requires an ApplicationAttendanceGate bound to the validated config boot, exact storage/log/roster/admission/closure graph and live health. The owner rejects config/object mismatch; the config runtime rejects a closure engine that is not the exact one bound by that gate.

This closes specific composition and accidental bypasses in the preferred owner API. It does not create the missing production root, scheduler, clock caller, enrollment workflow, HTTP/API or hardware attendance path.

## Baseline and protected identity

- Root: /root/att.
- Baseline marker: v0.2.91 (2026-11-11).
- Baseline MANIFEST.sha256: 441 entries, verified before edits; no Git metadata.
- HANDOFF_FINAL.md SHA-256: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d.
- docs/DECISIONS.md SHA-256: 52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3.
- Neither protected file was edited. Decision statuses and product rules were not changed.

## Implementation

ApplicationAttendanceGate checks all of the following before preferred owner startup or attendance:
- ConfigBootCoordinator is READY.
- LittleFsStorageOwner is READY and exposes its validated IFileSet.
- MonthSet references that exact IFileSet; LogStore references that exact MonthSet and is not faulted.
- RosterStore history is complete and the exact roster observer is attached to the log.
- LogAttendanceAdmission is attached to and observes the exact same LogStore.
- ClosureEngine is ready, not pending, and references the exact LogStore and RosterStore.
- Sensor startup permits H_OK or sensor-only H_SENSOR_FAULT. This exception is only for bounded read-only sensor authentication/capacity/index startup.
- Attendance and commit require H_OK. The existing pipeline rechecks admission when a sensor match is ready to commit, so a clock/health fault while a scan is outstanding prevents EV_SCAN append.

The preferred As608AttendanceOwner constructor now requires ApplicationAttendanceGate rather than arbitrary IAttendanceAdmission. It verifies exact LogStore/RosterStore identity and that the supplied Config equals the boot-loaded config and matches the roster UTC offset, log-backed admission parameters and closure schedule. The owner therefore fails closed before sensor startup if the graph/config is inconsistent.

As608OwnerConfigRuntime records whether its ClosureEngine is the exact closure instance bound by the owner's gate. A mismatch rejects canApply before ConfigApplyCoordinator commits a slot; applyAfterCommit also refuses the mismatch.

ClosureEngine gained a config-matching helper for its supported timetable/params/termStart fields. It does not invent termEnd handling; C18 remains unresolved. ConfigBootCoordinator's boot config is the baseline for the owner construction check, not a substitute for production config-apply sequencing.

## Files changed

- lib/as608/src/as608_attendance_pipeline.h
- lib/core/src/closure_engine.cpp
- lib/core/src/closure_engine.h
- lib/core/src/log_store.cpp
- lib/core/src/log_store.h
- lib/core/src/month_set.h
- lib/hal_esp32/src/application_attendance_gate.cpp (new)
- lib/hal_esp32/src/application_attendance_gate.h (new)
- lib/hal_esp32/src/as608_attendance_owner.cpp
- lib/hal_esp32/src/as608_attendance_owner.h
- lib/hal_esp32/src/as608_owner_config_runtime.cpp
- lib/hal_esp32/src/as608_owner_config_runtime.h
- test/test_as608_pipeline/test_main.cpp
- target/esp32_adapter_link/main.cpp
- target/esp32_adapter_link/README.md
- Final source manifest: 445 entries, excluding the manifest itself, build output and caches. Archive: att_v0.2.92_2026-11-11-dev.tar.gz; checksum is supplied in the external .sha256 sidecar.

## Test-first and native verification

Test-first regressions reproduced two real gaps:
1. A config that differed from the boot-loaded config, roster offset, admission parameters and closure schedule was accepted for owner startup.
2. As608OwnerConfigRuntime accepted a different ClosureEngine and allowed the config write.

The final NEEDS_SETUP fixture was reviewed and corrected so its ConfigBootCoordinator stops before constructing LittleFS storage, MonthSet, log, roster, admission or closure runtime services. The gate denial assertion reuses a separately built ready graph only as a test dependency fixture.

Exact final command:

    pio test -e native -f 'test_as608_pipeline'

Result: **16/16 passed**, exit 0. Log: /tmp/att_c3_gate_green5.log.

Current inventory: **731 C++ RUN_TEST registrations across 39 native suites**. Only test_as608_pipeline (16 cases) was run in this increment; the other 715 registrations were not run. SF2 remains 270 Node registrations across 12 test files and was not run. Full native N1 remains deferred by the approved roadmap.

## ESP32 compile/link verification

Exact final command:

    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

Result: **SUCCESS**, exit 0. Final relink log: /tmp/att_c3_gate_esp32_relink2.log. Pinned environment: Espressif32 8.0.0, Arduino-ESP32 3.3.12, Xtensa 14.2.0+20260121 and esptool 5.4.0. The smoke target also uses Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4.

The first successful link did not retain every new method in the final ELF because plain discarded-value member-pointer reads were garbage-collected. The smoke source was corrected to use uncalled link-only shims with qualified calls to the concrete config-runtime methods, then rebuilt. The final ELF symbol inspection confirms these methods are retained:
- ApplicationAttendanceGate: readyForSensorStartup, readyForAttendance, matchesConfig, matchesClosure.
- As608AttendanceOwner: beginStartup, beginScan, stepScan, canSetConfigAfterCommit.
- As608OwnerConfigRuntime: canApply, applyAfterCommit.
- ClosureEngine: matchesConfig.
- Both concrete config-runtime link shims.

PlatformIO reports RAM 67,892 bytes (20.7%) and flash 1,010,340 bytes (77.1%). Final ELF SHA-256: b6a2007f9adaf6713032202d7b4e9cf91643a2a83a6fa6432284c08db3f67515. Final binary SHA-256: 7339751fc18d2625e535a235c6eca6e977a53c676baa94c0e58fbe913882aaae.

This verifies compile/link compatibility and retained symbols in the inert smoke image only. Its setup does not construct ApplicationAttendanceGate, As608AttendanceOwner or As608OwnerConfigRuntime and does not call any of their methods. No LittleFS mount, NVS operation, I2C/UART transaction, Wi-Fi/AP start or attendance operation occurs. **Never flash this image.**

## Critical review and residual blockers

- No production main.cpp, scheduler, root, or caller uses this gate/owner/config-runtime composition. No global singleton or RTOS synchronization was added. The intended serialized single-loop requirement remains on the future root.
- The gate relies on the production root to update HealthFsm from truthful RTC, storage, config and sensor observations. A host HealthFsm test does not prove those inputs are wired correctly.
- LittleFsStorageOwner READY means its existing startup validation passed; it does not prove target VFS durability, low-space behavior, descriptor recovery, commit atomicity or power-loss behavior. C13.3 remains blocking.
- The RTC caller and storage convention are not supplied by this gate. Preserve signed P10/PHT (UTC+8); the DS3231 RTC storage convention remains OPEN. Do not reinterpret UTC/local time.
- E18 page-to-student mapping and U5 physical index semantics remain unresolved. No production enrollment or attendance mapping is inferred from the fake UART tests.
- C17 schedule-effective history and C18 termEnd semantics remain unresolved. This increment adds no schedule-history persistence and no termEnd policy.
- The gate does not solve U61 roster lifecycle enforcement at every operational boundary or U112 RAM-only request tickets.
- C1/C34 remain blocking: the narrow NVS erase guard is not a complete erase firewall and this increment adds no production root ordering.
- C6 and C2 remain unchanged. No production HTTP/session/PIN KDF integration or backup/restore swap/recovery transaction is added.

## Not run / not proven

Full native N1, SF2 Node suite, other native suites, hardware-probe build, flash, mounted ESP32 LittleFS/VFS runtime, NVS runtime, RTC transaction, OLED/AP behavior, physical AS608 scan-to-event, reboot/replay on target, low-space, interruption matrix, brownout, power-cut, soak and physical device tests were not run. No destructive NVS, sensor-template or filesystem operation was performed.

## Disposition

This increment hardens the preferred owner boundary and verifies the affected host suite plus pinned ESP32 compile/link compatibility. **C3 remains BLOCKING** until one production root composes the authoritative config boot, storage owner, one AS608 runtime/owner, real clock caller, attendance pipeline, closure and truthful health flow in one serialized scheduler. C13.3, C1/C34 and C6 remain separate blockers. No product decision was changed or inferred.
