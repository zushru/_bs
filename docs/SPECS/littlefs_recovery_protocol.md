# LittleFS recovery protocol — design proposal

**Transaction design remains proposal-only; destructive recovery is not authorized.**

**Status:** The user delegated D1–D11 engineering choices to assistant judgment on 2026-11-10. Those selected safety policies are recorded below as **DELEGATED engineering decisions**, not signed product rules. The detailed commit transaction, LittleFS commit primitive, persistent journal schema, and any destructive recovery procedure remain unapproved technical design. The existing docs/SPECS/month_set.md decision remains **DELEGATED**. This document does not implement, enable, or authorize truncateFile(), quarantineCopy(), quarantineFrom(), automatic tail repair, formatting, or any other destructive storage operation.

**Baseline reviewed:** v0.2.77 (2026-11-10)  
**Scope:** attendance event log stored as ordered monthly files through LittleFsFileSet, MonthSet, and LogStore.

## 1. Executive assessment

The code has useful failure detection but still does not have a crash-safe storage-recovery transaction.

Two separate concerns must be kept distinct:

1. **Startup scan:** fixed in v0.2.77. LogStore::open() is read-only with respect to storage. It preserves partial/corrupt/incomplete tails, fails closed, and does not replay a valid-looking prefix. A failure during the second replay pass resets any observers that already received records.
2. **Manual storage repair:** still unsafe. MonthSet::repair() performs quarantine-copy, later-file move, and truncate operations without a persistent transaction journal, a proven commit point, or a restart protocol.

Before v0.2.77, LogStore::open() could truncate a complete invalid EOF record as though it were a torn suffix. This ambiguity is no longer present in the current open path, but the same bytes now leave the device unavailable for attendance until a safe service recovery design exists.

**Selected safety position:** startup never deletes bytes. Any ambiguous tail, invalid record, metadata error, incomplete group, or failed read keeps attendance storage unavailable for writes and preserves original files. Recovery must be a separate, authenticated, physically serviced transaction after evidence staging and verification. No such transaction is implemented or enabled.

## 2. Verified current implementation facts

### 2.1 Record and append contract

- An event record is 16 bytes (REC_SIZE).
- LogStore::append() serializes one logical group into one IStorage::append() call, assigns contiguous sequence numbers, and sets FL_MORE on all but the last record in that group.
- MAX_GROUP is 16. MonthSet does not intentionally split one append group across month files.
- IStorage::append() and IFileSet::appendFile() define false as indeterminate: zero, partial, or complete bytes may already be present.
- On append failure, LogStore latches faulted_ and does not notify observers for that call. It does not retry the logical operation.
- A later open/replay may find a complete group that the original caller saw as failed. This is a separate idempotency problem; file repair cannot decide whether a user action should be replayed.

### 2.2 Current LogStore::open() behavior (v0.2.77)

Current source sequence:

1. Set faulted_ = true, advance the open generation, clear in-memory count/sequence/baseline, and reset observers.
2. Obtain checked total size and read complete 16-byte slots.
3. Decode records and require e.seq == i + 1 across the entire log.
4. Record the first invalid offset, partial-byte count, and unfinished trailing FL_MORE-group count when determinable. A later decodable record after an invalid slot is treated as interior corruption.
5. If any invalid record, partial bytes, sequence issue, or unfinished group exists, return failure with faulted_ still true. Do not truncate or replay any prefix.
6. If the second replay pass encounters a read/decode/sequence failure after any observer has received records, clear count/sequence/baseline and call onReset() on every observer again before returning failure.
7. Only a clean full scan and successful replay clears faulted_ and publishes the final count/sequence/baseline.

This path performs no storage mutation. A CRC-invalid complete EOF record, partial record, sequence-invalid record, or unfinished FL_MORE group is preserved. This is a deliberate availability trade-off: the device may need service after an interrupted append, but cannot silently delete potentially committed attendance data.

The behavior before v0.2.77 is historical only. It scanned a contiguous prefix, inferred a suffix boundary, walked back over FL_MORE records, called IStorage::truncate(), and replayed the retained prefix. That old behavior is not present in the current source.

### 2.3 Current MonthSet::repair() mutation

Current sequence:

1. quarantineCopy(fileIndex) — copy the affected file while retaining the original.
2. quarantineFrom(fileIndex + 1) — move the later files aside.
3. truncateFile(fileIndex, localOff) — truncate the original affected file to the chosen prefix.

