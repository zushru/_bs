# SF2 snapshot coordinator — design v0.3

Status: **architecture proposal only**. No firmware coordinator, HTTP route, lock, token table, or production snapshot storage was added.

## 1. Problem the coordinator must solve

The browser cannot combine independent reads of the attendance log, roster, Config, calendar, school profile and learner metadata. A snapshot assembled from different moments can create a false absence, omit a transfer, or use the wrong section/curriculum. The browser validator can detect disagreements in returned fields; it cannot make the ESP32 sources atomic.

The current system is a single-writer serialized application-loop design. Future HTTP reads must use that serialization boundary. The browser must not choose an arbitrary `section_id`: R2 says one section is configured on the device.

## 2. Source revision vector

A future `SnapshotTicket` should capture these values at one serialized boundary:

| Revision field | Source | Existing? |
|---|---|---|
| `logOpenGeneration` | `LogStore::openGeneration()` | Yes, v0.2.20 |
| `logRecordCount`, `logLastSeq` | `LogStore::count()`, `lastSeq()` | Yes |
| `eventAuditGeneration` | `EventGroupAudit::generation()` | Yes, volatile observer generation |
| `configGeneration` | selected A/B `CfgPick::seq` | Yes |
| `rosterRevision` | frozen event-log prefix identity plus roster replay result | Derived; no standalone counter |
| `calendarRevision`, `scheduleHistoryComplete` | timetable/no-class/calendar sources | Not complete; C17/E2 blocks historical certainty |
| `schoolProfileRevision` | reporting metadata store | Not implemented |
| `sectionProfileRevision` | reporting metadata store | Not implemented |
| `learnerMetadataRevision` | reporting metadata store | Not implemented |
| `movementRevision` | reporting metadata store | Not implemented |

Do not use wall-clock time as a source revision. RTC may move backwards; source sequence and explicit store generations are the revision identities.

## 3. Capture preconditions (all fail closed)

1. Request is authenticated; the requested month is syntactically valid and within the configured year/term policy. No client-selected section.
2. The authoritative `LogStore` is open and healthy. The exact `OpenInfo` from that open is retained; it must have no repaired/dropped tail and its `generation` must equal `LogStore::openGeneration()`.
3. One long-lived `EventGroupAudit`, attached after that open, returns `readyFor(log, exactOpenInfo)`. Unsupported event types continue to block until a type-specific validator exists. Do not attach one audit per request or exhaust the 16 observer slots.
4. `RosterStore::historyComplete()` is true, and a separate roster-transition semantic audit has passed. `historyComplete()` alone is only parser/overflow health; it does not prove chronological transition correctness.
5. Historical name projection has reached its explicit end state without error. The existing `RosterNameHistoryReader` currently aborts if the log count changes; a pinned-prefix reader is needed if export is paginated while attendance can append.
6. The config copy and `configGeneration` are captured together. C17/E2 and C18/E26 remain unresolved, so historical calendar/closure claims affected by these issues must be marked incomplete and cannot support `ABSENT_CONFIRMED`.
7. Reporting metadata store and school/section profiles are readable at one pinned metadata revision. Every month-overlap learner's reporting identity is resolved by student ID and valid-time date; missing fields block only the relevant final report, not normal attendance.
8. The `MonthSet`/underlying file driver is not directly modified outside `LogStore` during the snapshot. The app loop owns all writes. A raw driver write bypasses current in-memory revisions and invalidates the guarantee.

## 4. Snapshot ticket and prefix model

Proposed ticket contents:

```text
snapshotId                 opaque random identifier
month                      YYYY-MM
logOpenGeneration          captured LogStore open generation
logRecordCount             captured prefix length
logLastSeq                 captured prefix last sequence
eventAuditGeneration       audit generation at capture
configGeneration           selected config sequence; Config copied for the ticket
rosterRevision             derived from the captured log prefix
calendarRevision           captured calendar source revision (or incomplete)
metadataRevision           captured metadata transaction revision
createdMonotonic           monotonic tick; never RTC
expiresMonotonic           bounded ticket expiry
```

`LogStore` is append-only during normal operation. A future page reader may read only indexes `< logRecordCount`; an appended record after ticket creation must not change the ticket's prefix. A reopen, repair, truncate, storage fault, or changed prefix invalidates the ticket. The current `RosterNameHistoryReader` treats any append as a change, so it must not be reused unchanged for a multi-request pinned-prefix flow. A separate scratch `RosterStore` must be rebuilt from the same captured prefix rather than using the live observer that keeps changing as new records append.

