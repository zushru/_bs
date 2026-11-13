# Audit v0.2.87 — startup validator close fault

Date: 2026-11-11  
Scope: reuse of the native-only descriptor-close fault hook in LittleFS read-only startup preflight.

## Finding / change

`LittleFsFileSet::validateRecordFiles()` opens each canonical month file and wraps the descriptor in `ReadOnlyFileStorage` while `LogStore::open()` and `EventGroupAudit` validate it. v0.2.86 routed the adapter's `pread()` through the native-only test hook, but the validator still called `close()` directly. The existing `LittleFsFileSetTestOps::closeFn` was already available and used by append fault tests, so the native validator close path could be exercised without introducing another callback or changing the production interface.

The implementation now uses a small conditional helper: native builds call the injected `closeFn` when supplied and otherwise call libc `close()`; `ESP_PLATFORM` builds call `close()` directly. Both the normal validation completion path and descriptor-metadata-mismatch cleanup use the helper. A new registered regression makes the injected callback close the POSIX descriptor, then report `EIO`; it requires validation failure and unchanged file size/bytes.

## Critical review

- No new syscall-table field or public production API was added. The existing callback/context lifetime contract remains unchanged.
- The test checks the error return from the validator's real read-only descriptor close path and independently checks the exact file bytes afterward.
- The callback closes the descriptor before returning an error. It does not simulate a close that leaves the descriptor open, nor does it prove ESP-IDF/VFS close semantics.
- `opendir`, `readdir`, `closedir`, `stat`, `open`, and `fstat` are not injected by this increment. This does not close the broader metadata/enumeration fault-coverage gap in U110.
- `ESP_PLATFORM` uses direct `close()` as before. The local compiler did not have the Xtensa toolchain or the actual pinned ESP-IDF headers, so the target branch was not compiled in this increment.

## Build evidence

- Host object build: all **57/57** production library translation units compiled with `g++ -std=gnu++11 -Wall -Wextra -c` using the four library include roots and existing test stubs; exit 0.
- Strict changed-source check: `g++ -std=gnu++11 -Wall -Wextra -Werror -c -Ilib/core/src -Ilib/littlefs_file_set/src lib/littlefs_file_set/src/littlefs_file_set.cpp -o /tmp/att_v087_host_objects/littlefs_file_set_strict.o`; exit 0.
- Host dependency link: **32 objects** (all `lib/core/src` and `lib/littlefs_file_set/src` translation units) linked with `g++ -std=gnu++11 -Wall -Wextra -pthread` and a dummy `main()`; exit 0. Not a product executable.
- Neither the native Unity suite nor any SF2 test was run. PlatformIO, Xtensa, target build, ELF, flash, VFS runtime and physical tests were unavailable or deliberately deferred.

## Disposition

The host source now exposes both read and close failures along the validator's real `IStorage` path to native tests. Both relevant validator regressions remain **unverified** until the owning `test_littlefs_fileset` suite is executed. C13.3 remains blocking. No delegated or signed decision was changed.
