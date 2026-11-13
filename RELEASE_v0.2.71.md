# Release v0.2.71 — LittleFsFileSet ESP32 compile/link compatibility

Date: 2026-11-10
Baseline: v0.2.70
Project root: /root/att

## Increment scope

This increment adds the existing LittleFS file-set adapter to the inert ESP32 compile/link smoke composition and fixes the target portability issue discovered by that build. It does not implement production storage integration or crash recovery.

### Changes

1. Added LittleFsFileSet include, object and retained member-function references to target/esp32_adapter_link/main.cpp. The target retains all nine public operations in the final ELF. It only constructs the adapter and checks its path-validity result; it does not mount LittleFS or perform file I/O.
2. Added statFileSetEntry() in lib/littlefs_file_set/src/littlefs_file_set.cpp. Under ESP_PLATFORM, it uses stat() because the pinned ESP-IDF/newlib headers do not declare lstat(). POSIX host builds continue to use lstat(), preserving the existing symlink-root and symlink-month rejection tests. This target branch relies on the ESP LittleFS backend having no symbolic-link entry type; do not apply it to arbitrary filesystems.
3. Updated the target README, C13/U110/U111 evidence, testing workflow, critical audit, release metadata and versioned ELF verifier.
4. Added tools/verify_v0.2.71.sh elf, which runs the existing 12 NVS wrapper-routing checks and verifies all nine LittleFsFileSet public symbols are present in the exact target ELF.

No event or persistence format changed. No production mount owner or storage recovery behavior was added. truncateFile(), quarantineCopy() and quarantineFrom() remain fail-closed and return false. HANDOFF_FINAL.md and docs/DECISIONS.md are unchanged.

## Test evidence

### Host suite

Exact final command: pio test -e native -f 'test_littlefs_fileset'

Result: **20/20 passed**, exit 0.

This suite uses host POSIX temporary directories. It covers checked metadata, reads, append behavior, and no-follow symlink rejection. It is not an ESP32 LittleFS runtime test.

### Target build

The first attempt failed at compile time because lstat() was not declared by the pinned target headers (exit 1, 1,220.24 seconds). This demonstrated a gap that the host suite could not expose.

After the ESP_PLATFORM-scoped helper change, the exact final-source build was: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

Result: **SUCCESS**, exit 0, **1,522.09 seconds**.

Target details:
- Board: Espressif ESP32 Dev Module (esp32dev)
- Platform: Espressif32 8.0.0
- Arduino-ESP32: 3.3.12 (4.30312.261024)
- ESP-IDF: 5.5.5
- Xtensa toolchain: 14.2.0+20260121
- Adafruit SSD1306 2.5.17, GFX 1.12.6, BusIO 1.17.4

Final PlatformIO size report:
- RAM: **67,356 / 327,680 bytes (20.6%)**
- Application flash: **934,976 / 1,310,720 bytes (71.3%)**

### Exact final ELF

Exact command: tools/verify_v0.2.71.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"

Result: **exit 0**:
- **12/12** NVS wrapper call-routing checks passed (NVS_WRAPPER_ROUTING=PASS).
- **9/9** LittleFsFileSet public methods were retained in the final ELF.

This is static linked-image evidence only. The target never mounted a filesystem or called an operation. It does not prove stat() behavior on a mounted partition, NVS runtime safety, or physical device behavior.

## Test coverage boundaries

Current native inventory: **621 C++ RUN_TEST registrations across 35 suites**. Only the 20-case test_littlefs_fileset suite was run in v0.2.71. The other **601 registrations were not run**.

The v0.2.69 focused roster result (43/43 across test_roster_snapshot and test_month_roster) is historical and was not rerun in this increment. No full native run, SF2 Node suite, .mjs syntax suite, esp32-hardware-probe build, flash, serial interaction, physical sensor, Wi-Fi, OLED, RTC, power-cut, brownout, powerbank or soak test was performed.

## Critical limits

C13 remains blocking:
- truncateFile(), quarantineCopy() and quarantineFrom() are intentionally disabled.
- No production filesystem mount owner or serialized storage boundary exists.
- No target mount/runtime test, VFS short-write/fsync/close fault injection, descriptor cleanup test, full-filesystem test, or power-cut/brownout test exists.
- Partial append errors can leave an uncertain tail. LogStore::open() can still fail closed when a torn tail requires truncate.
- No crash-recovery journal or approved repair protocol was invented or enabled.

U112 durable request identity and exactly-once semantics remain unresolved. C3/C6 production root, scheduler, HTTP/auth route wiring and enrollment integration remain absent. U61 remains blocked by missing operational integration. C34 remains a limited NVS erase-route guard, not a full flash-erase firewall. E27 and all other delegated decisions are unchanged.

The smoke target is inert and must never be flashed as attendance firmware. No physical-device claim is made.

## Complete replacement archive

The release archive is a complete project replacement, excluding .git, .pio, caches and compiled build outputs. Verify its SHA-256 sidecar and MANIFEST.sha256 before replacing the existing tree.
