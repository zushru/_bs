import test from 'node:test';
import assert from 'node:assert/strict';
import { validateSnapshot } from '../snapshot_schema.mjs';
import { cloneSnapshot, makeValidSnapshot } from '../fixtures/snapshot_fixtures.mjs';

const valid = () => validateSnapshot(makeValidSnapshot());
const codes = (result) => new Set([...(result.issues ?? []), ...(result.blockers ?? [])].map((item) => item.code));

test('accepts a complete structurally consistent snapshot', () => {
  const result = valid();
  assert.equal(result.state, 'VALID');
  assert.equal(result.monthRoster.length, 3);
  assert.equal(result.classDays.length, 4);
});

test('rejects unsupported schema versions', () => {
  const snapshot = cloneSnapshot(); snapshot.schemaVersion = 2;
  assert.ok(codes(validateSnapshot(snapshot)).has('UNSUPPORTED_SCHEMA_VERSION'));
});

test('rejects snapshot month mismatch', () => {
  const snapshot = cloneSnapshot(); snapshot.month = '2026-10';
  assert.ok(codes(validateSnapshot(snapshot, { expectedMonth: '2026-11' })).has('SNAPSHOT_MONTH_MISMATCH'));
});

test('rejects malformed month values', () => {
  const snapshot = cloneSnapshot(); snapshot.month = '2026-13';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_MONTH'));
});

test('requires opaque snapshot identity', () => {
  const snapshot = cloneSnapshot(); snapshot.snapshotId = '';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_SNAPSHOT_ID'));
});

test('rejects a missing source revision', () => {
  const snapshot = cloneSnapshot(); delete snapshot.sourceRevision.rosterRevision;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_SOURCE_REVISION'));
});

test('rejects negative attendance sequence', () => {
  const snapshot = cloneSnapshot(); snapshot.sourceRevision.attendanceLastSeq = -1;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_SOURCE_REVISION'));
});

test('rejects config revision disagreement', () => {
  const snapshot = cloneSnapshot(); snapshot.reportingProfile.revision = 'wrong';
  assert.ok(codes(validateSnapshot(snapshot)).has('PROFILE_REVISION_MISMATCH'));
});

test('rejects calendar revision disagreement', () => {
  const snapshot = cloneSnapshot(); snapshot.calendar.revision = 'old';
  assert.ok(codes(validateSnapshot(snapshot)).has('CALENDAR_REVISION_MISMATCH'));
});

test('requires exactly one calendar row for every date in the month', () => {
  const snapshot = cloneSnapshot(); snapshot.calendar.days.pop();
  assert.ok(codes(validateSnapshot(snapshot)).has('CALENDAR_DAY_MISSING'));
});

test('rejects duplicate calendar dates', () => {
  const snapshot = cloneSnapshot(); snapshot.calendar.days[1].date = snapshot.calendar.days[0].date;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_CALENDAR_DATE'));
});

test('rejects a class day without a scheduled session', () => {
  const snapshot = cloneSnapshot();
  snapshot.calendar.days[0].kind = 'CLASS_DAY';
  snapshot.calendar.days[0].scheduledSessions = [];
  snapshot.calendar.days[0].reasonCode = null;
  snapshot.calendar.days[0].reasonText = null;
  assert.ok(codes(validateSnapshot(snapshot)).has('CLASS_DAY_WITHOUT_SESSIONS'));
});

test('rejects sessions attached to a non-class day', () => {
  const snapshot = cloneSnapshot(); snapshot.calendar.days[3].scheduledSessions = ['AM'];
  assert.ok(codes(validateSnapshot(snapshot)).has('NON_CLASS_DAY_HAS_SESSIONS'));
});

test('rejects duplicate student IDs', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[1].studentId = 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_STUDENT_ID'));
});

test('rejects student IDs outside the device identity range', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[0].studentId = 301;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_STUDENT_ID'));
});

test('rejects a learner with missing sex', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[0].sex = null;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_LEARNER_SEX'));
});

test('rejects malformed LRN values', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[0].identity.lrn = '12345';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_LRN'));
});

