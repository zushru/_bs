# v0.2.82 critical implementation audit

Date: 2026-11-11

## Scope

This audit covers the reusable LittleFS mount owner, read-only record validation, non-default LogStore sequence bases, and cross-month roster semantic validation introduced/continued in v0.2.82. It does not claim production-root or mounted-device integration.

## Findings addressed

1. **Per-file sequence base.** A month-scoped LogStore starts at the next expected global sequence. Both LogStore replay and EventGroupAudit enforce that base, and append preflights sequence exhaustion before writing.
2. **Month ordering.** Directory metadata is captured once, sorted by canonical YYYYMM key, and checked for strict increase before files are opened in chronological order.
3. **Record integrity.** Each read-only file descriptor is checked against the metadata snapshot. LogStore replay verifies event CRC/type/sequence and refuses corrupt, torn or incomplete-group files without mutating bytes.
4. **Month boundary.** Each file gets its own EventGroupAudit. A MORE group left pending at the end of one file is rejected; a following month cannot complete it.
5. **Roster group and payload semantics.** One heap-allocated RosterStore plus RosterTransitionAudit are fed chronologically across month files. RosterStore historyComplete() rejects malformed names, invalid UTF-8, noncanonical post-NUL padding, parser errors, pending SET payloads and interval overflow. RosterTransitionAudit rejects unknown, duplicate, backdated or non-forward lifecycle actions.
6. **Configuration input.** The owner requires an explicit UTC offset and checks the ConfigStore range. The production caller must pass the validated persisted value; the smoke-only fixture uses 480. P10 remains signed PHT (UTC+8).

## Test evidence

- Test-first cross-month duplicate deactivation: failed before implementation because owner returned true for invalid history. The Unity output identified the exact assertion failure.
- Final focused runs: monthset 23/23; littlefs_mount_owner 22/22; littlefs_fileset 28/28; event_group_audit 20/20; roster_snapshot 26/26; littlefs_serialized_fileset 3/3. Each suite was run separately; total 122/122 unique registrations. See RELEASE_v0.2.82.md.
- Current inventory: 667 C++ RUN_TEST registrations across 37 suite directories; 545 C++ registrations were not run. SF2 has 270 registrations across 12 files; its current suite was not run (257/257 is historical). Full native N1 was not run.

## Critical review / remaining risks

- The owner currently owns a mount backend and serialized IFileSet, but **no production composition root calls it**. The inert adapter-link target constructs the objects only; it deliberately does not call begin().
- The scratch roster and transition audit are allocated dynamically when at least one month file exists. Allocation failure fails closed. Heap headroom and fragmentation under real ESP32 runtime have not been measured.
- The per-file event audit intentionally rejects event types for which this release has no semantic validator (including legacy EV_BOOT test records, EV_SETTING and EV_REPAIR). A future production writer must add a schema and tests before such types can be admitted.
- This is not a full domain audit of all attendance/coverage invariants. It verifies selected event/group fields and complete roster history needed by the current roster state model. It is not a cryptographic integrity mechanism.
- The read-only validation loop checks descriptor size against the earlier metadata snapshot. It assumes the owner serializes all storage access during begin; there is no cross-process writer coordination.
- The owner unmount failure is surfaced via cleanupFailed() and the state remains FAULT. It does not attempt to format, repair, truncate or quarantine.
- Native tests use POSIX files and fake mount backend. They do not verify ESP-IDF LittleFS VFS semantics, fsync/close behavior, directory durability, RTOS locking under contention, power loss or actual partition configuration.
- The new RosterStreamObserver deliberately ignores onReset() so roster state survives attachment to each month-scoped LogStore. This depends on files being sorted and the global sequence check succeeding before observer attachment; the current validator enforces both. Keep this ordering invariant if refactored.
- LogStore::lastSeq() on an unopened/faulted zero-based constructor can be misleading if queried directly, but the validator checks OpenInfo.ok and never exposes a failed log. Do not use lastSeq() as a health check outside the established open/audit protocol.

## Not run / blockers

No full native suite, SF2 suite, production app build, hardware-probe build, device flash, mounted LittleFS test, VFS fault injection, brownout/power-cut, physical sensor/display/AP/power-bank test or destructive recovery test was performed. The current esp32-adapter-link target is an inert compile/link smoke image and must never be flashed.

C13 remains blocking until the production storage owner is integrated and the pinned target VFS actual runtime/durability semantics are measured, with power-cut evidence. C3 remains blocking because there is no production main/scheduler/API. D1–D11 remain delegated, unchanged.
