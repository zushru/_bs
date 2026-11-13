# Release v0.2.73 — C13.2 recovery protocol proposal

Date: 2026-11-10  
Baseline: v0.2.72  
Status: documentation-only development snapshot; the recovery proposal is not approved for implementation.

## Scope

This increment documents the storage-recovery design gap found during the approved C13 roadmap work. It changes no C++/JavaScript source, test registration, API, firmware target, event format, backup format, or persistent storage behavior.

The new document is:

- docs/SPECS/littlefs_recovery_protocol.md

Its first-line status is **PROPOSAL ONLY — NOT APPROVED FOR IMPLEMENTATION**. It does not authorize truncateFile(), quarantineCopy(), quarantineFrom(), LogStore automatic tail truncation changes, automatic formatting, or any recovery operation.

## Main findings recorded

1. LogStore::open() can attempt to truncate an invalid suffix at EOF if the IStorage backend permits truncation. A complete 16-byte record with bad CRC, or a final record with invalid sequence, can be treated as a suffix even though the bytes may represent previously committed data corruption rather than an interrupted append.
2. A trailing valid FL_MORE chain is also dropped back to its group start by the current open logic. The marker indicates incomplete framing but does not prove the cause.
3. The current LittleFsFileSet refuses truncation, so this backend fails closed when LogStore::open() requests it. That does not make the generic core policy safe for other storage backends.
4. MonthSet::repair() performs quarantineCopy(), quarantineFrom(), and truncateFile() sequentially. There is no durable transaction journal, commit point, verified copy contract, restart protocol or rollback protocol.
5. Existing fake repair tests do not interrupt the operations in the middle and do not establish target filesystem atomicity or power-cut safety.
6. EventGroupAudit is not a complete accepted-event schema; unsupported event types such as EV_BOOT, EV_SETTING and EV_REPAIR are rejected by its current default handling.
7. The pinned Arduino LittleFS wrapper may call format() after mount failure if its caller opts into formatOnFail. Production mount policy must explicitly prohibit silent format-on-failure.

## Proposed direction

The proposal recommends read-only startup inspection, preserving ambiguous bytes, a separate authorized recovery transaction, verified staging, one proven commit point, idempotent restart handling, post-switch structural and semantic validation, and retention of original data until policy approval.

No exact commit primitive, journal format, recovery policy or retention period is selected. Decisions D1–D11 require explicit user review. The existing month_set specification remains DELEGATED.

## Verification and evidence

This increment is documentation-only.

- No PIO test was run.
- No SF2 Node suite or syntax check was run.
- No ESP32 target build or ELF inspection was run.
- No hardware probe, flash, filesystem mount/runtime, physical device, power-cut or brownout test was run.
- The v0.2.72 host and inert ESP32 compile/link results remain historical and unchanged.

The exact v0.2.72 baseline archive SHA-256 was verified as:

    e3aa89d8765a6f4937e41d8d722e0c3b77667533fb286f75d964c33c71439d2d

Current inventory is unchanged: 628 C++ RUN_TEST registrations across 35 native suite directories and 257 SF2 Node test registrations across 12 files.

## Limitations

C13 remains blocking. No production LittleFS mount owner, mounted-target VFS behavior, verified commit primitive, recovery journal, destructive repair implementation, crash-recovery evidence or physical power-loss validation exists.

The proposal must be reviewed and specific decisions approved before any recovery implementation is planned. Even after approval, the chosen mechanism still needs focused host interruption tests, pinned-target VFS tests, and physical power-cut evidence before destructive operations can be enabled.
