# Specialized source audit — v0.2.20

Date: 2026-11-07  
Baseline: v0.2.19  
Status: development snapshot; no functional tests or compiler run.

## Scope

This increment completes Batch A of the SF2 source-projection work. It adds a reusable current-writer event-group observer, read-only roster interval/name projections, and the guards needed to distinguish the exact `LogStore::open()` result used for a live audit. It does not add an HTTP API, browser integration, metadata persistence, workbook normalization, XLSX writing, or firmware composition.

## Source changes

### 1. EventGroupAudit

New files:
- `lib/core/src/event_group_audit.h`
- `lib/core/src/event_group_audit.cpp`

It is a non-copyable `LogObserver` and validates current event-group framing and selected current record fields:
- global event sequence begins at one and remains contiguous;
- records in a commit group are contiguous and `FL_MORE` framing closes correctly;
- ROSTER SET is one header plus four payload records;
- CLOSE is one header plus exactly five 64-bit expected-student bitmap payload records for IDs 1..300, including when the expected count is zero;
- CLOSE bitmap popcount equals `EV_CLOSE.a`, and padding bits above ID 300 are zero;
- GAP chunks are contiguous and arithmetic is range-checked; any paired CLOCK_SET records follow the current writer's expected shape;
- standalone CLOCK_SET pairs, SCAN, MARK, and no-class events have selected fields/flags checked;
- event types without a current writer/schema validator fail closed.

It is intentionally **not** a full semantic verifier for every domain field or a cryptographic integrity mechanism. The `EV_BOOT` enum is used by existing LogStore tests as a generic log record but has no current production event writer/schema in the project source; the audit therefore rejects it. A future production writer must add an explicit validator and tests before that type can be accepted by this gate.

### 2. Live readiness and open-generation protection

`EventGroupAudit::readyFor(log, opened)` requires:
- the observer is attached and has no malformed or pending group or sequence error;
- `opened.ok` and no corruption, torn bytes, or dropped tail group for that open;
- the log is not faulted;
- the audit's observed record count and last sequence match the live `LogStore`.

A bug found during source review was corrected: the open-time `OpenInfo.records` and `lastSeq` cannot be compared to the current observer counts after a valid later append. `OpenInfo` now carries an in-memory `generation`; `LogStore::openGeneration()` increments on each open invocation. `readyFor()` requires the supplied generation to match the current generation, so an earlier clean `OpenInfo` cannot be reused after a later reopen that repaired/dropped a tail.

The generation token changes only the in-memory C++ API/struct layout. It does not alter the persisted 16-byte event record format or existing log bytes. A static source search found no direct `OpenInfo` aggregate initialization outside `LogStore::open()` in this project tree. This still needs a full native/target compile to confirm all downstream build consumers.

### 3. Restore replay reuse

`RestoreReplayAudit` now aliases and uses `EventGroupAudit` for the same current-writer group checks, and explicitly rejects `sequenceError()` as well as malformed/pending groups. This is stricter than the previous generic single-record fallback. Restores containing event types for which this release has no validator (including `EV_BOOT`, `EV_SETTING`, or `EV_REPAIR`) will fail closed; no production writer for those types was found in the current source tree.

### 4. Roster history projection

`RosterStore` adds read-only:
- `intervalCount()`;
- `intervalAt()` returning half-open `[startDayInclusive,endDayExclusive)` periods;
- `knownIdAt()` returning known IDs in ascending order;
- `historyComplete()` reflecting only parser errors and the existing 512-interval overflow state.

`RosterNameHistoryReader` streams historical `RA_SET` names with student ID, local effective day, source sequence and the exact stored 32-byte display-name string. It requires a ready event audit, uses bounded memory, and rejects reads if the log count/sequence or audit generation changes during streaming.

This does not add LRN, sex, name-component parsing, transfer/withdrawal reasons, destination, NLS status, curriculum or school-reporting metadata. It also does not solve backward-RTC non-monotonic effective dates, and `historyComplete()` does not prove all roster lifecycle transitions are chronologically sensible.

## Regression registrations authored (not run)

- `test_event_group_audit`: 19 new `RUN_TEST()` registrations.
- `test_roster_history`: 6 new `RUN_TEST()` registrations.
- Total new C++ registrations: 25.
- Total C++ registrations in tree: 343 across 21 native suites.
- Existing user-reported baseline: 318/318 native tests passed on v0.2.17. This is not a v0.2.20 result.
- SF2 browser layer: 144 Node test registrations from v0.2.18/v0.2.19, not run.

The new regression sources cover exact five-word CLOSE bitmap framing, count/popcount and padding validation, selected SCAN/MARK/no-class shapes, unsupported event fail-closed behavior, live audit after a valid append, stale `OpenInfo` after a clean reopen and after a tail-repairing reopen, interval access, sorted IDs, name streaming and mutation detection. **None were executed.**

## Critical limitations / open items

1. `EventGroupAudit` is not attached automatically during normal firmware boot and no production snapshot builder calls `readyFor()` yet.
2. `LogStore::open()` repairs a tail before reporting `tornBytes`/`droppedGroup`; the caller must reject that open for authoritative snapshot purposes. The audit does not undo the repair.
3. The observer detects mutations routed through `LogStore`, not direct out-of-band writes to its underlying storage driver. A future snapshot coordinator must own/serialize storage access.
4. `EventGroupAudit` consumes one of 16 `LogStore` observer slots; the production composition must budget observer count/lifetime.
5. This audit is not cryptographic and does not prove adversarial tamper resistance.
6. Unsupported event types fail closed. Any future event writer requires a reviewed schema and regression tests before snapshot/restore acceptance.
7. `RosterStore::historyComplete()` is only a parser/interval-capacity flag, not a semantic roster-history guarantee. C24/C61 remain open.
8. Historical display-name selection after backward RTC correction remains unresolved (U58).
9. Target RAM/CPU/replay-time impact is unknown (U59). No target build or hardware measurement was performed.
10. The production reporting metadata store, immutable snapshot endpoint, normalized workbook, and XLSX adapter remain absent.

## Static audit evidence

- 21 native test directories found.
- 343 `RUN_TEST(` source registrations counted.
- The two new suites account for 25 registrations (19 + 6).
- No direct `OpenInfo` aggregate initializers were found outside `LogStore::open()`.
- No v0.2.20 C++ test runner or compiler was invoked.

## Verification status

- Functional tests: **not run by instruction**.
- C++ compiler / PlatformIO native: **not run**.
- ESP32 target compile: **not run**.
- ESP32 hardware, log storage fault injection and timing: **not tested**.
- Archive integrity: 156/156 manifest entries verified in the source tree and again after a fresh archive extraction; no `.git/`, `.pio/`, `node_modules/` or `__pycache__/` directories are present in the package. Final archive size: 366,623 bytes.
