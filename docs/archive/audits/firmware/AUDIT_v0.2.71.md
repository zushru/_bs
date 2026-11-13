# Critical audit — v0.2.71 LittleFsFileSet ESP32 compile/link compatibility

Date: 2026-11-10
Baseline: v0.2.70
Project root: /root/att

## Scope

This increment addresses one demonstrated target-compatibility failure in the existing LittleFsFileSet adapter. It does not implement storage repair, change the persistence format, add a production mount owner, or enable destructive recovery operations.

## Finding 1: the host adapter compiled, but the pinned ESP32 newlib did not expose lstat()

The first inert ESP32 smoke build failed in lib/littlefs_file_set/src/littlefs_file_set.cpp: the pinned ESP-IDF/newlib headers did not declare lstat(). The earlier host suite passed because the host POSIX environment provides it. The existing host regressions also reject a symlinked mount root and symlinked month entries, so replacing lstat() with stat() unconditionally would have weakened tested host behavior.

The adapter now routes path metadata checks through statFileSetEntry():
- Under ESP_PLATFORM, it calls stat(). The ESP LittleFS backend has regular-file/directory entry types and no symbolic-link entry type.
- On POSIX hosts, it continues to call lstat(), preserving no-follow rejection of symlink roots and month entries.

The target branch therefore compiles without changing host symlink semantics. This distinction is specific to the current pinned ESP LittleFS backend; do not generalize the target stat() behavior to arbitrary POSIX/VFS backends.

## Finding 2: actual target compile and final link now include LittleFsFileSet

The inert smoke application now:
- includes littlefs_file_set.h;
- retains member-function references for all nine public operations (countChecked, monthKeyChecked, fileSizeChecked, readFile, appendFile, addFile, truncateFile, quarantineCopy, quarantineFrom);
- constructs one LittleFsFileSet("/littlefs") object and checks its path-validity result without mounting or accessing the filesystem.

This ensures the target compiler sees the ESP-specific translation unit, and the final linker retains the intended public methods. It does not call the file methods or prove the VFS behavior.

## Test-first and build evidence

The owning host suite was run before and after the adapter portability change. The final post-change exact command was:

pio test -e native -f 'test_littlefs_fileset'

Result: **20/20 passed**, exit 0. This keeps the POSIX lstat() symlink regressions covered.

The initial target attempt failed at compile time because lstat() was not declared by the pinned target headers (exit 1, 1,220.24 seconds). After the narrow platform helper change, the final exact command was:

pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

Result: **SUCCESS**, exit 0, **1,522.09 seconds**.

Pinned target:
- Board: ESP32 Dev Module (esp32dev)
- Platform: Espressif32 8.0.0
- Arduino-ESP32: 3.3.12 (4.30312.261024)
- ESP-IDF: 5.5.5
- Xtensa toolchain: 14.2.0+20260121

Final PlatformIO size report:
- RAM: **67,356 / 327,680 bytes (20.6%)**
- Application flash: **934,976 / 1,310,720 bytes (71.3%)**

## Exact final-ELF inspection

Command:
tools/verify_v0.2.71.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"

Result: **exit 0**:
- 12/12 existing NVS wrapper call-routing checks passed (NVS_WRAPPER_ROUTING=PASS).
- 9/9 LittleFsFileSet public method symbols were found in the final ELF.

This verifies symbol retention and the exact NVS call paths inspected by the existing script. It is not a runtime NVS safety proof and does not close C34.

## Critical residuals — C13 remains blocking

- truncateFile(), quarantineCopy() and quarantineFrom() still return false. They were intentionally not enabled because a safe recovery protocol has not been designed and approved.
- No production mount owner or storage serialization boundary exists.
- No target LittleFS mount, file open/read/write, short-write, fsync, close, descriptor-leak, full-filesystem, VFS error-injection, brownout or power-cut test was performed.
- stat() target compilation relies on current ESP LittleFS semantics. No target runtime evidence was obtained.
- Partial append failure remains indeterminate. A torn tail may cause LogStore::open() to fail closed because repair is disabled.
- U112 durable request identity and exactly-once semantics remain unresolved; this increment does not make writes transactional or safe to blindly retry.
- C3/C6 production entry point and route integration remain absent. No full native/SF2 suite or hardware-probe build was run.

## Protected source-of-truth files

HANDOFF_FINAL.md and docs/DECISIONS.md were not changed. No product decision or delegated decision was altered.
