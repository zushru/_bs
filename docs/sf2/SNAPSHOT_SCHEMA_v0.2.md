# Proposed SF2 reporting snapshot schema v0.2

Status: **design/reference contract only**. This is not an implemented HTTP API, persistent storage schema, or security boundary. `web/sf2/snapshot_schema.mjs` validates this proposal in the browser. A future ESP32 endpoint must generate the facts and revisions from authoritative device state.

## Contract rules

- One snapshot represents one configured device section, one curriculum profile, and one `YYYY-MM` report month.
- The client does not send a section selector. The device supplies its configured profile.
- Every calendar date in the month appears exactly once. This is required to distinguish a verified non-class day from a missing calendar record.
- Coverage appears exactly once per scheduled date/session.
- Every learner in the month-overlap roster has one attendance entry. For every class date on which that learner was enrolled, the entry must have AM and PM outcomes.
- `Event.seq` is a C++ `uint32_t`; all event references must be positive, at most `0xFFFFFFFF`, and no greater than `attendanceLastSeq`. No source sequence can be referenced by more than one attendance result or close group.
- `ABSENT_CONFIRMED` is derived, not a stored event. It must match complete coverage for the corresponding date/session and an enrollment interval containing that date.
- The browser ignores any `finalizable` property if one is added. Readiness is calculated from the complete envelope, the revision tuple and attendance outcomes.
- Server-side serialization/revision-recheck is still required. The browser can check internal agreement, but cannot prove that the server actually locked or rechecked the underlying stores.

## Top-level shape

```json
{
  "schemaVersion": 1,
  "snapshotId": "opaque-id",
  "month": "2026-11",
  "sourceRevision": {
    "attendanceLastSeq": 55,
    "rosterRevision": "roster-r12",
    "configGeneration": 3,
    "calendarRevision": "calendar-r4",
    "reportingProfileRevision": "reporting-rp2",
    "schoolProfileRevision": "school-sp1",
    "learnerMetadataRevision": "learner-meta-r3"
  },
  "roster": { "revision": "roster-r12" },
  "learnerMetadata": { "revision": "learner-meta-r3" },
  "reportingProfile": {},
  "schoolProfile": {},
  "calendar": { "revision": "calendar-r4", "configGeneration": 3, "days": [] },
  "learners": [],
  "attendance": [],
  "coverage": [],
  "movementHistory": [],
  "completeness": {
    "rosterHistoryComplete": true,
    "calendarComplete": true,
    "scheduleHistoryComplete": true,
    "attendanceSnapshotConsistent": true
  }
}
```

`sourceRevision` is a tuple, not a single counter. The separate `roster.revision`, `learnerMetadata.revision`, reporting-profile revision, school-profile revision and calendar revision must match this tuple. `configGeneration` is tied to the calendar because the current timetable is part of operational configuration. `learnerMetadataRevision` covers the reporting identities and movement information that are not present in the operational `RosterStore`.

## Reporting profile

Required fields:

- `profileId`, `revision`
- `curriculum`: `REGULAR_SHS` or `STRENGTHENED_SHS`
- `schoolYear`: consecutive `YYYY-YYYY`
- `gradeLevel`: `11` or `12`
- `sectionName`
- `effectiveFrom`, `effectiveToExclusive` (`null` means no known end)

`track`, `strand`, `course`, and `semesterLabel` are nullable. The meaning of the workbook's `Semester` field is still OPEN; the validator does not invent a term-to-semester mapping. The one-section rule means one effective section/curriculum profile is associated with the report. A class date outside that profile's effective range blocks validation.

## School and learner metadata

`schoolProfile` has its own revision and nullable `schoolName`, `schoolId`, `district`, `division`, `region`, `adviserName`, and `schoolHeadName`. Missing values are warnings at this design stage; the final template adapter must apply the approved required-field policy.

Each learner has:

- `studentId`: the existing never-reused device identity, 1..300. This does not replace or alter P9.
- `sex`: `M` or `F`, required for the template subtotals.
- `identity`: `firstName`, `lastName`, nullable `middleName`, nullable `extensionName`, and nullable 12-digit `lrn`.
- `enrollmentPeriods`: one or more non-overlapping `[startInclusive, endExclusive)` periods with `profileId` equal to the configured section profile.

