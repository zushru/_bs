# Release v0.2.72 — host append fault injection

Date: 2026-11-10
Baseline: v0.2.71
Status: development snapshot; not production firmware and not hardware-verified

## Scope

This increment adds deterministic host-only fault injection to LittleFsFileSet append operations. It does not change the IFileSet contract, event format, filesystem layout, product decisions or ESP32 syscall path.

The test-only interface is in lib/littlefs_file_set/src/littlefs_file_set_test.h. It is unavailable under ESP_PLATFORM. Native LittleFsFileSet instances may receive a per-instance callback table; the ordinary native constructor continues to use real POSIX syscalls. The ESP_PLATFORM constructor and append path retain their direct calls to write, fsync, pread and close.

## Changes

- Added a test-only callback seam for append write, fsync, verification pread and close.
- Added seven test_littlefs_fileset regressions:
  1. Short successful writes are retried until the full record group is written.
  2. EINTR on write is retried.
  3. A seven-byte prefix followed by EIO returns false and leaves the prefix visible.
  4. fsync failure returns false although the complete payload is visible.
  5. Verification pread failure returns false although the complete payload is visible.
  6. Altered read-back bytes cause false although the underlying file contains the original payload.
  7. Close callback error returns false after the test callback has actually closed the POSIX descriptor.
- Added DEVELOPMENT_ROADMAP.md and WORKING_MEMORY.md with the approved increment order, N1 native full-suite gate, SF2 S0/S1 plan, file map, blockers and restart workflow.
- Updated the current context, critical issue, unverified evidence, test history, changelog and README.

## Verification

Owning test command:

    pio test -e native -f 'test_littlefs_fileset'

Result: **27/27 passed**, exit 0. The exact final log is /tmp/att_v0272_green_test_littlefs.log.

Test-first evidence:
- Red PlatformIO wrapper log: /tmp/att_v0272_red_test_littlefs.log. Seven new assertions failed as expected; the wrapper then printed SIGBUS and an inconsistent test count after the test process returned nonzero.
- Direct native test executable log: /tmp/att_v0272_red_direct.log. It ran 27 registrations, 7 failed and 20 passed, exited normally with status 7, and did not reproduce SIGBUS.
- Final green PlatformIO run: 27/27, exit 0.

Affected ESP32 target command:

    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

Result: passed, exit 0, 1,769.31 seconds. Environment: Espressif32 8.0.0 / ESP32 Dev Module; Arduino-ESP32 3.3.12; ESP-IDF libraries 3.3.12; esptool 5.4.0; Xtensa 14.2.0+20260121. PlatformIO reported 67,356/327,680 bytes RAM (20.6%) and 934,976/1,310,720 bytes application flash (71.3%). Exact ELF inspection command:

    ./tools/verify_v0.2.72.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"

Result: 12/12 NVS wrapper-routing checks passed and 9/9 LittleFsFileSet public methods retained. This is compile/link/static-symbol evidence only. The image is inert and must never be flashed.

## Test inventory

- 628 C++ RUN_TEST registrations across 35 native suite directories.
- 27 selected registrations passed in test_littlefs_fileset.
- The other 601 native registrations were not run in this increment.
- SF2 remains 257 Node registrations across 12 test files. No SF2 code changed; no Node suite or syntax check was run in this increment.
- No full native suite, SF2 suite, hardware-probe build, flash or physical-device test.

## Limitations and non-claims

These are host POSIX fault-injection tests. They do not establish mounted ESP32 LittleFS VFS behavior, descriptor behavior on close failure, directory-entry durability, full/near-full filesystem behavior, power-loss persistence, crash recovery or data restoration.

The close callback closes the actual POSIX descriptor and then returns synthetic EIO. It tests error propagation, not a genuine close failure that leaves a descriptor open.

A false append result remains indeterminate: zero, partial or complete data may be present. LogStore continues to fail closed; this increment does not retry logical operations or reconcile duplicates.

truncateFile(), quarantineCopy() and quarantineFrom() remain disabled. No recovery semantics were invented or enabled. C13 remains blocking.

No signed or delegated decision changed. HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
