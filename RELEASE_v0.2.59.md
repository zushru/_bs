# Release v0.2.59 — coalesced GAP coverage index

Date: 2026-11-09  
Base: complete v0.2.58 archive extraction  
Base archive SHA-256: `431caa995cea1b1a315c9c3c80758c444706cb69d11b4189c4fa04fb8a09e4d5`  
Project root during development: `/root/att`  
Release status: development snapshot; C3 and C13 remain blocking

## Scope

1. Fixed U113 in the derived `LogIndex`: overlapping and adjacent half-open `EV_GAP` intervals are coalesced into a sorted union of disjoint spans. `gapOverlaps()` retains the same union-of-records meaning, but repeated retry records and contiguous chunks do not consume separate `MAX_GAPS` entries.
2. Kept every original `EV_GAP` record in the append-only log. No event encoding, record layout, sequence semantics, signed product rule or delegated storage-recovery decision changed.
3. Added fail-closed handling in `LogIndex` for zero-length GAP spans and spans whose computed end exceeds `UINT32_MAX`. The largest representable end (`UINT32_MAX`) remains accepted.
4. Added a fake-storage regression that writes a complete GAP record but returns false, reopens the same bytes, then retries the same interval 512 times. All 513 log records remain present while the derived index retains one span; a subsequent unrelated session remains covered.
5. Added interval-union, malformed-boundary and distinct-span overflow tests. Overflow is still raised by 513 genuinely disjoint spans and still forces Unverified closure coverage.

## Verification

Test-first command: `pio test -e native -f 'test_coreb2'`  
Initial result: expected failure in the new retry assertion — expected one indexed span, observed 512; process exit 1. This reproduced U113 before the implementation change.

Final command:

```sh
pio test -e native -f 'test_coreb2' -f 'test_restore_replay'
```

Result: **29/29 passed**, exit 0; `test_coreb2` 14/14 and `test_restore_replay` 15/15; 40.395 seconds aggregate suite time.

Source inventory: **584 C++ registrations across 35 suites**; **555 were not run** in this increment. This is targeted host-test evidence only.

## Remaining limits

- `appendGaps()` is still not exactly-once. Duplicate retries remain as separate append-only log records and consume storage. Index coalescing does not provide request identity or prevent duplicate disk writes; U112 remains unresolved.
- Tests use synthetic in-memory storage. No actual ESP32 LittleFS/VFS append, sync or close fault was injected.
- No ESP32 target build, ELF inspection, physical device, power-cut/brownout, full native suite or SF2 suite was run.
- Production firmware composition remains absent (C3); LittleFS truncate/quarantine and power-loss recovery remain unverified (C13).

## Changed files

- `lib/core/src/log_index.cpp`
- `lib/core/src/log_index.h`
- `lib/core/src/log_store.h`
- `test/test_coreb2/fakes.h`
- `test/test_coreb2/test_main.cpp`
- `README.md`, `RELEASE.md`
- `docs/AUDIT_v0.2.59.md`, `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/SPECS/month_set.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`
- `tools/verify_v0.2.59.sh`
- `MANIFEST.sha256`

`HANDOFF_FINAL.md` and `docs/DECISIONS.md` were not changed. No signed or delegated decision was altered. No firmware was flashed.
