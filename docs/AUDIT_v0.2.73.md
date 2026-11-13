# Critical audit — v0.2.73 C13.2 proposal

## Baseline and change type

- Root: /root/att
- Baseline archive: /storage/emulated/0/Download/att_v0.2.72_2026-11-10.tar.gz
- Baseline SHA-256: e3aa89d8765a6f4937e41d8d722e0c3b77667533fb286f75d964c33c71439d2d
- Baseline sidecar, manifest and independent extraction comparison passed.
- v0.2.73 is documentation-only. No source, test, API, persistent format, or runtime code changed.

## Source inspected

- lib/core/src/fileset.h
- lib/core/src/istorage.h
- lib/core/src/log_store.h and log_store.cpp
- lib/core/src/month_set.h and month_set.cpp
- lib/littlefs_file_set/src/littlefs_file_set.h and littlefs_file_set.cpp
- lib/core/src/event_group_audit.h and event_group_audit.cpp
- lib/core/src/restore_replay.h and restore_replay.cpp
- lib/core/src/backup_format.cpp
- test/test_monthset/test_main.cpp
- test/test_coreb/test_main.cpp
- test/test_littlefs_fileset/test_main.cpp
- docs/SPECS/month_set.md
- pinned Arduino LittleFS wrapper source/header and pinned esp_littlefs.h

## Findings and risk

### F1 — automatic suffix truncation is not justified by the evidence

LogStore::open() accepts a contiguous decoded prefix. If it finds an invalid record and no later decodable record, it treats the rest as a suffix, backs up over a trailing FL_MORE chain, and asks IStorage to truncate. The algorithm cannot distinguish a partial final write from a complete record that was previously committed and later suffered CRC/sequence damage.

Current LittleFsFileSet returns false for truncateFile(), so this particular backend fails closed. A future or fake IStorage that permits truncation can silently discard the invalid EOF record. This backend-specific fail-closed behavior must not be mistaken for a safe core algorithm.

### F2 — MonthSet::repair() has no transaction boundary

The existing sequence copies the affected file, moves subsequent files, and truncates the canonical file. There is no journal, operation ID, verified copy equality, preflight capacity proof, single authority switch, durable commit marker, restart recognition or rollback. Any interruption between steps can leave ambiguous canonical/quarantine state.

The existing test_partial_repair_is_repeatable fixture does not inject interruption inside copy, move or truncate; it preloads fake quarantine state and then invokes repair. It does not prove crash-safe idempotence.

### F3 — semantic validation is incomplete as a generic recovery gate

EventGroupAudit checks selected known group forms but rejects currently unsupported event types (including EV_BOOT, EV_SETTING and EV_REPAIR in its default handling). RestoreReplayAudit has stronger staged validation but is not a live storage swap or a production startup owner. CRC-valid records alone are not sufficient to authorize writes/exports.

### F4 — target link evidence is not VFS evidence

v0.2.72's inert target compiled and linked LittleFsFileSet and retained its methods in the ELF. It did not mount LittleFS or invoke file operations. POSIX write/fsync/readback/close behavior does not establish the pinned target's rename, ftruncate, directory-entry persistence, descriptor-close, power-loss or recovery guarantees.

### F5 — mount failure must not become implicit format authorization

The pinned Arduino LittleFS wrapper defaults formatOnFail to false, but can call format() after an ESP_FAIL mount result if the owner passes true. A production mount owner must not silently enable this behavior for attendance storage.

## Proposal disposition

docs/SPECS/littlefs_recovery_protocol.md is deliberately marked **PROPOSAL ONLY — NOT APPROVED FOR IMPLEMENTATION**. It describes a conservative read-only scan, recovery-required mode, preserving/staging originals, a verified authority switch, restart states, fault matrix and target/physical test requirements. It does not select a commit primitive or on-disk journal format.

The existing docs/SPECS/month_set.md status remains DELEGATED. EV_REPAIR remains explicitly unapproved. No destructive method was enabled.

## Verification

- Documentation/source inspection only.
- No PlatformIO native suite, no SF2 Node suite, no syntax check, no target build, no ELF inspection, no flash and no physical-device test.
- Targeted release verifier and archive integrity checks are separate documentation/release integrity checks; they are not functional tests.

## Required next approval

The user must review D1–D11 in the proposal, especially the treatment of complete invalid EOF records and incomplete FL_MORE groups, retention, journal format, commit primitive, low-space behavior, administrative authority, EV_REPAIR meaning, backup scope and formatting policy. No implementation should begin until those decisions are explicit. A passing host test suite cannot waive pinned-target VFS and power-cut testing.
