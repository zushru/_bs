# SF2 D2 — specialized development audit (v0.2.24)

Date: 2026-11-07  
Status: source-level and archive-integrity audit. Functional tests intentionally not run.

## Scope

- E27 delegated reporting behavior.
- Separation of operational display labels and legal reporting identity.
- Bitemporal metadata history at an explicit transaction revision.
- Movement categories and effective-date semantics.
- Cross-layer validation of month-roster intervals and source-prefix generations.

## Implemented

1. E27 is **DELEGATED**: same-local-day activation/deactivation histories remain blocked for authoritative monthly reporting only. Committed operational events are not rejected or rewritten.
2. Operational `RA_SET` display names remain diagnostic/UI values. They are never copied into legal SF2 name fields and changing/missing labels alone do not block the legal identity projection.
3. Metadata schema version 2 stores explicit legal-name components, LRN as a 12-digit string, sex and a source-status claim, all keyed by the existing never-reused device student ID. P9 is unchanged.
4. The selected metadata revision is independently materialized and checked for overlapping active records and LRN uniqueness. A later revocation cannot make an invalid earlier report revision appear safe.
5. `projectReportingRoster()` resolves identity coverage only across dates when the learner was enrolled. Missing or changed identity is not guessed.
6. Movements remain explicit bitemporal records. Same-learner/same-day collisions block; confirmed transition meanings are checked against enrollment interval boundaries. `NLS` must use `UNCONFIRMED` and never implies an exit boundary.
7. `SOURCE_CHECKED` remains an unauthenticated claim. Projection output always says `UNVERIFIED` and carries an incomplete warning because reviewer identity, verification time, source reference and immutable evidence are not modeled.
8. The D1-to-D2 boundary rechecks enrollment interval validity/overlap, learner object shape, student ID uniqueness/range and source-prefix identity.
9. Source-prefix field widths now match the C++ declarations: `openGeneration` is `uint64_t` and is accepted in JavaScript only while represented exactly as a positive safe integer; `recordCount`, `lastSeq`, and `auditGeneration` are `uint32_t`. Values beyond JavaScript's safe integer range are rejected pending an exact decimal-string wire representation.
10. Legal identity comparison now canonicalizes the name fields. JSON object key insertion order is not treated as a change to the learner's legal identity.

## Specialized findings and limitations

- `SOURCE_CHECKED` must not be interpreted as verification. No trusted reviewer, time, evidence reference or signed audit event exists.
- Same-month legal identity changes block rather than choosing first-day or last-day identity. School direction is still needed.
- Multiple movement records for one learner/date block. NLS stays unresolved and cannot be interpreted as withdrawal, transfer or dropout.
- Same-day activation/deactivation remains report-blocking under delegated E27 because current operational intervals are local-day granularity, not session/time granularity.
- The reporting metadata is a logical/browser reference model only. There is no durable metadata store, migration, compaction, encryption, authenticated API, snapshot server, backup integration or XLSX writer.
- No real learner PII should be entered into this reference model or exposed through an API until C6/C16/C26-C29 are addressed.
- `openGeneration` must be encoded as a decimal string if an eventual JSON API needs to support the full `uint64_t` range; current browser reference accepts only safe-integer numeric values and deliberately blocks larger values to avoid precision loss.
- A school-defined report-month rule is still needed for legal identity corrections effective mid-month (U63).

## Verification

- C++ `RUN_TEST` registrations: 376 across 23 suites. No C++ source or test registrations changed in this increment; later registrations remain unrun.
- SF2 Node registrations: **239** total, **34 added in v0.2.24**. Per-suite totals: attendance model 29; learner layout 11; metadata history 36; month roster 18; preview 10; reporting identity 21; section model 15; snapshot adapter 12; snapshot coordinator 17; snapshot schema 70.
- All 21 SF2 `.mjs` source, fixture and test files passed `node --check` syntax validation. This is not functional test evidence.
- No Node functional tests, C++ compiler, PlatformIO, ESP32 build, hardware test, metadata persistence, API integration, backup/restore test, XLSX round-trip or print test was performed.
- Archive manifest and SHA-256 integrity were verified after packaging. `.git/`, `.pio/`, `node_modules/` and caches are excluded.

## Install

Replace the complete `att/` directory from the full archive. Do not merge individual files.
