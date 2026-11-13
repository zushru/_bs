# v0.2.93 — C13.3 host read-fault coverage

Date: 2026-11-11
Baseline: v0.2.92, 445-entry manifest
Scope: extend the existing native-only LittleFsFileSet syscall seam through readFile().

## Yield

LittleFsFileSet::readFile() now routes native open, pread, and close through the existing per-instance test hooks. The ESP_PLATFORM branch remains direct open/pread/close calls. This closes a real testability hole: read-path I/O failures and EINTR could not previously be injected even though the test seam already declared the callbacks.

Four registered regressions cover open failure, pread failure, close failure, and one interrupted pread followed by successful retry. Existing append/validator byte-preservation assertions now use a fresh uninjected reader after a deliberately injected read/close fault.

## Baseline and integrity

- Remote root: /root/att; no Git metadata.
- README marker: v0.2.92 before edits.
- MANIFEST.sha256: 445 entries, all verified before edits.
- Protected SHA-256 unchanged:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: 52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3
- No active PIO process/session at preflight.

## Files changed

- lib/littlefs_file_set/src/littlefs_file_set.cpp
- test/test_littlefs_fileset/test_main.cpp
- Current documentation/release metadata and manifest.

No signed handoff, decision ledger, persisted event format, recovery policy, or destructive storage behavior was changed.

## Exact verification

Red reproduction:
- The four newly registered read-path regressions failed before the source patch: open/pread/close faults were not observed by readFile(), and injected EINTR was not reached.

Final focused native command:
    pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'
- 73/73 passed, exit 0:
  - test_littlefs_fileset: 47/47
  - test_littlefs_mount_owner: 23/23
  - test_littlefs_serialized_fileset: 3/3
- The final run exercises all four added tests. It is POSIX-host/fake-backend evidence, not mounted LittleFS proof.

ESP32 compile/link:
    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
- SUCCESS, exit 0, pinned Espressif32 8.0.0; Arduino-ESP32 3.3.12; esptool 5.4.0; Xtensa 14.2.0+20260121.
- PlatformIO memory: 67,892 B RAM (20.7%); 1,010,340 B application flash (77.1% of 1,310,720 B).
- Final ELF SHA-256: be804edcc9fc096f9c27ff3bb27994c9928eb295b5cf402ed982395e85d34405
- Final BIN SHA-256: 8162f9e3542cda56220af661d9a1d93a664fde50d53cea9c4e01be767f99e433
- ELF nm -C inspection found LittleFsFileSet read/append/record-validation methods, mount-owner begin/fileSet methods, and ESP32 mount/unmount methods. Destructive adapter methods remain disabled stubs.
- Full build log: /tmp/att_v0293_esp32_link.log (outside release tree).

## Inventory and not run

- Current source inventory: 735 C++ RUN_TEST registrations across 39 native suites.
- This increment ran only the three C13 suites (73 cases); 662 other C++ registrations were not run.
- SF2 remains 270 Node registrations across 12 test files; not run.
- Full native N1 remains deferred.
- No physical board operation, filesystem mount, target VFS fault injection, low-space test, restart/interruption test, power-cut/brownout test, flash, or NVS erase was performed.
- esp32-adapter-link is an inert compile/link smoke target. NEVER flash it.

## Critical disposition

C13.3 remains BLOCKING. The native test seam proves host error-path routing only. The target image proves compile/link and inspected symbols only. Neither establishes pinned ESP-IDF LittleFS behavior for descriptor closure, fsync semantics, file/directory-entry durability, low-space failures, restart consistency, or brownout/power-cut behavior.

Keep anomalous logs read-only and fail-closed. No format, truncate, quarantine, repair, recovery journal, or commit primitive was enabled or selected. D1–D11 remain unchanged. The production root and physical interruption matrix remain required before any durability or recovery claim.
