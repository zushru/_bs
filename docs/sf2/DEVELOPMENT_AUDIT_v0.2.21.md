# SF2 development audit — v0.2.21

Date: 2026-11-07  
Baseline: complete v0.2.20 archive  
Scope: logical reporting-metadata model and snapshot-coordinator architecture only

## Verdict

**Accept as a development/reference increment; do not treat as a device-ready metadata or export subsystem.** The bitemporal model is useful for separating effective dates from the revision at which a correction was recorded, but physical persistence, transaction durability, PII protection, backup integration and month-level identity selection remain unresolved.

## Source changes

- Added `web/sf2/metadata_history_model.mjs`.
- Added `web/sf2/test/metadata_history_model.test.mjs` with 26 authored cases.
- Added metadata and snapshot coordinator design documents.
- Updated SF2 status, test inventory, critical/unverified registers and release docs.
- No C++ source, C++ test, attendance event, Config, RosterStore, P9 mapping or hardware source changed.

## Specific audit findings

### 1. Bitemporal semantics

Each record has an effective interval `[validFrom, validToExclusive)` (or one `effectiveDate` for a movement) and is introduced/revoked at a monotonically increasing transaction revision. Corrections append a revoke and replacement. This preserves what the model knew at an earlier revision.

Limitation: the in-memory model requires the entire transaction history from revision 1. No checkpoint, compaction, migration, bounded replay or physical commit protocol exists. A history that grows without limit cannot be assumed to fit ESP32 storage or RAM.

### 2. Identity and section invariants

- LRN is a 12-digit string when present; leading zeros are preserved.
- LRN is not the primary key. P9 device IDs remain the never-reused sensor slot IDs.
- One effective section profile is permitted at a time.
- A learner cannot have overlapping identity versions, and the same active LRN cannot overlap between different device IDs.
- Missing identity and school metadata yields an incomplete reporting state rather than an operational-attendance failure.

Limitation: this validator does not yet cross-check each metadata interval against `RosterStore` enrollment intervals at every report date. It also does not select the official display identity for a month when name/LRN/sex changes partway through that month. U63 remains open.

### 3. Movement semantics

Movement is an explicit dated reporting fact. It is not inferred from roster activation/deactivation because the current operational roster cannot distinguish transfer, dropout/NLS, shifting or other causes. Reason/destination may remain missing and generate warnings.

Limitation: no approved school movement code list, actor attribution or secure correction workflow exists.

### 4. Snapshot consistency design

The design captures the exact `LogStore` open generation, live count/last sequence, `EventGroupAudit` generation, Config generation, roster prefix, calendar/schedule completeness and metadata revisions. It proposes bounded, short-lived snapshot tickets and pages rather than a single large synchronous response.

Limitation: this is documentation only. No lock, page reader, source revision registry, ticket store, HTTP adapter or browser page assembler exists. Existing `RosterNameHistoryReader` aborts when the log grows and cannot be reused unchanged for a multi-request export.

### 5. PII/security and recovery

The logical metadata includes LRN, legal name components, sex and school identity. C6/C16 remain unresolved; at-rest/in-transit confidentiality is not established. O8 backup currently covers Config + roster + log, not a future metadata journal. A metadata store cannot be enabled until it is a required component of authenticated backup/restore and the recovery protocol is tested.

### 6. Corrected reference-validator cases

Source review caught a false `VALID` expectation: the shared fixture intentionally omits adviser, school-head and academic-period metadata, so its correct state is `INCOMPLETE` with no structural issues. The expectation was corrected. Additional checks now reject duplicate IDs in the expected learner set, invalid IDs in movement filters, and unsupported/blank metadata lookup subjects.

## Verification evidence

- `node --check` passed for all `.mjs` modules under `web/sf2/` and all test files under `web/sf2/test/`.
- This was syntax-only. No `node --test`, `npm test`, C++ test, PlatformIO, target compile, device, API or XLSX test was run.
- 26 new JS test registrations; 170 total SF2 JS registrations, all unrun.
- 343 C++ source registrations across 21 suites are unchanged; 25 v0.2.20 registrations remain unrun.
- The only previously reported native pass result remains the user's 318/318 on v0.2.17.

## Release gates still blocking production

- C24: full authoritative historical roster/movement projection.
- C25: strict event-group audit attached in the live firmware path and full transition semantics.
- C26: physical metadata store, commit protocol, capacity, compaction and migration.
- C27: PII confidentiality at rest and in transit.
- C28: bounded snapshot protocol and measured main-loop/heap cost.
- C29: metadata included atomically in backup/restore.
- C17/E2, C18/E26: historical timetable and term-end rules.
- C19/C20/C23: custom cell legend acceptance, official template, Late/TARDY and teacher-dependent daily semantics.

## Conclusion

v0.2.21 improves the **logical data model** but does not move the project to production reporting storage or live snapshot delivery. Next work should focus on authoritative roster-transition audit and prefix-bounded replay before adding any metadata persistence or endpoint.
