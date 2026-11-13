# SPEC: fileset + month_set (lib/core/src/fileset.h, month_set.h/.cpp)

Status: **DELEGATED design; destructive methods remain unavailable.** C13.2 D1–D11 engineering choices were delegated to assistant judgment on 2026-11-10 and are recorded in [littlefs_recovery_protocol.md](littlefs_recovery_protocol.md). They do not authorize a destructive recovery transaction. v0.2.77 makes LogStore::open() read-only and fail-closed for anomalous tails; see the startup boundary below. v0.2.57 adds the non-destructive POSIX/VFS adapter subset; v0.2.72 adds append fault injection. The v0.2.77 inert target build is compile/link evidence only. No LittleFS mount, target VFS file operation, target fault injection or physical power-cut test has been performed.

## IFileSet contract

Every driver must implement checked metadata queries; value-only count/key/size methods are intentionally not accepted because zero is valid data and cannot encode I/O failure.

- `countChecked(out)` returns the entry count only when directory enumeration succeeds.
- `monthKeyChecked(i, out)` returns the parsed month key only when entry lookup/name validation succeeds. Keys must be strictly increasing and use the canonical six-digit `YYYYMM` format.
- `fileSizeChecked(i, out)` returns the real file length only when stat/size succeeds.
- `readFile(i, off, buf, n)` must reject invalid ranges and return false on any read failure.
- `appendFile(i, buf, n)` returns true only when the complete write passes the driver's durability criterion. False is indeterminate, not rollback: zero, partial, or complete bytes may already be present. Callers must fault the log and must not blindly retry without reopening/reconciling.
- `addFile(key)` creates one new empty canonical month file; caller verifies count, key and size afterward.
- `truncateFile`, `quarantineCopy` and `quarantineFrom` are destructive/recovery methods. Do not implement or expose them until their transaction and interruption behavior is defined and tested.

`IStorage::sizeChecked(out)` is also mandatory. The non-virtual `size()` convenience helper returns `SIZE_MAX` on query failure; boot/replay code must call `sizeChecked()` directly and inspect its result.

## MonthSet : IStorage

Presents month files as one logical byte stream, oldest first. `LogStore` sits above it.

| Operation | Rule |
|---|---|
| setMonth(key) | Caller sets from trusted time before appending. Month 0 = unset |
| sizeChecked(out) | Sum checked file sizes; fail on metadata error or size_t overflow |
| read | Uses checked count/size metadata; returns false on metadata, range, or file-read errors |
| append | Refused if month unset, input null/empty, metadata query fails, or a rollover would leave a non-16-byte-aligned prior file. A newly added file must be visible with the expected key, zero size and exactly one added entry before append |
| truncate(n) | Only in the last non-empty file; refuse if later non-empty files exist or metadata fails |
| planAt(off) | Returns a valid file/local offset only after successful checked metadata traversal |
| repair(plan) | Existing core sequence is quarantineCopy(file), quarantineFrom(later files), truncateFile(file, localOff). The driver and its crash behavior remain absent; do not expose this until recoverable semantics are proven |

## Append failure semantics (v0.2.58)

`IFileSet::appendFile`, `IStorage::append` and `LogStore::append` use boolean success but cannot provide transactional rollback. A false result means success was not established; it does not prove that storage is unchanged. A backend may have written only part of a record, one or more complete records from a commit group, or the entire requested buffer before a later sync/close error. `LogStore` latches `faulted()` and suppresses observer notification for that call. Do not retry blindly; reopen and replay the log first. If a complete commit group is present after reboot, replay may accept it even though the original caller saw false. Exactly-once request semantics require a separate idempotency/reconciliation design and are not provided by this interface.

### GAP retry/capacity caveat

appendGaps() is still not exactly-once. If a chunk was fully written but returned false because a later sync/close failed, repeating the same interval after reopening can append duplicate GAP records to the log. v0.2.59 changes the derived LogIndex to coalesce overlapping or adjacent half-open spans, so duplicate retries no longer consume additional MAX_GAPS slots or alter gapOverlaps() results. The index capacity now limits disjoint coverage spans rather than raw EV_GAP records; more than 512 disjoint spans still sets gapOverflow_ and makes coverage unprovable. The original event stream and on-disk format are unchanged. Duplicate retry records still consume log/storage capacity, and exactly-once request semantics remain unresolved under U112. Host-only regression evidence; actual ESP32 VFS/power-cut behavior remains unverified.

## Startup scan and recovery boundary (C13.2, v0.2.77)

- LogStore::open() is read-only with respect to storage. It never calls IStorage::truncate() and never discards an EOF suffix.
- Partial record bytes, an invalid complete record, a sequence discontinuity, interior corruption, or an unfinished FL_MORE group causes open() to fail and leaves the log faulted.
- A failed open does not replay any valid-looking prefix to attached observers. Derived roster, index, closure and attendance state therefore cannot treat a damaged stream as complete.
- OpenInfo.tornBytes, corruptAt and droppedGroup report anomaly dimensions where determinable. A metadata/read failure also fails closed.
- This does not implement manual recovery. MonthSet::repair() still has a non-transactional copy/move/truncate sequence and must not be called from production. LittleFsFileSet's destructive methods still return false.
- The detailed D1–D11 delegated engineering choices and implementation limits are in littlefs_recovery_protocol.md.

## Known gaps

- A non-destructive POSIX/VFS LittleFsFileSet subset exists and has passed an inert ESP32 compile/link and exact ELF symbol inspection, but it is not production-wired and has not mounted or operated on LittleFS at runtime. truncate and quarantine remain disabled.
- The current LittleFsFileSet implementation uses POSIX/VFS calls rather than Arduino fs::File. The pinned LittleFS wrapper exposes begin()/format()/end(); begin(formatOnFail=true) can format after an ESP_FAIL mount result, so the production owner must not silently enable that option. The current adapter's fsync/write/readback/close checks have compile/link evidence only; their runtime durability and error propagation on the pinned target remain unverified.
- Atomicity and recovery for copy, suffix quarantine, tail replacement, truncate, rename, file creation and directory-entry persistence under power loss remain UNVERIFIED (C13). The current MonthSet::repair() sequence has no durable journal or proven restart protocol. The test_partial_repair_is_repeatable fake does not exercise interruption inside copy/move/truncate (C13.2 proposal).
- Quarantined months are not read back; no restore path exists.
- EV_REPAIR (defined, unused). Proposed: `a = files quarantined`. NOT approved.
- Monthly file growth against the configured flash partition is unmeasured.
