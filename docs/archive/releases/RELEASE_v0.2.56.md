# Release v0.2.56 — checked storage metadata contracts

Date: 2026-11-09  
Base: complete v0.2.55 archive extraction  
Base archive SHA-256: `0709366b8cb444c7008c40e3ea9b061da8430dc811202dd54e84397fcd5dbb0e`  
Project root during development: `/root/att`  
Release status: development snapshot; C3 and C13 remain blocking

## Scope

This increment hardens the storage metadata contracts that the future LittleFS driver must implement.

1. `IStorage::sizeChecked()` is now pure virtual. Every storage implementation must explicitly distinguish a valid zero-byte stream from a failed metadata query. The base non-virtual `size()` helper is for non-critical convenience only and returns `SIZE_MAX` on query failure; boot/replay code uses `sizeChecked()` directly.
2. `IFileSet::countChecked()`, `monthKeyChecked()` and `fileSizeChecked()` are now pure virtual. The old value-only metadata interface was removed so a future file-set driver cannot silently inherit a fallback that converts an I/O failure into plausible metadata.
3. `MonthSet` now uses checked metadata for size, reads, month rollover, truncation planning, quarantine planning and repair bounds. It rejects arithmetic overflow and verifies a newly added month file is visible with the expected key, zero size, and exactly one additional entry before appending.
4. `LogStore::open()` fails closed if the storage size query fails and refuses a record count that cannot be represented by its next-sequence field.
5. `RestoreReplayAudit` returns the distinct `RESTORE_REPLAY_STAGE_SIZE_FAILED` result when the staging size query fails, separating I/O failure from an actual stream-size mismatch.
6. Added test fixtures for mandatory checked methods and regressions for file-set count/key/size query errors plus restore staging-size query failure.

## Test-first and verification

The initial test-first run against the old value-only behavior used:

```sh
pio test -e native -f 'test_monthset'
```

The four new metadata-error regressions all failed as expected; the ten pre-existing cases passed. Unity then terminated with SIGILL after the failed assertions. The command exited 1. This demonstrated that count, key and file-size query errors were being treated as ordinary values.

The exact PlatformIO command encoded in `tools/verify_v0.2.56.sh` was run directly. It invokes the 15 directly affected/implementer suites. Result: **264/264 passed**, exit 0, suite total **4m22.776s**. The wrapper script passed `bash -n`, and the manifest check passed separately; the wrapper itself was not run to avoid repeating the same suite. The exact PlatformIO invocation is recorded in `docs/TESTS.md`.

Static inventory: **557 C++ registrations across 34 suites**; **293 were not run** in this increment.

## Critical limitations

- This release does **not** implement a LittleFS `IFileSet` driver. No directory enumeration, file naming, file reads/writes, quarantine or recovery operation is now available on target.
- The pinned Arduino-ESP32 3.3.12 `fs::File` wrapper exposes no truncate method. Its `flush()` and `close()` return void, and the framework VFS implementation ignores underlying `fflush`/`fsync`/`fclose` errors. A driver must not claim durable append success solely from these wrapper calls.
- No journal, temp-file replacement protocol, quarantine transaction, rename behavior or power-cut recovery has been implemented or proven. C13 remains blocking.
- C3 remains blocking: there is no production firmware root or scheduler. No final-source ESP32 build/link, ELF inspection, SF2 run, flash, physical device or power-cut test was performed.
- E18 remains DELEGATED. No product decision, signed handoff rule or delegated decision changed. `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged.