Only learners with at least one enrollment interval overlapping the report month appear. This includes transfers out during the month. The month-overlap roster has no separate four- or 45-row export cap; pagination is driven later by a measured workbook layout. The device's never-reused ID space is still limited to 300 identities over its lifetime.

Names and LRNs are currently absent from the operational roster. These fields belong in a separate reporting-identity store/projection, not in the XLSX calculation model or a casual mutation of `RosterStore`.

## Calendar

`calendar.days` contains every day of the month, exactly once:

- `CLASS_DAY`: `scheduledSessions` contains `AM`, `PM`, or both; one-session days are structurally valid but block final calculation while R8 is open.
- `NON_CLASS_DAY`: no scheduled sessions and a non-empty reason code.
- `OUTSIDE_TERM`: no scheduled sessions and a non-empty reason code.

The calendar is a full-month, section-specific snapshot. A weekday timetable alone is insufficient. C17/E2 prevents safe reconstruction of historical scheduled sessions after timetable changes; `scheduleHistoryComplete` must remain false until durable historical schedule semantics are resolved.

## Attendance and coverage

Each `attendance` element is `{ "studentId": 1, "days": [...] }`. Each day is `{ "date": "YYYY-MM-DD", "am": {...}, "pm": {...} }`.

Session statuses accepted by the reference validator:

- `PRESENT`, `LATE`, `EXCUSED`: require `sourceEvent: { seq, type, status }`; type is `SCAN` or `MARK`; EXCUSED/CLEAR must be MARK.
- `ABSENT_CONFIRMED`: derived and has no `sourceEvent`; it must be present in the corresponding CLOSE expected-student bitmap.
- `CLEAR`: has a MARK source reference but remains unresolved until device derivation produces a final state.
- `UNRESOLVED`: explicitly unresolved; never mapped to X.
- `NOT_SCHEDULED`: only when the calendar confirms that session is not scheduled.

`coverage` rows contain `date`, `session`, `sessionClosed`, `coverageComplete`, `logHealthy`, and `gapAffected`. A closed row must also contain `closeCoverageFlag`, `closeEventSeq`, `bitmapPayloadSeqs`, `sourceLastSeq`, `expectedStudentIds`, and `expectedCount`. `coverageComplete` must equal `closeCoverageFlag`; a true close flag with a reported overlapping GAP is contradictory. The current project has `MAX_ID=300`, so one `EV_CLOSE` commit group must contain exactly five 64-bit `EV_PAYLOAD` bitmap records. `closeEventSeq` identifies the `EV_CLOSE`; `bitmapPayloadSeqs` lists all five immediately following record sequences in order; `sourceLastSeq` equals the final payload sequence. All six sequence values must be contiguous, unique across coverage rows, and within `attendanceLastSeq`. They must not overlap any `EV_SCAN` or `EV_MARK` sequence referenced by attendance. An unclosed row must use null for the close coverage flag, close sequence, payload sequence array, source last sequence, expected IDs and expected count. There must be a row for each scheduled session and no row for unscheduled sessions.

The ESP32 snapshot builder must obtain these references from the actual committed log group, decode all five bitmap payloads, check the bitmap popcount against the `EV_CLOSE.a` expected count, and validate the complete group/sequence framing. The browser checks the envelope and sequence consistency; it cannot prove that the server decoded the source bytes correctly. A generic `sessionClosed=true` flag is not evidence. An absence claim is invalid unless the matching row is closed, complete, healthy, gap-free, contains that student ID in its expected set, and has valid close-group provenance; the snapshot consistency flag must also be true. The validator cross-checks the close bitmap against the month-overlap enrollment intervals and rejects mismatches.

A non-class date has no attendance rows and no coverage rows. A class day with one scheduled session has an explicit `NOT_SCHEDULED` result for the other half, but remains blocked until a teacher-approved local daily-cell mapping is recorded.

## Movement history

