# SF2-SHS development plan

## Current stage: v0.2.81 — month-end and consecutive-absence section previews; XLSX writer still absent

Completed in v0.2.18-v0.2.23:
1. Recorded the reporting scope: Regular SHS and Strengthened SHS; one configured section per device; collect the section during setup; include all learners with enrollment overlapping any part of the report month; create one dynamic learner row per roster member; count `BLANK`/`E` as Present and `X`/both diagonal marks as Absent; preserve Late internally but omit Tardy from SF2.
2. Kept teacher-dependent rules provisional/open in `docs/sf2/TEACHER_QUESTIONS.md`. No response has been assumed.
3. Preserved the user-uploaded SHSF-2 workbook as reference-only. Its four learner placeholders cannot constrain output; the future writer must expand male/female learner blocks from the month roster and preserve subtotal rows. Its date-slot alignment, defined names, external link, merges and print settings still require normalization before writer selection.
4. Added a browser-side calculation foundation: daily session resolution, month-overlap roster selection, section totals, dynamic semantic row planning, snapshot schema/adapter and unresolved-record preview.
5. Added a bitemporal reporting metadata reference model. It is not persisted; no physical metadata encoding/backend is approved.
6. Added a revision-vector/ticket/page assembly reference model. It is not a server endpoint, source lock, authentication system or proof of an immutable ESP32 snapshot.
7. Added `EventGroupAudit` and read-only roster interval/name-history access. The audit is not attached in production firmware and is not a complete semantic/cryptographic verifier.
8. Added `RosterLogPrefix`, `RosterTransitionAudit`, prefix-bound name streaming and exact-prefix scratch-roster replay. The replay rejects a scratch `RosterStore` whose UTC offset differs from the explicit replay offset.
9. v0.2.23 adds a prefix-bound C++ `MonthRosterProjection`: it replays the exact prefix itself, validates intervals, selects every learner with at least one half-open interval overlapping the month, preserves every overlapping interval, reports enrollment on the final day separately, and checks scratch-roster mutation.
10. v0.2.23 adds browser `projectMonthRoster()` to reconcile the interval projection with the full prefix-bound name stream. It blocks missing names and distinct name changes during the report month instead of choosing one silently.
11. E27 is DELEGATED: same-day ACTIVATE/DEACTIVATE remains fail-closed for authoritative reporting only; it does not reject or rewrite operational roster events.
12. v0.2.25 adds D3 cross-layer join from D1/D2 to the proposed snapshot learner/profile/movement fragment, uses canonical decimal-string `uint64_t` source generations, and makes all browser calculations preview-only until authenticated identity evidence exists.

## v0.2.37 verification note

- Native C++: the user supplied a full PlatformIO summary of **413/413 passed across 23 suites** on v0.2.36, including all 44 `test_nvs_config` cases. This is host test evidence; no target build or hardware behavior is established.
- SF2 Node: v0.2.36 run supplied by the user was **238/257, 19 failed**. The failures were reproduced. v0.2.37 repairs month-roster source-sequence and ordering fixtures, blocker diagnostics expectations, malformed-input fixture, D1/D2 profile binding fixture, event sequence and coverage-gap fixtures. It also fixes preview state mapping, record-context diagnostic prioritization, and precise event status-mismatch classification. `npm test` now passes **257/257 locally**, and all 25 `.mjs` files pass `node --check`.
- D1/D2/D3 remain synthetic browser reference models. A passing suite does not establish a valid device snapshot, human identity review, or authoritative DepEd SF2 export. `PREVIEW_READY` remains non-authoritative; `finalExportReady` remains false.

## v0.2.81 preview increment

- `section_model.mjs` now emits `monthEndRegistration` (male/female/total) from validated enrollment intervals on the final calendar date. This is separate from the month-overlap row roster and counts a re-entered learner once.
- The model now emits `fiveConsecutiveAbsenceLearners` (male/female/total), counting distinct learners with at least five absent confirmed class dates in the report month. `X` and diagonals extend the run; `BLANK` and `E` reset it; a class date outside the learner's enrollment intervals resets it.
- The absence summary does not include prior-month carry-in. Adviser confirmation is still needed on class days versus calendar days and cross-month carry-in. These fields remain preview-only and are not workbook output.
- Focused validation passed 47/47 for `section_model.test.mjs`, `snapshot_adapter.test.mjs` and `preview_model.test.mjs`. The full SF2 suite was not run.

## Signed reporting decisions

- **SIGNED:** Both Regular SHS and Strengthened SHS are in scope; one device holds one curriculum/reporting profile and one section; ask for the section during initial setup.
- **SIGNED:** Learner rows are generated from every learner whose enrollment overlaps any part of the report month, including transfers out before export.
- **SIGNED:** Use the local HTML-builder AM/PM cell convention: AM-only = upper-left shading; PM-only = lower-right shading; both sessions present = blank; both absent = `x`; `E` = excused.
- **SIGNED:** `E` counts as Present in monthly totals. Preserve internal Late marks.
- **SIGNED:** Late is preserved internally and treated as attended for SF2 cell selection; no separate Tardy mark or tally is emitted.
- **OPEN:** One-session no-class mapping, missing-coverage resolution, `Semester` field meaning and acceptance of the customized legend by the school/LIS.
- **UNCHANGED:** P9 still binds the device student ID to the never-reused sensor slot. LRN is a separate reporting attribute, not a key. No attendance-core event or absence rule is changed by SF2.

