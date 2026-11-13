import test from 'node:test';
import assert from 'node:assert/strict';
import { projectMonthRoster } from '../month_roster_projection.mjs';
import { projectReportingRoster } from '../reporting_identity_projection.mjs';

const day = (iso) => Math.floor(Date.parse(`${iso}T00:00:00Z`) / 86400000);
const prefix = { openGeneration: '7', recordCount: 120, lastSeq: 120, auditGeneration: 9 };

function monthRoster(overrides = {}) {
  return projectMonthRoster({
    monthStartDay: day('2026-11-01'),
    monthEndExclusive: day('2026-12-01'),
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: day('2026-11-03'), endDayExclusive: day('2026-11-20') }],
    nameRevisions: [{ studentId: 1, effectiveDay: day('2026-11-03'), sourceSeq: 12, name: 'Kiosk nickname' }],
    sourcePrefix: { ...prefix }, expectedSourcePrefix: { ...prefix },
    rosterHistoryComplete: true, nameHistoryComplete: true,
    ...overrides,
  });
}

function schoolRecord() {
  return {
    recordId: 'school-r1', kind: 'SCHOOL_PROFILE', subjectId: 'SCHOOL',
    validFrom: '2026-06-01', validToExclusive: null,
    data: { schoolName: 'Example SHS', schoolId: '000123', district: 'District',
      division: 'Division', region: 'Region VIII', adviserName: 'Adviser', schoolHeadName: 'Head' },
  };
}

function sectionRecord() {
  return {
    recordId: 'section-r1', kind: 'SECTION_PROFILE', subjectId: 'section-r1',
    validFrom: '2026-06-01', validToExclusive: null,
    data: { profileId: 'section-r1', curriculum: 'REGULAR_SHS', schoolYear: '2026-2027',
      gradeLevel: '11', sectionName: '11-A', track: null, strand: null, course: null,
      academicPeriodLabel: 'Semester 1' },
  };
}

function learnerRecord({ recordId = 'learner-r1', studentId = 1, from = '2026-11-01', to = null,
  firstName = 'Legal', middleName = null, lastName = 'Name', lrn = '012345678901',
  sex = 'F', sourceStatus = 'SOURCE_CHECKED' } = {}) {
  return {
    recordId, kind: 'LEARNER_IDENTITY', subjectId: String(studentId), validFrom: from,
    validToExclusive: to,
    data: { legalName: { firstName, middleName, lastName, extensionName: null },
      lrn, sex, sourceStatus },
  };
}

function movementRecord({ recordId = 'movement-r1', studentId = 1, type = 'TRANSFER_OUT', effectiveDate = '2026-11-19',
  dateMeaning = 'LAST_ENROLLED_DAY', reasonCode = 'SCHOOL_TRANSFER', reasonText = 'Transferred',
  destinationSchool = 'Other SHS', destinationSection = null } = {}) {
  return {
    recordId, kind: 'MOVEMENT', subjectId: String(studentId), effectiveDate,
    data: { movementType: type, effectiveDateMeaning: dateMeaning, reasonCode, reasonText,
      destinationSchool, destinationSection },
  };
}

function metadata(records = [schoolRecord(), sectionRecord(), learnerRecord()]) {
  return { schemaVersion: 2, revision: 1, transactions: [
    { revision: 1, committed: true, changes: records.map((record) => ({ op: 'PUT', record })) },
  ] };
}

function project({ roster = monthRoster(), history = metadata(), asOfRevision = history.revision, month = '2026-11' } = {}) {
  return projectReportingRoster({ month, monthRoster: roster, metadataHistory: history, asOfRevision });
}

test('keeps operational display name separate from legal reporting name', () => {
  const result = project();
  assert.equal(result.learners[0].operational.displayName, 'Kiosk nickname');
  assert.equal(result.learners[0].reportingIdentity.legalName.firstName, 'Legal');
  assert.notEqual(result.learners[0].operational.displayName,
    `${result.learners[0].reportingIdentity.legalName.firstName} ${result.learners[0].reportingIdentity.legalName.lastName}`);
});

test('does not block legal reporting identity when operational display name changes in-month', () => {
  const roster = monthRoster({ nameRevisions: [
    { studentId: 1, effectiveDay: day('2026-11-03'), sourceSeq: 12, name: 'Old nickname' },
    { studentId: 1, effectiveDay: day('2026-11-12'), sourceSeq: 25, name: 'New nickname' },
  ] });
  assert.equal(roster.state, 'READY');
  assert.equal(roster.learners[0].nameState, 'CHANGED_IN_MONTH');
  const result = project({ roster });
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.issues.length, 0);
  assert.equal(result.learners[0].reportingIdentity.legalName.firstName, 'Legal');
  assert.ok(result.operationalDiagnostics.some((entry) => entry.code === 'OPERATIONAL_DISPLAY_NAME_CHANGED'));
});


