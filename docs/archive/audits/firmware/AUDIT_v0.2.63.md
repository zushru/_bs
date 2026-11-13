# Audit v0.2.63 — GAP epoch arithmetic and all-or-reject preflight

Date: 2026-11-09
Project root: /root/att
Base archive: v0.2.60, SHA-256 96eeacef35103f5e9ba76c5ffb036fb926d9c8aa33bb2f8dec71681206aa2fae

## Finding

The previous makeGapEvents() rounded minutes with (to - cur + 59) / 60 in uint32 arithmetic. For large spans, +59 could overflow. Near UINT32_MAX, rounding a final partial minute upward could also emit an EV_GAP whose epoch + minutes*60 exceeded the representable uint32 epoch domain. A chunk helper that merely clamps each emitted chunk can return a representable prefix while a remaining tail is impossible to encode.

That partial-prefix behavior is acceptable only for a caller that explicitly owns/resumes progress. It is not acceptable for appendGaps() or applyClockSet(), which promise to encode the complete requested range and must not start writing an impossible request.

## Implementation

- Rounded up without addition overflow: remaining / 60 + (remaining % 60 != 0).
- Capped each chunk to (UINT32_MAX - cur) / 60 whole minutes and emitted no zero-minute chunks.
- Added gapSpanEncodable() using uint64 intermediates. It rejects reversed ranges and any rounded whole-minute endpoint beyond UINT32_MAX.
- appendGaps() preflights the entire span before writing its first group.
- applyClockSet() validates the decision/action/source and then preflights the entire GAP span before writing any prefix or CLOCK_SET record.
- makeGapEvents() retains bounded prefix generation and communicates its exact next representable endpoint through the existing next pointer.
- Persistent EV_GAP and CLOCK_SET formats are unchanged.

## Test-first evidence and correction

- Added tests for a near-full uint32 range and an interval near UINT32_MAX. Before the arithmetic fix, the tests caught minute-rounding wrap and an invalid derived end.
- After making individual chunks safe, additional caller-level tests caught that appendGaps() and applyClockSet() still wrote a valid prefix before discovering the unrepresentable tail. These red tests failed with persisted record counts of 1 and 2 respectively.
- Added shared preflight before any write in both callers.
- Latest dependent command: pio test -e native -f test_coreb2 -f test_coreb -f test_hardening -f test_restore_replay — **91/91 passed**, exit 0.
- Final release verifier: ./tools/verify_v0.2.63.sh targeted — **246/246 passed across 12 suites**, exit 0, runtime 3:40.440.
- test_coreb2 proves bounded chunk endpoints and that appendGaps() rejects an unrepresentable range with zero storage mutation.
- test_coreb proves applyClockSet() rejects an unrepresentable interval with zero storage mutation, as well as prior typed-result behavior for successful, refused, faulted and uncertain writes.
- test_hardening and test_restore_replay passed to cover dependent gap append/closure and restored gap interpretation.

## Remaining concerns

- gapSpanEncodable() validates only the current integer representation. It does not prove RTC quality, intended civil-time semantics or physical clock behavior.
- Long representable spans can still persist multiple GAP groups. If a later append fails, a valid GAP prefix remains and applyClockSet() reports COMMIT_UNCERTAIN. A caller needs reconciliation/idempotency; none exists in production (C3/U112).
- Complete append failure retries can duplicate GAP records and consume storage; LogIndex coalescing only protects derived coverage capacity, not persistent log space.
- Partial writes and repair remain blocked by C13. Tests use in-memory host storage, not ESP32 LittleFS/VFS.
- No target build, ELF inspection, physical RTC, power-cut or hardware verification was performed.
