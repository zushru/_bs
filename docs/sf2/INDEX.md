# SF2 documentation index

## Governing inputs

| File | Role | Authority caution |
|---|---|---|
| `REQUIREMENTS.md` | SF2 workstream requirements | Contains signed/proposed distinctions; read with root `docs/DECISIONS.md`. |
| `D4_DECISIONS.md` | Reviewer-evidence policy decisions | Preserve SIGNED vs DELEGATED status. |
| `DATA_CONTRACT.md` | Proposed source/snapshot data contract | Contract/model, not proof of production data source. |
| `CORE_SOURCE_MAPPING.md` | Maps current core structures/events to reporting concepts | Verify mappings against source after any core schema change. |
| `TEACHER_QUESTIONS.md` | Open school-specific rules | Do not guess these rules to unblock export. |
| `TEMPLATE_AUDIT.md`, `UPLOAD_TEMPLATE_AUDIT_2026-11-10.md` | Reference workbook observations | Neither proves final school approval. |
| `FIXTURE_MATRIX_v0.2.md` | Fixture coverage plan | A planned fixture is not automatically a test. |

## Current model/design references

- `SNAPSHOT_SCHEMA_v0.2.md` — snapshot structure and validation.
- `SNAPSHOT_COORDINATOR_DESIGN_v0.3.md` and `SNAPSHOT_COORDINATOR_MODEL_v0.4.md` — coordinator proposal/reference model; no production server coordinator is implied.
- `REPORTING_IDENTITY_PROJECTION_v0.4.md` — legal identity/movement projection rules.
- `MONTH_ROSTER_PROJECTION_v0.5.md`, `ROSTER_HISTORY_PROJECTION.md`, `ROSTER_PREFIX_PROJECTION_v0.4.md` — report-month roster/history projection.
- `REPORTING_SNAPSHOT_JOIN_v0.1.md` — exact-source join boundary.
- `REPORTING_METADATA_DESIGN_v0.3.md` — logical metadata model; no physical store decision.
- `REVIEW_EVIDENCE_DESIGN_v0.6.md` — latest retained review-evidence design; it is not an implemented verifier.
- `NETWORK_SECURITY_BOUNDARY_v0.1.md` — accepted threat boundary; not an implementation audit.

## Source and tests

- Source modules: `../../web/sf2/*.mjs` and `../../web/sf2/fixtures/`.
- Tests: `../../web/sf2/test/*.test.mjs` (12 files; 270 registrations in v0.2.95).
- Command: `cd web/sf2 && npm test`.

## Reference inputs

`reference/` contains the supplied working template and a source workbook explicitly marked not final. Do not relabel either as approved or modify the originals without retaining provenance.

## Historical material

- `archive/context/NEXT_STEPS_legacy_v0.2.37.md` — preserved prior next-step notes; not current priority guidance.

- `archive/audits/` retains 15 SF2 development audits.
- `archive/design-revisions/` retains superseded `REVIEW_EVIDENCE_DESIGN_v0.1` through `v0.5`; v0.6 remains active as the latest retained revision, but remains design-only.

Do not load the archive unless investigating history or a regression.
