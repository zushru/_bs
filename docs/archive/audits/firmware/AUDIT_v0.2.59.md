# Audit v0.2.59 — GAP coverage-index retry safety

Date: 2026-11-09  
Base: v0.2.58 archive SHA-256 `431caa995cea1b1a315c9c3c80758c444706cb69d11b4189c4fa04fb8a09e4d5`  
Scope: `LogIndex` GAP interval projection, indeterminate `appendGaps()` retry and restore replay  
Result: U113 fixed at the derived-index layer; raw event and persistent formats unchanged

## Defect reproduced before the patch

`IStorage::append()` can write the complete requested record buffer and still return false. `LogStore::append()` correctly latches the current instance faulted and suppresses observer notification, but a fresh `LogStore::open()` can replay the complete GAP record. A caller retrying the same interval then appends another GAP record. Before this patch, `LogIndex::onRecord()` allocated one fixed slot per record; duplicates could fill `MAX_GAPS=512` and set `gapOverflow_`, making later closure coverage unprovable even though the logical coverage interval had not changed.

The test-first regression failed before the fix: after one persisted complete write plus 512 retries, the index reported 512 entries instead of one. The focused process exited 1. This confirmed the defect rather than relying only on source inspection.

## Implementation review

- `LogIndex` now stores GAP coverage as a sorted union of disjoint half-open spans.
- The insertion scan finds the first potentially touching span and merges each subsequent span whose start is at or before the growing end. It inserts or compacts by shifting the fixed array; it does not allocate heap memory.
- `gapOverlaps()` therefore returns the same result as scanning the union of original GAP records. `gapCount()` now reports merged disjoint spans, not raw `EV_GAP` record count.
- `MAX_GAPS` still bounds genuinely disjoint spans. Once that bound is exceeded, `gapOverflow_` remains sticky and coverage remains fail-closed.
- Zero-length spans and spans ending beyond `UINT32_MAX` set `gapOverflow_` and are not cached. An end exactly at `UINT32_MAX` is accepted.
- All original GAP records remain in the append-only log. There is no record rewriting, deduplication on disk, new sequence format, or change to `appendGaps()` commit grouping.

The sorted insertion/merge path is bounded by the fixed 512-entry array and uses no dynamic allocation. No ESP32 timing or stack/heap measurement was made; this is source and host-test evidence only.

## Regressions

1. Fake storage can now append all bytes then return false once. The test proves the original `LogStore` faults, a fresh instance accepts the complete record, and a retry is appended normally.
2. One initial persisted GAP plus 512 retries yields 513 raw records but one derived coverage span and no overflow.
3. A `ClosureEngine` attached after the retries rebuilds the same one-span index; a later session outside that GAP remains covered.
4. Overlapping and adjacent spans merge transitively; a disjoint span remains separate.
5. Zero-length and overflowing-end spans fail closed; the exact `UINT32_MAX` endpoint is accepted.
6. More than 512 genuinely disjoint spans still set overflow, and downstream closure still writes Unverified coverage.
7. Restore replay rebuilds its index using the changed projection and retains its existing expected status.

## Verification

Exact final command:

```sh
pio test -e native -f 'test_coreb2' -f 'test_restore_replay'
```

Result: **29/29 passed**, exit 0 (`test_coreb2` 14/14; `test_restore_replay` 15/15; 40.395 seconds aggregate suite time).

The `test_coreb2` suite also includes the existing randomized index/cache-vs-replay checks. No other native suite was run. No full regression was claimed.

## Residual risk and limits

- `appendGaps()` remains non-idempotent at the persistent log layer. Repeating an interval still writes duplicate records and consumes log storage. Durable request identity/reconciliation remains U112.
- The fake backend does not model LittleFS/VFS short writes, `fsync`, close errors, or actual reboot/power-loss behavior.
- No target build or physical device verification was performed. C13 (mount ownership, truncate/quarantine and power-loss recovery) and C3 (production firmware root/scheduler/routes) remain blocking.
- No SF2 source, hardware mapping, term policy, signed rule, delegated decision, `HANDOFF_FINAL.md` or `docs/DECISIONS.md` was changed.