test('treats legal-name objects with different property order as the same identity', () => {
  const first = learnerRecord({ recordId: 'learner-r1', from: '2026-11-01', to: '2026-11-12' });
  const second = learnerRecord({ recordId: 'learner-r2', from: '2026-11-12' });
  second.data.legalName = {
    lastName: 'Name', extensionName: null, firstName: 'Legal', middleName: null,
  };
  const result = project({ history: metadata([schoolRecord(), sectionRecord(), first, second]) });
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.issues.some((entry) => entry.code === 'LEGAL_IDENTITY_CHANGED_IN_MONTH'), false);
  assert.equal(result.learners[0].reportingIdentity.legalName.firstName, 'Legal');
});

test('blocks a legal reporting-name change during an enrolled portion of the report month', () => {
  const history = metadata();
  history.revision = 2;
  history.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-r1' },
    { op: 'PUT', record: learnerRecord({ recordId: 'learner-r1-before', from: '2026-11-01', to: '2026-11-12' }) },
    { op: 'PUT', record: learnerRecord({ recordId: 'learner-r2', from: '2026-11-12', firstName: 'Corrected' }) },
  ] });
  const result = project({ history });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'LEGAL_IDENTITY_CHANGED_IN_MONTH'));
});

test('allows a retroactive legal-identity correction at a newer metadata revision', () => {
  const history = metadata();
  history.revision = 2;
  history.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-r1' },
    { op: 'PUT', record: learnerRecord({ recordId: 'learner-r2', from: '2026-11-01', firstName: 'Corrected' }) },
  ] });
  const oldReport = project({ history, asOfRevision: 1 });
  const correctedReport = project({ history, asOfRevision: 2 });
  assert.equal(oldReport.learners[0].reportingIdentity.legalName.firstName, 'Legal');
  assert.equal(correctedReport.learners[0].reportingIdentity.legalName.firstName, 'Corrected');
});

test('flags missing reporting identity as incomplete, not absent or invalid', () => {
  const result = project({ history: metadata([schoolRecord(), sectionRecord()]) });
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(result.warnings.some((entry) => entry.code === 'REPORTING_IDENTITY_GAP'));
  assert.equal(result.learners[0].reportingIdentity, null);
});

test('requires legal identity coverage only for enrolled days, not the entire month', () => {
  const result = project({ history: metadata([
    schoolRecord(), sectionRecord(),
    learnerRecord({ from: '2026-11-03', to: '2026-11-20' }),
  ]) });
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.issues.length, 0);
  assert.equal(result.warnings.some((entry) => entry.code === 'REPORTING_IDENTITY_GAP'), false);
});

test('blocks a gap inside the actual enrollment interval', () => {
  const history = metadata([
    schoolRecord(), sectionRecord(),
    learnerRecord({ recordId: 'learner-a', from: '2026-11-03', to: '2026-11-10' }),
    learnerRecord({ recordId: 'learner-b', from: '2026-11-12', to: '2026-11-20' }),
  ]);
  // These are two versions in one same-student history and must be selected without a gap.
  const result = project({ history });
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(result.warnings.some((entry) => entry.code === 'REPORTING_IDENTITY_GAP'));
});

test('includes explicit transfer and withdrawal movements for separate learners without inferring them from deactivation', () => {
  const roster = monthRoster({
    knownStudentIds: [1, 2],
    intervals: [
      { studentId: 1, startDayInclusive: day('2026-11-03'), endDayExclusive: day('2026-11-20') },
      { studentId: 2, startDayInclusive: day('2026-11-05'), endDayExclusive: day('2026-11-22') },
    ],
    nameRevisions: [
      { studentId: 1, effectiveDay: day('2026-11-03'), sourceSeq: 12, name: 'Kiosk one' },
      { studentId: 2, effectiveDay: day('2026-11-05'), sourceSeq: 13, name: 'Kiosk two' },
    ],
  });
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(),
    learnerRecord({ recordId: 'learner-r2', studentId: 2, lrn: '012345678902' }),
    movementRecord(), movementRecord({ recordId: 'movement-r2', studentId: 2, type: 'WITHDRAWN',
      effectiveDate: '2026-11-21', dateMeaning: 'LAST_ENROLLED_DAY', destinationSchool: null })]);
  const result = project({ roster, history });
  assert.equal(result.movements.length, 2);
  assert.deepEqual(result.movements.map((item) => item.movementType), ['TRANSFER_OUT', 'WITHDRAWN']);
});

test('blocks multiple movement records for one learner on the same effective date', () => {
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(), movementRecord(),
    movementRecord({ recordId: 'movement-r2', type: 'WITHDRAWN', destinationSchool: null })]);
  const result = project({ history });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'MULTIPLE_MOVEMENTS_SAME_DAY'));
});

test('preserves NLS as an unnormalized school-reported movement classification', () => {
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(),
    movementRecord({ type: 'NLS', dateMeaning: 'UNCONFIRMED', destinationSchool: null })]);
  const result = project({ history });
  assert.equal(result.movements[0].movementType, 'NLS');
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(result.warnings.some((entry) => entry.code === 'NLS_SEMANTICS_UNCONFIRMED'));
});

