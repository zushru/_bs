# Release v0.2.85: unique ESP32 LittleFS mount ownership

Date: 2026-11-11  
Baseline: v0.2.84 working tree; HEAD `c60c5a3` with pre-existing v0.2.82/v0.2.83 dirty work preserved  
Status: development snapshot only. No production firmware, final ESP32 ELF, mounted VFS runtime, or hardware behavior is claimed.

## Increment

`Esp32LittleFsMountBackend` owns one ESP-IDF VFS mount registration and its `mounted_` / partition-label state. Its implicit copy and move operations allowed two C++ objects to represent the same registration. A copied object could call `unmountOwned()` against a registration still considered owned by the original object. The backend is now explicitly non-copyable and non-movable. This makes the registration owner identity-stable for its lifetime.

Four compile-time type-trait assertions cover copy construction, copy assignment, move construction and move assignment. No mount/unmount implementation, partition selection, formatting behavior, `IFileSet` API, persistent format, or recovery policy changed. The existing destructive maintenance methods remain disabled and fail closed.

## Test-first evidence

Red command:

```sh
pio test -e native -f 'test_littlefs_mount_owner'
```

The retained log `/tmp/att-v0.2.85-mount-ownership-red.log` showed all four new static assertions failing against the old header. PlatformIO stopped at compilation, before runtime tests; this is an intentional red reproduction, not a test pass.

Green command:

```sh
pio test -e native -f 'test_littlefs_mount_owner'
```

Result: **22/22 runtime cases passed**, exit 0. The four compile-time ownership assertions also compiled successfully. Log: `/tmp/att-v0.2.85-mount-ownership-green.log`.

Before the ownership patch, the three focused C13.3 storage suites passed **53/53** in one invocation:

```sh
pio test -e native -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset' -f 'test_littlefs_fileset'
```

Log: `/tmp/att-v0.2.85-c13.3-native.log`. Because the mount-owner test was then extended, its owning suite was rerun after the patch (22/22 above). The fileset and serialized-fileset implementations were not changed. This is focused host evidence, not full native N1 regression.

Static inventory remains **704 C++ registrations across 39 suites** and **270 SF2 Node registrations across 12 files**. The compile-time assertions do not add Unity runtime registrations. Full N1 and SF2 were not run.

## ESP32 target evidence

Command:

```sh
pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

Pinned environment: Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, Xtensa toolchain 14.2.0.

The target `main.cpp` compiled with the modified backend header. The complete `hal_esp32` static library compiled and archived successfully, including `esp32_littlefs_mount.cpp`, `esp32_i2c_bus_owner.cpp` and the other HAL translation units. `littlefs_file_set`, `core` and AS608 libraries also compiled and archived.

The full build was manually stopped while PlatformIO was compiling Arduino framework source `StreamString.cpp`, after a long serial framework rebuild. The retained log `/tmp/att-v0.2.85-c13.3-smoke-resume.log` has no final PlatformIO summary. No `firmware.elf` exists. This is **INCOMPLETE / NOT VERIFIED**, not a target build pass and not a compiler failure. The exact project PIO/SCons process tree was terminated; no build process remains.

No hardware-probe build, ELF inspection, flash/upload, mounted LittleFS transaction, VFS fault injection, format, recovery, or power-cut test was run. The adapter-link smoke target is inert and must never be flashed.

## Critical limits

- This closes only the C++ object-identity defect in the VFS mount backend. It does not make the backend a production root or prove actual mount/unmount behavior.
- The ESP32 HAL translation units compile, but final target linking and final ELF inspection remain unverified.
- No proof exists for directory-entry durability, file sync semantics, descriptor cleanup under ESP-IDF faults, real VFS error behavior, or power-loss recovery.
- D1–D11 remain delegated engineering policy; no destructive recovery transaction or journal is approved by this increment.
- Truncate/quarantine methods remain disabled. No automatic format, repair, or destructive operation was enabled.
- C13 remains blocking pending final target link, mounted-target fault validation, a proven commit/durability primitive, and physical interruption testing. C3 production-root integration remains blocked by the existing roadmap prerequisites.
