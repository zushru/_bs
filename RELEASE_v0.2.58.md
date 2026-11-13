# Release v0.2.58 — append failure semantics and core regressions

Date: 2026-11-09
Base: complete v0.2.57 archive extraction
Base archive SHA-256: `45d58256f7e9312044f55bf29950e0a64161533fca16ace25cd7cb218d4a8e4a`
Project root during development: `/root/att`
Release status: development snapshot; C3 and C13 remain blocking

## Scope

1. Clarified the public append contracts in IStorage, IFileSet and LogStore. A false append result means the requested success/durability condition was not established; storage may be unchanged, partially modified, or fully modified.
2. Documented that LogStore latches its fault on append failure, does not advance its in-memory count/sequence, and does not notify observers for that failed call. Callers must reopen and reconcile before retrying the same logical operation.
3. Added four test_monthset registrations using FakeFiles controls to simulate partial append, full append followed by a failure result, incomplete multi-record commit groups, and unavailable truncation during boot replay.
4. No production runtime behavior changed. The existing LogStore fault latch already provided the behavior covered by the tests.

## Verification

Exact command: `pio test -e native -f 'test_monthset'`

Result: **18/18 passed**, exit 0, 19.207 seconds.

The test-first attempts initially failed to compile because FakeFiles did not yet expose the required failure controls. After adding the controls, the behavior suite passed. The final count is 18 cases: 14 existing plus four new registrations.

Source inventory: **581 C++ registrations across 35 suites**; **563 were not run** in this increment.

## Important conclusions and limits

- A partial write returning false leaves the current LogStore faulted. It does not notify observers or allow an in-session retry.
- A complete record may be present after a fresh LogStore::open() even though the original append returned false. This is an indeterminate commit; a caller that blindly retries a logical action can create a duplicate.
- A partial byte tail or an incomplete commit group must be repaired/truncated before normal replay can proceed. If the backend refuses truncation, LogStore::open() fails closed and does not accept the incomplete group prefix.
- These regressions use a host FakeFiles backend. They simulate method return/write order; they do not inject real short writes, fsync failures, close failures or power cuts into ESP32 LittleFS.
- No request idempotency key or durable reconciliation ledger is implemented. U112 tracks this unresolved API-level risk.
- The v0.2.57 LittleFsFileSet destructive methods remain disabled. C13 remains blocking; no transaction or power-loss semantics were added to the delegated quarantine/repair design.
- No ESP32 target build, ELF inspection, physical device, flash, power-cut/brownout, full native regression or SF2 suite was run.
- No signed or delegated decision changed. HANDOFF_FINAL.md and docs/DECISIONS.md remain unchanged.
- Additional static review found U113: repeated appendGaps() after an indeterminate result may write duplicate GAP records. LogIndex stores each GAP separately, does not deduplicate, and has fixed MAX_GAPS=512 capacity; retries can consume capacity and force unverified coverage. The misleading header claim that overlap-only retry is simply safe was corrected. No GAP behavior or semantics changed, and no dedicated retry/overflow regression was run in this increment.
