import test from 'node:test';
import assert from 'node:assert/strict';
import { projectMonthRoster } from '../month_roster_projection.mjs';

const prefix = { openGeneration: '3', recordCount: 100, lastSeq: 100, auditGeneration: 8 };
function build(overrides = {}) {
  return projectMonthRoster({
    monthStartDay: 100,
    monthEndExclusive: 131,
    knownStudentIds: [1, 2, 3],
    intervals: [
      { studentId: 1, startDayInclusive: 90, endDayExclusive: 110 },
      { studentId: 1, startDayInclusive: 115, endDayExclusive: null },
      { studentId: 2, startDayInclusive: 131, endDayExclusive: null },
      { studentId: 3, startDayInclusive: 70, endDayExclusive: 100 },
    ],
    // Revisions are supplied in global event-log sequence order.
    nameRevisions: [
      { studentId: 3, effectiveDay: 70, sourceSeq: 4, name: 'Carlo Dela Cruz' },
      { studentId: 1, effectiveDay: 90, sourceSeq: 5, name: 'Ana Reyes' },
      { studentId: 2, effectiveDay: 131, sourceSeq: 20, name: 'Ben Cruz' },
    ],
    sourcePrefix: { ...prefix },
    expectedSourcePrefix: { ...prefix },
    rosterHistoryComplete: true,
    nameHistoryComplete: true,
    ...overrides,
  });
}

test('selects month-overlap roster once per student and preserves separated intervals', () => {
  const result = build();
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.learners.map((learner) => learner.studentId), [1]);
  assert.equal(result.total, 1);
  assert.equal(result.learners[0].enrollmentIntervals.length, 2);
});

test('uses half-open boundaries: end at month start and start at month end do not overlap', () => {
  const result = build();
  assert.equal(result.learners.some((learner) => learner.studentId === 2), false);
  assert.equal(result.learners.some((learner) => learner.studentId === 3), false);
});

test('preserves original interval boundaries instead of clipping them to the month', () => {
  const result = build();
  assert.equal(result.learners[0].enrollmentIntervals[0].startDayInclusive, 90);
  assert.equal(result.learners[0].enrollmentIntervals[0].endDayExclusive, 110);
});

test('identifies learners enrolled on the final report day separately from month overlap', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 120, endDayExclusive: 131 }],
    nameRevisions: [{ studentId: 1, effectiveDay: 120, sourceSeq: 5, name: 'Ana Reyes' }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].enrolledOnLastDay, true);
});

test('uses the final included day, not the exclusive month end, for enrollment status', () => {
  const result = build({
    knownStudentIds: [1, 2],
    intervals: [
      { studentId: 1, startDayInclusive: 100, endDayExclusive: 130 },
      { studentId: 2, startDayInclusive: 100, endDayExclusive: 131 },
    ],
    nameRevisions: [
      { studentId: 1, effectiveDay: 100, sourceSeq: 5, name: 'Ana Reyes' },
      { studentId: 2, effectiveDay: 100, sourceSeq: 6, name: 'Ben Cruz' },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].enrolledOnLastDay, false);
  assert.equal(result.learners[1].enrolledOnLastDay, true);
});

test('includes a learner who entered and exited inside the month', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 104, endDayExclusive: 109 }],
    nameRevisions: [{ studentId: 1, effectiveDay: 104, sourceSeq: 5, name: 'Ana Reyes' }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 1);
  assert.equal(result.learners[0].enrolledOnLastDay, false);
});

test('rejects mismatched or incomplete source prefixes', () => {
  const result = build({ expectedSourcePrefix: { ...prefix, recordCount: 99 } });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'SOURCE_PREFIX_MISMATCH'));
});

test('rejects incomplete roster or name history', () => {
  const result = build({ rosterHistoryComplete: false, nameHistoryComplete: false });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'ROSTER_HISTORY_INCOMPLETE'));
  assert.ok(result.warnings.some((entry) => entry.code === 'OPERATIONAL_NAME_HISTORY_INCOMPLETE'));
});

test('rejects overlapping intervals for one student', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [
      { studentId: 1, startDayInclusive: 90, endDayExclusive: 120 },
      { studentId: 1, startDayInclusive: 110, endDayExclusive: 140 },
    ],
    nameRevisions: [{ studentId: 1, effectiveDay: 90, sourceSeq: 5, name: 'Ana Reyes' }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'OVERLAPPING_ENROLLMENT_INTERVALS'));
});