The sequence has no durable operation ID, journal, commit point, copy verification contract, restart-state recognition, or rollback procedure. A failure after any individual operation can leave a different mixture of original, copied, moved, and truncated files.

The existing test_partial_repair_is_repeatable does not exercise a crash during these operations. Its fake pre-populates the quarantine vectors to simulate a previously completed copy/move, then invokes repair. It does not establish on-filesystem idempotence, atomicity, or power-cut safety.

### 2.4 Current LittleFS adapter and platform evidence

- LittleFsFileSet assumes the mount already exists and that one caller serializes operations.
- It validates canonical YYYYMM.bin names and fails metadata scans on unexpected entries.
- Append uses POSIX/VFS write(), fsync(), size inspection and pread() verification. A false result remains indeterminate.
- New month creation uses O_CREAT | O_EXCL, then fsync(), close and a size check. The durability of the parent directory entry across actual power loss has not been proven.
- truncateFile(), quarantineCopy(), and quarantineFrom() currently return false.
- The v0.2.71/v0.2.72 inert ESP32 adapter-link target proves compile/link compatibility and symbol retention only. It does not mount LittleFS or perform a VFS file operation.
- The pinned Arduino LittleFS.begin() wrapper defaults formatOnFail=false; if a caller passes true, it can call format() after an ESP_FAIL mount result. The eventual production mount owner must not silently opt into format-on-failure. Mount failure must never be interpreted as permission to erase the attendance partition.
- The installed framework-arduinoespressif32-esp32-libs 3.3.12 package sdkconfig records CONFIG_VFS_SUPPORT_DIR=y, CONFIG_LITTLEFS_PAGE_SIZE=256, CONFIG_LITTLEFS_MAX_PARTITIONS=3, CONFIG_LITTLEFS_READ_SIZE=128, CONFIG_LITTLEFS_WRITE_SIZE=128, and CONFIG_LITTLEFS_BLOCK_CYCLES=512; CONFIG_LITTLEFS_FLUSH_FILE_EVERY_WRITE is not set. The pinned esp_littlefs.h therefore defines ESP_LITTLEFS_ENABLE_FTRUNCATE. This is package configuration/header evidence, not proof that LittleFS was linked into the inert smoke image or that ftruncate works safely at runtime. The exact target build did not mount LittleFS or invoke ftruncate; the current LittleFsFileSet::truncateFile() still returns false.
- The POSIX host seam added in v0.2.72 tests append syscall error propagation only. It does not inject directory metadata, rename, truncate, sync-directory, partition-full, LittleFS block-device, or power-loss faults.
- v0.2.88 extends the host-only fault seam in `validateRecordFiles()` to path metadata (`lstat`), read-only file `open`/`fstat`, and `opendir`/`readdir`/`closedir`, with seven registered but unrun regressions. The hooks do not cover `scan_()`/ordinary metadata paths and do not test ESP-IDF VFS behavior; the close callbacks release the real POSIX resource before reporting the injected error.

### 2.5 Semantic and restore validation boundaries

- EventGroupAudit checks current commit-group framing and selected event semantics after replay. It is not a cryptographic check or a complete validator of future event types. Its current default handling rejects unsupported types such as EV_BOOT, EV_SETTING, and EV_REPAIR; a production recovery gate cannot blindly treat this observer as a complete accepted-event schema until the supported event inventory is reconciled.
- RestoreReplayAudit rechecks staged bytes and runs fresh roster, closure/index, and event-group observers. It rejects a staged log if open reports torn bytes, a dropped group, corruption, or count/sequence mismatch. It does not modify the live log or perform a storage swap.
- A recovery path must not treat CRC-valid records alone as proof that the attendance stream is semantically safe.
- Backup CRCs and event CRCs are error-detection mechanisms, not cryptographic authenticity guarantees for quarantined data.

## 3. Recovery invariants required before implementation

The protocol must prove all of the following. A single unproven invariant blocks enabling destructive operations.

