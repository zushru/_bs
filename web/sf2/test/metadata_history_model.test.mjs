import test from 'node:test';
import assert from 'node:assert/strict';
import {
  validateMetadataHistory, resolveMetadataAt, movementHistoryForMonth, metadataRecordsAtRevision,
} from '../metadata_history_model.mjs';

const record = (recordId, kind, subjectId, data, range = {}) => ({
  recordId, kind, subjectId,
  validFrom: range.from ?? '2026-06-01',
  validToExclusive: range.to ?? null,
  data,
});
const school = () => record('school-1', 'SCHOOL_PROFILE', 'SCHOOL', {
  schoolName: 'Example SHS', schoolId: '000123', district: 'District A',
  division: 'Division A', region: 'Region VIII', adviserName: null, schoolHeadName: null,
});
const section = () => record('section-1', 'SECTION_PROFILE', 'section-1', {
  profileId: 'section-1', curriculum: 'REGULAR_SHS', schoolYear: '2026-2027',
  gradeLevel: '11', sectionName: '11-A', track: null, strand: null, course: null,
  academicPeriodLabel: null,
});
const learner = (id = 1, recordId = `learner-${id}`, lrn = '012345678901', range = {}) => record(
  recordId, 'LEARNER_IDENTITY', String(id), {
    legalName: { firstName: 'Ana', middleName: null, lastName: 'Dela Cruz', extensionName: null },
    lrn, sex: 'F', sourceStatus: 'UNVERIFIED',
  }, range,
);
const movement = (recordId = 'move-1', id = 1, type = 'TRANSFER_OUT', effectiveDate = '2026-11-10') => ({
  recordId, kind: 'MOVEMENT', subjectId: String(id), effectiveDate,
  data: { movementType: type, effectiveDateMeaning: 'UNCONFIRMED', reasonCode: 'TRANSFER', reasonText: 'Transferred',
    destinationSchool: 'Other SHS', destinationSection: null },
});
const base = () => ({ schemaVersion: 2, revision: 1, transactions: [
  { revision: 1, committed: true, changes: [
    { op: 'PUT', record: school() },
    { op: 'PUT', record: section() },
    { op: 'PUT', record: learner() },
    { op: 'PUT', record: movement() },
  ] },
] });
const codes = (value) => new Set(value.issues.map((item) => item.code));

test('accepts valid metadata history while keeping teacher-dependent fields incomplete', () => {
  const result = validateMetadataHistory(base(), { expectedStudentIds: [1] });
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.issues.length, 0);
  assert.ok(result.warnings.some((item) => item.code === 'ACADEMIC_PERIOD_LABEL_OPEN'));
});

test('rejects unknown schema version', () => {
  const value = base(); value.schemaVersion = 3;
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_METADATA_ENVELOPE'));
});

test('rejects missing transaction history', () => {
  const value = base(); value.transactions = [];
  assert.ok(codes(validateMetadataHistory(value)).has('METADATA_REVISION_MISMATCH'));
});

test('requires sequential non-empty transactions', () => {
  const value = base(); value.transactions[0].revision = 2;
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_METADATA_TRANSACTION'));
});

test('rejects duplicate record IDs', () => {
  const value = base(); value.transactions[0].changes.push({ op: 'PUT', record: school() });
  assert.ok(codes(validateMetadataHistory(value)).has('DUPLICATE_METADATA_RECORD_ID'));
});

test('rejects malformed effective date interval', () => {
  const value = base(); value.transactions[0].changes[2].record.validToExclusive = '2026-05-31';
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_VALID_TIME_RANGE'));
});

test('rejects invalid LRN but preserves leading-zero string LRNs', () => {
  const good = validateMetadataHistory(base());
  assert.equal(good.state, 'INCOMPLETE');
  assert.equal(good.issues.length, 0);
  const value = base(); value.transactions[0].changes[2].record.data.lrn = '1234';
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_LRN'));
});

test('does not require incomplete reporting metadata for device attendance setup', () => {
  const value = base(); value.transactions[0].changes[2].record.data.sex = null;
  const result = validateMetadataHistory(value, { expectedStudentIds: [1] });
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(result.warnings.some((item) => item.code === 'SEX_MISSING'));
});

test('flags missing learner metadata as incomplete, not malformed', () => {
  const result = validateMetadataHistory(base(), { expectedStudentIds: [1, 2] });
  assert.equal(result.state, 'INCOMPLETE');
  assert.ok(result.warnings.some((item) => item.code === 'REPORTING_IDENTITY_MISSING'));
});

test('rejects multiple overlapping section profiles on one device', () => {
  const value = base();
  const other = section(); other.recordId = 'section-2'; other.subjectId = 'section-2';
  other.data.profileId = 'section-2'; other.data.sectionName = '11-B';
  value.transactions[0].changes.push({ op: 'PUT', record: other });
  assert.ok(codes(validateMetadataHistory(value)).has('MULTIPLE_CONFIGURED_SECTIONS'));
});