test('rejects overlapping enrollment periods', () => {
  const snapshot = cloneSnapshot();
  snapshot.learners[0].enrollmentPeriods.push({
    profileId: snapshot.reportingProfile.profileId,
    startInclusive: '2026-11-01', endExclusive: '2026-11-20',
  });
  assert.ok(codes(validateSnapshot(snapshot)).has('OVERLAPPING_ENROLLMENT_PERIODS'));
});

test('rejects an enrollment interval with a mismatched section profile', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[0].enrollmentPeriods[0].profileId = 'other';
  assert.ok(codes(validateSnapshot(snapshot)).has('SECTION_PROFILE_MISMATCH'));
});

test('selects learners enrolled at any point during the month', () => {
  const result = valid();
  // Male learners first; within each sex group, preserve the source roster order.
  assert.deepEqual(result.monthRoster.map((learner) => learner.studentId), [1, 2, 3]);
});

test('flags incomplete historical roster without pretending the schema is malformed', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.rosterHistoryComplete = false;
  const result = validateSnapshot(snapshot);
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(codes(result).has('ROSTER_HISTORY_INCOMPLETE'));
});

test('flags incomplete timetable history separately from calendar rows', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.scheduleHistoryComplete = false;
  assert.ok(codes(validateSnapshot(snapshot)).has('SCHEDULE_HISTORY_INCOMPLETE'));
});

test('flags inconsistent snapshot revisions', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.attendanceSnapshotConsistent = false;
  assert.ok(codes(validateSnapshot(snapshot)).has('SNAPSHOT_INCONSISTENT'));
});

test('rejects duplicate attendance entries for a learner', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance.push(structuredClone(snapshot.attendance[0]));
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_ATTENDANCE_ENTRY'));
});

test('rejects attendance for an unknown learner', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance.push({ studentId: 99, days: [] });
  assert.ok(codes(validateSnapshot(snapshot)).has('ATTENDANCE_FOR_UNKNOWN_LEARNER'));
});

test('reports missing expected learner/date attendance as incomplete', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days.pop();
  assert.ok(codes(validateSnapshot(snapshot)).has('MISSING_EXPECTED_ATTENDANCE'));
});

test('rejects attendance outside enrollment dates', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days.push(structuredClone(snapshot.attendance[1].days[0]));
  assert.ok(codes(validateSnapshot(snapshot)).has('ATTENDANCE_OUTSIDE_EXPECTED_DAYS'));
});

test('rejects attendance on a confirmed non-class day', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days.push({ date: '2026-11-04', am: { status: 'UNRESOLVED' }, pm: { status: 'UNRESOLVED' } });
  assert.ok(codes(validateSnapshot(snapshot)).has('ATTENDANCE_ON_NON_CLASS_DAY'));
});

test('rejects an event sequence beyond the snapshot last sequence', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].am.sourceEvent.seq = snapshot.sourceRevision.attendanceLastSeq + 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('EVENT_SEQUENCE_OUT_OF_RANGE'));
});

test('rejects duplicate event references', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].pm.sourceEvent.seq = 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_EVENT_REFERENCE'));
});

test('rejects event status disagreement', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].am.sourceEvent.status = 'LATE';
  assert.ok(codes(validateSnapshot(snapshot)).has('EVENT_STATUS_MISMATCH'));
});

test('rejects EXCUSED unless it comes from a MARK event', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[1].days[0].am.sourceEvent.type = 'SCAN';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_EVENT_PROVENANCE'));
});

test('rejects confirmed absence without complete closed healthy coverage', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage.find((row) => row.date === '2026-11-03' && row.session === 'PM').coverageComplete = false;
  assert.ok(codes(validateSnapshot(snapshot)).has('FALSE_CONFIRMED_ABSENCE'));
});

test('rejects ABSENT_CONFIRMED for a learner outside enrollment', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days[0].am = { status: 'ABSENT_CONFIRMED', sourceEvent: null };
  snapshot.learners[0].enrollmentPeriods[0].startInclusive = '2026-11-03';
  assert.ok(codes(validateSnapshot(snapshot)).has('ATTENDANCE_OUTSIDE_EXPECTED_DAYS'));
});