1. **Original preservation:** before a recovery commit, every original month file remains byte-for-byte available.
2. **Single writer:** no attendance, closure, roster, clock, restore, backup, or administrative writer can mutate the affected stream during inspection or recovery.
3. **Unambiguous authority:** after reboot, the system can determine which complete generation is authoritative without guessing based on file presence or timestamps.
4. **Verified staging:** every staged copy is checked against the exact source size and bytes before it can become active.
5. **Durable intent:** recovery phase and operation identity survive reset, and a partial/invalid journal is distinguishable from a committed state.
6. **One commit point:** the switch from the old log to the recovered log has a platform primitive whose atomicity and durability are established on the pinned LittleFS build.
7. **Restart idempotence:** repeating boot recovery at every possible interruption point converges to the same safe state and never deletes the only valid copy.
8. **Fail-closed ambiguity:** any inconsistent journal, missing source, conflicting generation, failed read, metadata error, or unverifiable operation leaves storage read-only/recovery-required.
9. **Post-switch validation:** the selected log is read back and checked for canonical month ordering, file alignment, event CRC, exact sequence continuity, valid group framing, and required roster/closure/index invariants before writes resume.
10. **No automatic formatting:** a mount or recovery failure never triggers filesystem format, partition erase, or a reset-to-empty attendance log.
11. **Evidence retention:** original corrupted bytes and later-file suffixes remain recoverable until the separate retention/deletion policy is approved.
12. **No invented records:** recovery can preserve, select, or explicitly remove a proven suffix under an approved policy; it cannot synthesize attendance events or fill sequence gaps.

## 4. Proposed inspection classifications

The names below are proposed report classifications, not current API enum values.

| Classification | Evidence | Proposed safe startup result |
|---|---|---|
| CLEAN | All month files enumerate; keys increase; every 16-byte record decodes; sequence is continuous; group audit is complete; required semantic audits pass. | Eligible for normal operation. |
| PARTIAL_RECORD_TAIL | Final file size is not a multiple of 16. | Preserve bytes; recovery-required/read-only. Do not truncate automatically. |
| INVALID_COMPLETE_SUFFIX | One or more complete records at EOF fail decode or sequence validation, with no later decodable record. | Preserve bytes; recovery-required/read-only. Do not assume torn append. |
| INCOMPLETE_GROUP | Final decoded record or chain indicates FL_MORE without a terminal record. | Preserve bytes; recovery-required/read-only until explicit policy determines whether the group can be discarded. |
| INTERIOR_CORRUPTION | A later decodable record exists after an invalid slot, or a nonrecoverable sequence discontinuity occurs. | Recovery-required; no automatic truncation. |
| SEMANTIC_INVALID | Record bytes decode but commit-group, roster, close, gap, or domain invariants fail. | Recovery-required; do not expose attendance or export as healthy. |
| METADATA_UNKNOWN | Directory enumeration, stat, file-size, path validation, or read fails. | Storage fault; never report as empty. |
| FILESET_INVALID | Unexpected path, invalid month name, duplicate/non-increasing month key, non-regular entry, or malformed directory layout. | Storage fault; do not modify files automatically. |
| RESTORE_STAGE_INVALID | Backup authentication, size, record count, staged bytes, event groups, roster or index checks fail. | Reject staging; never swap it into the live log. |

A read-only scanner should return exact file key, file offset, logical stream offset, byte count, record index, last valid sequence, group start/end, and error reason. It must not modify bytes, rename files, create directories, or change mount state.

## 5. Proposed transaction model — concept only

No concrete filesystem primitive is selected in this proposal. A design that depends on an unverified rename, truncate, directory-sync, or close guarantee is not acceptable.

### Phase A — quiesce and inspect

1. Disable new write admissions and stop/finish existing serialized work.
2. Confirm that all append, backup, restore, clock, roster, closure, and administrative mutations are quiescent.
3. Close all owned descriptors; record any close error as a hard stop.
4. Enumerate the complete fileset and perform a read-only byte scan.
5. Run sequence and event-group checks, then the required domain observers.
6. Produce a recovery plan bound to the exact source inventory (month keys, file sizes and source digests). Any source change invalidates the plan.

### Phase B — preflight

1. Validate all arithmetic, file ranges, record boundaries and month ordering.
2. Calculate space needed for a full preserved source generation, staged recovered generation, journal/manifest copies, and implementation-specific headroom.
3. Verify available space using a checked API. Unknown free space or insufficient space means abort with no mutation.
4. Check that an independently recoverable backup exists only as additional assurance; a backup acknowledgement is not a substitute for preserving the source files.
5. Allocate a non-colliding operation ID. Do not rely on wall-clock time alone for uniqueness.