test('rejects overlapping identity versions for one student', () => {
  const value = base();
  value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [{ op: 'PUT', record: learner(1, 'learner-1b', '112345678901') }] });
  assert.ok(codes(validateMetadataHistory(value)).has('OVERLAPPING_ACTIVE_METADATA'));
});

test('rejects duplicate LRN across overlapping learners', () => {
  const value = base();
  value.transactions[0].changes.push({ op: 'PUT', record: learner(2, 'learner-2') });
  assert.ok(codes(validateMetadataHistory(value)).has('LRN_REUSED_ACROSS_DEVICE_IDS'));
});


test('rejects LRN reassignment to another device ID even across non-overlapping validity periods', () => {
  const value = base();
  value.transactions[0].changes[2].record.validToExclusive = '2026-11-01';
  const next = learner(2, 'learner-2', '012345678901', { from: '2026-11-02' });
  value.transactions[0].changes.push({ op: 'PUT', record: next });
  assert.ok(codes(validateMetadataHistory(value)).has('LRN_REUSED_ACROSS_DEVICE_IDS'));
});

test('SOURCE_CHECKED metadata is still flagged without verifiable reviewer evidence', () => {
  const value = base();
  value.transactions[0].changes[2].record.data.sourceStatus = 'SOURCE_CHECKED';
  const result = validateMetadataHistory(value);
  assert.ok(result.warnings.some((entry) => entry.code === 'SOURCE_CHECKED_CLAIM_UNATTESTED'));
});

test('accepts adjacent identity versions without overlap', () => {
  const value = base();
  value.revision = 2;
  const next = learner(1, 'learner-1b', '112345678901', { from: '2026-11-01' });
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-1' },
    { op: 'PUT', record: next },
  ] });
  const result = validateMetadataHistory(value);
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.issues.length, 0);
});

test('rejects revoke of missing or already revoked record', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'missing' },
    { op: 'REVOKE', targetRecordId: 'learner-1' },
    { op: 'REVOKE', targetRecordId: 'learner-1' },
  ] });
  const result = validateMetadataHistory(value);
  assert.ok(codes(result).has('INVALID_REVOKE_TARGET'));
});

test('resolves effective-time identity at the requested metadata revision', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-1' },
    { op: 'PUT', record: learner(1, 'learner-1b', '112345678901', { from: '2026-11-01' }) },
  ] });
  assert.equal(resolveMetadataAt(value, { kind: 'LEARNER_IDENTITY', subjectId: 1,
    effectiveDate: '2026-10-31', asOfRevision: 1 }).record.recordId, 'learner-1');
  assert.equal(resolveMetadataAt(value, { kind: 'LEARNER_IDENTITY', subjectId: 1,
    effectiveDate: '2026-11-02', asOfRevision: 2 }).record.recordId, 'learner-1b');
});

test('keeps prior transaction revision readable after a correction', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-1' },
    { op: 'PUT', record: learner(1, 'learner-corrected', '112345678901') },
  ] });
  assert.equal(resolveMetadataAt(value, { kind: 'LEARNER_IDENTITY', subjectId: 1,
    effectiveDate: '2026-11-02', asOfRevision: 1 }).record.recordId, 'learner-1');
  assert.equal(resolveMetadataAt(value, { kind: 'LEARNER_IDENTITY', subjectId: 1,
    effectiveDate: '2026-11-02', asOfRevision: 2 }).record.recordId, 'learner-corrected');
});

test('rejects unknown curriculum and does not infer program track', () => {
  const value = base(); value.transactions[0].changes[1].record.data.curriculum = 'UNKNOWN';
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_CURRICULUM'));
});

test('keeps semester/term ambiguity as a warning', () => {
  const result = validateMetadataHistory(base());
  assert.ok(result.warnings.some((item) => item.code === 'ACADEMIC_PERIOD_LABEL_OPEN'));
});

test('rejects a movement date that is not a real calendar date', () => {
  const value = base(); value.transactions[0].changes[3].record.effectiveDate = '2026-02-30';
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_MOVEMENT_DATE'));
});

test('movement report includes effective-month records at a pinned revision', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'PUT', record: movement('move-2', 1, 'TRANSFER_IN', '2026-12-01') },
  ] });
  const result = movementHistoryForMonth(value, { studentIds: [1], month: '2026-11', asOfRevision: 2 });
  assert.equal(result.movements.length, 1);
  assert.equal(result.movements[0].recordId, 'move-1');
});

test('movement query excludes other student IDs and dates outside month', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'PUT', record: movement('move-2', 2, 'TRANSFER_IN', '2026-11-15') },
  ] });
  const result = movementHistoryForMonth(value, { studentIds: [1], month: '2026-11' });
  assert.equal(result.movements.length, 1);
  assert.equal(result.movements[0].subjectId, '1');
});

test('rejects future or out-of-range revision queries', () => {
  const result = resolveMetadataAt(base(), { kind: 'SCHOOL_PROFILE', subjectId: 'SCHOOL',
    effectiveDate: '2026-11-01', asOfRevision: 2 });
  assert.equal(result.state, 'INVALID_QUERY');
});

