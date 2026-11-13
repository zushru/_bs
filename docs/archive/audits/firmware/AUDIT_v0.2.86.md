# Audit v0.2.86 — startup record-validator read fault

Date: 2026-11-11  
Scope: native-only fault injection for `LittleFsFileSet::validateRecordFiles()`.

## Finding / change

The append path already accepted the test-only `LittleFsFileSetTestOps::preadFn`, but startup validation wrapped an opened month file in `ReadOnlyFileStorage`, whose `read()` always called libc `pread()` directly. Therefore the existing read-fault seam could not force an I/O error through the actual `LogStore::open()` path used by startup preflight.

The native-only path now forwards reads through `preadFn` when supplied, with libc `pread()` as the default. The `ESP_PLATFORM` branch remains exactly direct `pread()` and the test hook remains unavailable on target builds. A new regression creates a valid one-record month file, injects one `EINTR` followed by `EIO`, requires validation failure, and checks both file size and every record byte are unchanged.

## Critical review

- The hook is borrowed from `LittleFsFileSetTestOps`; the test keeps both the ops table and context alive for the duration of the adapter and call.
- The fault occurs in the `IStorage::read()` path used by `LogStore::open()`, rather than returning a synthetic failure from `validateRecordFiles()` itself.
- The test checks the failure channel, unchanged size and byte-for-byte record equality through the normal read API. It is still only a host test source until executed.
- Existing `validateRecordFiles()` closes the opened descriptor on the `LogStore::open()` failure branch via the common `close(fd)` path. The new hook does not change descriptor ownership.
- No production ABI or ESP32 behavior is intended to change. A target build is still required later to verify conditional compilation in the real pinned toolchain.

## Inventory discrepancy found

The uploaded v0.2.85 archive has 39 suite `test_main.cpp` files with 710 active `RUN_TEST` calls (the `test_littlefs_fileset` suite has 28). Its README, TESTS and WORKING_MEMORY documents reported 704, understating the source inventory by six. This increment adds one active registration: the current source count is 711 across 39 suites, with `test_littlefs_fileset` at 29. No historical passed count is inferred from this source inventory.

## Verification boundary

No test or ESP32 build was executed because the user directed that verification wait until RDC becomes available. A host-only syntax check of the modified source passed with `g++ -std=gnu++11 -Wall -Wextra -fsyntax-only -Ilib/core/src -Ilib/littlefs_file_set/src lib/littlefs_file_set/src/littlefs_file_set.cpp` (exit 0). This checks only the native conditional branch; it is not a Unity test or a target compile.

When available, run first:

```sh
pio test -e native -f 'test_littlefs_fileset'
```

Expected test registration count for that suite is now 29; this is inventory, not a pass count. Do not run full N1. Add dependent storage suites only if the owning suite reveals an interface regression.

No target build, ELF inspection, mounted ESP-IDF VFS operation, VFS fault injection, format/repair, truncate/quarantine, power-cut, brownout, flash or physical hardware test was performed.

## Disposition

The native startup-validator read path is now injectable at source level. U110 is **partially advanced but unverified**. C13.3 remains blocked by target VFS behavior, actual stat/enumeration/fault evidence, durability/commit-point proof, recovery journaling and physical interruption tests.
