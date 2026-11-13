# Audit v0.2.58 — append-failure and indeterminate-commit semantics

Date: 2026-11-09
Base: v0.2.57 archive SHA-256 `45d58256f7e9312044f55bf29950e0a64161533fca16ace25cd7cb218d4a8e4a`
Scope: IStorage/IFileSet/LogStore append contracts and test/test_monthset
Result: host-tested contract clarification; no production behavior change

## Tests added

1. test_partial_append_failure_latches_and_prevents_retry: FakeFiles writes seven bytes and returns false. LogStore becomes faulted, does not notify the observer, and refuses another append without invoking the backend again.
2. test_complete_write_reported_failed_is_indeterminate_after_reopen: FakeFiles writes one complete record but returns false to simulate a later sync/close failure. The original LogStore faults and does not notify; a fresh LogStore::open() accepts the complete record. The outcome is therefore indeterminate, not rolled back.
3. test_incomplete_commit_group_is_not_replayed_when_repair_fails: FakeFiles writes only the first full record of a two-record group. That record carries FL_MORE. On reopen, LogStore must drop the incomplete group; with truncate disabled, open fails closed and the prefix is not exposed.
4. test_partial_append_reopen_fails_closed_without_tail_repair: FakeFiles leaves a seven-byte tail, and a fresh LogStore refuses to open when truncate is unavailable. The bytes remain unchanged.

## Verification

Exact final command: `pio test -e native -f 'test_monthset'`
Result: **18/18 passed**, exit 0, 19.207 seconds.
The initial compile-only test-first failures were expected: test code referenced failure controls not yet added to FakeFiles. No behavior test failed after the test fixture was completed.

## Semantics proven at the core boundary

- A failed append is not a transactional rollback guarantee.
- LogStore suppresses observer notifications and latches its fault for the current instance.
- The same LogStore instance cannot retry after the failure.
- A complete write reported failed may be accepted by replay after reboot.
- An incomplete commit group is not accepted as a prefix; it requires tail truncation, and open fails closed if that operation is unavailable.

## What this does not prove

- The FakeFiles backend is synthetic. No actual LittleFS VFS short-write, fsync, close, metadata or descriptor fault was injected.
- No flash power interruption, brownout, full-partition behavior, filesystem remount or target-specific recovery was tested.
- No production HTTP request ID/idempotency ledger exists. A caller retrying after an indeterminate result may duplicate a complete event; this remains U112.
- The v0.2.57 LittleFsFileSet still refuses truncate/quarantine, so a partial tail may require service recovery. C13 remains blocking.
- No final ESP32 build, ELF inspection, physical hardware, full native regression or SF2 suite was run.

No delegated storage-recovery semantics, product decisions, signed rules, or E18/U5 decisions were changed.
## Additional static finding: GAP retry capacity (U113)

The old appendGaps() comment said a retry was safe because it only adds overlap. That was too broad. LogIndex::onRecord() stores every EV_GAP separately, does not deduplicate/merge, and has MAX_GAPS=512. If appendGaps() returns false after a complete chunk actually reached storage, replay can load that chunk; retrying the whole original interval can add duplicate entries. Enough retries can set gapOverflow_, which makes session coverage unprovable. The log_store.h comment now states that false can mean absent/partial/complete, and repeated intervals are not capacity-neutral or exactly-once. No implementation semantics were changed. A dedicated complete-write-failure + reopen + retry test, and a design for idempotency/resume/compaction, remain required before relying on retry behavior.
