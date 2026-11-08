# v0.2.48 — shared NVS startup guard and Codespaces migration

This development snapshot consolidates the probe's NVS-startup guard into a reusable ESP32 HAL adapter, fixes the Xtensa final-ELF inspector, and adds a reproducible GitHub Codespaces environment. It is not production firmware.

## Source changes

- Added `NvsStartupGuardPolicy`, a hardware-independent fail-closed policy with nine regression tests. It latches the first NVS init result, blocks only the Arduino startup fallback call that erases the entire DATA_NVS partition in one `esp_partition_erase_range()` operation; ordinary sector erases remain allowed. It does not prevent explicit `nvs_flash_erase()` or sector-by-sector application erasure.
- Added ESP32-only `Esp32NvsStartupGuard` linker wrappers around `nvs_flash_init()` and `esp_partition_erase_range()`. The ESP32 adapter smoke target and diagnostic probe both use the same implementation. Native builds exclude the ESP-IDF-specific implementation with `ARDUINO_ARCH_ESP32` gating.
- Removed duplicated probe-only guard code and its old helper header. The probe reports the shared guard's latched status before using I2C/OLED/Wi-Fi.
- Fixed `tools/inspect_probe_wrappers.sh` to disassemble internal Xtensa branch targets independently. Added a positive/negative shell regression fixture.
- Added `.devcontainer`, Codespaces doctor/test/build scripts, and import instructions. The ESP32 build script compiles/links only; it has no flash/upload command.

## Verification status

Confirmed in the connected Android Termux/PRoot environment:

- Full PlatformIO native suite: **533 test cases, 533 succeeded in 00:19:09.008**, across 33 suites. This is host-test evidence, not ESP32/device verification.
- New NVS startup policy suite: **9/9 passed**, also included in the 533-case aggregate run.
- SF2 reference-model Node suite: **257/257 passed** locally; all 25 `.mjs` modules passed `node --check`.
- Xtensa wrapper-inspector regression: positive branch-target delegation fixture and negative missing-delegate fixture both passed.
- Before the final `ARDUINO_ARCH_ESP32` compile guard was added to the ESP-IDF wrapper translation unit, the shared-guard ESP32 adapter smoke target compiled/linked successfully: 47,028 B RAM (14.4%), 912,812 B flash (69.6%). Its final ELF inspection reported `PROBE_WRAPPER_ROUTING=PASS`.
- Before that same native-only compile-guard correction, the shared-guard diagnostic probe compiled/linked successfully: 46,756 B RAM (14.3%), 928,156 B flash (70.8%). The probe ELF was not inspected.

A final incremental PRoot rebuild after adding the `ARDUINO_ARCH_ESP32` guard stalled before emitting build output and was terminated. Therefore, **the exact final source tree's ESP32 compile/link and both final ELF inspections remain pending**. The included Codespaces build script performs those checks in the cloud VM; it never uploads firmware.

## Critical limitations

- No firmware was flashed. The user cannot currently connect/flash the ESP32 through Termux.
- No `SAFE_NVS_INIT=PASS`, NVS error-path runtime, I2C, OLED pixel, first-beacon, Wi-Fi security or DHCP behavior is hardware-verified.
- The guard is a reusable startup component, not a fully integrated production boot owner. It blocks only the specific one-call Arduino startup fallback that erases the whole DATA_NVS partition. It does not block explicit `nvs_flash_erase()` or a deliberate sector-by-sector application erase. Every future production Arduino-ESP32 target must link the wrappers and check startup status before application NVS access or service startup. C6 and the production integration blockers remain.
- The wrappers depend on the exact Arduino-ESP32 startup call graph. Inspect final ELF routing for every target/configuration that uses them; never infer it from symbol presence alone.
- No automatic erase/recovery path was added. E30 is DELEGATED and unchanged.

Install/import only from the complete archive. Do not merge selected files into an older tree.
