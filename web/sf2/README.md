# Browser-side SF2 reference model

Status: **design/reference code only**. Not integrated with the ESP32 dashboard, HTTP API, persistent reporting stores, or XLSX writer.

## Modules

- `attendance_model.mjs`: DOM-free AM/PM daily conversion, per-learner monthly totals, month-overlap selection.
- `learner_layout.mjs`: semantic male-then-female learner row planning and pagination. Page capacity must be measured from an approved, normalized template; no default or fixed four/45-row ceiling exists.
- `section_model.mjs`: section-level completeness checks and daily male/female/combined totals.
- `snapshot_schema.mjs`: validates the proposed revision-bound snapshot envelope, full-month calendar, month-overlap roster, enrollment intervals, attendance provenance, coverage and movement references.
- `snapshot_adapter.mjs`: adapts a structurally validated proposed snapshot into the pure section calculation as `PREVIEW_READY` only. It never grants authoritative export readiness or generates a workbook.
- `preview_model.mjs`: produces record-oriented diagnostics for missing/unresolved session results, coverage gaps and unclosed sessions. It always reports `finalExportReady: false` until an approved template writer exists.
- `snapshot_coordinator_model.mjs`: pure host/browser reference for source revision capture, short-lived ticket shape, page assembly, duplicate/missing page rejection and monotonic expiry. Its C++ `uint64_t` open generation is represented as decimal text. It is not a production server lock, auth system, secure ticket generator, persistence layer or HTTP API.
- `source_prefix.mjs`: exact JSON contract for the C++ roster source prefix.
- `reporting_snapshot_join.mjs`: D1/D2 to snapshot-compatible learner/profile/movement join; never returns authoritative readiness.
- `lib/core/src/roster_snapshot.*` provides the C++ prefix identity, roster-transition audit and scratch-roster replay used by the future device snapshot builder. The browser ticket model does not call or verify that C++ code.
- This browser code remains separate from the C++ source projection; no API adapter is wired yet.
- `fixtures/snapshot_fixtures.mjs`: synthetic November 2026 report fixture with mid-month enrollment, transfer-outs, Late, Excused, confirmed absences and coverage evidence.

## Proposed snapshot contract

See `../../docs/sf2/SNAPSHOT_SCHEMA_v0.2.md` and `../../docs/sf2/CORE_SOURCE_MAPPING.md`. The contract remains proposed. The ESP32 does not yet implement the endpoint and browser validation cannot prove that a future server actually created an immutable snapshot.

The proposed snapshot must contain every date in the report month. It must tie roster, identity metadata, section profile, school profile and calendar revisions back to one source revision tuple. Calendar rows identify each class/non-class day; coverage rows identify every scheduled session. An absence is accepted only when the matching session is closed, coverage-complete, log-healthy and gap-free, and the learner is enrolled on that date.

## Rules carried forward

- One configured section and curriculum profile per device.
- Include each learner whose enrollment overlaps any part of the report month, including transfers out before export.
- Preserve the existing internal marks; use the signed local AM/PM cell convention only in this adapter.
- `E` counts as Present in the monthly day total.
- Late is preserved and provisionally treated as attended for quadrant selection; TARDY total stays deferred.
- Whole-day non-class dates are excluded; one-session class days remain blocked pending R8.
- Missing/invalid records and untrusted absence evidence block final calculation.

## Tests

Node's built-in test runner, no external package dependencies:

```sh
cd web/sf2
npm test
```

v0.2.19 adds 89 authored registrations: 67 snapshot-schema, 12 snapshot-adapter and 10 preview-model cases. Together with the 55 v0.2.18 SF2 cases, there are 144 registered SF2 JavaScript cases. **None have been run.** The C++ `EventGroupAudit` and roster-history projections are independent source modules and do not make this browser contract a production API.

## v0.2.22 coordinator model boundary
The coordinator model requires an explicitly current revision vector before each page is accepted/finalized. It accepts caller-supplied monotonic times and source snapshots; it does not generate a cryptographic ticket ID, acquire a device lock, or make stores immutable. Its page-size and TTL limits are model-only placeholders and are not approved ESP32 protocol limits.

There are 257 Node registrations across all SF2 modules. The complete suite passed 257/257 locally in v0.2.37 after fixing the v0.2.36 failures. All 25 `.mjs` modules also pass `node --check`. This is host/reference-model validation only; it does not establish browser UI integration, ESP32 snapshot correctness, authenticated reporting or XLSX export readiness.

- `month_roster_projection.mjs`: projects prefix-pinned enrollment intervals; operational `RA_SET` display-name revisions are separate diagnostics and never supply legal report names.
- `reporting_identity_projection.mjs`: joins the month-overlap roster with bitemporal legal reporting identities and explicit movement history at one metadata revision. Legal identity changes during enrolled days block the month report; missing identity/movement facts remain incomplete rather than being inferred. `SOURCE_CHECKED` remains an unaudited claim: output stays `UNVERIFIED` until reviewer attribution and immutable verification evidence are designed. Multiple movement records for the same learner/date block rather than being ordered arbitrarily.

## v0.2.24 reporting identity boundary
Metadata history is schemaVersion 2. `LEARNER_IDENTITY.data.legalName` is distinct from operational `RA_SET` display labels. `projectReportingRoster()` requires a pinned metadata revision and covers only enrolled days within the report month. It blocks an effective legal identity change during those days because U63 has no school-approved first-day/end-day selection rule. Movement taxonomy includes `WITHDRAWN` and raw `NLS`; `effectiveDateMeaning` is explicit or `UNCONFIRMED`. No persistence or PII endpoint exists. E27 is DELEGATED to remain fail-closed for same-day roster lifecycle transitions in reporting.


## v0.2.25 cross-layer boundary

The D3 join rechecks exact source-prefix equality, learner IDs, enrollment intervals and movement semantics. `validateSnapshot()` can still return `VALID` for structurally sound input that a caller constructed independently; that is not upstream proof. `calculateSnapshotMonth()` now returns `PREVIEW_READY` only and always sets `authoritativeExportReady:false` until a trusted identity-review verifier exists. See `../../docs/sf2/REPORTING_SNAPSHOT_JOIN_v0.1.md` and `../../docs/sf2/REVIEW_EVIDENCE_DESIGN_v0.1.md`.


## v0.2.26 security boundary

The browser-side calculations remain preview-only. D4's reviewer-evidence architecture is documented in `../../docs/sf2/REVIEW_EVIDENCE_DESIGN_v0.2.md`; no browser credential flow, signature verifier, trusted key registry, PII store or API exists. A browser-supplied `verified` flag or `SOURCE_CHECKED` string must never authorize an export.


## Identity review status (v0.2.30)

D4 policy is specified in `docs/sf2/REVIEW_EVIDENCE_DESIGN_v0.6.md`: one ADMIN self-review, four school-configurable source categories, revision-bound review, commit-sequence ordering, and advisory-only RTC time. This remains design only. No verifier or PII persistence exists. O8 backup scope remains config + roster + log; reporting identity persistence and authoritative export remain disabled until an approved recovery path exists.
