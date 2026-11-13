# Critical audit — v0.2.90 C13 host integration

## Scope and result

Only `test/test_littlefs_mount_owner/test_main.cpp` changed. The new case verifies the reusable object chain from `LittleFsStorageOwner` through `SerializedFileSet`, `MonthSet`, and `LogStore`, followed by constructing a new owner and replaying the stored event. The three C13 host suites passed 69/69. This is a POSIX-host regression with a fake mount backend, not target integration.

## Critical findings

1. **The owner is not the production root.** The inert adapter target retains owner/backend symbols but explicitly never calls `begin()`. No production `main`, scheduler, or sole storage writer composes this owner with the application. C3 remains a blocker.
2. **Per-call serialization is not transaction serialization.** `SerializedFileSet` acquires/releases its mutex for each `IFileSet` method. A higher-level `MonthSet` call makes multiple metadata and mutation calls; another independent caller could interleave between them. The handoff's single-writer application boundary must be established before concurrent production use. The new test is single-threaded and does not claim race safety.
3. **Target VFS behavior is unproven.** The mounted owner and fileset have not been exercised through ESP-IDF's LittleFS VFS. Host syscall hooks call POSIX functions; their synthetic close/closedir callbacks release actual POSIX handles before returning an injected error. That does not establish target close-resource behavior or target errno mapping.
4. **`fsync()` success is not a proven commit point.** The pinned installed component header is 1.22.3, but the implementation source was not available locally for this review. Even source inspection alone would not prove power-cut persistence. File contents, directory entries, rename, close and ftruncate semantics need target experiments and controlled interruption tests. Do not change D5: no primitive is accepted.
5. **No free-space behavior was measured.** There is no actual full/near-full filesystem test, ENOSPC behavior test on target, or proof that error paths leave descriptors and storage in a known state.
6. **Restart test scope is narrow.** The fresh owner is a new C++ object on the same host POSIX directory. It verifies startup validation/replay logic, not unmount/remount behavior, ESP32 reboot, filesystem recovery after reset, or persistence after power loss.
7. **Recovery remains prohibited.** `truncateFile()`, `quarantineCopy()`, and `quarantineFrom()` return false. `MonthSet::repair()` remains non-transactional and must not be used by production. No journal or commit point is designed or enabled. Automatic format remains forbidden.

## Required next evidence before C13.3 can close

- Compose one production LittleFS owner and one serialized application writer at the production root; do not expose parallel direct fileset/storage routes.
- On the exact ESP32 target and partition configuration, mount a dedicated non-destructive test filesystem and exercise stat/enumeration/read/write/fsync/close/unmount and every supported injected error boundary.
- Measure exact file and directory-entry behavior, descriptor lifecycle, ENOSPC/near-full behavior, and restart/replay after controlled interruption.
- Preserve source bytes and compare exact file inventory/content plus event/sequence/group/roster semantics after every interruption.
- Keep D5/D6 unchanged until a single commit primitive is proven; any journal/transaction requires a separately reviewed design and exhaustive interruption matrix.
- Physical power-cut/brownout tests are mandatory before durability claims. Do not flash the inert adapter-link image or run destructive storage tests on the attendance partition.

**Disposition: C13.3 remains blocking.** This increment strengthens host composition coverage only.
