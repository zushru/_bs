# Development increment v0.2.87 — startup validator close-fault injection

Date: 2026-11-11  
Base: uploaded `att_v0.2.86_2026-11-11-dev.tar.gz`  
Base archive SHA-256: `038d064c2adcdbeafa65f3b11fba4dc6e8f5de14de4f9b229488ebd6c353fc0a`

## Change

- Reused the existing native-only `LittleFsFileSetTestOps::closeFn` in the read-only descriptor close path used by `LittleFsFileSet::validateRecordFiles()`. The hook is also used when descriptor metadata does not match the earlier path snapshot and the descriptor must be released before returning failure.
- Added `test_record_validation_fails_closed_on_close_error_without_mutation`. A valid one-record month file is scanned and replay-validated; the injected close callback closes the POSIX descriptor, then returns `-1` with `errno=EIO`. The test requires `validateRecordFiles()` to return false and verifies file size and exact bytes remain unchanged.
- The ESP32 conditional branch remains a direct `close()` call. No production interface, storage format, mount behavior, recovery policy, or destructive operation changed.

## Build and verification status

**Status: host source build passed; tests and ESP32 target are pending.**

- `test_littlefs_fileset` now has 30 active registrations; the project contains 712 active C++ `RUN_TEST` registrations across 39 suites. The v0.2.86 read-fault test and this v0.2.87 close-fault test are both present but were not executed, as requested.
- All 57 production `lib/**/*.cpp` translation units compiled with host `g++ -std=gnu++11 -Wall -Wextra -c`, using project library include directories and the existing test stubs: **57/57**, exit 0. The host build does not define `ESP_PLATFORM`; ESP-IDF-specific conditional branches are not validated.
- Strict changed-source command: `g++ -std=gnu++11 -Wall -Wextra -Werror -c -Ilib/core/src -Ilib/littlefs_file_set/src lib/littlefs_file_set/src/littlefs_file_set.cpp -o /tmp/att_v087_host_objects/littlefs_file_set_strict.o` — exit 0.
- Host link command: `g++ -std=gnu++11 -Wall -Wextra -pthread /tmp/att_v086_host_link_main.cpp $(find /tmp/att_v087_host_objects -type f \( -name 'lib_core_src_*.o' -o -name 'lib_littlefs_file_set_src_*.o' \) | sort) -o /tmp/att_v087_storage_link_probe` — **32 objects, exit 0**. This verifies the host storage dependency closure only; it is not a product executable.
- `pio`, `platformio`, `xtensa-esp32-elf-g++`, `arduino-cli` and `idf.py` were not available in the local environment. No PlatformIO test, ESP32 build, ELF inspection, SF2 test, hardware-probe build, flash, mounted VFS test, or physical test was run.

## Critical review and remaining uncertainty

- The regression exercises the validator's actual descriptor-close result path through the host-only callback; it does not inject a false return directly into `validateRecordFiles()`.
- The synthetic callback closes the POSIX descriptor successfully before reporting `EIO`. It checks fail-closed propagation and byte preservation, but does not model a failed close that leaves a target descriptor/resource open.
- The target branch still calls the real `close()` directly. Actual ESP-IDF LittleFS close-error behavior, descriptor lifecycle, and error injection remain unverified.
- `stat`, `opendir`, `readdir`, `closedir`, `open` and `fstat` remain direct system calls. Their target failure behavior is not covered by this increment.
- C13.3 remains incomplete: no production root wires the storage owner; no mounted-target VFS fault tests, durable commit primitive proof, recovery journal/transaction, restart replay, near-full/full partition validation, or physical power-cut/brownout evidence exists. `truncateFile()`, `quarantineCopy()` and `quarantineFrom()` remain disabled. D1–D11 are unchanged.

## Next verification

When RDC/PlatformIO is available, run the owning suite first:

```sh
pio test -e native -f 'test_littlefs_fileset'
```

Do not represent the current host object build as a target build or test pass. Never flash the inert `esp32-adapter-link` target.