### Phase C — stage and verify

1. Copy the full affected source set, including the damaged bytes and all later month files, into a recovery namespace outside /log. The live adapter rejects unexpected entries inside /log.
2. Do not overwrite an existing recovery object with the same name. A collision must be treated as an integrity error.
3. Persist each staged file using the exact target-supported calls; check every return.
4. Read each staged file back and compare its exact size and bytes to the captured source. A size-only check is insufficient.
5. Persist and verify a manifest describing all source/staged files, offsets, lengths, digests, operation ID, intended retained prefix and protocol version.
6. Persist a prepared/intent marker using a separately proven durable commit primitive. If the marker cannot be read back exactly, abort and preserve both source and staging.

CRC32 may detect accidental changes but is not collision-resistant authentication. The format and digest choice need to be selected and versioned. It must not be used to claim adversarial integrity.

### Phase D — switch authority

The current code assumes a fixed /log directory. A future design must explicitly select and validate how a fully prepared replacement becomes authoritative.

Candidates requiring independent investigation:
- a one-object active-generation manifest/pointer update;
- an atomic directory rename/swap, if the pinned LittleFS/VFS stack actually guarantees the required semantics;
- a versioned directory layout with a proven selection marker.

Two sequential renames are not a transaction merely because each rename succeeds. A cross-partition NVS pointer plus LittleFS files is not atomic without a cross-store protocol. Do not select a candidate based on desktop POSIX behavior or upstream documentation alone.

The design must identify the exact **single commit point**, what a reset before/after it means, and how the boot owner resolves a torn marker. If no adequate primitive is available, the product must remain in recovery-required/read-only state; do not improvise destructive fallback behavior.

### Phase E — verify and retain

1. After switching, read the selected generation from the mounted target filesystem.
2. Re-run all structural and semantic checks against the exact selected bytes.
3. Compare the resulting record count, last sequence, month files, and retained-prefix boundary to the committed recovery manifest.
4. Only then mark storage healthy and reopen write admissions.
5. Retain the old generation and quarantine evidence until a separate retention and deletion policy is approved.
6. Cleanup must be its own recoverable operation; cleanup failure must not invalidate an otherwise committed generation or cause deletion of the current active one.

## 6. Boot/restart state machine requirements

The implementation must represent durable states, not infer them from a loose set of files:

- **NORMAL:** active generation is uniquely identified and passed full validation.
- **RECOVERY_REQUIRED:** source is readable but has an anomaly or uncommitted recovery state. No writes, enrollment, closure, backup acknowledgement, or export that implies healthy data.
- **RECOVERY_STAGING:** a specific operation ID and source inventory exist; original generation remains authoritative.
- **RECOVERY_PREPARED:** every staged byte and the prepared manifest are verified; original remains authoritative.
- **RECOVERY_COMMITTING:** the one commit primitive may have been invoked; reboot must determine authority only from its documented durable semantics.
- **RECOVERY_VERIFYING:** selected generation is checked; no writes until every check passes.
- **RECOVERY_COMMITTED:** new generation is active; old bytes retained.
- **RECOVERY_ABORTED:** no authority switch occurred; original remains active and unmodified.
- **RECOVERY_AMBIGUOUS:** journal/marker/files disagree or the commit result cannot be proven. Preserve all generations and require explicit service action.

The precise state encoding, journal redundancy, write ordering, and marker semantics remain open. Merely writing a state byte at each phase is not enough unless torn writes and stale copies are detectable and the write itself has a verified commit primitive.

## 7. Fault-injection and interruption matrix

A future implementation test plan must interrupt or inject an error at each boundary below, reboot, and prove the selected state and exact preserved data. Host fakes are useful for control-flow coverage but cannot prove LittleFS atomicity or power-loss behavior.

