# att — ESP32 fingerprint attendance

**Development snapshot v0.2.48 (2026-11-08).** This is not production firmware and is not hardware-verified. Read `docs/CONTEXT.md`, then `docs/CRITICAL.md`, before changing code. Replace the entire project tree from the release archive; do not merge selected files into an older tree.

## Project structure

- `lib/core/src`: hardware-independent attendance, roster, config, backup/replay, enrollment, and API logic.
- `lib/as608/src`: AS608 transport-facing attendance/enrollment modules and shared sensor ownership logic.
- `lib/hal_esp32/src`: ESP32 adapters for UART, NVS, RNG, OLED, SoftAP, and the shared NVS startup guard. These are adapters, not a complete production firmware composition.
- `test/`: 33 native suites with 533 C++ `RUN_TEST` registrations.
- `target/esp32_adapter_link/`: inert compile/link smoke application. Never flash it.
- `target/esp32_hardware_probe/`: diagnostic-only I2C/OLED/SoftAP probe. It is not attendance firmware.
- `web/sf2/`: isolated browser-side SF2 reference model. It is not integrated with a production API, dashboard or XLSX writer.
- `docs/`: controlling context, critical issues, decisions, verification evidence, module specs and SF2 design notes.

## GitHub Codespaces

The current GitHub `main` branch is an older snapshot. Use a separate `codespaces/v0.2.48` branch. Follow `CODESPACES_IMPORT.md` to upload the complete release archive into that branch and extract it in a Codespace. The Codespaces container uses Debian 12, PlatformIO Core 6.2.0 and Node.js 22.

After importing and rebuilding the dev container, run:

```sh
./tools/codespace-doctor.sh
./tools/codespace-test.sh
./tools/codespace-build-esp32.sh
```

These scripts test and compile/link only. They contain no firmware upload command.

## Local commands (Termux + Debian PRoot)

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test.log
npm --prefix web/sf2 test
find web/sf2 -type f -name '*.mjs' -print0 | xargs -0 -n1 node --check

# Compile/link only; do not flash these diagnostic images.
pio run -j 2 -c platformio.esp32-smoke.ini -e esp32-adapter-link
pio run -j 2 -c platformio.esp32-probe.ini -e esp32-hardware-probe
bash tools/inspect_probe_wrappers.sh .pio/build/esp32-hardware-probe/firmware.elf
```

See `docs/TESTS.md` for exact passing logs and their limits. Host tests and target compile/link do not prove hardware behavior.

## NVS startup guard — scope limitation

`Esp32NvsStartupGuard` intercepts the specific Arduino-ESP32 pre-`setup()` call to `esp_partition_erase_range()` that erases the entire default DATA_NVS partition after `nvs_flash_init()` reports `NO_FREE_PAGES` or `NEW_VERSION_FOUND`. It does **not** prevent an explicit `nvs_flash_erase()` call or an application that deliberately erases the partition sector-by-sector. It is not a general NVS erase firewall and is not yet wired into a production boot owner. Inspect the final ELF routing for every target that uses it.

## Hardware and product constraints

The approved device uses USB-only firmware updates, an AS608 sensor on 3V3, I2C OLED/RTC/EEPROM, always-on operation and a fixed generic SoftAP SSID `att`. Signed-off behavior is in `HANDOFF_FINAL.md`; approval/delegation status is in `docs/DECISIONS.md`. Do not change DELEGATED decisions without approval. Actual pins and device output remain subject to the user's hardware confirmation.
