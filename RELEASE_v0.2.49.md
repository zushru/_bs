# Release v0.2.49 — AS608 target link coverage and NVS route verification

**Date:** 2026-11-09  
**Base:** remote working tree v0.2.48  
**Status:** focused host tests and both ESP32 target builds passed; final-ELF NVS routing passed for both images. No device was flashed. This is a development snapshot, not production firmware.

## Changes

1. Extended the inert esp32-adapter-link target to compile/link the AS608 Stream bridge, raw adapter, single-owner runtime, occupancy reader, attendance client, enrollment session and FSM. The composition is function-local static in setup() after Arduino initialization. No UART setup or sensor command is performed.
2. Guarded the ESP-IDF-only NVS wrapper implementation with ARDUINO_ARCH_ESP32, keeping the pure policy available to native tests.
3. Fixed the ELF inspector's Xtensa branch-target false negative by following internal labels inside wrapper functions. Added 11 positive/negative fixtures, all passing.
4. Added tools/verify_v0.2.49.sh with targeted, links and full modes. The links mode preserves both final ELFs before switching the separate PlatformIO configurations, which reuse the shared .pio/build directory.

## Verification

### Native targeted suites

| Suite | Result |
|---|---:|
| test_nvs_startup_guard_policy | 9/9 |
| test_hardware_probe | 4/4 |
| test_as608 | 15/15 |
| test_as608_gate | 26/26 |
| test_enroll | 36/36 |
| **Total** | **90/90** |

Static source inventory remains 533 registrations across 33 suites. The other 443 registrations were not run. Inspector fixtures passed 11/11.

### ESP32 target builds

Resolved packages: Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, esptool 5.4.0, Xtensa toolchain 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4.

| Target | Result | RAM | Flash |
|---|---|---:|---:|
| esp32-adapter-link | SUCCESS | 63,652 B (19.4%) | 926,248 B (70.7%) |
| esp32-hardware-probe | SUCCESS | 46,764 B (14.3%) | 928,880 B (70.9%) |

Final-ELF inspection passed for both targets. Arduino startup reaches both wrappers, each wrapper delegates to the real IDF API, and the standard default/named/pointer NVS erase APIs route through the wrapped partition-erase boundary.

## Important limitations

- The NVS guard is not a general erase firewall: smaller/chunked erases and alternate/direct flash erase paths are not blocked. C34 remains a deployment blocker until production startup explicitly links and gates the guard and has a reviewed recovery/migration path.
- Target compile/link and ELF inspection do not prove hardware behavior or safe power-loss recovery.
- No device was flashed. AS608 UART, sensor indexing, OLED visibility, I2C device protocols, AP first beacon, DHCP, NVS retention, power/brownout and soak behavior remain unverified.
- esp32-adapter-link is an inert link-smoke target and must never be flashed. The hardware probe is diagnostic-only and not attendance firmware.
- C3 remains blocking: no production scheduler, scan pipeline adoption, enrollment/attendance UI or production HTTP route has been integrated. E18 remains delegated and unresolved for production activation.

## Verification commands

```sh
./tools/verify_v0.2.49.sh targeted
./tools/verify_v0.2.49.sh links

# Full native suite only at an integration/release checkpoint:
./tools/verify_v0.2.49.sh full
```

Logs and preserved ELF snapshots are written to /tmp/att-v0.2.49-verification/. The scripts never upload or flash.

## Decision impact

No product decision changed. E29 remains SIGNED (att); E30/E31/E12 remain DELEGATED; O8 remains SIGNED and unchanged.
