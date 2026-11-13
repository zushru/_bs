# Release v0.2.50 — AS608 target composition and verification hardening

**Date:** 2026-11-09  
**Base:** remote working tree v0.2.48  
**Status:** native targeted suites passed; both ESP32 targets compiled/linked; both final ELFs passed NVS wrapper routing inspection; no device test or flash was performed.

## Changes

- Added an ESP32-only compile guard around the IDF-specific implementation in `esp32_nvs_startup_guard.cpp`. The platform-independent NVS policy header remains available to native tests. This fixes the native compile failure caused by unconditional inclusion of `esp_partition.h`.
- Expanded the inert `esp32-adapter-link` target to compile/link the AS608 composition: Arduino `Stream` transport, raw AS608 adapter, occupancy refresh, enrollment session/FSM, attendance client and single-owner `As608Runtime`. It constructs the in-memory composition in `setup()` after Arduino initialization. It does not begin `Serial2`, issue sensor commands, start Wi-Fi, initialize application NVS handles, request entropy or run attendance/enrollment.
- Corrected the Xtensa ELF inspector to inspect internal branch targets in addition to linear disassembly. Xtensa variable-length instructions caused the prior inspector to miss the actual partition-erase delegate after a branch. Added a fixture for that branch-target case.
- Fixed the verification workflow: each target ELF is inspected immediately after that target builds because the two PlatformIO configuration files share `.pio/build` and the later build can replace the earlier output.
- Added an incremental verification script with `targeted`, `links` and `full` modes, plus a source fingerprint so the link step cannot rely on stale targeted results.
- Added Desktop Commander/slow-phone workflow instructions. Full native verification is now a release checkpoint, not the default per-increment action.
- Decision ledger and signed handoff were not changed. No product behavior, hardware mapping, or production firmware flow was added.

## Verification performed

### Targeted native tests

| Suite | Result |
|---|---:|
| `test_nvs_startup_guard_policy` | 9/9 |
| `test_hardware_probe` | 4/4 |
| `test_as608_gate` | 26/26 |
| `test_as608` | 15/15 |
| `test_enroll` | 36/36 |
| **Total** | **90/90** |

The first NVS policy attempt failed during native compilation because `esp_partition.h` was included in a native build. No assertions ran in that attempt. After adding the platform guard, the five targeted suites compiled and passed. The other 443 native registrations were not run.

### ESP32 compile/link

| Target | Result | RAM | Flash |
|---|---|---:|---:|
| Inert `esp32-adapter-link` (expanded AS608 composition) | SUCCESS, 1,144.83 s | 63,652 B (19.4%) | 926,248 B (70.7%) |
| Diagnostic `esp32-hardware-probe` | SUCCESS, 1,023.89 s | 46,764 B (14.3%) | 928,880 B (70.9%) |

Pinned environment reported by PlatformIO: Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, esptool 5.4.0, Xtensa toolchain 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4.

### Final-ELF routing

Both target ELFs independently reported `NVS_WRAPPER_ROUTING=PASS`: Arduino `initArduino()` reaches both wrappers; both wrappers delegate to their real ESP-IDF functions; and the standard NVS erase APIs route to the wrapped partition-erase boundary.

The inspector fixture suite passed 11 scenarios: three accepted paths (including an internal-branch delegate) and eight rejected/missing-route cases.

## Critical limitations

1. **C34 remains blocking.** The wrapper only rejects an exact whole-DATA_NVS `esp_partition_erase_range` request. Partial/sector erases and alternate/direct flash erase paths are not covered.
2. Standard NVS erase APIs may deinitialize open NVS handles before the lower-level erase is denied. Do not treat a denied erase as a recovery workflow.
3. Both target builds are smoke/diagnostic images, not production firmware. The inert smoke target must never be flashed.
4. No NVS corruption test, AP runtime test, I2C/OLED test, DHCP test, AS608 sensor test, attendance test, power-cut test, soak test or physical device verification was performed.
5. The AS608 target composition only proves compile/link compatibility. It does not establish UART baud/timing, sensor packet behavior, page mapping, enrollment flow or attendance operation.

## Install

Install only by replacing the full project tree with the complete v0.2.50 archive. Do not merge individual files into an older tree.