test('rejects duplicate active movement IDs by global record ID', () => {
  const value = base(); value.transactions[0].changes.push({ op: 'PUT', record: movement() });
  assert.ok(codes(validateMetadataHistory(value)).has('DUPLICATE_METADATA_RECORD_ID'));
});

test('rejects duplicate IDs in the expected learner set', () => {
  const result = validateMetadataHistory(base(), { expectedStudentIds: [1, 1] });
  assert.ok(codes(result).has('DUPLICATE_EXPECTED_STUDENT_ID'));
});

test('rejects invalid student IDs in movement history queries', () => {
  const result = movementHistoryForMonth(base(), {
    studentIds: [0, 301, 'not-an-id'], month: '2026-11',
  });
  assert.equal(result.state, 'INVALID_QUERY');
  assert.deepEqual(result.movements, []);
});


test('rejects unsupported kinds and blank subjects in metadata lookups', () => {
  const unsupported = resolveMetadataAt(base(), {
    kind: 'UNSUPPORTED', subjectId: 'x', effectiveDate: '2026-11-01',
  });
  const blankSubject = resolveMetadataAt(base(), {
    kind: 'SCHOOL_PROFILE', subjectId: ' ', effectiveDate: '2026-11-01',
  });
  assert.equal(unsupported.state, 'INVALID_QUERY');
  assert.equal(blankSubject.state, 'INVALID_QUERY');
});


test('requires a distinct legalName object instead of flat operational-name fields', () => {
  const value = base();
  const data = value.transactions[0].changes[2].record.data;
  delete data.legalName;
  Object.assign(data, { firstName: 'Operational', lastName: 'Label' });
  assert.ok(codes(validateMetadataHistory(value)).has('LEGAL_REPORTING_NAME_REQUIRED'));
});

test('preserves NLS as a separate raw movement type without inventing its meaning', () => {
  const value = base();
  const item = value.transactions[0].changes[3].record;
  item.data.movementType = 'NLS';
  const result = validateMetadataHistory(value);
  assert.equal(result.issues.length, 0);
  assert.ok(result.warnings.some((entry) => entry.code === 'NLS_SEMANTICS_UNCONFIRMED'));
});


test('requires NLS movement meaning to remain unconfirmed until school semantics are known', () => {
  const value = base();
  const item = value.transactions[0].changes[3].record;
  item.data.movementType = 'NLS';
  item.data.effectiveDateMeaning = 'LAST_ENROLLED_DAY';
  assert.ok(codes(validateMetadataHistory(value)).has('MOVEMENT_DATE_MEANING_MISMATCH'));
});

test('rejects a movement date meaning that contradicts its declared movement type', () => {
  const value = base();
  value.transactions[0].changes[3].record.data.effectiveDateMeaning = 'FIRST_ENROLLED_DAY';
  assert.ok(codes(validateMetadataHistory(value)).has('MOVEMENT_DATE_MEANING_MISMATCH'));
});

test('rejects numeric LRN values to preserve leading zeroes', () => {
  const value = base();
  value.transactions[0].changes[2].record.data.lrn = 12345678901;
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_LRN_TYPE'));
});


test('materializes metadata at a pinned revision without exposing later corrections', () => {
  const value = base(); value.revision = 2;
  value.transactions.push({ revision: 2, committed: true, changes: [
    { op: 'REVOKE', targetRecordId: 'learner-1' },
    { op: 'PUT', record: learner(1, 'learner-corrected', '112345678901') },
  ] });
  const revision1 = metadataRecordsAtRevision(value, 1);
  const revision2 = metadataRecordsAtRevision(value, 2);
  assert.equal(revision1.state, 'OK');
  assert.equal(revision2.state, 'OK');
  assert.ok(revision1.records.some((entry) => entry.recordId === 'learner-1'));
  assert.ok(!revision1.records.some((entry) => entry.recordId === 'learner-corrected'));
  assert.ok(revision2.records.some((entry) => entry.recordId === 'learner-corrected'));
});


test('validates LRN uniqueness at the selected historical revision after later revocation', () => {
  const value = base();
  value.revision = 3;
  value.transactions.push(
    { revision: 2, committed: true, changes: [
      { op: 'PUT', record: learner(2, 'learner-2', '012345678901') },
    ] },
    { revision: 3, committed: true, changes: [
      { op: 'REVOKE', targetRecordId: 'learner-2' },
    ] },
  );
  assert.equal(validateMetadataHistory(value).state, 'INCOMPLETE');
  assert.equal(metadataRecordsAtRevision(value, 2).state, 'INVALID_HISTORY');
  assert.equal(metadataRecordsAtRevision(value, 3).state, 'OK');
});

test('rejects movement date meaning when it is missing rather than inferring interval semantics', () => {
  const value = base();
  delete value.transactions[0].changes[3].record.data.effectiveDateMeaning;
  assert.ok(codes(validateMetadataHistory(value)).has('INVALID_MOVEMENT_DATE_MEANING'));
});
