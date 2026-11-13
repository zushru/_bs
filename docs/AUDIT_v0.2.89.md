# Critical audit — v0.2.89

## Scope

This increment closes a fail-open metadata classification gap in `LittleFsFileSet::scan_()` and extends the existing native-only syscall injection seam to the ordinary scan/count path. It does not introduce a new product abstraction. On ESP32, the conditional branch retains direct calls to the real VFS/libc functions.

## Defect

Previously `scan_()` used `ensureLogDirectory_(false)` and, after any failure, repeated root and `/log` metadata lookups to decide whether the directory was merely absent. If the first `/log` stat failed with `EIO` and the second call succeeded, scan returned true with count zero. A transient storage error could therefore be represented to callers as valid empty storage.

## Corrected behavior

- Root metadata is read once; any error or non-directory root fails.
- `/log` metadata is read once; only `ENOENT` is treated as a valid empty log directory. `EIO`, permission failures and other errors fail closed.
- A present `/log` must be a directory.
- Month enumeration uses existing `opendir`/`readdir`/`closedir` wrappers on native host builds and direct calls under `ESP_PLATFORM`.
- Each month entry must parse and refer to a regular file; host stat injection reaches this path too.
- `count` and `nextKey` are initialized to zero and assigned only after successful enumeration and directory close. A partial pass followed by `readdir` or `closedir` failure cannot leak a partial count.

The host callbacks wrap real POSIX resources, not a fake directory model. Fault callbacks fail once; the synthetic `closedir` error is reported only after the real directory handle is closed, so this tests propagation rather than a persistent host resource leak.

## Test-first evidence

- Red: four scan fault cases initially failed because `scan_()` did not use the existing hooks. After routing the hooks, two additional transient-stat red cases failed because the repeated metadata lookup could convert the first error into empty success. The PlatformIO wrapper emitted signals after expected Unity assertion output; those runs are not treated as clean red exits.
- Green: `pio test -e native -f 'test_littlefs_fileset'` — 43/43 passed.
- Dependent green: `pio test -e native -f 'test_littlefs_mount_owner'` — 22/22 passed, exit 0.

The static C++ inventory is 725 registrations across 39 suites (the owning suite has 43). This is not full native N1; 270 SF2 registrations were not run.

## Target evidence boundary

`pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link` compiled the modified LittleFS translation unit and archived `liblittlefs_file_set.a` using the pinned Xtensa/ESP-IDF toolchain. It then rebuilt unrelated HAL objects. The build session was deliberately terminated before the full HAL archive/final application link to conserve phone resources. There is no `firmware.elf` or `firmware.bin`; this is not a complete target build or ELF verification.

## Adversarial review

- The corrected `ENOENT` classification is intentionally narrow. A permission/I/O/corruption error must not make an absent-data claim.
- The scan does not expose a count until `closedir` succeeds. This is correct for callers that use count to select month files.
- Host symlink rejection remains based on `lstat`; the ESP32 branch uses `stat` as required by the pinned newlib. This is a known platform semantic difference, not proven equivalent behavior.
- Directory callback tests establish host error propagation. They do not prove target LittleFS VFS error mapping, whether a failed real `closedir` releases resources, or whether metadata is durable after power loss.
- The test fixture's one-shot callback mutation is limited to test state and allows byte-preservation checks after a synthetic first failure. It does not alter production behavior.
- No destructive path was added or enabled. `truncateFile()`, quarantine, automatic format, repair and recovery remain disabled.
- No production composition root guarantees one mount owner/application serialization. C13.3 still blocks production storage use.

## Disposition

The ordinary host scan/count path now has deterministic fault coverage and the transient-stat false-empty defect is closed at source/native-test level. The ESP32 storage translation unit also compiles under the pinned target toolchain. Do not claim mounted-target VFS, durability, restart, or physical behavior. C13.3/U110 remain blocking; no signed or delegated decision was changed.
