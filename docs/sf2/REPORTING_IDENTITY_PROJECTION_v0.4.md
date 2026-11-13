# SF2 reporting identity projection — design v0.4

Status: **host/browser reference model only**. This is not a storage format, device API, authorization rule, or school-approved data-entry policy.

## E27 disposition

On 2026-11-07 the user delegated E27 to assistant judgment. The selected rule is:

- Same-local-day `ACTIVATE`/`DEACTIVATE` transitions remain fail-closed for authoritative report-month roster projection.
- This is a reporting restriction only. It does not reject, edit, or rewrite the committed operational roster event.
- Do not relax the rule until a separate event-time projection defines session-overlap eligibility and RTC rollback semantics.

## 1. Keep three identities separate

### Operational roster identity

`RA_SET` holds the operational display label used by device roster/UI workflows. It is limited by the event format to 32 UTF-8 bytes and is historically readable through `RosterNameHistoryReader`.

This value is **not** a legal name. It may be an abbreviation, nickname, clipped name, or later correction. It must never fill any SF2 legal-name field.

### Legal reporting identity

The proposed metadata record kind `LEARNER_IDENTITY` is keyed by the existing device `studentId` (1..300), without replacing P9. Its schema-v2 `data` shape is:

```json
{
  "legalName": {
    "firstName": "Example",
    "middleName": null,
    "lastName": "Learner",
    "extensionName": null
  },
  "lrn": "012345678901",
  "sex": "F",
  "sourceStatus": "SOURCE_CHECKED"
}
```

`legalName` is explicit so no adapter can mistake a device display label for a reportable legal name. LRN remains text to preserve leading zeroes. `sourceStatus` is only a caller-supplied administrative claim. The projection always emits `sourceStatus: UNVERIFIED`; the original claim is retained as `sourceStatusClaim`. Even `SOURCE_CHECKED` triggers an incomplete warning because the model has no authenticated reviewer identity, timestamp, source reference or immutable verification event. It cannot prove that a school source document was actually checked.

The operational roster projection now returns display-name revisions as `operationalDiagnostics`. Missing, changing, or backdated operational display labels do not select or block the legal reporting identity. Malformed source-prefix/interval input still blocks the month roster.

## 2. Bitemporal rules

Every identity fact has:

- Effective time: `validFrom` through `validToExclusive` (`[start,end)`).
- Recorded time: the append-only metadata transaction revision.

A report selects an explicit `asOfRevision`. A retroactive correction creates a new transaction; it must not rewrite an older report revision. A mid-period effective change must be represented by revoking the former record and inserting the preserved pre-change interval plus the new post-change interval in one transaction. A simple revoke-and-replace with only the new range would erase the prior effective segment at the new revision.

The current schema is **metadata history schemaVersion 2** because the legal-name object and movement date semantics differ from the v0.2.21 draft. No durable schema has been installed.

## 3. Report-month identity resolution

`projectReportingRoster()` consumes:

- one `READY` `projectMonthRoster()` result with exact source-prefix identity;
- one metadata history and explicit `asOfRevision`;
- the selected `YYYY-MM`.

For every day in each month-overlap enrollment interval, the projection requires exactly one effective legal identity. It does not require metadata on days before enrollment or after transfer-out.

If legal name, LRN, or sex changes during enrolled days within the same report month, the projection blocks rather than choosing first-day, last-day, or an arbitrary version. U63 remains unresolved; a teacher/school rule is required before this can be relaxed.

If a learner's metadata has a gap during enrolled days, the result is `INCOMPLETE`, not Absent. A legal identity missing entirely does not make the learner disappear from the month roster.

A report projection is never considered complete solely because `SOURCE_CHECKED` was supplied. Identity verification stays unresolved until a reviewer-attributed, immutable evidence design is implemented.

The returned learner keeps separate branches:

- `operational`: optional display label and its state, for UI diagnostics only;
- `reportingIdentity`: legal-name components, LRN, sex and source status.

Diagnostics contain device student IDs and error codes, not LRN or legal-name values.

## 4. Explicit movements

Allowed proposed movement types now include:

- `ENROLLED`
- `TRANSFER_IN`
- `TRANSFER_OUT`
- `SHIFTED`
- `WITHDRAWN`
- `DROPPED`
- `NLS`
- `OTHER`

A movement is an explicit bitemporal metadata event. It is never inferred from `RA_DEACTIVATE`. `NLS` is preserved as a distinct raw school-reported category; its exact meaning, source code and form mapping remain unresolved. It is not converted into `DROPPED`, `WITHDRAWN`, or a particular SF2 remark.

Every movement must state `effectiveDateMeaning`:

