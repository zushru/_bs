# SF2 D3 — specialized development audit (v0.2.25)

Date: 2026-11-07  
Status: source-level/static audit. Functional tests intentionally not run.

## Scope

- Exact JSON representation for C++ `RosterLogPrefix.openGeneration` (`uint64_t`).
- Reconciliation of the D1 month-overlap roster and D2 bitemporal identity/movement projection.
- Snapshot-fragment mapping and the distinction between structural validity and authority.
- Static JavaScript syntax, test registration accounting and documentation consistency.

## Findings corrected in this increment

1. **The v0.2.24 prefix contract was not wire-safe for full `uint64_t`.** It represented `openGeneration` as a JavaScript number and rejected values outside the safe-integer range. v0.2.25 makes canonical unsigned decimal text the only representation, accepts the entire positive `uint64_t` range and rejects numeric, signed, padded, zero and overflow values. The remaining prefix counters stay within `uint32_t`.
2. **D1 and D2 had no shared prefix helper.** D1, D2 and the host snapshot-coordinator model now share one validator/comparator, avoiding separate, drifting rules for exact source identity.
3. **The D1→D2→snapshot join was missing.** `joinReportingRosterForSnapshot()` now checks month, exact prefix, learner ID set, intervals, identity, profiles and movements before mapping a snapshot-compatible fragment.
4. **The join previously relied too heavily on the upstream interval projections.** It now independently rejects intervals that do not overlap the requested month, are unsorted or overlap each other—even when both input projections contain the same forged interval list. It also requires D2 diagnostic collections to be actual arrays; malformed `issues`/`warnings` objects cannot be mistaken for an empty blocking-issue list. Adjacent half-open intervals remain valid.
5. **The join previously passed some malformed profile/identity field shapes downstream.** It now checks required section fields, legal first/last names, nullable name components, LRN shape and sex before constructing the fragment. Movement text fields are null/undefined or non-empty strings, not blank strings.
6. **The initial join test referenced nonexistent top-level result fields.** The contract returns these fields only inside `snapshotFragment`; the test now checks the actual return shape. This was a source-review correction, not an executed test failure.
7. **Malformed upstream diagnostics could bypass a shallow shape check.** The D3 join now requires D2 `issues` and `warnings` to be arrays, rather than allowing a non-array `issues` object with no `length` property to pass as an empty issue list.
8. **Schema validity could be misread as authority.** The adapter now labels a valid pure calculation `PREVIEW_READY`, returns `authoritativeExportReady:false`, and exposes `authority.state:NOT_ESTABLISHED`. The D3 join always remains `INCOMPLETE` for missing authenticated identity review evidence.

## Current architecture boundary

- D1 is still a read-only reference projection from one exact roster-log prefix.
- D2 is still an in-memory bitemporal reporting identity and movement reference projection.
- D3 consumes these proposed object shapes, checks internal consistency and maps only legal identity fields. Operational display labels are not used.
- The resulting fragment is not a device-authenticated snapshot. A caller can fabricate D1/D2-shaped objects or bypass this browser join and construct a schema-valid snapshot directly.
- `validateSnapshot()` remains a structural/semantic schema validator. `PREVIEW_READY` is not export authorization, and there is no XLSX writer.
- No PII persistence, API response, event-log PII record, firmware composition root, source lock, authenticated reviewer model or signing key was implemented.

## E27 and product rules

E27 remains **DELEGATED** to assistant judgement as explicitly accepted by the user on 2026-11-07. Same-local-day ACTIVATE/DEACTIVATE histories remain fail-closed for authoritative reporting only, because the existing roster projection stores local-day half-open intervals and cannot establish AM/PM session eligibility across intraday lifecycle transitions or RTC rollback. Live roster events are not rejected or rewritten. No attendance mark semantics, P9 identity mapping or operational roster parser changed.

## Verification and test accounting

- Node registrations in v0.2.25: **17 new** — 4 source-prefix, 10 D1/D2 join, 1 snapshot-adapter and 2 snapshot-coordinator. Total: **256**. All are authored but unrun.
- Static `node --check` passed for all **25** `.mjs` modules under `web/sf2/`. Syntax only; no Node test runner was executed.
- C++ registrations remain **376 across 23 suites**. The user-reported **318/318** PlatformIO native result is for v0.2.17 only; 58 later registrations remain unrun. v0.2.25 changed no C++ source or test registration.
- No C++ compiler, PlatformIO, ESP32 target build, firmware integration, device test, API test, persistence/power-cut test, XLSX generation, round-trip or print test was performed.
- Final archive was independently extracted; all 187 `MANIFEST.sha256` entries verified, with 188 extracted files including the manifest. The `.tar.gz` SHA-256 sidecar was independently compared with the packaged archive.

## Remaining blockers / next work

1. Review `REVIEW_EVIDENCE_DESIGN_v0.1.md` before implementing any reviewer-verification path. Decide whether individual reviewers are required and how they authenticate offline; a shared admin PIN cannot prove an individual reviewer.
2. Resolve the accepted source categories, reviewer permissions, time-quality rules, canonical identity digest, signature model and key provisioning. Do not assume that an RTC timestamp is trusted time.
3. Resolve C6, C13, C16, C25, C26, C27, C28 and C29 before metadata persistence or a PII endpoint. Specifically: caller security enforcement, power-loss semantics, encryption/key provisioning, source audit wiring, metadata backend, PII protection, snapshot lock/paging and backup/restore coverage.
4. Keep official SF2 template normalization, school acceptance, one-session-day mapping, TARDY totals, Semester mapping and month identity-correction policy open pending evidence/teacher confirmation.

## Decision required

No implementation decision is required to accept this reference-model increment. Batch D4 should begin with review of reviewer identity, evidence-source and trusted-time policy—not with a PII storage or HTTP implementation.
