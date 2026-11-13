import test from 'node:test';
import assert from 'node:assert/strict';
import { projectMonthRoster } from '../month_roster_projection.mjs';
import { projectReportingRoster } from '../reporting_identity_projection.mjs';
import { joinReportingRosterForSnapshot } from '../reporting_snapshot_join.mjs';
import { validateSnapshot } from '../snapshot_schema.mjs';
import { makeValidSnapshot } from '../fixtures/snapshot_fixtures.mjs';

const day = (iso) => Math.floor(Date.parse(`${iso}T00:00:00Z`) / 86400000);
const prefix = { openGeneration: '18446744073709551615', recordCount: 12,
  lastSeq: 12, auditGeneration: 4 };

function makeMonthRoster() {
  return projectMonthRoster({
    monthStartDay: day('2026-11-01'), monthEndExclusive: day('2026-12-01'),
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: day('2026-10-01'),
      endDayExclusive: day('2026-11-04') }],
    nameRevisions: [{ studentId: 1, effectiveDay: day('2026-10-01'),
      sourceSeq: 3, name: 'Kiosk nickname' }],
    sourcePrefix: { ...prefix }, expectedSourcePrefix: { ...prefix },
    rosterHistoryComplete: true, nameHistoryComplete: true,
  });
}

function makeMetadata() {
  return { schemaVersion: 2, revision: 1, transactions: [{ revision: 1, committed: true, changes: [
    { op: 'PUT', record: { recordId: 'school-1', kind: 'SCHOOL_PROFILE', subjectId: 'SCHOOL',
      validFrom: '2026-06-01', validToExclusive: null,
      data: { schoolName: 'Example SHS', schoolId: '000123', district: 'District',
        division: 'Division', region: 'Region VIII', adviserName: 'Adviser', schoolHeadName: 'Head' } } },
    { op: 'PUT', record: { recordId: 'section-1', kind: 'SECTION_PROFILE', subjectId: 'section-1',
      validFrom: '2026-06-01', validToExclusive: null,
      data: { profileId: 'section-1', curriculum: 'REGULAR_SHS', schoolYear: '2026-2027',
        gradeLevel: '11', sectionName: '11-A', track: null, strand: null, course: null,
        academicPeriodLabel: 'Semester 1' } } },
    { op: 'PUT', record: { recordId: 'learner-1', kind: 'LEARNER_IDENTITY', subjectId: '1',
      validFrom: '2026-11-01', validToExclusive: null,
      data: { legalName: { firstName: 'Legal', middleName: null, lastName: 'Name', extensionName: null },
        lrn: '012345678901', sex: 'F', sourceStatus: 'SOURCE_CHECKED' } } },
    { op: 'PUT', record: { recordId: 'movement-1', kind: 'MOVEMENT', subjectId: '1',
      effectiveDate: '2026-11-04',
      data: { movementType: 'TRANSFER_OUT', effectiveDateMeaning: 'FIRST_NOT_ENROLLED_DAY',
        reasonCode: 'TRANSFER', reasonText: 'Transferred', destinationSchool: 'Other SHS',
        destinationSection: null } } },
  ] }] };
}

function makeProjection() {
  const monthRoster = makeMonthRoster();
  const reportingRoster = projectReportingRoster({ month: '2026-11', monthRoster,
    metadataHistory: makeMetadata(), asOfRevision: 1 });
  return { monthRoster, reportingRoster };
}

test('joins D1 intervals, D2 legal identity and movement into snapshot-compatible fields', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'INCOMPLETE');
  assert.equal(joined.authoritativeReady, false);
  assert.equal(joined.snapshotFragment.learners.length, 1);
  assert.deepEqual(joined.snapshotFragment.learners[0], {
    studentId: 1, sex: 'F',
    identity: { firstName: 'Legal', middleName: null, lastName: 'Name',
      extensionName: null, lrn: '012345678901' },
    enrollmentPeriods: [{ profileId: 'section-1',
      startInclusive: '2026-10-01', endExclusive: '2026-11-04' }],
  });
  assert.deepEqual(joined.snapshotFragment.movementHistory, [{
    studentId: 1, date: '2026-11-04', type: 'TRANSFER_OUT',
    effectiveDateMeaning: 'FIRST_NOT_ENROLLED_DAY', reasonCode: 'TRANSFER',
    reasonText: 'Transferred', destinationSchool: 'Other SHS', destinationSection: null,
  }]);
  assert.equal(JSON.stringify(joined).includes('Kiosk nickname'), false);
  assert.ok(joined.warnings.some((entry) => entry.code === 'AUTHENTICATED_IDENTITY_REVIEW_MISSING'));
});

test('rejects malformed D2 diagnostics instead of treating them as an empty issue list', () => {
  for (const patch of [{ issues: null }, { issues: {} }, { warnings: {} }]) {
    const { monthRoster, reportingRoster } = makeProjection();
    const malformed = { ...reportingRoster, ...patch };
    const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster,
      reportingRoster: malformed });
    assert.equal(joined.state, 'BLOCKED');
    assert.ok(joined.issues.some((entry) => entry.code === 'D2_REPORTING_ROSTER_INVALID'));
  }
});

