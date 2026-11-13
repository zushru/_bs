# D4 decisions — learner identity review evidence

Status: **policy design complete in v0.2.30; implementation blocked.** D4-1/R11 and D4-2/R12 are SIGNED by the user. D4-3/R13 through D4-5/R15, D4-7/R16 and D4-8/R17 are DELEGATED engineering choices, not user-signed decisions. D4-6 is out of scope under the selected threat model. No reviewer verifier, PII store, HTTP adapter, UI, backup component or XLSX export has been implemented.

| # | Decision | Status | Selected rule |
|---|---|---|---|
| D4-1 | Threat model | **SIGNED (R11)** | Protect against unauthorized Wi-Fi clients only. Physical flash access/modification and malicious authenticated administrators are out of scope. Preserve the accepted plain-HTTP/AP-passphrase-leak risk. |
| D4-2 | Reviewer separation | **SIGNED (R12)** | One authorized administrator may enter and confirm the same learner identity. The action is self-review, never independent verification. |
| D4-3 | Reviewer attribution | **DELEGATED (R13)** | Record `actorRole=ADMIN` and `reviewMode=SELF_REVIEW`; no self-entered operator name, ID or label. This identifies an authorization role, not a human. |
| D4-4 | Source categories and evidence minimization | **DELEGATED (R14)** | Four broad built-in categories; one-time school-configurable accepted set; one or more categories per review. No document uploads, document numbers, free-text evidence locator or proof-of-authenticity claim. |
| D4-5 | Review time and ordering | **DELEGATED (R15)** | Committed metadata order tuple (store lineage, transaction revision, change index) is authoritative. Optional RTC observation is labelled `UNTRUSTED_RTC` and is diagnostic only. |
| D4-6 | Device trust anchor/signature | **OUT OF SCOPE under SIGNED R11** | No device signature, secure element, reviewer public-key registry or physical-tamper protection claim. |
| D4-7 | Revision invalidation | **DELEGATED (R16)** | Review binds to the exact identity revision and records the source-policy revision. Corrections require a new review; disabling a selected source category also stales the review. Historical revisions and snapshots are not mutated; affected report periods require an explicit re-generation decision. |
| D4-8 | PII persistence and backup | **DELEGATED (R17)** | O8 remains unchanged. No reporting PII persistence/API/export until O8 is explicitly amended or a separate companion backup/recovery workflow is approved. |

## Why this is convenience-first without weakening the claims

- Four plain-language source choices replace a school-specific document catalogue. A one-time school setting disables categories that are not accepted locally.
- One review confirmation covers all populated canonical identity fields. The server generates the reviewed-field set; the administrator does not tick each field.
- Missing report-required values are flagged separately and never block normal attendance.
- No document images, IDs/numbers, arbitrary evidence references, or typed reviewer names are collected.
- The state is named `SELF_REVIEWED`, not `VERIFIED`.
- Any identity correction automatically makes the new revision stale until reviewed again.

## Required workflow

1. Configure enabled source categories once for the school. Defaults are the four built-in categories; the administrator must confirm the set or disable unsupported categories.
2. Enter/update legal identity fields separately from operational roster display names.
3. Display all supplied values and missing-field warnings.
4. Select one or more enabled source categories.
5. Confirm once that all displayed identity details were checked against those sources.
6. The future authenticated server action binds the review to the exact identity revision, source-policy revision and committed metadata sequence.

The category selection and self-review are administrator declarations. They do not authenticate a human, prove document authenticity, or establish DepEd/LIS acceptance.

## D4 interim stop rules

- `SOURCE_CHECKED` remains an untrusted legacy claim. Browser-supplied `verified`, `reviewed`, `commitSequence`, or timestamp fields cannot authorize anything.
- `SELF_REVIEWED` cannot be shown until the authenticated server action and durable commit complete. A failed/ambiguous write must leave the review incomplete.
- RTC time is never authoritative. If unavailable, omit it; do not block review solely because the clock is unavailable.
- A new identity revision or source-policy revision invalidates the current review. Prior review records are retained for history.
- O8 remains SIGNED: backup = config + roster + log. No workaround may hide a reporting-metadata backup inside that bundle.
- Until the user explicitly approves a metadata recovery route, do not persist PII or expose it through the device API/browser cache/XLSX output.
- E27 remains DELEGATED and unchanged. D3 remains `INCOMPLETE` / `PREVIEW_READY`; `authoritativeExportReady:false`.

## Remaining implementation blockers

C6 (HTTP authentication enforcement), C13 (power-loss behavior), C24/C25/C30/E27 (source-history completeness), C26 (metadata store), C27 (network/PII route enforcement), C28 (bounded snapshot service), C29/O8 (backup/recovery), C31 (server-side source/review gate), and the official SHS template/school reporting rules. Policy decisions are complete; production capability is not.

See `REVIEW_EVIDENCE_DESIGN_v0.6.md` for the detailed behavior and illustrative logical record. It is not a wire format or persistence schema.
