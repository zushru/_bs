import test from 'node:test';
import assert from 'node:assert/strict';
import { buildLearnerRows, paginateLearnerRows } from '../learner_layout.mjs';

const learner = (studentId, sex, fullName = `Learner ${studentId}`) => ({ studentId, sex, fullName });
const roster = (male, female) => ({
  state: 'READY',
  male,
  female,
  total: male.length + female.length,
});

test('semantic row plan places male learners before female learners', () => {
  const result = buildLearnerRows(roster([learner(1, 'M'), learner(2, 'M')], [learner(3, 'F')]));
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.rows.map((row) => [row.studentId, row.sex]), [[1, 'M'], [2, 'M'], [3, 'F']]);
  assert.deepEqual(result.counts, { male: 2, female: 1, total: 3 });
});

test('row plan uses stable student IDs as keys rather than learner names', () => {
  const result = buildLearnerRows(roster([learner(4, 'M', 'Same Name')], [learner(5, 'F', 'Same Name')]));
  assert.deepEqual(result.rows.map((row) => row.key), [4, 5]);
});

test('row plan blocks duplicate student IDs across sex groups', () => {
  const result = buildLearnerRows(roster([learner(6, 'M')], [learner(6, 'F')]));
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.issues[0].code, 'DUPLICATE_STUDENT_ID');
});

test('row plan blocks inconsistent or incomplete roster counts', () => {
  const result = buildLearnerRows({ ...roster([learner(7, 'M')], []), total: 2 });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.issues[0].code, 'INVALID_MONTH_ROSTER');
});

test('pagination keeps every learner and creates continuation pages above capacity', () => {
  const plan = buildLearnerRows(roster(
    Array.from({ length: 4 }, (_, i) => learner(i + 1, 'M')),
    Array.from({ length: 3 }, (_, i) => learner(i + 5, 'F')),
  ));
  const result = paginateLearnerRows(plan, { learnerRowsPerPage: 3 });
  assert.equal(result.state, 'READY');
  assert.equal(result.totalPages, 3);
  assert.deepEqual(result.pages.map((page) => page.rows.length), [3, 3, 1]);
  assert.deepEqual(result.pages.flatMap((page) => page.rows.map((row) => row.studentId)), [1, 2, 3, 4, 5, 6, 7]);
});

test('pagination identifies a sex group that continues across a page break', () => {
  const plan = buildLearnerRows(roster(
    Array.from({ length: 4 }, (_, i) => learner(i + 1, 'M')),
    [learner(5, 'F')],
  ));
  const result = paginateLearnerRows(plan, { learnerRowsPerPage: 3 });
  assert.equal(result.pages[0].continuesGroupOnNext, true);
  assert.equal(result.pages[1].continuesGroupFromPrevious, true);
  assert.equal(result.pages[1].continuesGroupOnNext, false);
});

test('pagination does not label a male-to-female page boundary as a continued group', () => {
  const plan = buildLearnerRows(roster([learner(1, 'M')], [learner(2, 'F'), learner(3, 'F')]));
  const result = paginateLearnerRows(plan, { learnerRowsPerPage: 1 });
  assert.equal(result.pages[0].continuesGroupOnNext, false);
  assert.equal(result.pages[1].continuesGroupFromPrevious, false);
});

test('pagination requires a measured positive page capacity and has no default', () => {
  const plan = buildLearnerRows(roster([learner(8, 'M')], []));
  assert.equal(paginateLearnerRows(plan).state, 'BLOCKED');
  assert.equal(paginateLearnerRows(plan, { learnerRowsPerPage: 0 }).state, 'BLOCKED');
  assert.equal(paginateLearnerRows(plan, { learnerRowsPerPage: 1.5 }).state, 'BLOCKED');
});

test('an empty month roster produces no learner pages rather than a fabricated learner row', () => {
  const plan = buildLearnerRows(roster([], []));
  const result = paginateLearnerRows(plan, { learnerRowsPerPage: 10 });
  assert.equal(result.state, 'READY');
  assert.equal(result.totalPages, 0);
  assert.equal(result.totalLearnerRows, 0);
});

test('pagination independently rejects a malformed externally supplied row plan', () => {
  const result = paginateLearnerRows({
    state: 'READY',
    rows: [{ kind: 'LEARNER', key: 1, studentId: 1, sex: 'M', fullName: '' }],
    counts: { male: 1, female: 0, total: 1 },
  }, { learnerRowsPerPage: 10 });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.issues[0].code, 'INVALID_LEARNER_ROW_PLAN');
});

test('pagination independently rejects a row plan with duplicate student IDs', () => {
  const row = { kind: 'LEARNER', key: 1, studentId: 1, sex: 'M', fullName: 'Duplicate' };
  const result = paginateLearnerRows({
    state: 'READY',
    rows: [row, { ...row }],
    counts: { male: 2, female: 0, total: 2 },
  }, { learnerRowsPerPage: 10 });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.issues[0].code, 'DUPLICATE_STUDENT_ID');
});