test('marks movement date meaning as unresolved rather than assuming interval semantics', () => {
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(),
    movementRecord({ dateMeaning: 'UNCONFIRMED' })]);
  const result = project({ history });
  assert.ok(result.warnings.some((entry) => entry.code === 'MOVEMENT_DATE_MEANING_UNCONFIRMED'));
  assert.equal(result.state, 'INCOMPLETE');
});

test('rejects movement records that reference learners outside the month roster', () => {
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(),
    movementRecord({ recordId: 'movement-r2', effectiveDate: '2026-11-10' })]);
  history.transactions[0].changes.push({ op: 'PUT', record: {
    ...movementRecord({ recordId: 'movement-r3' }), subjectId: '2',
  } });
  const result = project({ history });
  assert.ok(result.warnings.some((entry) => entry.code === 'MOVEMENT_FOR_NON_MONTH_LEARNER'));
});

test('does not return LRN or full legal names in diagnostics', () => {
  const history = metadata([schoolRecord(), sectionRecord(),
    learnerRecord({ lrn: 'bad-lrn' })]);
  const result = project({ history });
  const diagnosticText = JSON.stringify({ issues: result.issues, warnings: result.warnings });
  assert.equal(diagnosticText.includes('012345678901'), false);
  assert.equal(diagnosticText.includes('Dela Cruz'), false);
});

test('rejects an invalid or non-month-overlap roster projection', () => {
  const result = project({ roster: { state: 'BLOCKED', learners: [], issues: [{ code: 'BAD' }] } });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.learners.length, 0);
});



test('does not treat SOURCE_CHECKED as verified without an audit attestation', () => {
  const result = project();
  assert.equal(result.learners[0].reportingIdentity.sourceStatus, 'UNVERIFIED');
  assert.equal(result.learners[0].reportingIdentity.sourceStatusClaim, 'SOURCE_CHECKED');
  assert.ok(result.warnings.some((entry) => entry.code === 'LEGAL_IDENTITY_SOURCE_ATTESTATION_MISSING'));
  assert.equal(result.state, 'INCOMPLETE');
});

test('requires SHIFTED movement date to match an enrollment interval boundary', () => {
  const history = metadata([schoolRecord(), sectionRecord(), learnerRecord(),
    movementRecord({ type: 'SHIFTED', dateMeaning: 'SECTION_CHANGE_DAY', effectiveDate: '2026-11-10',
      destinationSchool: null, destinationSection: '11-B' })]);
  const result = project({ history });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'MOVEMENT_ROSTER_BOUNDARY_MISMATCH'));
});


test('revalidates overlapping enrollment intervals at the D1-to-D2 boundary', () => {
  const roster = monthRoster();
  roster.learners[0].enrollmentIntervals.push({
    startDayInclusive: day('2026-11-10'), endDayExclusive: day('2026-11-25'),
  });
  const result = project({ roster });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'OVERLAPPING_MONTH_ROSTER_INTERVAL'));
});

test('uses exact decimal uint64 generations and enforces uint32 audit generations', () => {
  const wideOpenGeneration = monthRoster();
  wideOpenGeneration.sourcePrefix.openGeneration = '18446744073709551615';
  const accepted = project({ roster: wideOpenGeneration });
  assert.notEqual(accepted.state, 'BLOCKED');
  assert.equal(accepted.sourcePrefix.openGeneration, '18446744073709551615');
  assert.equal(accepted.issues.some((entry) => entry.code === 'MONTH_ROSTER_PREFIX_INVALID'), false);

  const numericGeneration = monthRoster();
  numericGeneration.sourcePrefix.openGeneration = 9007199254740992;
  const numeric = project({ roster: numericGeneration });
  assert.equal(numeric.state, 'BLOCKED');
  assert.ok(numeric.issues.some((entry) => entry.code === 'MONTH_ROSTER_PREFIX_INVALID'));

  const outOfRangeGeneration = monthRoster();
  outOfRangeGeneration.sourcePrefix.openGeneration = '18446744073709551616';
  const outOfRange = project({ roster: outOfRangeGeneration });
  assert.equal(outOfRange.state, 'BLOCKED');
  assert.ok(outOfRange.issues.some((entry) => entry.code === 'MONTH_ROSTER_PREFIX_INVALID'));

  const wideAuditGeneration = monthRoster();
  wideAuditGeneration.sourcePrefix.auditGeneration = 0x100000000;
  const rejected = project({ roster: wideAuditGeneration });
  assert.equal(rejected.state, 'BLOCKED');
  assert.ok(rejected.issues.some((entry) => entry.code === 'MONTH_ROSTER_PREFIX_INVALID'));
});

test('rejects malformed learner entries without throwing', () => {
  const roster = { ...monthRoster(), learners: [null] };
  const result = project({ roster });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'MONTH_ROSTER_NOT_READY'));
});

test('rejects a metadata revision newer than the available transaction history', () => {
  const result = project({ asOfRevision: 9 });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'INVALID_METADATA_REVISION'));
});
