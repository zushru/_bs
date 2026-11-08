import test from 'node:test';
import assert from 'node:assert/strict';
import { calculateSnapshotMonth } from '../snapshot_adapter.mjs';
import { cloneSnapshot, fixtureExpectations, makeValidSnapshot } from '../fixtures/snapshot_fixtures.mjs';

test('adapts a complete snapshot into the existing pure section calculator', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  assert.equal(result.state, 'PREVIEW_READY');
  assert.equal(result.report.summary.monthRosterCount, 3);
  assert.equal(result.report.learners.length, 3);
});

test('preserves enrollment-overlap roster and transfer-out learner', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  assert.deepEqual(result.report.learners.map((row) => row.studentId).sort((a, b) => a - b), [1, 2, 3]);
});

test('preserves the signed local AM/PM cell conversion', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  const actual = Object.fromEntries(result.report.learners.map((learner) => [
    learner.studentId,
    Object.fromEntries(learner.monthly.days.map(({ date, result: day }) => [date, day.code])),
  ]));
  assert.deepEqual(actual, fixtureExpectations.dailyCodes);
});

test('counts E as present in month totals', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  const learner = result.report.learners.find((row) => row.studentId === 2);
  assert.equal(learner.monthly.summary.presentDays, 2);
  assert.equal(learner.monthly.summary.absentDays, 0);
  assert.equal(learner.monthly.summary.excusedDays, 1);
});

test('preserves Late observation while keeping Tardy deferred', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  const learner = result.report.learners.find((row) => row.studentId === 1);
  assert.equal(learner.monthly.summary.observedLateSessions, 1);
  assert.equal(learner.monthly.summary.tardyTotal, null);
  assert.equal(learner.monthly.summary.tardyCalculation, 'DEFERRED');
});

test('does not invent attendance on non-class days', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  assert.ok(result.report.dailyTotals.every((row) => row.date !== '2026-11-04'));
});

test('blocks calculation when roster history is incomplete', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.rosterHistoryComplete = false;
  const result = calculateSnapshotMonth(snapshot);
  assert.equal(result.state, 'BLOCKED');
});

test('blocks calculation when schedule history is incomplete', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.scheduleHistoryComplete = false;
  assert.equal(calculateSnapshotMonth(snapshot).state, 'BLOCKED');
});

test('blocks calculation when source revisions are inconsistent', () => {
  const snapshot = cloneSnapshot(); snapshot.completeness.attendanceSnapshotConsistent = false;
  assert.equal(calculateSnapshotMonth(snapshot).state, 'BLOCKED');
});

test('blocks calculation on missing expected attendance', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days.pop();
  assert.equal(calculateSnapshotMonth(snapshot).state, 'BLOCKED');
});

test('blocks calculation instead of mapping missing sessions to X', () => {
  const snapshot = cloneSnapshot(); delete snapshot.attendance[0].days[0].pm;
  assert.equal(calculateSnapshotMonth(snapshot).state, 'BLOCKED');
});

test('a structurally valid snapshot is preview-only without authenticated identity evidence', () => {
  const result = calculateSnapshotMonth(makeValidSnapshot());
  assert.equal(result.state, 'PREVIEW_READY');
  assert.equal(result.authoritativeExportReady, false);
  assert.equal(result.authority.state, 'NOT_ESTABLISHED');
  assert.equal(result.authority.code, 'AUTHENTICATED_IDENTITY_REVIEW_MISSING');
  assert.equal(result.workbookGenerated, false);
  assert.equal(result.workbookWriterAvailable, false);
});

test('a caller-supplied SOURCE_CHECKED claim cannot promote a raw snapshot', () => {
  const snapshot = cloneSnapshot();
  snapshot.learners[0].identity.sourceStatus = 'SOURCE_CHECKED';
  const result = calculateSnapshotMonth(snapshot);
  assert.equal(result.state, 'PREVIEW_READY');
  assert.equal(result.authoritativeExportReady, false);
  assert.equal(result.authority.state, 'NOT_ESTABLISHED');
});