For a fixed ticket, copy small mutable inputs (Config/profile revision identifiers) or retain immutable historical versions until ticket expiry. Do not copy the entire month attendance tensor into ESP32 RAM. Memory must remain bounded and measured on the target.

## 5. API shape: revise the single-response proposal

A single response to `GET /api/v1/export/sf2-data?month=YYYY-MM` can contain thousands of learner/date/session results. The existing architecture uses a synchronous core `WebServer` in the main loop. Sending a large response from one handler can monopolize that loop and delay UART polling, attendance scheduling, closure and health work. No measured safe maximum response size or serving time exists.

**Recommended future protocol (not implemented):**

1. `POST /api/v1/export/sf2-snapshots` with `{ "month": "YYYY-MM" }` creates a short-lived ticket for the configured section and returns `snapshotId`, revision vector, expiry and page descriptors.
2. `GET /api/v1/export/sf2-snapshots/{snapshotId}/{resource}?cursor=...` returns one bounded page (roster, calendar, attendance, movement data). Every page repeats `snapshotId` and the complete revision vector.
3. The client assembles pages only when every page has the same ticket/revisions and all page counts/checksums agree. The browser runs `validateSnapshot()` and calculation only after every resource is complete.
4. If the token expires, log generation changes, storage faults, metadata retention is lost, or any source revision no longer resolves, the device returns a stale-snapshot error (proposed HTTP 409) and the browser discards all pages. Never mix pages from different snapshots.

The single GET route proposed earlier should be treated as a conceptual data request, not yet a final transport contract. If the team later proves that one synchronous bounded transfer does not hurt the scan loop on hardware, a single route may be reconsidered. No async web-server dependency is selected; current libraries are not pinned for that purpose.

## 6. Client/server responsibilities

### ESP32

- Validate auth, month and configured profile.
- Capture the revision vector and ticket.
- Derive effective statuses from `DayView`/authoritative device state; never ask the browser to replay raw event bytes.
- Obtain expected-student IDs from the actual six-record CLOSE group (one `EV_CLOSE` + five `EV_PAYLOAD`) and validate them through `EventGroupAudit` plus roster transition validation.
- Emit explicit missing/incomplete facts. Never manufacture `coverageComplete`, `calendarComplete`, or `rosterHistoryComplete` in the browser.
- Emit source event/close-group references and consistent revisions.
- Enforce per-ticket page limits, expiry, authentication and no-store headers.

### Browser

- Request one snapshot ticket for one month.
- Download all pages and reject mismatched tokens, revisions, duplicate/missing page indexes, IDs or source sequences.
- Validate the completed envelope with the SF2 schema validator.
- Calculate local AM/PM diagonal marks using the signed reporting-layer decisions R4-R7; do not modify device marks.
- Keep unresolved rows visible; final export remains blocked until all teacher-dependent and template blockers are cleared.

## 7. Failure behavior

| Failure | Required behavior |
|---|---|
| No auth / expired admin token | Return 401; do not reveal whether IDs/LRNs exist |
| Invalid month or malformed cursor | Return 422 |
| Config/log/storage not healthy | Return a fault or incomplete state; do not return confirmed absences |
| Event audit false/stale open generation | Refuse snapshot creation |
| Roster interval overflow or semantic transition error | Refuse a finalizable roster snapshot |
| C17/E2 or C18/E26 unresolved | `scheduleHistoryComplete=false`; block absence finalization for affected scope |
| Metadata record missing | Return explicit incomplete warning; no invented school/LRN/sex/name fields |
| Source invalidated during page reads | Return 409; discard the whole snapshot |
| Client asks for another section | Reject; configured section only |

## 8. Known blockers and required implementation order

1. Decide and implement metadata persistence after C13, PII encryption and backup/restore design are addressed.
2. Add a roster-transition semantic audit (C24/U61).
3. Add a pinned-log-prefix projection API; do not weaken the current reader's changed-snapshot protection.
4. Define calendar completeness after C17/E2 and C18/E26.
5. Add a source revision registry and ticket coordinator in the serialized application loop.
6. Add bounded page serializers and only then an authenticated HTTP adapter; measure main-loop latency under AP + sensor activity on hardware.
7. Add client page assembly, full snapshot validation and browser preview.

No part of this coordinator is production-ready in v0.2.21.
