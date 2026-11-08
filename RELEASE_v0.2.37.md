# Release v0.2.37 — 2026-11-08

Status: **SF2 browser reference-model regression repair.** Development snapshot only; not production firmware and not hardware-verified. Replace the complete `att/` tree. Never merge individual files into an older tree.

## Verification evidence

- User-provided v0.2.36 PlatformIO summary: **23/23 native suites passed; 413/413 cases succeeded** in `00:08:56.629`, including `test_nvs_config` (44 cases).
- User-provided v0.2.36 SF2 Node summary: **238 passed, 19 failed out of 257**. The same 19 failures were reproduced from the exact v0.2.36 archive in this environment.
- v0.2.37 SF2 Node suite: **257/257 passed, 0 failed, 0 skipped**, executed locally using `npm test`. All 25 SF2 `.mjs` modules passed `node --check`.
- v0.2.37 does not modify `lib/` or the native C++ `test/` tree. The user's 413/413 native result therefore applies to unchanged C++ source/tests, but a clean full-archive rerun remains recommended.

## Changes

### Test fixtures and expectations

Corrected seven categories of invalid fixture/expectation assumptions without weakening validation:

1. Month-roster name revisions are supplied in global event-source sequence order.
2. Snapshot test helpers inspect both structural `issues` and completeness `blockers`; the validator intentionally keeps those collections distinct.
3. Month roster preserves source order within the male-then-female groups; corrected expected IDs to match the documented ordering.
4. The event-sequence boundary fixture now uses `attendanceLastSeq + 1`, rather than sequence 8 that was already allocated to a CLOSE group.
5. The malformed-roster test now corrupts the projected output rather than passing an ignored parameter to the fixture builder.
6. The D1/D2 snapshot integration fixture rebinds all synthetic learners to the selected one-section profile.
7. The coverage-gap fixture sets `gapAffected`, `coverageComplete` and `closeCoverageFlag` consistently, avoiding a deliberately contradictory snapshot that should be `INVALID`.

### Reference-model implementation

- `preview_model.mjs`: a valid schema plus `PREVIEW_READY` calculation now produces preview state `PREVIEW_READY`, not `BLOCKED`. It still reports `finalExportReady: false` and `workbookWriterAvailable: false`.
- `preview_model.mjs`: learner/date/session-aware diagnostics are prioritized above generic copies; deduplication keys preserve distinct contextual records.
- `snapshot_schema.mjs`: distinguishes event-status disagreement (`EVENT_STATUS_MISMATCH`) from malformed/wrong-type event provenance (`INVALID_EVENT_PROVENANCE`). Both remain blocking.

## Unchanged boundaries

- C++ implementation and native tests are byte-for-byte unchanged from v0.2.36. Test inventory remains 413 C++ registrations across 23 suites and 257 SF2 Node registrations.
- No signed product rule, delegated decision, E12 counter-domain policy, E27 rule, hardware pin, storage format, event encoding, NVS adapter, backup scope or physical behavior was changed.
- D1/D2/D3 remain browser reference models only. `PREVIEW_READY` is not authorization. No production HTTP/API route, persistent reporting-PII store, snapshot assembly endpoint, dashboard integration, approved XLSX writer or school-accepted workbook exists. O8 remains unchanged.
- C6 remains blocking: production boot wiring, PIN PBKDF2 verifier/writer and KDF iteration policy, secure RNG, token/session bridge and protected HTTP routes are missing. E12 remains DELEGATED; no counter-domain choice was made.
- No ESP32 target build, hardware test, NVS power-loss test, LittleFS atomicity test, restore swap/recovery test, soak or brownout test was performed.

## Verification commands

From a clean full v0.2.37 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test 2>&1 | tee sf2-node-test.log)
```

The Node pass is locally reproduced in this environment; rerun on the user's Android/Termux environment to close the package-level confirmation.