## Current blockers before an authoritative month roster/snapshot

- **C24/C30/E27:** `RosterTransitionAudit` rejects backwards effective days, unknown/redundant lifecycle actions and same-day activate/deactivate ambiguity. E27 is DELEGATED to keep the reporting-only guard. The live `RosterStore` parser is unchanged. D2 now joins exact-prefix operational intervals with separate bitemporal legal identity and movement metadata, but no production persistence/API exists.
- **C25:** `EventGroupAudit` and exact-prefix replay exist as source helpers but are not installed in a production composition root. The group audit rejects event types without a validator and is not a complete domain/cryptographic verifier.
- **C26/C27/C29:** No durable PII metadata store, at-rest/in-transit privacy design or backup/restore integration exists. Do not persist or expose LRN yet.
- **C28:** Snapshot paging is only a pure model. No device-side lock/ticket/page reader, HTTP auth, target response measurement or loop-latency test exists.
- **C17/E2:** Historical timetable versions are not persisted, so current-timetable replay cannot prove historical coverage. E26 now selects an inclusive term end and aligns source boundaries, but current regressions are NOT RUN and no production day-view route uses the shared guard. Do not claim reliable historical absence/coverage until both conditions are closed.
- **C19/C20/C23:** The custom AM/PM convention and template version/print layout still need school confirmation; the user-selected no-Tardy SF2 rule is fixed locally, while the one-session non-class-day case remains open.

## Next safe increments

### Batch D1 — month-overlap roster projection (implemented in v0.2.23)
- The C++ projection and browser reconciliation model are implemented. The exact-source replay, half-open month overlap, interval preservation, month-end enrollment flag, historical name revisions, prefix/scratch mutation checks and fail-closed name ambiguity are documented in `MONTH_ROSTER_PROJECTION_v0.5.md`.
- The user-provided v0.2.36 native summary reports 413/413 cases passed across 23 suites. The v0.2.36 Node summary reported 238/257 and 19 failures; all 19 were independently reproduced. v0.2.37 corrects seven categories of invalid test fixtures/expectations and three preview/diagnostic implementation defects; the complete Node suite passes 257/257 locally. C++ source/tests are unchanged in v0.2.37. A clean full-archive rerun in the user environment remains the release confirmation.
- E27 is DELEGATED; D1 and D2 retain the reporting-only same-day lifecycle rejection.

### Batch D2 — reporting identity and movement alignment (implemented in v0.2.24)
1. `projectReportingRoster()` consumes the D1 month-overlap roster plus bitemporal metadata at an explicit revision.
2. Operational `RA_SET` display names are diagnostics only; legal report names are never copied from them.
3. Explicit legal identity, LRN, sex and movement facts are separate metadata records. Transfer/withdrawal/NLS are not inferred from deactivation.
4. Mid-month legal identity changes, missing identity coverage, same-day movement collisions and invalid movement boundary relationships fail closed or remain incomplete.
5. `SOURCE_CHECKED` is only a claim; output identity remains `UNVERIFIED` until reviewer attribution and immutable verification evidence exist.
6. Keep all school/learner PII fields absent from the live API until C26/C27/C29 are resolved.


### Batch D3 — cross-layer snapshot join (implemented in v0.2.25)
1. Centralized source-prefix validation. C++ `uint64_t openGeneration` is canonical unsigned decimal text in JSON; all other prefix counters remain bounded `uint32_t` values.
2. Added `joinReportingRosterForSnapshot()` to require exact D1/D2 month, source-prefix, learner-ID, enrollment-interval and movement alignment before mapping into snapshot-compatible fields.
3. Rechecked movement-date meaning and enrollment boundaries at the D2-to-snapshot boundary; operational display labels are excluded from the mapped fragment.
4. Revalidates intervals at the join boundary: every interval must overlap the report month, be sorted and non-overlapping, and exactly match between D1 and D2. It also checks mapped legal identity and section profile fields against snapshot requirements.
5. Added an integration fixture that maps D1 and D2 output into the schema and demonstrates that `validateSnapshot()` can be structurally `VALID` while the authoritative join remains `INCOMPLETE`.
6. Changed `calculateSnapshotMonth()` to `PREVIEW_READY` only. It always reports `authoritativeExportReady:false`; `SOURCE_CHECKED` and caller-supplied `VERIFIED` flags cannot authorize it.
7. Added the reviewer-evidence architecture proposal in `REVIEW_EVIDENCE_DESIGN_v0.1.md`. No identity review verifier, persistence, endpoint or signing key was implemented.