test('rejects known IDs without any enrollment interval', () => {
  const result = build({ intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: 120 }] });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'STUDENT_WITHOUT_INTERVAL'));
});

test('reports operational display-name changes without blocking legal reporting roster', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [
      { studentId: 1, effectiveDay: 90, sourceSeq: 5, name: 'Ana Reyes' },
      { studentId: 1, effectiveDay: 120, sourceSeq: 50, name: 'Ana Santos' },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].nameState, 'CHANGED_IN_MONTH');
  assert.equal(result.learners[0].displayName, null);
  assert.equal(result.learners[0].nameHistory.length, 2);
  assert.ok(result.warnings.some((entry) => entry.code === 'OPERATIONAL_DISPLAY_NAME_CHANGED'));
});

test('same-day operational name revisions preserve source sequence as a diagnostic', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [
      { studentId: 1, effectiveDay: 100, sourceSeq: 11, name: 'Ana Reyes' },
      { studentId: 1, effectiveDay: 100, sourceSeq: 16, name: 'Ana R. Reyes' },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.learners[0].nameHistory.map((revision) => revision.sourceSeq), [11, 16]);
  assert.equal(result.learners[0].nameState, 'CHANGED_IN_MONTH');
  assert.ok(result.warnings.some((entry) => entry.code === 'OPERATIONAL_DISPLAY_NAME_CHANGED'));
});

test('identical repeated display-name values do not create a false name conflict', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [
      { studentId: 1, effectiveDay: 90, sourceSeq: 5, name: 'Ana Reyes' },
      { studentId: 1, effectiveDay: 110, sourceSeq: 20, name: 'Ana Reyes' },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].displayName, 'Ana Reyes');
  assert.equal(result.learners[0].nameHistory.length, 2);
});

test('reports backwards operational name effective days without treating them as legal-name evidence', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [
      { studentId: 1, effectiveDay: 120, sourceSeq: 5, name: 'Ana Santos' },
      { studentId: 1, effectiveDay: 110, sourceSeq: 20, name: 'Ana Reyes' },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.ok(result.warnings.some((entry) => entry.code === 'BACKDATED_OPERATIONAL_NAME_REVISION'));
});

test('rejects duplicate name source sequences', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [
      { studentId: 1, effectiveDay: 90, sourceSeq: 5, name: 'Ana Reyes' },
      { studentId: 1, effectiveDay: 91, sourceSeq: 5, name: 'Ana Santos' },
    ],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'DUPLICATE_NAME_SOURCE_SEQUENCE'));
});

test('rejects operational display names exceeding 32 UTF-8 bytes', () => {
  const longName = '界'.repeat(11); // 33 UTF-8 bytes, above operational 32-byte limit
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [{ studentId: 1, effectiveDay: 90, sourceSeq: 5, name: longName }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'INVALID_NAME_REVISION'));
});

test('rejects name revisions that are not supplied in global source-sequence order', () => {
  const result = build({
    knownStudentIds: [1, 2],
    intervals: [
      { studentId: 1, startDayInclusive: 90, endDayExclusive: null },
      { studentId: 2, startDayInclusive: 90, endDayExclusive: null },
    ],
    nameRevisions: [
      { studentId: 1, effectiveDay: 90, sourceSeq: 20, name: 'Ana Reyes' },
      { studentId: 2, effectiveDay: 90, sourceSeq: 10, name: 'Ben Cruz' },
    ],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((entry) => entry.code === 'NAME_REVISIONS_OUT_OF_SOURCE_ORDER'));
});

test('allows a valid empty month roster without fabricating a learner row', () => {
  const emptyPrefix = { openGeneration: '4', recordCount: 0, lastSeq: 0, auditGeneration: 1 };
  const result = projectMonthRoster({
    monthStartDay: 100,
    monthEndExclusive: 131,
    knownStudentIds: [],
    intervals: [],
    nameRevisions: [],
    sourcePrefix: { ...emptyPrefix },
    expectedSourcePrefix: { ...emptyPrefix },
    rosterHistoryComplete: true,
    nameHistoryComplete: true,
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 0);
  assert.deepEqual(result.learners, []);
});

test('reports missing operational display name without blocking legal reporting roster', () => {
  const result = build({
    knownStudentIds: [1],
    intervals: [{ studentId: 1, startDayInclusive: 90, endDayExclusive: null }],
    nameRevisions: [],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].nameState, 'MISSING');
  assert.ok(result.warnings.some((entry) => entry.code === 'MISSING_OPERATIONAL_DISPLAY_NAME'));
});