| Boundary / fault | Required property after restart |
|---|---|
| Read-only scan fails at any byte | No bytes changed; writes blocked. |
| Directory enumeration/stat fails | Never treat as empty; no mutation. |
| Existing unexpected file/name/directory | Preserve all data; no auto-delete. |
| Source inventory changes after plan | Invalidate plan; restart read-only inspection. |
| Insufficient space / ENOSPC before staging | No canonical mutation; abort. |
| Create recovery namespace fails | Original active and unchanged. |
| Short copy or partial write | Staging marked incomplete; original remains authoritative. |
| Copy reports false after complete write | Read back and reconcile exact staging; never blindly duplicate/overwrite. |
| File sync, close, or read-back failure | No prepared/commit marker; original remains authoritative. |
| Power loss midway through any source file copy | Restart can distinguish incomplete staging from complete staging; original preserved. |
| Power loss after all copies but before prepared marker | Verify again; never infer prepared from filenames alone. |
| Prepared marker partial/torn/stale | Original remains active unless an exact committed marker is proven. |
| Power loss immediately before commit primitive | Original remains active. |
| Power loss during commit primitive | Authority is deterministically resolved from proven primitive semantics, otherwise RECOVERY_AMBIGUOUS. |
| Power loss immediately after commit primitive | New generation is selected only if commit is proven; old generation retained. |
| Read-back/semantic check fails after switch | Do not enable writes; preserve both generations and enter RECOVERY_AMBIGUOUS/RECOVERY_REQUIRED. |
| Power loss during old-generation cleanup | Active generation remains valid; cleanup can resume without deleting active data. |
| Repeated recovery invocation | Same operation ID/state converges; no duplicate quarantine, repeated truncate, or loss of original bytes. |
| Partition full during journal update | No guessed state transition; keep whichever generation is provably authoritative, otherwise read-only. |
| Brownout during append, file creation, sync, rename/switch, truncate, or journal update | Verified on physical target with byte-for-byte pre/post comparison. |

## 8. Required tests before any destructive implementation

### Host tests

- A read-only scanner with exhaustive byte/record/group fault fixtures.
- Complete invalid final record, wrong-sequence final record, partial record tail, CRC-valid unfinished group, damaged terminal group record, interior corruption, malformed semantic group, and unknown event type cases.
- Multi-file corruption across first/middle/last month.
- Every transaction phase interrupted before and after each persistent operation.
- ENOSPC, short write, false-after-full-write, sync, close, read-back mismatch, failed enumeration/stat, path collision, and stale/corrupt journal.
- Repeated boot/recovery idempotency assertions comparing every source/quarantine/staging byte.
- Assertions that no data-destructive operation occurs on scan, preflight failure, journal ambiguity or unapproved state.
- Restore staging remains separate; invalid staged content cannot replace live storage.

### Pinned target VFS tests

Run against the actual pinned LittleFS component and exact partition/mount configuration:
- open, read, write, fsync, close, ftruncate, rename, unlink, mkdir, directory enumeration and parent-directory metadata behavior.
- Determine whether fsync is supported and what persistence guarantee it provides.
- Determine close-error behavior and whether a failed close retains a descriptor/resource.
- Determine rename atomicity under reset/power loss, including replacement semantics and cross-directory behavior.
- Determine file and directory-entry durability after create/rename/unlink.
- Determine behavior when power is interrupted during truncate, rename or journal update.
- Determine CONFIG_VFS_SUPPORT_DIR and exact enabled LittleFS configuration in the production target.
- Verify whether formatOnFail can ever be enabled by the production mount owner. It must not silently format attendance data.

### Physical device tests

- Controlled power cuts and brownouts at every persistent transition in the matrix.
- Repeat at near-full and full partition conditions.
- Reboot/scan after each interruption and compare exact file bytes, file inventory, event count, last sequence, groups and semantic projections.
- Long-cycle/repetition testing to catch flash wear and stale recovery artifacts.
- No physical test is implied by this proposal or by the v0.2.72 target link result.

## 9. Delegated engineering decisions (2026-11-10)

The user delegated the C13.2 engineering choices to assistant judgment on 2026-11-10, with reliability, optimization and user convenience as priorities. The following are **DELEGATED engineering decisions, not user-signed product rules**. They do not alter HANDOFF_FINAL.md, docs/DECISIONS.md, O8, the event format, or the delegated month_set specification.

