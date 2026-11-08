# SF2 browser snapshot fixture matrix v0.2

All fixtures are synthetic. They validate browser logic only and do not represent real learners or actual school records.

## Complete base fixture

- Reporting month: November 2026; explicit row for all 30 calendar dates.
- Four class dates: 2, 3, 5 and 6 November; 4 November is a declared holiday; other dates are explicit non-class dates in the fixture calendar.
- Three learners are included by enrollment overlap:
  - Student 1 (M): enrolled 1 October through 3 November inclusive; transfer-out effective 4 November.
  - Student 2 (F): enrolled beginning 5 November and remains enrolled.
  - Student 3 (F): enrolled 3-5 November inclusive; transfer-out effective 6 November.
- Each scheduled AM/PM session has a closed, complete, healthy, gap-free coverage row with the exact expected-student IDs/count copied from the synthetic close bitmap.
- Attendance cases include both sessions present, Late + confirmed absent, Excused + confirmed absent, both sessions absent, and AM-only attendance.
- Seven attendance event references occupy sequences 1..7. Eight close groups then occupy sequences 8..55: each group consists of one `EV_CLOSE` plus five contiguous `EV_PAYLOAD` records (300-ID bitmap). Each coverage row records its exact close sequence, five payload sequences and final group sequence. All references are globally unique and bounded by `attendanceLastSeq=55`; close-time expected sets are cross-checked with half-open enrollment intervals.

## Expected local cell outcomes

| Student | Date | AM | PM | Cell | Monthly day count |
|---|---|---|---|---|---|
| 1 | 2026-11-02 | Present | Present | Blank | Present |
| 1 | 2026-11-03 | Late | Confirmed absent | Upper-left shading | Present |
| 2 | 2026-11-05 | Excused | Confirmed absent | E | Present |
| 2 | 2026-11-06 | Present | Present | Blank | Present |
| 3 | 2026-11-03 | Confirmed absent | Confirmed absent | x | Absent |
| 3 | 2026-11-05 | Present | Confirmed absent | Upper-left shading | Present |

Late-session observations are retained. A monthly TARDY total remains null/deferred and is never set to zero by this model.

## Negative and incomplete cases authored

- Missing/duplicate calendar date; invalid month and unsupported schema version.
- Revision disagreement among roster, learner metadata, reporting profile, school profile and calendar; calendar/config-generation mismatch.
- Wrong section profile, class date outside profile effective range, malformed school year/grade/curriculum.
- Duplicate or out-of-range student IDs; duplicate LRN; missing sex/name; invalid LRN; overlapping or wrong-profile enrollment periods.
- Missing calendar row, class day without sessions, non-class day with sessions.
- Missing/duplicate/unknown attendance entries; missing expected learner/date or session; attendance outside enrollment/class dates.
- Missing coverage, duplicate/extra coverage, unclosed/incomplete/unhealthy coverage, GAP, close sequence beyond the source revision, missing/truncated/non-contiguous/duplicated close-group payload sequence, close groups sharing source sequences, malformed close expected bitmap/count, unknown IDs in a close bitmap, and disagreement between close-time expectation and enrollment history.
- Event source sequence outside snapshot or uint32 range, duplicate source event reference, source event status mismatch, invalid SCAN/MARK provenance, attendance/close-group sequence collision, unresolved or not-scheduled result with an event reference.
- `ABSENT_CONFIRMED` without matching trusted coverage or on a day outside enrollment.
- CLEAR/UNRESOLVED, NOT_SCHEDULED/calendar mismatch, and the open one-session-class-day rule.
- Invalid movement references/dates and missing transfer reason/destination warnings.
- Missing school metadata/LRN warnings; a caller-supplied `finalizable=true` cannot bypass readiness.

The new tests were authored but intentionally not executed in v0.2.19.

## Source encoding boundary

The synthetic close groups model the actual `LogStore::append()` framing: one `EV_CLOSE` followed by five 64-bit expected-ID payloads for the device's `MAX_ID=300`. The browser fixture does not include raw event bytes or validate CRCs; a future ESP32 snapshot builder must decode the actual committed records and validate the group before producing these projections.
