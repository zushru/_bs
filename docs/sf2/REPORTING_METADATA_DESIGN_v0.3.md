# SF2 reporting-metadata store — design v0.3

Status: **proposed logical model; NOT approved as a storage format or product rule**.

This design follows the signed R1/R2/R3 decisions and preserves P9. It does not modify `Config`, `RosterStore`, the 16-byte attendance event format, or operational setup behavior. `web/sf2/metadata_history_model.mjs` is a browser/host reference validator for the logical model only; it is not an ESP32 backend.

## 1. Boundary and identity

The operational attendance model remains authoritative for whether a device ID is known, active, and expected on a date. Reporting metadata adds facts that the attendance core does not own:

- school identity and reporting details;
- one configured section/curriculum profile for the device;
- LRN, sex, and legal/reporting name components for a device student ID;
- transfer, shifting, dropped/NLS and other movement details.

**P9 remains unchanged:** `studentId` is the never-reused sensor slot ID in `1..300`. LRN is a separate reporting identifier, never a primary key and never a replacement for the device ID. Existing 32-byte OLED/display names remain unchanged and are not parsed into legal name components.

A device has one active reporting section/profile at a time (R1/R2). A section profile is not a second timetable: operational session times and late thresholds remain in `Config`. A section profile records the school-reporting classification only.

## 2. Two timelines are required

A plain `updatedAt` date is insufficient. Every reporting fact has two distinct notions of time:

1. **Effective time:** the school date/range when the value is intended to apply.
2. **Recorded revision:** the monotonically increasing metadata transaction in which the device learned or corrected the value.

This is necessary when an administrator corrects an earlier LRN, sex, legal name, school profile or transfer record retroactively. The corrected report can use the corrected effective-time value at a new metadata revision, while an earlier generated snapshot remains reproducible from its older revision.

The recommended logical representation is an append-only transaction stream:

```json
{
  "schemaVersion": 1,
  "revision": 4,
  "transactions": [
    {
      "revision": 4,
      "committed": true,
      "changes": [
        { "op": "REVOKE", "targetRecordId": "learner-7-v1" },
        {
          "op": "PUT",
          "record": {
            "recordId": "learner-7-v2",
            "kind": "LEARNER_IDENTITY",
            "subjectId": "7",
            "validFrom": "2026-06-01",
            "validToExclusive": null,
            "data": {
              "firstName": "Example",
              "middleName": null,
              "lastName": "Learner",
              "extensionName": null,
              "lrn": "012345678901",
              "sex": "F"
            }
          }
        }
      ]
    }
  ]
}
```

The JSON is illustrative, not a selected on-device representation. A real transaction is visible only after its entire storage commit group is valid. `committed:true` in the browser reference validator does not prove physical durability or authenticity. The v0.2.21 reference model accepts a complete history beginning at revision 1; it has no checkpoint, compaction or bounded-replay mechanism.

### Transaction semantics

- `PUT` creates a new immutable record with a globally unique `recordId`.
- `REVOKE` appends a reference to a prior active record; it does not edit/delete the old record.
- A multi-change transaction is all-or-nothing. For a correction, `REVOKE` + replacement `PUT` belong to one committed transaction.
- Revisions are strictly sequential. A torn/uncommitted transaction is not included in the current revision and must never produce a partially applied profile.
- To resolve the state as of revision `R`, apply transactions through `R`, remove records revoked through `R`, then select records whose effective interval contains the requested date.
- Profile and identity effective ranges use `[validFrom, validToExclusive)`. Movement is an event with one `effectiveDate`, not an interval.
- `recordId` and `studentId` are distinct. A new metadata revision must not allocate or reuse a sensor ID.

## 3. Logical record types

### SCHOOL_PROFILE

One school identity for the effective period. Proposed fields: `schoolName`, `schoolId`, `district`, `division`, `region`, `adviserName`, `schoolHeadName`. These are reporting-only fields. Missing values warn/block final export as required by the accepted template; they do not block clock, fingerprint, roster or attendance operation.

### SECTION_PROFILE

Exactly one configured section at any effective date. Proposed fields:

- `profileId`;
- `curriculum`: `REGULAR_SHS` or `STRENGTHENED_SHS` (R1);
- `schoolYear`, `gradeLevel`, `sectionName` (R2);
- optional `track`, `strand`, `course` or curriculum-specific offering label;
- optional `academicPeriodLabel`.

The `Semester`/three-term meaning is OPEN (R9). The store must retain the field as unknown/null until the school confirms it. Never substitute Term 1/2/3 for Semester without an approved rule. A report month spanning a section/curriculum profile change is blocked until the applicable form policy is known.

### LEARNER_IDENTITY

Keyed by the existing `studentId` string. Proposed fields: `lrn`, `sex`, `firstName`, `middleName`, `lastName`, `extensionName`. LRN is stored as text to preserve leading zeroes. The current browser schema validates 12 digits when supplied, but its exact format/requiredness must be confirmed against the school-approved source. Sex and name components may be incomplete during attendance setup; incomplete metadata must not stop normal attendance, but must block the corresponding final report fields/subtotals.

Identity versions are effective-dated and transaction-revisioned. Two current records for the same student may not overlap in effective time. The same non-empty LRN may not be assigned to two different students over overlapping effective periods.

