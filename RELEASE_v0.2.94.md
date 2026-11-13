# v0.2.94 — C13.3 host month-creation fault coverage

Date: 2026-11-12
Baseline: v0.2.93, 447-entry manifest
Scope: extend the native-only LittleFsFileSet syscall seam to addFile().

## Yield

LittleFsFileSet::addFile() now routes host open(O_CREAT|O_EXCL), fsync, and close through the existing per-instance native test hooks. The ESP_PLATFORM branch still calls the direct POSIX/VFS functions and does not include the host test interface.

The test hook's openFn now receives mode_t mode, allowing it to faithfully wrap both read-only opens and exclusive month-file creation. readFile()'s host helper supplies mode zero for O_RDONLY; addFile() supplies 0600 for exclusive creation.

Three regressions cover:
- open failure: no month file is created.
- fsync failure: addFile() returns false, but the created empty month file remains.
- close failure: addFile() returns false, but the created empty month file remains.

The last two outcomes are important: a false return does not mean the operation had no side effect. The adapter deliberately does not unlink or retry a possibly created month file. A caller must reconcile the fileset before attempting a second creation.

## Baseline and integrity

- Remote root: /root/att; no Git metadata.
- README marker before edits: v0.2.93.
- MANIFEST.sha256: 447 entries, all verified before edits.
- Protected SHA-256 unchanged:
  - HANDOFF_FINAL.md: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d
  - docs/DECISIONS.md: 52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3
- No active PIO process/session at preflight.

## Files changed

Implementation/tests:
- lib/littlefs_file_set/src/littlefs_file_set.cpp
- lib/littlefs_file_set/src/littlefs_file_set_test.h
- test/test_littlefs_fileset/test_main.cpp

Release/audit records:
- RELEASE_v0.2.94.md
- docs/AUDIT_v0.2.94.md
- Current release pointers, test/changelog/context/working-memory/critical/unverified/roadmap records, README and manifest.

No signed handoff, decision ledger, persisted event format, recovery policy, or destructive storage behavior was changed.

## Exact verification

Test-first red:
- Before the implementation change, the three new addFile() regressions failed: the direct production calls bypassed the injected open/fsync/close failures. The expected red output was observed through RDC, but was not separately saved as a log file.
- During test development, two intermediate runs exposed fixture counter/arming mistakes; those were corrected in the tests, not by weakening assertions.

Final focused native command:
    pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'
- 76/76 passed, exit 0:
  - test_littlefs_fileset: 50/50
  - test_littlefs_mount_owner: 23/23
  - test_littlefs_serialized_fileset: 3/3
- Full log: /tmp/att_v0294_c13_native.log.

ESP32 compile/link:
    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
- SUCCESS, exit 0; build duration 1,987.27 seconds.
- Pinned packages: Espressif32 8.0.0; Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12; esptool 5.4.0; Xtensa 14.2.0+20260121.
- RAM: 67,892 B (20.7% of 327,680 B).
- Application flash: 1,010,340 B (77.1% of 1,310,720 B).
- Final ELF SHA-256: 18af01b5664e794b22f27e2d3acb383dbbbdfb3c00135a4a08d3198cfb677660
- Final BIN SHA-256: 40ce2e4416b2a59c524bf40d97b7d648566b0054d894e9aa7cdbcd88a689c909
- ELF nm -C inspection retained LittleFsFileSet::addFile, readFile, appendFile, validateRecordFiles, LittleFsStorageOwner, and Esp32LittleFsMountBackend methods.
- Full build log: /tmp/att_v0294_esp32_link.log.

The smoke target is inert. Its link-only references do not call the storage methods or mount LittleFS. It was not flashed. This compile/link plus symbol evidence is not runtime proof.

## Inventory and not run

- Current source inventory: 738 C++ RUN_TEST registrations across 39 native suites.
- This increment ran only the three C13 suites (76 cases); the other 662 C++ registrations were not run.
- SF2 remains 270 Node registrations across 12 files; not run.
- Full native N1 remains deferred.
- No mounted ESP32 LittleFS operation, VFS fault injection, near-full/low-space test, directory sync/durability test, restart/interruption test, power-cut/brownout test, flash, or physical-device test was performed.

## Critical disposition

C13.3 remains BLOCKING. The new tests establish host fault-path routing and preserve the important indeterminate-side-effect contract for month-file creation. They do not prove the pinned ESP-IDF LittleFS behavior for exclusive create, descriptor close, file data durability, parent-directory entry durability, low-space errors, remount consistency, or power loss.

addFile() still creates an empty month file before its sync/close result is fully known. On a false result after creation, blindly retrying returns false because the path already exists; callers must reconcile through the storage owner and fail closed. There is no rollback or auto-delete.

Keep anomalous logs read-only. No format, truncate, quarantine, repair, journal, or commit primitive was enabled or selected. D1–D11 remain unchanged. The production root, target VFS validation, interruption matrix, and physical power-cut/brownout proof remain required.