| ID | Selected policy | Required implementation constraint |
|---|---|---|
| D1 | Ordinary startup never discards any log suffix. | Any ambiguous tail leaves the log faulted/read-only. No automatic truncation or best-effort recovery. |
| D2 | A valid but unfinished FL_MORE group is preserved in full. | Do not replay the preceding prefix as a healthy log; require an explicit recovery workflow. |
| D3 | A complete CRC-invalid or sequence-invalid EOF record is corruption, not presumed torn append data. | Preserve it and every later byte/file; do not infer a discard boundary from location. |
| D4 | Preserve original bytes and recovery evidence indefinitely by default. | No automatic expiry, overwrite or cleanup. If evidence capacity is exhausted, stop writes and report storage fault rather than deleting evidence. Any future cleanup is a separate explicit operation after verified evidence export. |
| D5 | No LittleFS primitive is currently accepted as the recovery commit point. | Do not rely on rename, truncate, file sync, directory sync, or marker replacement until the pinned target's exact durability and interruption semantics are measured. If no adequate primitive is proven, keep recovery-required/read-only. |
| D6 | Do not create a persistent recovery journal format before D5 is proven. | A future versioned journal must identify the operation and old/new generations, bind the complete source inventory and retained boundary, detect incomplete/conflicting records, and validate all staged bytes. CRC is error detection, not authenticity. The exact bytes/schema remain intentionally unselected. |
| D7 | Unknown or insufficient free space aborts before any mutation. | No in-place truncate, partial quarantine, source deletion, or capacity-reclaim fallback. |
| D8 | Destructive recovery/export/deletion requires one authenticated ADMIN, fresh explicit typed confirmation, and physical service presence. | No normal remote/API recovery endpoint. Do not claim this boundary exists until C6 production authentication and the physical service workflow are implemented. A second administrator is not required; do not represent self-review as independent approval. |
| D9 | EV_REPAIR remains reserved and unused. | Do not emit it until its exact fields and semantics are specified and EventGroupAudit validates it. A future EV_REPAIR may describe an already committed operation but can never establish the commit point. |
| D10 | Preserve signed O8: ordinary backup remains config + roster + attendance log. | Quarantine/recovery evidence is excluded from ordinary BACKUP_ACK. A future recovery-evidence package must be a separate, versioned, authenticated export of the original bytes and inventory. It must not silently include R17 reporting PII or claim to be a complete SF2 metadata backup. |
| D11 | Normal boot and recovery never format the attendance filesystem. | Formatting is not an automatic fallback. Any exceptional destructive service procedure requires physical presence, a separately verified evidence export where readable, and explicit typed data-loss confirmation. |

### Immediate behavior required by D1–D3

- \`LogStore::open()\` is a read-only inspection/replay gate. It never calls truncate, quarantine, rename, unlink, mkdir, or format.
- A partial record tail, invalid complete record, sequence error, interior corruption, or unfinished FL_MORE group makes \`open()\` fail and leaves \`faulted()\` true.
- No valid-looking prefix is replayed to observers when any anomaly exists. This prevents roster/index/closure projections from being mistaken for complete history.
- \`OpenInfo\` reports the observed corrupt offset, trailing byte count, and unfinished-group count where those can be determined. A metadata/read error remains a fail-closed open failure.
- The current implementation change does not add a repair path. The destructive LittleFS adapter methods remain disabled.

D5 and D6 are deliberate technical no-go decisions, not permission to guess. C13.3 must first establish the production mount/storage owner and experimentally validate the exact pinned LittleFS/VFS durability behavior. Physical interruption tests remain mandatory for any durability claim.

## 10. Explicit non-goals and current disposition

- D1–D11 are delegated engineering choices, not user-signed product rules.
- This document does not choose a commit primitive or define a final on-disk journal schema.
- This document does not alter the event/backup format or signed O8 backup scope.
- This document does not authorize EV_REPAIR.
- This document does not make host POSIX behavior equivalent to ESP32 LittleFS.
- This document does not change the delegated status in docs/SPECS/month_set.md.
- truncateFile(), quarantineCopy(), and quarantineFrom() remain disabled in LittleFsFileSet.
- No automatic format, destructive fallback, or in-place best-effort repair is acceptable.
- v0.2.77 changes LogStore::open() to preserve anomalous bytes and fail closed; it does not implement repair, quarantine, export, or a journal.

**Next gate:** establish the production LittleFS mount/storage owner and validate exact pinned-target VFS behavior (including sync, close, metadata durability, and interruption). Only after a single commit point is demonstrated may a separately tested journal and recovery transaction be implemented. Physical power-cut evidence remains mandatory; until then, anomalous storage stays read-only.
