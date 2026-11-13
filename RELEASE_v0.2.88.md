# v0.2.88 — read-only validator metadata and enumeration fault injection

**Status: development-only. Native tests were not executed; ESP32 target build remains unverified.**

Baseline archive: `att_v0.2.87_2026-11-11-dev.tar.gz`  
Baseline SHA-256: `36377663b8b73d54927956a880a1e4f2d07bac18ea3f82638389174fec8f0342`  
Active worktree: `/mnt/data/att_v0.2.88_work` (archive has no Git metadata)  
Protected files: `HANDOFF_FINAL.md` and `docs/DECISIONS.md` were not edited.

## Change

Extends the native-only `LittleFsFileSetTestOps` seam used by `LittleFsFileSet::validateRecordFiles()` with callbacks for path metadata (`statFn`, backed by POSIX `lstat()`), read-only file open (`openFn`), descriptor metadata (`fstatFn`), and directory open/iteration/close (`opendirFn`, `readdirFn`, `closedirFn`). The directory callbacks wrap actual POSIX `DIR*` handles; no fake directory implementation is introduced.

The hooks are deliberately scoped to the read-only validator. `scan_()` and ordinary `IFileSet` metadata/enumeration paths still call the OS directly. Under `ESP_PLATFORM`, the code continues to call the target `stat()`, `opendir()`, `readdir()`, `closedir()`, `open()` and `fstat()` directly. No product API, storage format, mount ownership, recovery policy, or destructive operation changed.

## Added regressions

Seven test registrations were added to `test/test_littlefs_fileset/test_main.cpp`:

- log-directory `stat` returns `EIO` and is not mistaken for a valid empty directory;
- month-file `stat` returns `EIO`;
- month-file `open` returns `EIO`;
- descriptor `fstat` returns `EIO` and the opened descriptor is closed;
- `opendir` returns `EIO`;
- `readdir` returns `EIO` and the directory handle is closed;
- `closedir` closes the real handle and then reports `EIO`.

The byte-preservation regressions compare the canonical record bytes after the failure. **All seven new tests remain unexecuted.** The v0.2.86 read-fault and v0.2.87 close-fault regressions also remain unexecuted. No red/green result is claimed.

## Build evidence

- All **57/57** production library translation units compiled with host `g++ -std=gnu++11 -Wall -Wextra -c`, using the four project library include roots and five existing test-stub include roots; exit 0, no diagnostics.
- The changed `littlefs_file_set.cpp` host branch compiled with `-Wall -Wextra -Werror`; exit 0.
- The `ESP_PLATFORM` preprocessor branch also compiled with host `g++ -DESP_PLATFORM -Wall -Wextra -Werror`; exit 0. This is a conditional-source syntax check using Linux headers/compiler, **not** an Xtensa, ESP-IDF, Arduino-ESP32 or firmware target build.
- The owning test translation unit compiled with `-Werror` against a temporary Unity macro shim. This checks C++ syntax only; it is not the real Unity header/runtime and did not execute assertions.
- A **32-object** core + LittleFS host dependency closure linked with a dummy `main()`; exit 0. This is not a firmware executable.

`pio`, `platformio` and the Xtensa compiler are unavailable in this local environment. No native test, SF2 test, ESP32 target build, ELF inspection, hardware probe, flash, mounted LittleFS runtime, or physical test was performed.

Source registration inventory: **719 C++ `RUN_TEST` registrations across 39 suites**; `test_littlefs_fileset` now contains **37** registrations. Registration count is not execution evidence.

## Critical review and remaining uncertainty

- The callbacks exercise error propagation through the validator's native path. They do not prove that the pinned ESP-IDF LittleFS VFS returns the same errors or has the same descriptor/directory lifetime semantics.
- The injected `closeFn` and `closedirFn` close the host resource successfully before reporting synthetic `EIO`. They prove that the validator rejects a reported close error; they do not prove how a genuinely failed target close behaves or whether a resource remains open.
- The syscall seam does not cover `scan_()` or the regular `countChecked()` metadata path. Those calls remain direct and still lack deterministic host metadata/enumeration fault injection.
- No `opendir`/`readdir`/`closedir` error test has run. Test-fixture compilation does not establish runtime assertions.
- C13.3 remains blocking: there is no production root using the mount/storage owner, mounted-target VFS fault evidence, proven durable commit point, restart-idempotent journal/transaction, or power-cut/brownout evidence. `truncateFile()`, `quarantineCopy()` and `quarantineFrom()` remain disabled. D1–D11 and signed decisions remain unchanged.

## Next verification

When RDC/PlatformIO is available, run the owning suite first:

```sh
pio test -e native -f 'test_littlefs_fileset'
```

This `-f` filter selects the suite directory, not an individual Unity case. Then build only the affected ESP32 smoke target using the project's pinned configuration. Never flash the inert `esp32-adapter-link` image. Do not report the host branch syntax check as a target build.
