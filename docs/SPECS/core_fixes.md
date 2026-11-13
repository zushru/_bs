# SPEC: Core B fixes (docs/patches/core_fixes.patch, applied in this tree)

Status: MEASURED on device as part of the 122 and 142 runs. Origin: ChatGPT review finding plus my verification. Tests: test_hardening.

## Changes to handoff files
| File | Change |
|---|---|
| log_store.h / .cpp | makeGapEvents() reports bounded progress via optional next; appendGaps() writes multiple groups only after full-span encodability preflight; v0.2.63 uses overflow-safe ceil-minute arithmetic, bounds chunk ends to UINT32_MAX and rejects unrepresentable rounded spans before any append. attach() refuses duplicate registration and rolls back its slot and reset if replay fails |
| closure_engine.cpp | Long outage gaps use appendGaps (was a single group, which silently dropped the tail of a span over about 640 to 730 days) |
| clock_policy.cpp | applyClockSet writes earlier GAP groups first, then the last chunks with the CLOCK_SET pair |
| test_coreb2/test_main.cpp | Two tests use distinct observers (double registration is now refused). Changed by fix_b2_tests.py |

## Invariant
A representable gap span is preflighted before appendGaps() or applyClockSet() starts writing. A later multi-group append can still fail after a valid prefix is persisted; retry may append duplicate GAP records. LogIndex coalesces overlaps only in its derived coverage index and does not deduplicate persistent records.

## Known gaps
- Clock-set GAP path is tested with a 4-year span on the host only.
