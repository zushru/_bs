# Release v0.2.52 — AS608 startup and occupancy gates

**Date:** 2026-11-09  
**Base:** complete v0.2.51 tree  
**Status:** 39 focused native tests passed. An ESP32 smoke build and ELF inspection passed for the startup/HAL changes, but were completed before the final platform-independent occupancy-pipeline guard change. No flash or physical sensor test.

## Changes

- Added `As608StartupCoordinator` to sequence enrollment-owner authentication, capacity read, complete occupancy-index refresh, release of enrollment ownership, attendance-owner authentication and matching-capacity validation.
- Startup invalidates stale occupancy before beginning. It reports READY only after the complete index refresh and both authenticated capacity checks succeed.
- Failure paths invalidate occupancy. A poisoned transport retains sensor ownership; non-poison failures release setup ownership only when pending sensor work is drained.
- Added `As608AttendanceClient::invalidateReady()` so a post-setup capacity disagreement revokes attendance readiness.
- Removed a duplicate-occupancy-reference hazard: `As608Runtime` retains the exact `Occupancy&` used by its FSM and reader, and startup obtains that same reference.
- Added five startup gate cases, covering complete refresh, stale occupancy invalidation, partial index failure, capacity disagreement, poisoned transport and enrollment authentication rejection.
- Added explicit non-invoked startup coordinator and readiness-invalidation link anchors to the inert ESP32 adapter smoke application.
- Hardened `As608AttendancePipeline`: it receives the runtime's `const Occupancy&`, refuses scan initiation when `valid` is false, and rechecks validity immediately before appending `EV_SCAN`. This prevents an event append when the map is invalid at either boundary.

## Native verification

The release verifier completed with exit code 0 and printed `TARGETED_V0_2_52=PASS`.

| Suite | Result |
|---|---:|
| `test_as608_gate` | 31/31 passed |
| `test_as608_pipeline` | 8/8 passed |
| **Focused total** | **39/39 passed** |

Test-first evidence: the new pipeline test compile initially failed because the old constructor had no occupancy parameter. After adding the shared occupancy dependency and both checks, all eight pipeline cases passed. The verifier then reran both suites successfully.

Current inventory: **546 C++ `RUN_TEST` registrations across 34 suites**. The other **507** registrations were not run.

## ESP32 smoke-build evidence

Initial command:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

- Exit code: **0**
- Platform: Espressif32 8.0.0; board `esp32dev`
- Arduino-ESP32: 3.3.12; ESP-IDF libraries: 3.3.12 / IDF 5.5.5
- Xtensa toolchain: 14.2.0+20260121; esptool: 5.4.0
- Adafruit SSD1306 2.5.17; GFX 1.12.6; BusIO 1.17.4
- RAM: **63,676 B (19.4%)**
- Flash: **926,476 B (70.7%)**
- Build duration: **1,882.09 s**

The new startup coordinator source was compiled and linked. The exact image was checked with:

```sh
./tools/inspect_probe_wrappers.sh .pio/build/esp32-adapter-link/firmware.elf
```

Result: exit code **0**, `NVS_WRAPPER_ROUTING=PASS`. This confirms the inspector's listed NVS wrapper call routes in that ELF only.

**Final-source qualification:** the occupancy-gate change to `As608AttendancePipeline` was made after this successful target build. A subsequent target rebuild began but PlatformIO cleared the cached build objects and started recompiling framework/dependency objects. That redundant rebuild was stopped after 255.81 s (exit code 143); it is **not a pass**. The final occupancy-gate source is verified by the native pipeline suite, not by a post-change ESP32 target build. The successful target build remains valid evidence for the startup/HAL changes and the smoke link anchors.

The smoke application is inert: it does not invoke startup, send UART commands, start Serial2, start Wi-Fi or open application NVS handles. Never flash this target.

## Critical limitations

1. **E18 remains DELEGATED.** Physical index-page support, 32-byte payload, bit order, page numbering and ID 300 remain unverified. No `page = studentID - 1` production mapping was added.
2. **Startup is not mandatory at the production API boundary.** No production `main.cpp` or serialized application scheduler exists. The public `runtime.attendance().beginSetup()` path can bypass the startup coordinator. The event pipeline now blocks commits while occupancy is invalid, but direct raw-client scans can still be initiated outside the pipeline.
3. **No physical verification.** Sensor authentication, index ACK format, bitmap order, capacity report, UART timing, actual stored pages, attendance behavior, RTC, filesystem durability, power interruption and recovery were not tested.
4. **C34 remains blocking.** The NVS startup erase guard is a partial interception, not a complete erase firewall or production recovery path.
5. The remaining 507 native registrations, full native suite and SF2 Node suite were not run. The separate ESP32 diagnostic probe was not built. No image was flashed.

## Install

Install only by replacing the complete project tree with the full v0.2.52 archive. Do not merge individual files into an older tree. Never flash the inert adapter-link smoke image.
