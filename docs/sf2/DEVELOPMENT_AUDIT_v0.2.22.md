# SF2 development audit — v0.2.22

Date: 2026-11-07  
Baseline: v0.2.21 complete source archive  
Scope: historical roster prefix reconstruction, roster transition semantics, historical display-name reading, snapshot revision/page model, documentation and archive packaging.

## Verdict

**Accept as a development-only source snapshot; do not treat as production-ready or as an authoritative reporting API.** The changes improve the ability to reconstruct one coherent historical roster prefix without mutating the live roster. They do not solve reporting identity, metadata persistence, calendar reconstruction, PII protection, or production snapshot delivery.

## Changes in this increment

1. `RosterLogPrefix` binds a read to the current `LogStore::openGeneration()`, record count, last sequence and `EventGroupAudit` generation.
2. `RosterTransitionAudit` checks the chronology/state of roster events before they are used for reporting: first SET, rename effective-day order, valid deactivate/activate transitions, no duplicate transitions, no action before initial SET, and no same-day activate/deactivate ambiguity.
3. `replayRosterPrefix()` reconstructs a caller-owned scratch `RosterStore` from exactly the captured prefix, checks transition chronology and confirms final known/active state. The live roster is not mutated.
4. `RosterNameHistoryReader` can bind to an explicit prefix; its original constructor remains strict and captures its own prefix.
5. `snapshot_coordinator_model.mjs` provides a pure host-side revision-vector, ticket, page-assembly and expiry reference model. No server implementation was added.
6. A source audit found that the transition audit offset and scratch roster offset could differ. Added `RosterStore::utcOffsetMin()`, `RPR_OFFSET_MISMATCH`, a guard before scratch reset, and one regression registration.

## Critical findings and residual risks

### A. E27 remains OPEN

The day-granular roster interval model uses `[startDayInclusive,endDayExclusive)`. The new reporting audit rejects a deactivate/reactivate on the same local day because it cannot represent intraday boundaries reliably. This is a conservative reporting guard, not a change to the live `RosterStore` parser or a signed product rule. Real operational histories must be inspected before the rule is approved or relaxed.

### B. Backwards RTC corrections are not repaired

The audit rejects per-student roster event dates that move backwards. It does not infer corrected historical times or rewrite intervals. A legitimate backdated correction requires an explicit migration/correction model.

### C. Scratch replay constraints

- Caller must allocate the scratch `RosterStore` with static/heap storage; the object is approximately 16 KiB and must not be put on a small task stack.
- Scratch and transition audit must use the same UTC offset; the new guard enforces this.
- `RosterPrefixReplayResult::RPR_OK` is required before any scratch data is used. On failure, discard the entire scratch roster and any streamed names.
- Prefix checks protect against changes visible through `LogStore`/`EventGroupAudit`; direct out-of-band `IStorage` writes are not observed. Production must enforce one serialized writer and prevent direct raw-storage mutation while a snapshot is active.
- `RosterTransitionAudit::matches()` checks known/active state. The interval table is independently rebuilt by replaying the full prefix into a fresh scratch `RosterStore`; it is not compared to a separately persisted canonical interval table because none exists.

### D. Event log validation remains partial

`EventGroupAudit` is a current-writer structural and selected-field gate, not a complete semantic validator or cryptographic verifier. It rejects event types without a registered schema validator and is not attached by the production firmware. A ready log open alone is not enough to authorize a report.

### E. Reporting remains incomplete

The projection contains device student IDs, enrollment intervals and 32-byte operational display-name revisions only. It does not contain LRN, sex, legal name components, transfer/NLS reasons or destinations, school/section metadata, curriculum, or a historically complete school calendar. It cannot yet populate SHSF-2.

### F. Snapshot model is not a server

The JS coordinator has no ESP32 lock, authentication, cryptographic ticket ID, persistent ticket lifecycle, immutable source store, HTTP endpoint, response-size benchmark or loop-latency evidence. The page-size and TTL limits are placeholders, not approved firmware limits.

## Decisions preserved

- P9 student ID = never-reused sensor slot remains unchanged.
- E2, E18 and E26 remain unchanged.
- E27 is OPEN.
- The signed single-section, curriculum-profile, month-overlap roster, local AM/PM convention and E-as-Present reporting decisions remain unchanged.
- Teacher-dependent TARDY, one-session no-class, `Semester` and custom-legend acceptance remain open/provisional.

## Source/test inventory

- C++ source registrations: **360 across 22 suites**.
  - 318/318 passed is user-reported for v0.2.17 only.
  - 25 v0.2.20 cases and 17 v0.2.22 cases are authored but unrun.
- SF2 Node registrations: **187**, all unrun.
- `node --check` passed for every `web/sf2/*.mjs`, `web/sf2/test/*.test.mjs` and `web/sf2/fixtures/*.mjs` file. Syntax checking is not functional verification.
- No C++ compiler, PlatformIO, Node test runner, ESP32 target build, sensor/device run, HTTP benchmark, XLSX round-trip or print test was performed.

## Packaging audit

- Entire archive extracted before checksum verification.
- `.git/`, `.pio/`, `node_modules/` and cache directories excluded.
- `MANIFEST.sha256` regenerated from the complete source tree; archive contents and manifest were independently checked after extraction.
