# ATT ESP32 hardware probe — C13.3 VFS check

This directory builds a diagnostic-only ESP32 image. It is not attendance firmware, a production root, or an update candidate.

## Build

From the repository root:

```sh
pio run -c platformio.esp32-probe.ini -e esp32-hardware-probe -j 2
```

Review the exact source and target configuration before any separately authorized diagnostic flash. Do not flash while another device test/soak is active. Never flash `esp32-adapter-link`; that target is inert compile/link smoke only.

## Partition-table flashing warning

The current probe environment in platformio.esp32-probe.ini does not set
board_build.partitions to target/esp32_attendance/partitions_att.csv. Its
build logs show a partitions.bin is generated, but this repository does not
establish that artifact as the signed attendance table. Do not flash the
probe partitions.bin or a full bundle containing it to the device. This
probe build is not a partition-table migration image.

The probe VFS routine creates, writes, reads, renames and removes an
ephemeral test path when the signed geometry guard passes. A matching
partition map is not, by itself, authorization to run those filesystem
operations. Keep the current device on its existing firmware while the
partition discrepancy is unresolved.

## C13.3 behavior

After the guarded NVS startup check, the probe inspects the data partition named `spiffs`. It compares the observed address and size with the signed handoff layout (`0x190000`, `0x270000`). A mismatch is fail-closed: it logs `LFS_OWNER=SKIP` and does not mount or access the filesystem. The recorded 2026-11-12 device diagnostic reported 0x290000 / 0x160000; only this SPIFFS pair is retained in the repository, not the complete table or raw capture. The probe rejects the pair, and the discrepancy must be reconciled from a full read-only device table dump before any filesystem exercise.

Only when the partition layout matches does the probe:

1. Mount through `Esp32LittleFsMountBackend` with `format_if_mount_failed=false` and `LittleFsStorageOwner` read-only structural/semantic preflight.
2. Create one unique `.att_c13_vfs_<run-id>` directory under `/littlefs` using exclusive `mkdir` semantics.
3. Write a deterministic 512-byte payload, retrying short writes and `EINTR`.
4. Call file `fsync`, `fstat`, and exact `pread` readback; close the descriptor.
5. Rename the file, enumerate the directory, reopen and verify the bytes again.
6. Unlink the test file, remove the unique directory, and attempt directory `fsync` at each relevant metadata boundary.
7. Unmount only the mount owned by this probe.

A directory-sync result of `UNSUPPORTED` is reported separately and is not evidence of durable directory metadata. Unexpected sync errors fail the VFS probe. A failed close is not retried because the descriptor state after close failure is not safe to assume. If any file or directory descriptor close fails, the probe does not unregister the LittleFS VFS; it logs `LFS_UNMOUNT=SKIP` and leaves the mount registered until reboot.

## Scope and limits

- No automatic format, erase, truncate, quarantine, repair, low-space fill, power-cut, brownout or template operation is performed.
- The probe removes only paths it successfully created during the current run. If power is lost before cleanup, a `.att_c13_vfs_<run-id>` directory can remain. It is not automatically removed on a later boot because ownership cannot be inferred safely.
- A successful POSIX/VFS sequence is not proof of crash consistency, LittleFS directory durability, production commit semantics or power-loss recovery.
- This target is a hardware diagnostic, not production attendance firmware.
- Do not alter the signed partition table to make this probe pass. Resolve any observed partition-layout discrepancy through the project's normal authority/review process first.

## Evidence state on 2026-11-12

The probe helper's host POSIX suite passes 3/3, and all four host C13 suites pass 79/79 normally and under ASan/UBSan. This is host-only evidence. The ESP32 target build was not run because PlatformIO is unavailable in the execution environment; no target runtime or physical test is claimed.
