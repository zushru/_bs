# v0.2.37 specialized audit — 2026-11-08

## Scope

Audited the v0.2.36 archive against the user's complete native and SF2 test output, then corrected the SF2 regression failures. This is a source/test/docs snapshot, not a firmware release.

## Input evidence

1. User-provided native summary: all 23 suites passed; **413/413 test cases succeeded** in `00:08:56.629`. `test_nvs_config` passed, including the previously failing read-I/O fixture.
2. User-provided Node summary: **238 passed, 19 failed / 257 total**.
3. Independently ran `npm test` on the extracted v0.2.36 archive and reproduced all 19 failures.

## Root-cause classification

### Fixture/expectation defects (seven categories)

- Month-roster source-name revision fixture was not in global sequence order.
- Snapshot test helper ignored the validator's separate `blockers` collection and examined only `issues`.
- Snapshot expected month-roster order `[1,3,2]`, but the policy is male first and preserve source order within sex groups; fixture correctly projects `[1,2,3]`.
- Event-range test used sequence 8, which was already an in-range CLOSE-group sequence. It now uses `attendanceLastSeq + 1`.
- Malformed reporting-identity test passed an unsupported property into the fixture builder rather than corrupting its returned roster.
- D1/D2 integration fixture retained two synthetic learners bound to a different section profile, invalid for a one-section snapshot.
- Coverage-gap test contradicted itself by flagging a gap while both completion flags remained true; the snapshot was correctly INVALID. The fixture now models a genuine incomplete gap.

### Implementation defects/corrections (three)

- Preview state mapping expected obsolete `READY`; the snapshot calculator deliberately returns `PREVIEW_READY`. A valid schema plus preview-ready calculation now yields `PREVIEW_READY`, never authoritative `READY` or export permission.
- Structured preview diagnostics had contextual learner/date/session fields, but generic validator/calculator copies appeared first and callers found those. Contextual records are now prioritized and deduplicated without discarding their identities.
- Source status disagreement was grouped with malformed provenance. A structurally valid reference with mismatching status now reports `EVENT_STATUS_MISMATCH`; invalid shape/type still reports `INVALID_EVENT_PROVENANCE`. This is more precise error classification, not weaker validation.

## Test and static verification

- `npm test`: **257 tests, 257 pass, 0 fail, 0 skipped** (local environment).
- `node --check`: **25/25 `.mjs` modules passed** (local environment). The final re-run completed with 257 pass, 0 fail, 0 skipped in 1.455 seconds.
- Source inventory: **257 Node test registrations**; unchanged.
- Source inventory: **413 C++ `RUN_TEST` registrations across 23 suites**; unchanged.
- Independent tree comparison: `lib/` is byte-for-byte unchanged from v0.2.36; native C++ `test/` is byte-for-byte unchanged from v0.2.36. The user's 413/413 native result applies to the unchanged C++ files.
- No C++ build was run during this patch because no C++ source/test changed. No ESP32 target build or hardware test was run.

## Files changed

Production/reference modules:
- `web/sf2/preview_model.mjs`
- `web/sf2/snapshot_schema.mjs`

SF2 test fixtures/expectations:
- `web/sf2/test/month_roster_projection.test.mjs`
- `web/sf2/test/preview_model.test.mjs`
- `web/sf2/test/reporting_identity_projection.test.mjs`
- `web/sf2/test/reporting_snapshot_join.test.mjs`
- `web/sf2/test/snapshot_schema.test.mjs`

Project documentation and release records were updated, including `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`, `docs/ADAPTER_DESIGN.md`, `docs/SPECS/api_logic.md`, `docs/SPECS/config_runtime_guard.md`, the SF2 README/next-steps/reference docs, `docs/CHANGELOG.md`, root README and release notes. Previously stale statements that the PIN-auth, NVS and config-runtime test cases were unrun were updated against the user's 413/413 v0.2.36 native result.

## Unchanged policy and residual blockers

- No `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, signed rule, delegated decision, hardware map, event format, persisted schema or production API was changed.
- D1/D2/D3 are synthetic browser reference models. Passing host tests do not prove that a future ESP32 endpoint supplies authentic immutable source data. `SOURCE_CHECKED` remains an unverified claim.
- `PREVIEW_READY` is non-authoritative. `finalExportReady` remains false. There is no approved normalized XLSX writer, confirmed current official workbook, or school-approved legend.
- O8 remains signed: backup scope is config + roster + log. No reporting PII is persisted/exposed until a separately approved recovery path or O8 amendment exists.
- C6 remains blocking: no production boot composition, PBKDF2 verifier/writer, chosen PIN KDF work factor, secure RNG, token/session action, HTTP adapter or protected-route wiring. E12 remains DELEGATED; no counter-domain choice was made.
- C2/C13 restore swap and power-loss durability; C8 soak/brownout; C17/E2 schedule history; C18/E26 term-end semantics; C24/C25 live roster/event audits; C26-C29 PII/snapshot/backup security; official template and school rules remain unresolved.

## Required user-side confirmation

Install the complete archive (do not merge files), then run:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test 2>&1 | tee sf2-node-test.log)
```

Expected Node summary: 257 passed. The native summary should remain 413/413 because its code is unchanged. This validates the installed full snapshot only at host level; it does not replace ESP32 build/device tests.