| Meaning | Intended interpretation |
|---|---|
| `FIRST_ENROLLED_DAY` | First local calendar day included in enrollment |
| `LAST_ENROLLED_DAY` | Last local calendar day included in enrollment |
| `FIRST_NOT_ENROLLED_DAY` | First local calendar day excluded from enrollment; aligns with the current day-granular `endExclusive` boundary |
| `SECTION_CHANGE_DAY` | Date of a section-shift event; exact reporting semantics remain open |
| `EVENT_DAY` | Event date with no enrollment-boundary assertion |
| `UNCONFIRMED` | Date is recorded but the date's boundary meaning is not known |

The validator checks broad type/meaning compatibility. The browser projection checks confirmed `FIRST_ENROLLED_DAY`, `LAST_ENROLLED_DAY`, and `FIRST_NOT_ENROLLED_DAY` against stored interval boundaries. An unconfirmed date is preserved but not reconciled; it makes the reporting projection incomplete. Movement reason/destination fields remain verbatim and nullable. Missing detail creates warnings.

No movement type or reason is mapped to a school-form cell/summary until school instructions are confirmed.

## 5. Revision and audit limits

The current metadata model stores revision order only. It does not identify an individual staff member because the project uses a shared admin credential, and it does not claim a trusted wall-clock recording timestamp. Adding a user identity or pretending the shared PIN identifies the actor would be false attribution.

`SOURCE_CHECKED` is a claim entered by the administrator, not cryptographic evidence. The model cannot prove correctness against LIS or school source records.

## 6. Integration limits

- No ESP32 metadata persistence or transaction journal.
- No LittleFS adapter, compaction, migration, or power-loss proof.
- No LRN/identity API or browser local storage.
- No PII authorization implementation; C6/C27 remain blockers under the approved network-only threat scope. C16 physical-flash confidentiality is out of scope.
- No metadata backup/restore component; C2/C29 remain blockers.
- No snapshot endpoint or immutable server-side ticket; C28 remains blocking.
- No XLSX writer or school-approved template.
- No actor-level audit attribution.

Do not expose or persist real learner PII until C6/C26-C29 and the applicable D4 reviewer/source/time policy gates are resolved. C16 physical-flash confidentiality is out of scope under R11.

## 7. D2 integrity guardrails

- A source-status claim does not prove verification. `SOURCE_CHECKED` is preserved only as a claim; the report identity remains `UNVERIFIED` and the projection remains `INCOMPLETE` until an authenticated audit model exists.
- LRN must be a 12-digit string. One LRN cannot be assigned to different never-reused device IDs, even in non-overlapping effective periods. A correction to the same device ID may be bitemporal.
- NLS is required to use `UNCONFIRMED` and cannot assert an exit boundary.
- Multiple movement records for the same learner on one effective date are report-blocking; no sort-by-ID or transaction order is treated as a school-approved movement ordering.
- `SHIFTED` is checked against an enrollment interval boundary. A free-standing section-change date is insufficient evidence.
- E27 remains DELEGATED and reporting-only; same-day lifecycle transitions fail closed and operational events are not edited.


## 8. v0.2.24 audit clarifications

- Operational display-name history is diagnostic only; it is not an input to legal identity selection.
- Movement records for the same student on the same effective date are blocking because the current policy has no deterministic ordering/meaning for those collisions.
- `NLS` must use `UNCONFIRMED`; the model must not convert it into a withdrawal/exit boundary.
- Selected historical metadata revisions are rechecked for overlap and duplicate LRN assignments at that exact revision.
- Legal-name comparison uses a fixed canonical field order. JSON object property insertion order is not a legal identity change.
- D1 enrollment intervals are independently rechecked at the D1-to-D2 boundary, even when the input says `READY`.
- The C++ `RosterLogPrefix.openGeneration` is `uint64_t`. Since JavaScript numbers cannot represent every `uint64_t` exactly, v0.2.25 serializes it as a canonical unsigned decimal string and validates the full range. `recordCount`, `lastSeq` and `auditGeneration` remain bounded `uint32_t` JSON numbers. See `source_prefix.mjs` and `REPORTING_SNAPSHOT_JOIN_v0.1.md`.
- The current reference is not an API or persistence format. `SOURCE_CHECKED` remains a claim, not evidence.


## v0.2.25 D3 boundary

- D1, D2 and the snapshot coordinator now share the exact decimal-string `uint64_t` open-generation contract.
- `joinReportingRosterForSnapshot()` enforces same-month, same-prefix, same-ID-set, same-interval and movement-boundary joins before producing a snapshot-compatible fragment. It never returns authoritative `READY`.
- A schema-valid snapshot remains only structurally valid. `calculateSnapshotMonth()` now returns `PREVIEW_READY` (never authoritative `READY`) and exposes `authoritativeExportReady: false` until authenticated reviewer evidence and a trusted verifier exist.