### Batch D4 — reviewer-evidence policy (design complete in v0.2.30; no verifier implemented)
1. R11/D4-1 is SIGNED: protect against unauthorized Wi-Fi clients only. Physical flash attacks and malicious authenticated administrators are out of scope; preserve the handoff's accepted plain-HTTP/AP-passphrase-leak risk.
2. R12/D4-2 is SIGNED: one authorized administrator may enter and confirm, labelled `SELF_REVIEW`.
3. R13/D4-3 is DELEGATED: record only `actorRole=ADMIN` and `reviewMode=SELF_REVIEW`; no operator-supplied identity label.
4. R14/D4-4 is DELEGATED: four built-in, school-configurable source categories; one review can select multiple categories. No uploads, document numbers, free-text locators, or claims of authenticity/DepEd acceptance.
5. R15/D4-5 is DELEGATED: committed metadata order tuple (store lineage, transaction revision, change index) orders reviews. RTC time is optional and explicitly `UNTRUSTED_RTC`; it never drives validity or report dates.
6. R16/D4-7 is DELEGATED: reviews bind to an exact identity revision; any correction requires a new review. Disabling a selected source category also stales that review. Prior revisions/snapshots are preserved; corrections do not silently rewrite old reports.
7. R17/D4-8 is DELEGATED: O8 remains unchanged. Reporting PII is not persisted or exposed until the user explicitly amends O8 or approves a separate companion recovery workflow.
8. The workflow is one-time category setup, then one source selection and one confirmation per learner identity revision. The server generates the populated-field review scope. Missing report-required fields block reporting only, never attendance.
9. Policy is complete, but no reviewer verifier, PII store, API, UI, backup component or XLSX export exists. See `REVIEW_EVIDENCE_DESIGN_v0.6.md`.

### Batch C6a — PIN-attempt sequencing core (implemented in v0.2.31)
1. Added `PinAuthCoordinator` as a hardware-independent wrapper around `PinLockout`.
2. Boot loads the persisted attempt count. A failed read latches the coordinator.
3. Every valid candidate attempt increments and persists the counter before the verifier runs.
4. A matching credential clears and persists the counter before the required action can issue a session or continue setup.
5. PIN change validates the new PIN, verifies the current PIN through the same coordinator, then calls an injected durable writer. Weak new PINs are rejected before consuming an old-PIN attempt.
6. Storage, verifier and indeterminate-action errors latch the coordinator. The adapter must fail all protected routes and revoke/disable sessions while latched.
7. Thirteen regression registrations were added but intentionally not run. Only source syntax checks were performed.

**Still open:** no concrete NVS attempt store, PBKDF2 verifier/writer, secure random source, session/fault integration or HTTP adapter. C6 remains blocking. The helper can be bypassed until every secret route uses it.

### Next implementation stage — non-PII device/API and storage prerequisites
- Do not implement identity persistence while O8/C29 remains unresolved. Complete the concrete C6 adapters and route wiring without exposing PII.
- Resolve C13 power-loss behavior and C26 transaction/capacity/migration design before choosing metadata storage.
- Keep snapshot paging bounded under C28; chunking within one synchronous HTTP handler is not sufficient.
- Do not weaken D3 or trust browser-supplied review flags. A server-side review gate is required by C31.

### Batch E — physical metadata/persistence decision (design only)
- Resolve C13 power-loss behavior, C27 PII security and C29 backup/recovery before selecting NVS, LittleFS or event-log storage.
- Measure the 300-ID/45-active-learner/high-turnover history budget. Include indexes, checkpoints, revisions and tombstones.
- Do not implement a storage adapter until its transaction, compaction, migration and backup semantics are approved.

### Batch F — official template normalization
- Obtain the exact school/LIS-accepted workbook/version. Keep the current upload reference-only.
- Repair or rebuild broken defined names and external links only after an element-by-element audit of drawings, merges, styles, formulas, headers, labels and print setup.
- Generate exactly one learner row per report-month roster member, with separate male/female blocks and repositioned subtotals; no four-row or 45-person cap. Page layout/print behavior must be validated separately.
- Test workbook structure, high-turnover month fixtures and actual print/PDF output at zero, one, four, 45, 46 and more than 45 month-overlap learners.

### Batch G — calculations, API and dashboard
- Resolve the remaining teacher-dependent rules (including the one-session non-class-day case) before final calculation release. Do not add a separate Tardy mark or tally to SF2; Late remains internal and projects as attended.
- Implement one-section read-only snapshot pages only after C6/C22/C26-C29 and source consistency are resolved.
- Keep all browser code, fonts/libraries and approved template assets local so the exporter works offline.
- Select an XLSX writer only after it round-trips this exact workbook's styles, merges, drawings, names, dimensions and print settings without loss.

## Must not be shortcut

Do not write XLSX bytes on the ESP32. Do not accept an arbitrary section selector. Do not infer absence from missing data. Do not trust browser completeness flags as source evidence. Do not change P9, E2, E18 or E26 without the required decision process. Do not treat the user's custom AM/PM convention as DepEd-approved until school/LIS acceptance is confirmed. Do not persist or expose LRN until metadata storage, backup, authentication and PII protection are designed. Do not call a host reference model a production API or immutable snapshot.
