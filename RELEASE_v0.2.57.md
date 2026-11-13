# Release v0.2.57 — non-destructive LittleFS VFS adapter subset

Date: 2026-11-09  
Base: complete v0.2.56 archive extraction  
Base archive SHA-256: `13b4cad80ac4dd641c5e6c8177a86be2a0d6928f1375b7b3bc4d822f1df3b7dc`  
Project root during development: `/root/att`  
Release status: development snapshot; C3 and C13 remain blocking

## Scope

1. Added `lib/littlefs_file_set/src/littlefs_file_set.*` as a dedicated POSIX/VFS-backed implementation of the non-destructive `IFileSet` subset. It assumes the application has already mounted LittleFS at the supplied mountpoint.
2. Added checked enumeration and metadata for canonical `/log/YYYYMM.bin` entries, numeric month ordering, ranged reads, 16-byte-aligned appends, and creation of a new empty month file. It rejects malformed names, unexpected subdirectories, symlinked month entries and symlinked mount roots, plus an absent/unmounted root. An absent `/log` directory is a valid empty store.
3. `appendFile()` validates the pre-existing file size and `off_t` bounds, writes in bounded chunks, calls `fsync()`, checks the resulting size and reads the new bytes back before returning success. This detects many visible I/O errors but does not prove persistence through power loss.
4. `truncateFile()`, `quarantineCopy()` and `quarantineFrom()` deliberately return false. No destructive recovery or data movement is enabled.
5. The caller owns the mount and must serialize all operations. This class has no mutex, mount lifecycle, singleton guard, or production caller.

## Verification

Exact command:

```sh
pio test -e native -f 'test_littlefs_fileset' -f 'test_monthset'
```

Result: **34/34 passed**, exit 0: `test_littlefs_fileset` 20/20 and `test_monthset` 14/14. The adapter tests use a temporary host POSIX filesystem; they are not LittleFS emulation or target VFS tests.

Source inventory: **577 C++ registrations across 35 native suites**; **543 were not run** in this increment.

## Critical limitations

- **C13 remains blocking.** No ESP32 target build, actual LittleFS mount, on-device error injection, power-cut test, journal, atomic replacement protocol, truncate or quarantine exists. The source tree pins Espressif32 8.0.0; the installed framework's `esp_littlefs` v1.22.3 VFS implementation was reviewed for `pread()`/`fsync()`/`close()` error handling, but compile/link and runtime compatibility were not tested. Source: https://github.com/joltwallet/esp_littlefs/blob/v1.22.3/src/esp_littlefs.c.
- An append that fails after writing some bytes can leave a partial or otherwise uncertain tail. The adapter returns false; `LogStore` latches its storage fault. Since truncation/quarantine are disabled, a subsequent boot cannot repair the tail and fails closed. The regression test verifies the host file is unchanged by the attempted boot recovery.
- The reviewed v1.22.3 VFS close path can retain its descriptor-cache entry when `lfs_file_close()` itself fails. Treat close failure as fatal; do not blindly retry close.
- File `fsync()` plus read-back is not evidence of flash commit survival. Directory entry durability, metadata ordering, full-filesystem behavior, flash wear and power-cut outcomes remain unverified.
- C3 remains blocking: no production boot/scheduler or storage owner exists. The adapter is not instantiated in a product firmware root. No ESP32 target build, ELF inspection, flash, NVS operation, physical device, full native regression or SF2 test was run.
- No signed or delegated decision changed. `HANDOFF_FINAL.md` and `docs/DECISIONS.md` remain unchanged.
