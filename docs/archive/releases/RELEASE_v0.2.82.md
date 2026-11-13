# Release v0.2.82: LittleFS read-only startup owner and cross-month roster audit

Date: 2026-11-11  
Baseline: v0.2.81  
Status: development snapshot; focused native storage/roster suites; ESP32 inert compile/link smoke result recorded below. Not production firmware and not hardware-verified.

## Increment

### Reusable LittleFS startup owner

Adds a mount owner that mounts without formatting, validates the expected VFS root and canonical month-file inventory, checks 16-byte record alignment, then performs read-only CRC/type/sequence/group validation before exposing the serialized IFileSet. Failed validation keeps storage unavailable and attempts to unmount only the mount owned by this backend. Destructive recovery operations remain disabled.

### Global event sequence and roster-history semantics

LogStore and EventGroupAudit now accept an explicit first sequence so each month file can be replayed with a continuous global sequence. Startup validation orders month files chronologically and validates each file independently for CRC, sequence and commit-group completeness; a commit group cannot cross a file boundary.

A single heap-allocated RosterStore and RosterTransitionAudit are carried across month-scoped LogStores. This catches invalid roster transitions spanning month files, invalid roster SET payload names, malformed UTF-8 and nonzero bytes after the first NUL. The configured UTC offset is a required owner argument and is range-checked (-720..+840 minutes); callers must pass the validated persisted configuration. P10 remains signed PHT (UTC+8 / 480 minutes).

### Test-first evidence

The new cross-month duplicate-deactivation regression failed against the earlier owner because startup accepted the invalid history. After implementation, the owning suite and directly affected suites passed. The API change requiring an explicit UTC offset was propagated to all test and smoke call sites.

## Files changed

- `lib/core/src/log_store.cpp`
- `lib/core/src/log_store.h`
- `lib/core/src/event_group_audit.cpp`
- `lib/core/src/event_group_audit.h`
- `lib/littlefs_file_set/src/littlefs_file_set.cpp`
- `lib/littlefs_file_set/src/littlefs_file_set.h`
- `lib/littlefs_file_set/src/littlefs_mount_owner.cpp`
- `lib/littlefs_file_set/src/littlefs_mount_owner.h`
- `lib/littlefs_file_set/src/littlefs_serialized_fileset.cpp/.h`
- `lib/hal_esp32/src/esp32_littlefs_mount.cpp/.h`
- `target/esp32_adapter_link/main.cpp`
- `target/esp32_adapter_link/README.md`
- `test/test_monthset/test_main.cpp`
- `test/test_event_group_audit/test_main.cpp`
- `test/test_littlefs_fileset/test_main.cpp`
- `test/test_littlefs_mount_owner/test_main.cpp`
- `test/test_littlefs_serialized_fileset/test_main.cpp`
- release/audit/README/working-memory documentation and manifest

## Verification

Focused owning/dependent commands, executed as separate runs:

- `pio test -e native -f 'test_monthset'` — **23/23 passed**, exit 0 (LogStore non-default sequence base and sequence exhaustion).
- `pio test -e native -f 'test_littlefs_mount_owner'` — **22/22 passed**, exit 0.
- `pio test -e native -f 'test_littlefs_fileset'` — **28/28 passed**, exit 0.
- `pio test -e native -f 'test_event_group_audit'` — **20/20 passed**, exit 0.
- `pio test -e native -f 'test_roster_snapshot'` — **26/26 passed**, exit 0.
- `pio test -e native -f 'test_littlefs_serialized_fileset'` — **3/3 passed**, exit 0. Log: `/tmp/test_littlefs_serialized_fileset_green.log`.

The six results are separate focused runs, not one combined invocation. They total **122/122 unique registrations**. The current inventory is 667 C++ registrations across 37 suites; the other **545** C++ registrations were not run. The current SF2 inventory is 270 Node registrations across 12 files (present in the v0.2.81 baseline); the SF2 suite was not run. The last recorded 257/257 SF2 pass is historical and does not cover the later 13 registrations. Full native N1 and SF2 remain deferred.

ESP32 inert smoke target:

- Exact command: `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link` — **SUCCESS**, exit 0, 2,963.63 seconds. Build log: `/tmp/esp32_v082_build.log`; exit marker: `/tmp/esp32_v082_build.exit`.
- RAM: **67,684/327,680 bytes (20.7%)**. Application flash: **984,168/1,310,720 bytes (75.1%)**, leaving 326,552 bytes.
- Exact ELF inspection: `./tools/verify_v0.2.82.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` — **PASS**, exit 0. The verifier passed 12 NVS wrapper-routing checks, 6 LittleFS owner/backend/serialization symbol checks, and 7 NVS/config/AP/PIN symbol-retention checks. Log: `/tmp/esp32_v082_elf_verify.log`.
- The subsequent native PlatformIO test run cleaned the shared `.pio/build/esp32-adapter-link` output before its ELF/BIN hashes were captured; no image hash is recorded. This does not alter the successful build or inspection result.
- The target is inert: it never calls the storage owner's `begin()`, does not mount/read/write the partition, and must never be flashed.

## Critical boundaries and not run

- No production firmware root/main/scheduler exists. The new owner is reusable infrastructure; normal device boot does not yet instantiate or call it.
- No mounted-target LittleFS runtime, ESP-IDF VFS fault injection, file/directory durability or physical power-cut/brownout test was run.
- No device flash or physical sensor/OLED/RTC/AP/power-bank test was run.
- No auto-format, repair, truncate, quarantine, journal, destructive recovery, partition change or event-format change was introduced.
- Unsupported event types without a current semantic validator remain fail-closed.
- The inert smoke target is not production firmware. Never flash it.

C13 remains blocking: mounted-target durability and power-loss behavior are unverified; no recovery commit primitive or journal is approved; destructive recovery remains disabled. D1–D11 remain delegated engineering choices, not signed product rules.