test('treats CLEAR as incomplete rather than as absence', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days[0].am = {
    status: 'CLEAR', sourceEvent: { seq: 1, type: 'MARK', status: 'CLEAR' },
  };
  const result = validateSnapshot(snapshot);
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(codes(result).has('CLEARED_SESSION_UNRESOLVED'));
});

test('treats UNRESOLVED as incomplete rather than as absence', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].am = { status: 'UNRESOLVED' };
  assert.equal(validateSnapshot(snapshot).state, 'INCOMPLETE');
});

test('rejects NOT_SCHEDULED when the calendar schedules that session', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].am = { status: 'NOT_SCHEDULED' };
  assert.ok(codes(validateSnapshot(snapshot)).has('SCHEDULE_STATUS_MISMATCH'));
});

test('keeps a one-session class day incomplete pending R8', () => {
  const snapshot = cloneSnapshot();
  snapshot.calendar.days.find((day) => day.date === '2026-11-02').scheduledSessions = ['AM'];
  snapshot.attendance.forEach((entry) => {
    const day = entry.days.find((item) => item.date === '2026-11-02');
    if (day) day.pm = { status: 'NOT_SCHEDULED', calendarConfirmed: true };
  });
  snapshot.coverage = snapshot.coverage.filter((row) => !(row.date === '2026-11-02' && row.session === 'PM'));
  const result = validateSnapshot(snapshot);
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(codes(result).has('PARTIAL_DAY_POLICY_OPEN'));
});

test('requires coverage for every scheduled date/session', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage.pop();
  assert.ok(codes(validateSnapshot(snapshot)).has('COVERAGE_ROW_MISSING'));
});

test('rejects coverage on non-class dates', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage.push({ date: '2026-11-04', session: 'AM', sessionClosed: false, coverageComplete: false, logHealthy: true, gapAffected: false, closeCoverageFlag: null, closeEventSeq: null, bitmapPayloadSeqs: null, sourceLastSeq: null, expectedStudentIds: null, expectedCount: null });
  assert.ok(codes(validateSnapshot(snapshot)).has('COVERAGE_FOR_UNSCHEDULED_SESSION'));
});

test('rejects duplicate coverage rows', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage.push(structuredClone(snapshot.coverage[0]));
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_COVERAGE_ROW'));
});

test('flags coverage gaps even when no absence is claimed', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage[0].gapAffected = true;
  snapshot.coverage[0].coverageComplete = false;
  snapshot.coverage[0].closeCoverageFlag = false;
  const result = validateSnapshot(snapshot);
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(codes(result).has('COVERAGE_GAP'));
});

test('rejects movement history referencing an unknown learner', () => {
  const snapshot = cloneSnapshot(); snapshot.movementHistory[0].studentId = 299;
  assert.ok(codes(validateSnapshot(snapshot)).has('MOVEMENT_UNKNOWN_LEARNER'));
});

test('rejects movement dates outside the report month', () => {
  const snapshot = cloneSnapshot(); snapshot.movementHistory[0].date = '2026-10-31';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_MOVEMENT_DATE'));
});

test('returns metadata warnings without blocking attendance calculation', () => {
  const snapshot = cloneSnapshot(); snapshot.schoolProfile.schoolHeadName = null;
  snapshot.learners[0].identity.lrn = null;
  const result = validateSnapshot(snapshot);
  assert.equal(result.state, 'VALID');
  assert.ok(result.warnings.some((item) => item.code === 'MISSING_SCHOOL_HEAD_NAME'));
  assert.ok(result.warnings.some((item) => item.code === 'MISSING_LRN'));
});


test('rejects an LRN assigned to more than one learner', () => {
  const snapshot = cloneSnapshot(); snapshot.learners[1].identity.lrn = snapshot.learners[0].identity.lrn;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_LRN'));
});

test('rejects class days outside the effective section profile period', () => {
  const snapshot = cloneSnapshot(); snapshot.reportingProfile.effectiveToExclusive = '2026-11-03';
  assert.ok(codes(validateSnapshot(snapshot)).has('CLASS_DAY_OUTSIDE_PROFILE_RANGE'));
});

