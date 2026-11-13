# Critical audit — v0.2.95 (2026-11-12)

## Yield

One test-only increment closes a missing composition regression between `LittleFsFileSet::addFile()` failure semantics, `MonthSet::append()`, and `LogStore` fault latching.

## Source review

- `MonthSet::append()` propagates a false `addFile()` result without attempting `appendFile()`.
- `LogStore::append()` latches its fault when storage append fails, preventing a blind second append on that instance.
- A new `LogStore::open()` performs a fresh read-only scan. The regression confirms that a visible empty month file is reconciled as a clean zero-record stream before another append is accepted.
- The test models `addFile()` returning false after inserting exactly one empty month entry. It does not model actual LittleFS persistence or prove that the directory entry survives reset.
- This behavior is distinct from `appendFile()` returning false: in the latter case bytes may be partial or complete, and replay/reconciliation must account for possible event payload bytes.

## Files changed

- `test/test_monthset/test_main.cpp`
- `README.md`, `RELEASE.md`, `RELEASE_v0.2.95.md`
- `docs/AUDIT_v0.2.95.md`, `docs/TESTS.md`, `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/WORKING_MEMORY.md`, `docs/CRITICAL.md`, `docs/DEVELOPMENT_ROADMAP.md`, `docs/UNVERIFIED.md`
- `MANIFEST.sha256`

## Verification

Focused native command: `pio test -e native -f 'test_monthset' -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'`.

Result: 100/100 passed. Exact output: `/tmp/att_v0295_c13_native.log`. Live and extracted manifest checks plus archive SHA verification are completed by the release batch.

## Critical limitations

This test is native host logic evidence only. It does not establish target VFS behavior, file or directory durability, crash consistency, a commit point, low-space behavior, power-cut safety, or production application wiring. Do not interpret this test as C13.3 completion.
