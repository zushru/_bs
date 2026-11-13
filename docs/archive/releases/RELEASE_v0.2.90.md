# v0.2.90 — C13 host integration checkpoint

**Status: development-only. C13.3 remains BLOCKING and is not complete.** This increment adds host integration evidence; it does not establish pinned-target persistence or physical durability.

## Baseline and integrity

- Root: `/root/att`; baseline README marker `v0.2.89 (2026-11-11)`.
- The baseline `MANIFEST.sha256` had 437 entries and passed `sha256sum -c` before edits. The tree has no Git metadata.
- Protected files were not changed: `HANDOFF_FINAL.md` SHA-256 `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`; `docs/DECISIONS.md` SHA-256 `52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3`.

## Change

- Added `test_mounted_owner_to_monthset_logstore_reopen` in `test/test_littlefs_mount_owner/test_main.cpp`.
- The test obtains the `SerializedFileSet` exposed by a ready `LittleFsStorageOwner`, builds `MonthSet` and `LogStore` over it, opens an empty stream, appends one EV_SCAN event, then creates a new backend/owner instance and verifies read-only preflight and LogStore replay of the event from the same POSIX directory.
- Production implementation and interfaces were not changed. No signed or delegated decision was changed. D1–D11 remain delegated; format, truncate, quarantine, and repair remain disabled.

## Exact verification

Command:

```sh
pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'
```

Result: **69/69 passed, exit 0**, runtime 60.31 s. Suite totals:

| Suite | Passed |
|---|---:|
| `test_littlefs_fileset` | 43/43 |
| `test_littlefs_mount_owner` | 23/23 |
| `test_littlefs_serialized_fileset` | 3/3 |

The new owner-to-log test also passed independently in `test_littlefs_mount_owner` (23/23, exit 0). Current source inventory: **726 C++ `RUN_TEST` registrations across 39 suites**; 657 registrations outside these three suites were not run. Full native N1 and the SF2 Node suite were not run.

## Evidence limits and unresolved C13.3 gates

- Tests use POSIX temporary directories and `FakeMountBackend`. They do not mount ESP-IDF LittleFS, inject errors through the actual target VFS, or demonstrate LittleFS metadata/file-data persistence.
- The pinned installed `esp_littlefs.h` declares LittleFS **1.22.3**, the no-format registration fields, mount/unmount APIs, and `ESP_LITTLEFS_ENABLE_FTRUNCATE` conditional on `CONFIG_VFS_SUPPORT_DIR`. The component implementation source was not present in the installed package tree during inspection. Exact sync/close/rename/ftruncate semantics and durability remain **[UNVERIFIED]**.
- The existing `target/esp32_adapter_link` is intentionally inert: it retains symbols but does not call `LittleFsStorageOwner::begin()` or mount/access a filesystem. It must never be flashed. No ESP32 target build/ELF was run in v0.2.90 because only a native test changed. The prior v0.2.89 target attempt compiled/archived the modified storage TU but stopped before final link; it produced no ELF.
- No mounted-target fault injection for mount/enumeration/stat/read/write/sync/close; no low-space/full-volume run; no descriptor-lifecycle measurement; no directory-entry/file-data durability test; no reboot/restart interruption matrix; and no physical power-cut/brownout test was performed.
- No production firmware root/scheduler consumes the mount owner. The storage mutex serializes each `IFileSet` call; it does not make a multi-call `MonthSet`/`LogStore` transaction atomic against a second independent caller. The product's one-writer application owner remains a C3 integration requirement.
- No durable commit primitive or journal is selected. No automatic recovery, truncate, quarantine, repair, or formatting is authorized. A false append remains indeterminate and anomalous startup remains fail-closed.

## Deliberately not run

- `pio test -e native` (full N1), SF2 Node suite, ESP32 build/link/ELF inspection, hardware-probe build, flash, mounted VFS operations, NVS, destructive storage operations, full/near-full storage, physical reboot, power-cut, brownout, or soak tests.

## Disposition

C13.3 is **partially advanced, not closed**. Completion requires production-owner adoption at the proper application boundary, non-destructive mounted testing on the pinned ESP32 LittleFS/VFS, exact error and space/descriptor testing, restart/interruption evidence, and physical power-cut/brownout validation before any durability claim. Do not infer these from host tests or an inert target link.