test('rejects a malformed non-object session result', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days[0].am = 'PRESENT';
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_SESSION_RESULT'));
});


test('rejects a roster revision different from the snapshot revision tuple', () => {
  const snapshot = cloneSnapshot(); snapshot.roster.revision = 'stale-roster';
  assert.ok(codes(validateSnapshot(snapshot)).has('ROSTER_REVISION_MISMATCH'));
});

test('rejects calendar derived from a different config generation', () => {
  const snapshot = cloneSnapshot(); snapshot.calendar.configGeneration = 2;
  assert.ok(codes(validateSnapshot(snapshot)).has('CALENDAR_CONFIG_GENERATION_MISMATCH'));
});


test('rejects learner metadata from a different revision', () => {
  const snapshot = cloneSnapshot(); snapshot.learnerMetadata.revision = 'stale-identity';
  assert.ok(codes(validateSnapshot(snapshot)).has('LEARNER_METADATA_REVISION_MISMATCH'));
});


test('rejects a closed-session expectedCount that disagrees with the bitmap', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage[0].expectedCount += 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_CLOSE_EXPECTED_SET'));
});

test('rejects duplicate IDs in a closed-session expected bitmap', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage[0].expectedStudentIds.push(snapshot.coverage[0].expectedStudentIds[0]);
  snapshot.coverage[0].expectedCount += 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_CLOSE_EXPECTED_SET'));
});

test('rejects a close bitmap that references a learner outside the month roster', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage[0].expectedStudentIds.push(99);
  snapshot.coverage[0].expectedCount += 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('CLOSE_EXPECTED_UNKNOWN_LEARNER'));
});

test('rejects close bitmap and enrollment-history disagreement', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage[0].expectedStudentIds = [];
  snapshot.coverage[0].expectedCount = 0;
  assert.ok(codes(validateSnapshot(snapshot)).has('CLOSE_ROSTER_EXPECTATION_MISMATCH'));
});

test('rejects an unclosed session that claims a finalized expected bitmap', () => {
  const snapshot = cloneSnapshot();
  const row = snapshot.coverage[0]; row.sessionClosed = false;
  assert.ok(codes(validateSnapshot(snapshot)).has('UNTRUSTED_OPEN_EXPECTED_SET'));
});

test('requires confirmed absence learner to be present in the CLOSE expected bitmap', () => {
  const snapshot = cloneSnapshot();
  const row = snapshot.coverage.find((item) => item.date === '2026-11-03' && item.session === 'PM');
  row.expectedStudentIds = []; row.expectedCount = 0;
  assert.ok(codes(validateSnapshot(snapshot)).has('FALSE_CONFIRMED_ABSENCE'));
});

test('requires exact EV_CLOSE and bitmap PAYLOAD sequence provenance', () => {
  const snapshot = cloneSnapshot();
  delete snapshot.coverage[0].closeEventSeq;
  delete snapshot.coverage[0].bitmapPayloadSeqs;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_CLOSE_GROUP_PROVENANCE'));
});

test('rejects a non-contiguous EV_CLOSE bitmap payload sequence range', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage[0].bitmapPayloadSeqs[2] += 1;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_CLOSE_GROUP_PROVENANCE'));
});

test('rejects duplicate close-group sequence references across coverage rows', () => {
  const snapshot = cloneSnapshot();
  const first = snapshot.coverage[0];
  const second = snapshot.coverage[1];
  second.closeEventSeq = first.closeEventSeq;
  second.bitmapPayloadSeqs = [...first.bitmapPayloadSeqs];
  second.sourceLastSeq = first.sourceLastSeq;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_CLOSE_GROUP_SEQUENCE'));
});

test('rejects attendance event provenance that overlaps a close-group sequence', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days[0].am.sourceEvent.seq = snapshot.coverage[0].closeEventSeq;
  assert.ok(codes(validateSnapshot(snapshot)).has('DUPLICATE_EVENT_REFERENCE'));
});

