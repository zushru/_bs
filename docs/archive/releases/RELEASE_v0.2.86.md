# Development increment v0.2.86 — startup validator read-fault injection

Date: 2026-11-11  
Base: uploaded `att_v0.2.85_2026-11-11.tar.gz`  
Base archive SHA-256: `7136938e69ec25bed9cb8e2e0d101ac4b8d72bf7b9f3d7197808b6cf297746a4`

## Change

- Reused the existing native-only `LittleFsFileSetTestOps::preadFn` in `ReadOnlyFileStorage`, the read-only IStorage adapter passed to `LogStore::open()` during `LittleFsFileSet::validateRecordFiles()`.
- The ESP32 branch retains its direct `pread()` call. No production interface, storage format, mount semantics or firmware composition was changed.
- Added `test_record_validation_fails_closed_on_pread_error_without_mutation`. It creates a valid one-record month file, injects `EIO` through `preadFn`, requires `validateRecordFiles()` to return false, and verifies the stored file size and record bytes are unchanged.

## Verification status

**Status: pending focused test; not release-verified.**

- Owning command to run when RDC/PlatformIO is available: `pio test -e native -f 'test_littlefs_fileset'`.
- The test was not run in this increment, so there is no red reproduction or green result. A host-only syntax check of the modified source passed: `g++ -std=gnu++11 -Wall -Wextra -fsyntax-only -Ilib/core/src -Ilib/littlefs_file_set/src lib/littlefs_file_set/src/littlefs_file_set.cpp` (exit 0). This did not compile the Unity test or the ESP32 branch. Current owning-suite source inventory: 29 registrations; total project inventory: 711 C++ registrations across 39 suites. Recounting the uploaded v0.2.85 archive found 710 active registrations before this change, not 704 as its README/TESTS/WORKING_MEMORY recorded; the prior inventory was understated by six.
- No SF2 tests, Unity native tests, ESP32 build, ELF inspection, flash, mounted VFS operation or physical-device test was run.
- The local worktree is `/mnt/data/att_v0.2.86_work`, extracted from the uploaded archive. RDC was unavailable; the live `/root/att` checkout was not inspected or modified. This work must be reconciled into the phone checkout only after its status and protected hashes are checked.

## Safety and remaining uncertainty

- This is host fault-injection source coverage, not evidence of ESP-IDF LittleFS behavior. It does not inject `stat`, `readdir`, `open`, `fsync` or `close` faults into the actual target VFS.
- It does not prove file data or directory-entry durability, error semantics, power-loss behavior, unmount behavior or recovery safety.
- C13.3 remains incomplete. Destructive recovery stays disabled; D1–D11 delegated policy is unchanged.
- The inert adapter-link target remains build-only and must never be flashed.

See `docs/AUDIT_v0.2.86.md`, `docs/TESTS.md` and U110 in `docs/UNVERIFIED.md`.
