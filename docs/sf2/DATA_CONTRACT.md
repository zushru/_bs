# SF2 reporting data contract

Status: **proposed browser/API contract; not a deployed endpoint or storage schema**.

The detailed wire shape and validation rules are maintained in [`SNAPSHOT_SCHEMA_v0.2.md`](SNAPSHOT_SCHEMA_v0.2.md). The source-to-core mapping is in [`CORE_SOURCE_MAPPING.md`](CORE_SOURCE_MAPPING.md).

## Non-negotiable boundaries

- The ESP32 remains authoritative for attendance, operational roster state, timetable/configuration, no-class state, event-log closure and coverage evidence.
- The browser receives one consistent, authenticated, read-only snapshot for the configured section and reporting month. The client cannot choose an arbitrary section.
- The browser calculates the SF2 report and will eventually write the approved XLSX template. No XLSX generation, spreadsheet-cell manipulation or report formatting belongs on the ESP32.
- The attendance source events remain unchanged. SF2 codes are a reporting projection only.
- Missing or unreliable facts never become absence.
- The contract must preserve the device's existing student ID and sensor-slot rules. LRN is a separate reporting identifier, not a replacement key.
- The custom AM/PM diagonal convention is a local, user-selected reporting rule. School acceptance remains open; do not claim official compliance.

## Snapshot completeness

A usable snapshot has:

1. a revision tuple tying attendance, roster, operational config, calendar, reporting profile, school profile and learner metadata together;
2. one calendar row for every date in the month;
3. every learner whose enrollment overlaps any part of the month;
4. one effective attendance result for each applicable learner/date/session;
5. one coverage row for every scheduled date/session;
6. exact close-group provenance (`EV_CLOSE` and all five `EV_PAYLOAD` sequence references) for finalized sessions;
7. explicit completeness flags from the device; and
8. transfer/movement history where the source system has enough facts.

`VALID` means only that the proposed snapshot is internally consistent enough for browser calculation. It does not mean that an XLSX template is approved, teacher-dependent policy is settled, the API is deployed, or a report is accepted by DepEd.

## Identity review and persistence gate (v0.2.30)

The D4 review policy is specified in `REVIEW_EVIDENCE_DESIGN_v0.6.md`. It permits one authenticated ADMIN self-review, four school-configurable source categories, exact identity-revision binding, and commit-sequence ordering. The browser cannot authenticate the action or establish a durable commit. O8 remains config + roster + log; no reporting PII may be persisted or exposed until a separately approved metadata recovery path exists. The current snapshot remains preview-only.

## Current limitations

- `RosterStore` cannot enumerate its complete interval history and overwrites historical names in its in-memory name table (C24).
- The log's close-time expected bitmap is authoritative for closure; the future endpoint must extract the full committed close group, not infer attendance expectation from today's roster.
- Historical timetable revisions are not stored (C17/E2), so a complete historical class calendar cannot yet be certified after schedule changes.
- The uploaded XLSX remains reference-only and structurally unsafe (C20).
- No production snapshot HTTP route, reporting metadata store, dashboard integration or XLSX writer exists (C21/C22).

Do not implement integration by weakening this contract. Resolve the source limitations or return an explicitly incomplete snapshot.


## v0.2.21 metadata and snapshot coordination boundary
The bitemporal metadata model is defined separately in `REPORTING_METADATA_DESIGN_v0.3.md` and validated by `web/sf2/metadata_history_model.mjs`. It is not part of the production device data contract yet. The production snapshot coordinator design is in `SNAPSHOT_COORDINATOR_DESIGN_v0.3.md`; it requires a stronger revision vector and bounded pages than the current browser envelope can itself prove. No endpoint, persistence adapter, or commit guarantee exists.
