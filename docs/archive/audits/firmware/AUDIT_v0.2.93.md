# v0.2.93 critical audit — LittleFS read-path fault seam

## Scope

This increment is a narrow C13.3 source/testability correction. It does not claim to complete C13.3, choose a recovery commit primitive, or enable destructive recovery.

## Finding and correction

Before this increment, LittleFsFileSet::readFile() bypassed the native-only LittleFsFileSetTestOps callbacks and called open(), pread(), and close() directly. The test operations table already declared open/read/close hooks, and the record validator used them, but ordinary file reads did not. Consequently, read I/O errors and EINTR retry behavior could not be deterministically exercised through the fileset.

The native branch now uses the existing helpers/callbacks for open and close and the existing optional pread callback, falling back to POSIX calls when no callback is supplied. The ESP32 branch remains direct syscalls under ESP_PLATFORM. No public production interface or on-disk format changed.

## Test-first evidence

Four regressions were registered before the implementation change. The red run reproduced all four failures. After implementation, the new tests passed. Existing tests that intentionally kept read/close faults active were adjusted to verify unchanged bytes through a fresh, uninjected reader; this prevents the verification read itself from being rejected by the same synthetic fault.

Final focused result: 73/73 across test_littlefs_fileset (47), test_littlefs_mount_owner (23), and test_littlefs_serialized_fileset (3).

## Target compatibility evidence

The pinned ESP32 inert compile/link target succeeded. The final ELF contains the affected LittleFsFileSet and mount-owner methods. The target build did not execute them, mount LittleFS, write a test file, inspect physical descriptors, inject real VFS errors, or cut power. No firmware was flashed.

ELF SHA-256: be804edcc9fc096f9c27ff3bb27994c9928eb295b5cf402ed982395e85d34405

## Residual review

- False append/read outcome: a failed operation can still leave bytes visible; callers must continue to reconcile before retry. This increment does not change that contract.
- Close behavior: the native injected close callback can model a close error after actually closing the host descriptor. It does not prove ESP-IDF descriptor/resource behavior after a close failure.
- Persistence: host fsync() and POSIX directory behavior do not prove LittleFS file or directory-entry durability.
- Capacity: no target near-full/full-partition or ENOSPC behavior was tested.
- Restart: no target remount/replay or interruption matrix was run.
- Recovery: D5 still accepts no commit primitive; D6 still forbids a journal before D5 is proven. Format, truncate, quarantine, repair and restore swap remain disabled.
- Composition: LittleFsStorageOwner remains a reusable component, not a production-root integration.
- Physical: no mount, flash, power cut, brownout, sensor, RTC, AP, or attendance runtime test was performed.

C13.3 remains BLOCKING. No product decision or signed/delegated rule changed.
