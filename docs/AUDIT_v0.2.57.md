# Audit v0.2.57 — LittleFS VFS adapter subset

Date: 2026-11-09  
Base: v0.2.56 archive SHA-256 `13b4cad80ac4dd641c5e6c8177a86be2a0d6928f1375b7b3bc4d822f1df3b7dc`  
Scope: `lib/littlefs_file_set/src/littlefs_file_set.*`, `test/test_littlefs_fileset/test_main.cpp`, and directly affected MonthSet behavior  
Result: **host-tested development subset only; not approved as production storage**

## Changes reviewed

- Dedicated PlatformIO library, kept separate from the broad ESP32 HAL so the native suite compiles only the adapter and its direct core dependencies.
- Mountpoint syntax validation; root must exist as a directory. The root path is normalized without a trailing slash before `lstat()`, so a host symlink used as the mount root is rejected.
- Missing `/log` is accepted as an empty set only when the mount root exists. The first new month creates `/log`.
- Directory enumeration accepts only canonical regular `YYYYMM.bin` files, rejects malformed entries/subdirectories/symlinked files, and orders month keys numerically.
- Range reads use bounded `pread()` calls. Appends accept nonempty whole 16-byte records, require existing file alignment, check `size_t` and `off_t` bounds, use `O_APPEND`, call `fsync()`, verify resulting size and read back the newly appended bytes.
- Empty month creation uses `O_CREAT | O_EXCL`, then `fsync()`, close-status checking and zero-size verification.
- `truncateFile()`, `quarantineCopy()` and `quarantineFrom()` explicitly refuse all operations.

## Test evidence

Exact final command:

```sh
pio test -e native -f 'test_littlefs_fileset' -f 'test_monthset'
```

Result: **34/34 passed**, exit 0 — `test_littlefs_fileset` 20/20 and `test_monthset` 14/14.

Coverage includes missing-vs-unmounted directory handling, first-month creation, numerical month ordering, malformed filenames/directories, root and entry symlinks, read ranges, whole-record append requirements, existing partial-file refusal, rollover/reopen through a fresh `MonthSet`/`LogStore`, and torn-tail fail-closed behavior with no mutation.

The corrected root-symlink test was test-first: it failed against the original `stat()` behavior. A follow-up `lstat()` patch still failed because the root path had a trailing slash; the final implementation removes that slash before `lstat()`. The final 34-case focused run passed after this change.

All filesystem tests run against a temporary **host POSIX filesystem**. They do not emulate flash erase/program behavior or LittleFS metadata commits. The test confirms the host implementation's visible calls and file contents only.

## Blockers and failure semantics

1. **Partial append is possible.** A write, sync, read-back or close error can occur after some bytes have been written. Returning `false` does not imply rollback. The caller must stop normal logging after any append failure; `LogStore` latches its storage fault.
2. **There is no recovery path in this adapter.** A torn tail is not modified; subsequent `LogStore::open()` fails closed because all destructive operations are disabled. This is safer than guessing, but it can leave the device unable to resume attendance logging until a reviewed recovery workflow exists.
3. **No transaction protects directory metadata.** `fsync()` is called on the new file, not on the parent directory. The file's name/length may not survive a power cut even when the call succeeds. Directory-entry durability and ordering are unverified.
4. **Read-back is not a power-loss test.** It verifies data visible through the same mounted filesystem after the write, not that flash commits survive power removal or that the next mount will reconstruct the same bytes.
5. **Descriptor cleanup is uncertain on a close error.** The inspected `joltwallet/esp_littlefs` v1.22.3 close path can return before removing its descriptor-cache entry when `lfs_file_close()` fails. Treat close failure as a fatal storage fault; do not blindly retry close. A controlled unmount/reboot recovery policy has not been implemented.
6. **No concurrency protection.** Every call must be serialized by one application owner. This class has no mutex, does not mount or unmount LittleFS, and is not instantiated by a production root.
7. **Target compatibility is unverified.** The source tree pins Espressif32 8.0.0 and the installed framework was inspected for the LittleFS VFS calls, but this increment did not compile/link the final ESP32 target. POSIX-host success does not prove the exact target toolchain provides compatible `lstat`, `pread`, `fsync`, `off_t` or VFS semantics.
8. **No fault injection on target.** No synthetic failures were injected into ESP32 mount, enumeration, metadata, open, short write, full filesystem, sync, read-back, close, or directory operations.

## Required before C13 can close

- Build/link the final ESP32 target against the pinned framework and exact LittleFS version.
- Integrate one mount owner; establish mount failure behavior, mountpoint identity and serialized access. Never format on mount failure.
- Add an error-injectable filesystem boundary and verify all failure returns, especially short/partial writes, fsync and close.
- Design quarantine and tail-repair as a recoverable transaction before enabling destructive methods. Preserve original bytes until the replacement is verified; prove restart/retry behavior at every transition.
- Verify parent-directory and file metadata durability for the exact LittleFS port.
- Perform controlled power-cut/brownout tests on the actual device at create/write/sync/close/recovery boundaries, comparing all file bytes, sequence continuity and reopened log state.
- Test near-full and full partitions, corrupted directory entries, descriptor exhaustion and repeated reboot/recovery.

## Scope not changed

- No production firmware root, scheduler, HTTP routes, web UI or enrollment service was added (C3 remains blocking).
- No sensor, page mapping, E18/U5, schedule-history or term decision was changed.
- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged.
- No ESP32 target build, ELF inspection, physical device, flash, NVS, power-cut, brownout, full native regression or SF2 suite was run.