test('rejects D1/D2 prefix mismatch, including a different uint64 generation', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  const mismatched = { ...reportingRoster, sourcePrefix: {
    ...reportingRoster.sourcePrefix, openGeneration: '18446744073709551614' } };
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster,
    reportingRoster: mismatched });
  assert.equal(joined.state, 'BLOCKED');
  assert.ok(joined.issues.some((entry) => entry.code === 'SOURCE_PREFIX_MISMATCH'));
});

test('rechecks intervals at the D2-to-snapshot boundary', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  reportingRoster.learners[0].enrollmentIntervals[0].endDayExclusive = day('2026-11-05');
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'BLOCKED');
  assert.ok(joined.issues.some((entry) => entry.code === 'ENROLLMENT_INTERVAL_JOIN_MISMATCH'));
});

test('does not allow a caller status or SOURCE_CHECKED claim to authorize the join', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  const forged = { ...reportingRoster, state: 'READY', authenticatedReviewState: 'VERIFIED' };
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster,
    reportingRoster: forged });
  assert.equal(joined.state, 'INCOMPLETE');
  assert.equal(joined.authoritativeReady, false);
});



test('rejects a forged month roster whose intervals do not overlap the report month', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  const outside = { startDayInclusive: day('2026-12-01'), endDayExclusive: null };
  monthRoster.learners[0].enrollmentIntervals = [outside];
  reportingRoster.learners[0].enrollmentIntervals = [outside];
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'BLOCKED');
  assert.ok(joined.issues.some((entry) => entry.code === 'LEARNER_INTERVAL_NOT_MONTH_OVERLAP'));
});

test('rejects forged overlapping or out-of-order enrollment intervals even when D1 and D2 agree', () => {
  const cases = [
    [
      { startDayInclusive: day('2026-10-01'), endDayExclusive: day('2026-11-10') },
      { startDayInclusive: day('2026-11-05'), endDayExclusive: null },
    ],
    [
      { startDayInclusive: day('2026-11-05'), endDayExclusive: null },
      { startDayInclusive: day('2026-10-01'), endDayExclusive: day('2026-11-04') },
    ],
  ];
  for (const intervals of cases) {
    const { monthRoster, reportingRoster } = makeProjection();
    monthRoster.learners[0].enrollmentIntervals = intervals;
    reportingRoster.learners[0].enrollmentIntervals = intervals.map((entry) => ({ ...entry }));
    const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
    assert.equal(joined.state, 'BLOCKED');
    assert.ok(joined.issues.some((entry) => entry.code === 'OVERLAPPING_OR_UNSORTED_JOINED_INTERVALS'));
  }
});


test('blocks legal identity fields that cannot satisfy the snapshot schema', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  reportingRoster.learners[0].reportingIdentity.legalName.firstName = 42;
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'BLOCKED');
  assert.ok(joined.issues.some((entry) => entry.code === 'LEGAL_IDENTITY_FIELDS_INVALID'));
});

test('blocks an invalid section profile instead of emitting a malformed snapshot fragment', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  reportingRoster.sectionProfile.data.curriculum = 'UNSUPPORTED';
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'BLOCKED');
  assert.ok(joined.issues.some((entry) => entry.code === 'SECTION_PROFILE_FIELDS_INVALID'));
});

test('mapped D1/D2 fields are structurally accepted by snapshot schema but remain non-authoritative', () => {
  const { monthRoster, reportingRoster } = makeProjection();
  const joined = joinReportingRosterForSnapshot({ month: '2026-11', monthRoster, reportingRoster });
  assert.equal(joined.state, 'INCOMPLETE');
  const snapshot = makeValidSnapshot();
  const fragment = joined.snapshotFragment;
  snapshot.reportingProfile = fragment.reportingProfile;
  snapshot.schoolProfile = fragment.schoolProfile;
  snapshot.learners[0] = fragment.learners[0];
  // The proposed device is configured for one section. Retained synthetic
  // learners must reference that same profile, not the fixture's old profile.
  for (const learner of snapshot.learners.slice(1)) {
    for (const period of learner.enrollmentPeriods) period.profileId = fragment.reportingProfile.profileId;
  }
  snapshot.movementHistory = [
    ...fragment.movementHistory,
    ...snapshot.movementHistory.filter((movement) => movement.studentId !== 1),
  ];
  const rosterRevision = `log-${prefix.openGeneration}-${prefix.recordCount}-${prefix.lastSeq}-audit-${prefix.auditGeneration}`;
  snapshot.sourceRevision.rosterRevision = rosterRevision;
  snapshot.roster.revision = rosterRevision;
  snapshot.sourceRevision.reportingProfileRevision = fragment.reportingProfile.revision;
  snapshot.sourceRevision.schoolProfileRevision = fragment.schoolProfile.revision;
  snapshot.sourceRevision.learnerMetadataRevision = `metadata-${joined.metadataRevision}`;
  snapshot.learnerMetadata.revision = `metadata-${joined.metadataRevision}`;
  assert.equal(validateSnapshot(snapshot).state, 'VALID');
  assert.equal(joined.authoritativeReady, false);
  assert.equal(joined.state, 'INCOMPLETE');
});