### MOVEMENT

An immutable, effective-dated event keyed to `studentId`: `ENROLLED`, `TRANSFER_IN`, `TRANSFER_OUT`, `SHIFTED`, `DROPPED`, or `OTHER`, with optional reason code/text and destination school/section. Missing details remain missing and produce warnings; they are never inferred from `RA_DEACTIVATE` alone.

The current operational roster has only activation/deactivation and a single display name. It cannot establish transfer vs dropout, destination, or reason. A movement record must be explicitly entered/confirmed by an administrator. It must be cross-checked against the roster interval but must not silently rewrite that interval.

## 4. Setup and lifecycle

- Initial operational setup asks for curriculum, grade level and section name as required by R1/R2. Track/strand/offerings are required only when the selected curriculum/form requires them.
- School reporting metadata (school name/ID, district/division/region, adviser and school head) can be completed later, as proposed by the user.
- Missing school-reporting fields do not block attendance operation. They do block or visibly warn the final report according to the confirmed field requirement.
- Learner identity metadata is linked to the existing device ID after learner registration. A missing LRN/sex/name component is a reporting completeness issue, not an attendance authorization issue.
- Changing the one-section profile mid-school-year, changing curriculum, or reassigning the device to another section has no approved operational workflow yet. Do not add a silent edit path. Treat a profile change as blocked pending an explicit rollover/migration rule.
- A report uses every learner whose authoritative roster interval overlaps the month (R3), including transfer-outs. The report roster is not just today's active roster.

## 5. Persistence options — no choice approved

| Option | Benefit | Problem | Assessment |
|---|---|---|---|
| Existing 16-byte attendance event log | Existing append-only ordering and backup path | Current event types do not represent the required PII/history; adding new event types expands `EventGroupAudit`, DayView, replay, restore and compatibility scope | **Do not use as the first implementation** |
| NVS A/B blobs | Familiar config transaction pattern | NVS partition is only `0x5000` (20 KiB); 300 learner identities plus revisions/movements and history are not a safe fit; repeated full rewrites also need wear/capacity measurements | **Not recommended for the complete metadata store** |
| Dedicated LittleFS metadata journal | Keeps reporting PII and schemas separate from attendance events; append-only transactions fit historical revisions | LittleFS adapter does not exist; C13 power-loss/atomicity is unverified; at-rest encryption is unresolved; backup/restore currently omits this component | **Preferred direction, but blocked** |

The recommended next design is a dedicated reporting-metadata journal with versioned transaction frames, checksums, explicit commit framing and replay. CRC protects against accidental corruption only; it is not encryption or a MAC. Do not assume `flush`, rename, truncate or copy is atomic. Any checkpoint/compaction scheme must retain a recoverable prior generation and be tested with actual power cuts.

No binary field widths, file names, page sizes, compaction cadence, storage adapter or encryption key source are approved by this document.

## 6. Security, backup and restore

LRN, learner names, sex, school identity and movement details are sensitive reporting data. Before the snapshot endpoint exists:

- authenticate every reporting metadata read/write and every export page;
- never expose reporting data via public `GET /status`;
- use `Cache-Control: no-store`; do not put LRN/identities in URL query strings, browser localStorage, debug logs or serial logs;
- do not log PINs, tokens, LRN or full legal names;
- enforce the existing PIN/token/lockout duties in the caller (C6 is still open);
- explicitly resolve at-rest confidentiality. Existing NVS encryption/flash key provisioning is not established (C16), and LittleFS plaintext would expose the new PII on physical extraction.

The signed backup currently contains config + roster + attendance log (O8). If reporting metadata is persisted separately, backup/restore/year rollover must include it as a required versioned component. A restore that installs attendance but drops its reporting metadata is incomplete and must not silently succeed. The backup inner format, authenticated component manifest, restore replay and storage swap need a coordinated version change; C2/C12/C13 remain blockers.

## 7. What this model does not decide

- The physical persistence format or encryption implementation.
- LRN requiredness and exact source validation.
- The Semester/term label (R9).
- School-specific movement/NLS codes and required reason/destination fields.
- Whether name/sex changes inside a report month use start-of-month, end-of-month or another approved rule.
- Who may perform corrections and how a shared admin session is attributed. Current architecture has no individual staff identity.
- Whether the school accepts the customized AM/PM cell convention (R4/R10).

## 9. Identity review policy pointer (v0.2.30)

The review policy is maintained in `REVIEW_EVIDENCE_DESIGN_v0.6.md`. It is deliberately not implemented by this metadata-history validator. The validator must not treat `sourceStatus: SOURCE_CHECKED` as evidence or authorize reports. The future server-side review action must bind a self-review to the exact identity record revision and source-policy revision, and must receive its committed sequence from the persistence layer. O8 remains unchanged; metadata persistence/API/export stay disabled pending an approved recovery workflow.

## 8. Reference validator status

`web/sf2/metadata_history_model.mjs` validates the logical proposal and supports as-of-revision/effective-date lookups and month movement selection. It is host/browser reference code only. It does not read ESP32 storage, authenticate a caller, prove transaction commit durability, or replace `snapshot_schema.mjs`. Its 23 regression cases are authored but were not run in v0.2.21.
