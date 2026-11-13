# Specialized audit — v0.2.49

Date: 2026-11-09  
Base: remote working tree v0.2.48, source manifest verified before editing  
Review: source audit, focused PlatformIO tests, two ESP32 target builds and final-ELF call-routing inspection. No device was flashed.

## Increment 1 — AS608 target compile/link coverage

Extended target/esp32_adapter_link/main.cpp with Arduino.h and as608_runtime.h. The smoke target now keeps link references to the Stream UART bridge, raw AS608 adapter, single-owner As608Runtime, occupancy reader, attendance client, enrollment session and FSM.

The roster and occupancy objects are function-local statics constructed from setup() after Arduino initialization. This avoids cross-translation-unit initialization against global Serial2. The 0U sensor password is a link-only sentinel. No referenced sensor method is invoked; Serial2 is not begun and no UART command is transmitted. The new code does not start Wi-Fi, request entropy, open/write NVS or transact over I2C.

This is compile/link coverage only. There is still no production scheduler, production scan/event commit path, HTTP/UI integration or E18 student-ID/page resolution. C3 remains blocking.

## Increment 2 — native-only compile guard

The first focused native attempt failed before any assertion because esp32_nvs_startup_guard.cpp unconditionally included ESP-IDF headers in the native environment. The ESP-IDF wrapper implementation is now enclosed by ARDUINO_ARCH_ESP32. The platform-independent nvs_startup_guard_policy.h remains available to native tests.

After the correction, the policy suite passed 9/9. Both ESP32 target builds subsequently compiled the guarded translation unit successfully.

## Increment 3 — Xtensa-aware ELF inspector

The first real smoke ELF inspection found that the inspector could not see the call from __wrap_esp_partition_erase_range to the real esp_partition_erase_range. This was an inspector false negative. Xtensa uses variable-length instructions; linear disassembly after an unconditional branch lost the alternate basic block. The actual internal branch target contained the delegate call.

The inspector now follows internal branch labels within each function and disassembles those target blocks. Its fixture harness tests three accepted shapes and eight missing-route cases (11 total), including positive and negative branch-target fixtures. All 11 pass. Both actual final ELFs pass the revised inspector.

## Verification evidence

### Native suites

| Suite | Result |
|---|---:|
| test_nvs_startup_guard_policy | 9/9 |
| test_hardware_probe | 4/4 |
| test_as608 | 15/15 |
| test_as608_gate | 26/26 |
| test_enroll | 36/36 |
| **Targeted total** | **90/90** |

Static inventory remains 533 registrations across 33 suites. The other 443 registrations were not run. SF2 source did not change.

### ESP32 target builds

Resolved packages: Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, ESP-IDF 5.5.5, esptool 5.4.0, Xtensa toolchain 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4.

| Target | Result | RAM | Flash | Build time |
|---|---|---:|---:|---:|
| esp32-adapter-link | SUCCESS | 63,652 B (19.4%) | 926,248 B (70.7%) | 1,144.83 s |
| esp32-hardware-probe | SUCCESS | 46,764 B (14.3%) | 928,880 B (70.9%) | 1,023.89 s |

The adapter smoke size increase is expected because AS608 symbols are intentionally retained. These are not production firmware size measurements.

### Real final-ELF routing

Both final ELFs passed tools/inspect_probe_wrappers.sh. The inspector confirmed:
- initArduino() calls both linker wrappers.
- The NVS init wrapper delegates to the real nvs_flash_init().
- The partition-erase wrapper delegates to the real esp_partition_erase_range(), including the internal Xtensa branch target.
- nvs_flash_erase(), nvs_flash_erase_partition() and nvs_flash_erase_partition_ptr() reach the wrapped boundary.

The two PlatformIO target configurations reuse/replace the shared .pio/build environment directory. The probe ELF was preserved outside that directory before rebuilding the smoke target. The v0.2.49 verifier now explicitly copies each final ELF before switching configurations.

## Residual risks and non-claims

- The NVS guard blocks only an exact full-partition esp_partition_erase_range call. Smaller/chunked ranges and alternate/direct flash erase paths remain possible.
- Final-ELF routing is not runtime execution. No NVS corruption was induced and no device was flashed.
- No evidence exists for NVS retention across power cuts, AS608 UART timing/frames, index bit order, OLED visibility, I2C device protocols, AP first-beacon security, channel/regulatory behavior, DHCP delivery, power-bank runtime, brownout or soak reliability.
- The probe is diagnostic-only, not production firmware. Do not flash the inert adapter smoke.
- C1/C2/C3/C4/C6/C8/C12/C13 and the existing SF2 blockers remain open as recorded in docs/CRITICAL.md.

## Decision impact

No SIGNED, SIGNED-HANDOFF, DELEGATED or OPEN product decision changed. E18, E30, E31, E12 and O8 remain as recorded in docs/DECISIONS.md.