`movementHistory` is an array of month-scoped events referencing the month-overlap roster. Supported proposed types: `ENROLLED`, `TRANSFER_IN`, `TRANSFER_OUT`, `SHIFTED`, `WITHDRAWN`, `DROPPED`, `NLS`, `OTHER`. Each movement includes an explicit `effectiveDateMeaning`: `FIRST_ENROLLED_DAY`, `LAST_ENROLLED_DAY`, `FIRST_NOT_ENROLLED_DAY`, `SECTION_CHANGE_DAY`, `EVENT_DAY`, or `UNCONFIRMED`. The validator checks date-meaning/type compatibility; `UNCONFIRMED` and raw `NLS` remain warnings and cannot be silently normalized. NLS is accepted only with `effectiveDateMeaning: UNCONFIRMED`; it must not imply a learner-exit boundary. Multiple movement records for one learner on the same effective date block report projection. `SHIFTED` must match an enrollment interval boundary; the single-section model cannot infer a section change from text alone. Reason/destination fields are nullable at this stage; missing transfer reasons or destinations produce warnings because NLS/remarks may be incomplete. No movement facts are inferred from a bare roster deactivation.

## Completeness and readiness

The device explicitly supplies:

- `rosterHistoryComplete`
- `calendarComplete`
- `scheduleHistoryComplete`
- `attendanceSnapshotConsistent`

False values produce an incomplete snapshot and block report calculation. Missing or non-boolean flags make the schema invalid. These flags must be calculated by the device, never set by the browser.

The validator returns one of:

- `VALID`: shape, revisions, expected attendance, event references and coverage pass this proposed contract; calculation may proceed.
- `INCOMPLETE`: shape is valid, but there are unresolved/missing facts or false completeness flags; calculation/export is blocked.
- `INVALID`: malformed data, revision disagreement, impossible roster/event/coverage relationships, or false absence claims; export is blocked.

`VALID` does not mean the official template is approved, all school metadata is present, or XLSX export is available. The current preview model deliberately returns `finalExportReady: false` because no approved template writer exists.

## Source reconciliation

See `CORE_SOURCE_MAPPING.md` for the actual event and roster fields in v0.2.17. The proposed schema is intentionally separate from C++ storage and must be generated from a consistent device snapshot. No endpoint has been implemented.

## Close-group source sequence example

For a 300-ID device, a valid close group can be represented as:

```json
{
  "date": "2026-11-02",
  "session": "AM",
  "sessionClosed": true,
  "coverageComplete": true,
  "closeCoverageFlag": true,
  "logHealthy": true,
  "gapAffected": false,
  "closeEventSeq": 8,
  "bitmapPayloadSeqs": [9, 10, 11, 12, 13],
  "sourceLastSeq": 13,
  "expectedStudentIds": [1],
  "expectedCount": 1
}
```

The example is synthetic. The bitmap IDs must be decoded from all five payload records in the actual `LogStore` stream. The browser must not synthesize the list from the learner roster.


## v0.2.21 metadata-history boundary
The separate `metadata_history_model.mjs` validates the proposed bitemporal logical metadata history described in `REPORTING_METADATA_DESIGN_v0.3.md`. It is **not** the device snapshot API schema and is not persisted by v0.2.21. The snapshot envelope's current revision tuple does not yet include the complete log open-generation/count/last-sequence and live `EventGroupAudit` generation fields described in `SNAPSHOT_COORDINATOR_DESIGN_v0.3.md`; the architecture design is stricter than the current browser envelope. Do not infer that a snapshot has been atomically captured or physically committed because its metadata JSON passes validation.


## v0.2.25 D3 provenance boundary

`validateSnapshot()` remains a schema and relationship validator, not an upstream provenance or identity-review verifier. The D3 integration helper `joinReportingRosterForSnapshot()` checks the D1/D2 exact prefix and learner/interval/movement joins, but it deliberately returns `INCOMPLETE` because authenticated reviewer evidence is not implemented. A caller can still construct a structurally valid snapshot directly; therefore the browser calculator now labels a successful calculation `PREVIEW_READY`, sets `authoritativeExportReady:false`, and never grants an authoritative `READY` state. The proposed schema's `VALID` result must not be treated as authorization to export.