test('rejects source-event references on unresolved and not-scheduled outcomes', () => {
  const unresolved = cloneSnapshot();
  unresolved.attendance[0].days[0].am = {
    status: 'UNRESOLVED',
    sourceEvent: { seq: 1, type: 'MARK', status: 'CLEAR' },
  };
  assert.ok(codes(validateSnapshot(unresolved)).has('UNRESOLVED_HAS_EVENT_REFERENCE'));

  const notScheduled = cloneSnapshot();
  notScheduled.calendar.days.find((day) => day.date === '2026-11-02').scheduledSessions = ['AM'];
  for (const entry of notScheduled.attendance) {
    const day = entry.days.find((item) => item.date === '2026-11-02');
    if (day) day.pm = {
      status: 'NOT_SCHEDULED',
      calendarConfirmed: true,
      sourceEvent: { seq: 1, type: 'MARK', status: 'CLEAR' },
    };
  }
  assert.ok(codes(validateSnapshot(notScheduled)).has('NOT_SCHEDULED_HAS_EVENT_REFERENCE'));
});

test('rejects attendance log sequence values outside the C++ uint32 range', () => {
  const snapshot = cloneSnapshot();
  snapshot.sourceRevision.attendanceLastSeq = 0x1_0000_0000;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_SOURCE_REVISION'));
});

test('rejects source event sequence values outside the C++ uint32 range', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days[0].am.sourceEvent.seq = 0x1_0000_0000;
  snapshot.sourceRevision.attendanceLastSeq = 0x1_0000_0000;
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_EVENT_PROVENANCE'));
});

test('rejects a truncated CLOSE bitmap payload for the 300-ID device space', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage[0].bitmapPayloadSeqs.pop();
  snapshot.coverage[0].sourceLastSeq = snapshot.coverage[0].bitmapPayloadSeqs.at(-1);
  assert.ok(codes(validateSnapshot(snapshot)).has('INVALID_CLOSE_GROUP_PROVENANCE'));
});

test('requires the exported coverage result to agree with the EV_CLOSE coverage flag', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage[0].closeCoverageFlag = false;
  assert.ok(codes(validateSnapshot(snapshot)).has('CLOSE_COVERAGE_FLAG_MISMATCH'));
});

test('rejects a gap-free CLOSE flag that contradicts a reported overlapping GAP', () => {
  const snapshot = cloneSnapshot();
  snapshot.coverage[0].gapAffected = true;
  assert.ok(codes(validateSnapshot(snapshot)).has('CLOSE_COVERAGE_GAP_CONTRADICTION'));
});


test('accepts explicit withdrawal and NLS movements without normalizing their meaning', () => {
  const snapshot = cloneSnapshot();
  snapshot.movementHistory.push({ studentId: 2, date: '2026-11-20', type: 'WITHDRAWN',
    effectiveDateMeaning: 'FIRST_NOT_ENROLLED_DAY', reasonCode: null, reasonText: null,
    destinationSchool: null, destinationSection: null });
  snapshot.movementHistory.push({ studentId: 3, date: '2026-11-21', type: 'NLS',
    effectiveDateMeaning: 'UNCONFIRMED', reasonCode: null, reasonText: null,
    destinationSchool: null, destinationSection: null });
  const result = validateSnapshot(snapshot, { expectedMonth: '2026-11' });
  assert.ok(result.warnings.some((entry) => entry.code === 'NLS_SEMANTICS_UNCONFIRMED'));
  assert.ok(result.warnings.some((entry) => entry.code === 'MOVEMENT_DATE_MEANING_UNCONFIRMED'));
});


test('does not let NLS imply a learner exit date', () => {
  const snapshot = cloneSnapshot();
  snapshot.movementHistory[0].type = 'NLS';
  snapshot.movementHistory[0].effectiveDateMeaning = 'LAST_ENROLLED_DAY';
  const result = validateSnapshot(snapshot);
  assert.ok(result.issues.some((entry) => entry.code === 'MOVEMENT_DATE_MEANING_MISMATCH'));
});

test('rejects a movement without explicit effective-date meaning', () => {
  const snapshot = cloneSnapshot();
  delete snapshot.movementHistory[0].effectiveDateMeaning;
  const result = validateSnapshot(snapshot, { expectedMonth: '2026-11' });
  assert.ok(result.issues.some((entry) => entry.code === 'INVALID_MOVEMENT_RECORD'));
});
