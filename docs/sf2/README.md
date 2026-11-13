# SF2-SHS workstream

**Current reference-model behavior (v0.2.81):** `BLANK` and `E` count as Present; `X` and both AM/PM diagonal marks count as Absent. `E` is reserved for both sessions Excused; a Present/Late + Excused full day maps to `BLANK` and counts as Present. Diagonal marks count toward five consecutive absences. Late remains internal and is not separately shown in SF2. The semantic roster planner generates one learner row per report-month learner with no four-row/45-row cap. The section preview also separates month-end registered learners from the month-overlap roster and counts learners with at least five absent confirmed class dates by sex. Streaks reset on class dates outside the learner's enrollment intervals. The streak count is report-month scoped; class-day/calendar-day meaning and cross-month carry-in remain unconfirmed. This remains a pure preview/reference layer; there is no production API or XLSX writer, and O8 still blocks authoritative reporting export.

Design-stage reporting layer. The ESP32 remains authoritative; the browser-side calculation and preview foundation is reference-only. No production API, persistent reporting metadata store or XLSX writer exists. SF2 Node test registrations: 257. The v0.2.36 user run exposed 19 failures (238/257); v0.2.37 corrects seven categories of invalid test fixtures/expectations and three preview/diagnostic implementation defects. All 257 Node tests now pass locally, and all 25 `.mjs` modules pass `node --check`. This is reference-model host evidence only.

- `REQUIREMENTS.md` / root `docs/DECISIONS.md` — proposals and signed reporting decisions.
- `NEXT_STEPS.md` — current development order and release gates.
- `DATA_CONTRACT.md` / `SNAPSHOT_SCHEMA_v0.2.md` — proposed revision-bound browser snapshot.
- `CORE_SOURCE_MAPPING.md` — mapping from current core events and roster data.
- `ROSTER_HISTORY_PROJECTION.md` — v0.2.20 C++ roster-history interface and limitations.
- `REPORTING_METADATA_DESIGN_v0.3.md` — proposed bitemporal logical metadata model; no physical storage decision. `metadata_history_model.mjs` validates this JSON-like model in browser/host tests only (26 cases included in the current 257/257 local pass).
- `SNAPSHOT_COORDINATOR_DESIGN_v0.3.md` — proposed revision-pinned/paged server coordinator; no firmware implementation or endpoint.
- `TEACHER_QUESTIONS.md` — school-specific rules still awaiting teacher confirmation.
- `TEMPLATE_AUDIT.md` — supplied workbook is a reference-only structural starting point.
- `REVIEW_EVIDENCE_DESIGN_v0.6.md`, `D4_DECISIONS.md` and `NETWORK_SECURITY_BOUNDARY_v0.1.md` — D4 policy design is complete. v0.2.34 adds a reusable NVS PIN-attempt store and boot owner for C6, and the user reports 413/413 native tests passed in v0.2.36. No PBKDF2 verifier, production HTTP adapter, PII store or reporting backup path is implemented. R14-R17 are DELEGATED design choices, not user-signed rules. O8 remains unchanged and blocks PII persistence. See `DEVELOPMENT_AUDIT_v0.2.37.md` and root `RELEASE.md` for the current host-test status.

The reporting layer does not change signed attendance rules, P9, or the authoritative attendance log. PII persistence/export remains blocked by C6/C13/C26/C27/C28/C29 and the unresolved implementation gates. The D4 reviewer/source/time/revision policy is specified in v0.2.30. v0.2.31 adds PIN-attempt ordering but does not close C6. C16 physical-flash confidentiality is explicitly out of the approved threat scope.


## v0.2.27 D4 network boundary

The user selected protection against unauthorized Wi-Fi clients only. The signed handoff's WPA2 AP + admin PIN/token model and plain-HTTP/AP-passphrase-leak residual risk remain unchanged. Physical access/flash attacks and malicious authenticated administrators are out of scope. No device signing key or reviewer public-key registry is required under this threat model. The shared admin PIN still cannot identify a unique human reviewer. D4-2 is SIGNED: one administrator may enter and confirm, but the action must be labelled self-reviewed rather than independently verified. D4-3 is DELEGATED to role-only attribution (`ADMIN` + `SELF_REVIEW`); no self-entered operator identity label is stored and no human identity is claimed. The HTTP adapter is absent (C6); `SOURCE_CHECKED` remains a claim, D3 remains `INCOMPLETE`, and calculations remain `PREVIEW_READY` only. See `REVIEW_EVIDENCE_DESIGN_v0.5.md`, `D4_DECISIONS.md` and `NETWORK_SECURITY_BOUNDARY_v0.1.md`.



## v0.2.81 focused verification

The section, snapshot-adapter and preview-model suites passed 47/47 in the v0.2.81 working tree. This focused result covers month-end registration boundaries, re-entry deduplication, leap-year February, sex-grouped five-class-date absence counts, enrollment-gap streak resets and adapter propagation. The complete SF2 suite was not rerun in this increment. The earlier 257/257 result remains historical v0.2.37 evidence only.
