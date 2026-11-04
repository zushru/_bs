# SPEC: Core B fixes (docs/patches/core_fixes.patch, applied in this tree)

Status: MEASURED on device as part of the 122 and 142 runs. Origin: ChatGPT review finding plus my verification. Tests: test_hardening.

## Changes to handoff files
| File | Change |
|---|---|
| log_store.h / .cpp | makeGapEvents gains optional `uint32_t* next`. New appendGaps(log, from, to, cause) writes as many commit groups as needed. attach() refuses duplicate registration and rolls back its slot and reset if the replay fails |
| closure_engine.cpp | Long outage gaps use appendGaps (was a single group, which silently dropped the tail of a span over about 640 to 730 days) |
| clock_policy.cpp | applyClockSet writes earlier GAP groups first, then the last chunks with the CLOCK_SET pair |
| test_coreb2/test_main.cpp | Two tests use distinct observers (double registration is now refused). Changed by fix_b2_tests.py |

## Invariant
A gap span is either fully covered by GAP records or the write reports failure. Chunks already written remain true gaps, so a retry only adds overlap.

## Known gaps
- Clock-set GAP path is tested with a 4-year span on the host only.
