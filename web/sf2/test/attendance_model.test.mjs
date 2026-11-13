import test from 'node:test';
import assert from 'node:assert/strict';
import {
  resolveDailyCell,
  calculateLearnerMonth,
  selectMonthlyRoster,
} from '../attendance_model.mjs';

const committed = (status) => ({ status, eventCommitted: true });
const present = () => committed('PRESENT');
const late = () => committed('LATE');
const excused = () => committed('EXCUSED');
const absent = () => ({
  status: 'ABSENT_CONFIRMED',
  absenceEvidence: {
    sessionClosed: true,
    coverageComplete: true,
    studentExpected: true,
    logHealthy: true,
    snapshotConsistent: true,
  },
});
const notScheduled = () => ({ status: 'NOT_SCHEDULED', calendarConfirmed: true });

const pair = (am, pm) => resolveDailyCell(am, pm);

test('present in both sessions produces a blank full-day cell', () => {
  const result = pair(present(), present());
  assert.equal(result.state, 'READY');
  assert.equal(result.code, 'BLANK');
  assert.equal(result.countsAsPresentDay, true);
});

test('morning-only attendance produces upper-left shading and counts as absent', () => {
  const result = pair(present(), absent());
  assert.equal(result.code, 'AM_ONLY');
  assert.equal(result.countsAsAbsentDay, true);
  assert.equal(result.countsAsPresentDay, false);
});

test('afternoon-only attendance produces lower-right shading and counts as absent', () => {
  const result = pair(absent(), present());
  assert.equal(result.code, 'PM_ONLY');
  assert.equal(result.countsAsAbsentDay, true);
  assert.equal(result.countsAsPresentDay, false);
});

test('absence in both sessions produces x', () => {
  const result = pair(absent(), absent());
  assert.equal(result.code, 'X');
  assert.equal(result.countsAsAbsentDay, true);
  assert.equal(result.countsAsPresentDay, false);
});

test('excused in both sessions produces E and counts as present', () => {
  const result = pair(excused(), excused());
  assert.equal(result.code, 'E');
  assert.equal(result.countsAsPresentDay, true);
});

test('excused in one session and absent in the other produces the corresponding diagonal and counts as absent', () => {
  const morningExcused = pair(excused(), absent());
  const afternoonExcused = pair(absent(), excused());
  assert.equal(morningExcused.code, 'AM_ONLY');
  assert.equal(morningExcused.countsAsAbsentDay, true);
  assert.equal(afternoonExcused.code, 'PM_ONLY');
  assert.equal(afternoonExcused.countsAsAbsentDay, true);
});

test('a full day split between Present and Excused is blank and counts as present in either session order', () => {
  const excusedThenPresent = pair(excused(), present());
  const presentThenExcused = pair(present(), excused());
  for (const result of [excusedThenPresent, presentThenExcused]) {
    assert.equal(result.state, 'READY');
    assert.equal(result.code, 'BLANK');
    assert.equal(result.countsAsPresentDay, true);
    assert.equal(result.countsAsAbsentDay, false);
  }
});

test('a full day split between Late and Excused is also blank and counts as present', () => {
  const result = pair(late(), excused());
  assert.equal(result.state, 'READY');
  assert.equal(result.code, 'BLANK');
  assert.equal(result.amStatus, 'PRESENT');
  assert.equal(result.countsAsPresentDay, true);
});

test('late is treated as attended for the diagonal mark but the diagonal counts as absent', () => {
  const result = pair(late(), absent());
  assert.equal(result.code, 'AM_ONLY');
  assert.equal(result.countsAsAbsentDay, true);
  assert.equal(result.countsAsPresentDay, false);
  assert.equal(result.amStatus, 'PRESENT');
  assert.equal('lateSessions' in result, false);
});

test('late in both sessions produces a blank cell with no late marker', () => {
  const result = pair(late(), late());
  assert.equal(result.code, 'BLANK');
  assert.equal('lateSessions' in result, false);
});

test('missing session data is unresolved, never inferred as absence', () => {
  assert.equal(pair(present(), null).state, 'UNRESOLVED');
});

test('Clear is unresolved until the device derives a final session outcome', () => {
  assert.equal(pair({ status: 'CLEAR' }, present()).state, 'UNRESOLVED');
});

test('an absence without all trusted evidence is unresolved', () => {
  const weakAbsence = {
    status: 'ABSENT_CONFIRMED',
    absenceEvidence: { sessionClosed: true, coverageComplete: false },
  };
  assert.equal(pair(weakAbsence, absent()).state, 'UNRESOLVED');
});

test('a whole-day non-class date is excluded', () => {
  assert.equal(pair(notScheduled(), notScheduled()).state, 'EXCLUDED');
});

test('a one-session non-class date is blocked pending its own marking rule', () => {
  const result = pair(notScheduled(), present());
  assert.equal(result.state, 'UNRESOLVED');
  assert.ok(result.issues.some((issue) => issue.code === 'PARTIAL_DAY_SCHEDULE_POLICY_REQUIRED'));
});

test('monthly totals count blank and E as present but both diagonal directions and X as absent', () => {
  const result = calculateLearnerMonth({
    month: '2026-11',
    classDays: [
      { date: '2026-11-02', am: present(), pm: present() },
      { date: '2026-11-03', am: excused(), pm: absent() },
      { date: '2026-11-04', am: present(), pm: absent() },
      { date: '2026-11-05', am: absent(), pm: absent() },
      { date: '2026-11-06', am: excused(), pm: present() },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.summary.presentDays, 2);
  assert.equal(result.summary.absentDays, 3);
  assert.equal(result.summary.excusedDays, 0);
  assert.equal(result.summary.morningOnlyDays, 2);
  assert.equal(result.days[4].result.code, 'BLANK');
  assert.equal('observedLateSessions' in result.summary, false);
});

test('E remains a present day in monthly totals', () => {
  const result = calculateLearnerMonth({
    month: '2026-11',
    classDays: [{ date: '2026-11-02', am: excused(), pm: excused() }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.days[0].result.code, 'E');
  assert.equal(result.summary.presentDays, 1);
  assert.equal(result.summary.absentDays, 0);
});

test('diagonal half-day marks count toward five consecutive absent days', () => {
  const classDays = [
    { date: '2026-11-02', am: present(), pm: absent() },
    { date: '2026-11-03', am: absent(), pm: present() },
    { date: '2026-11-04', am: absent(), pm: absent() },
    { date: '2026-11-05', am: excused(), pm: absent() },
    { date: '2026-11-06', am: absent(), pm: late() },
  ];
  const result = calculateLearnerMonth({ month: '2026-11', classDays });
  assert.equal(result.state, 'READY');
  assert.equal(result.summary.absentDays, 5);
  assert.equal(result.summary.maxConsecutiveAbsenceDays, 5);
  assert.equal(result.summary.hasFiveConsecutiveAbsences, true);
});

test('monthly SF2 summary omits late/tardy metrics while counting late as present', () => {
  const result = calculateLearnerMonth({
    month: '2026-11',
    classDays: [{ date: '2026-11-02', am: late(), pm: present() }],
  });
  assert.equal(result.summary.presentDays, 1);
  assert.equal('tardyTotal' in result.summary, false);
  assert.equal('tardyCalculation' in result.summary, false);
  assert.equal('observedLateSessions' in result.summary, false);
});

test('five consecutive confirmed x days are counted across class days', () => {
  const classDays = Array.from({ length: 5 }, (_, i) => ({
    date: `2026-11-0${i + 2}`,
    am: absent(),
    pm: absent(),
  }));
  const result = calculateLearnerMonth({ month: '2026-11', classDays });
  assert.equal(result.state, 'READY');
  assert.equal(result.summary.maxConsecutiveAbsenceDays, 5);
  assert.equal(result.summary.hasFiveConsecutiveAbsences, true);
});

test('duplicate class dates block monthly calculation', () => {
  const day = { date: '2026-11-02', am: present(), pm: present() };
  assert.equal(calculateLearnerMonth({ month: '2026-11', classDays: [day, day] }).state, 'BLOCKED');
});

test('a class date outside the requested month blocks monthly calculation', () => {
  const day = { date: '2026-10-30', am: present(), pm: present() };
  assert.equal(calculateLearnerMonth({ month: '2026-11', classDays: [day] }).state, 'BLOCKED');
});

test('an unresolved day blocks the monthly result instead of returning partial totals', () => {
  const result = calculateLearnerMonth({
    month: '2026-11',
    classDays: [{ date: '2026-11-02', am: present(), pm: { status: 'CLEAR' } }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.summary, null);
});

test('monthly roster includes learners who enroll during the month', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [{
      studentId: 8,
      sex: 'F',
      fullName: 'Learner Eight',
      enrollmentPeriods: [{ startInclusive: '2026-11-12', endExclusive: null }],
    }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 1);
  assert.equal(result.female[0].studentId, 8);
});

test('monthly roster includes learners who exit before the export date', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [{
      studentId: 9,
      sex: 'M',
      fullName: 'Learner Nine',
      enrollmentPeriods: [{ startInclusive: '2026-10-01', endExclusive: '2026-11-10' }],
    }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 1);
  assert.equal(result.male[0].studentId, 9);
});

test('monthly roster excludes enrollment intervals entirely outside the report month', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [
      { studentId: 10, sex: 'M', fullName: 'Before', enrollmentPeriods: [{ startInclusive: '2026-10-01', endExclusive: '2026-11-01' }] },
      { studentId: 11, sex: 'F', fullName: 'After', enrollmentPeriods: [{ startInclusive: '2026-12-01', endExclusive: null }] },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 0);
});

test('multiple enrollment periods include a learner once and preserve the source order', () => {
  const first = { studentId: 12, sex: 'M', fullName: 'Learner Twelve', enrollmentPeriods: [
    { startInclusive: '2026-11-01', endExclusive: '2026-11-08' },
    { startInclusive: '2026-11-08', endExclusive: '2026-11-20' },
  ] };
  const second = { studentId: 13, sex: 'F', fullName: 'Learner Thirteen', enrollmentPeriods: [
    { startInclusive: '2026-11-01', endExclusive: null },
  ] };
  const result = selectMonthlyRoster({ month: '2026-11', learners: [first, second] });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 2);
  assert.deepEqual(result.male.map((learner) => learner.studentId), [12]);
  assert.deepEqual(result.female.map((learner) => learner.studentId), [13]);
});

test('monthly roster blocks duplicate stable student IDs', () => {
  const learner = { studentId: 14, sex: 'M', fullName: 'Duplicate', enrollmentPeriods: [{ startInclusive: '2026-11-01', endExclusive: null }] };
  const result = selectMonthlyRoster({ month: '2026-11', learners: [learner, { ...learner }] });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((issue) => issue.code === 'DUPLICATE_STUDENT_ID'));
});

test('monthly roster blocks missing sex rather than guessing a subtotal group', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [{ studentId: 15, fullName: 'Unknown Sex', enrollmentPeriods: [{ startInclusive: '2026-11-01', endExclusive: null }] }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((issue) => issue.code === 'INVALID_LEARNER_SEX'));
});

test('monthly roster has no hard 45-learner ceiling', () => {
  const learners = Array.from({ length: 60 }, (_, index) => ({
    studentId: index + 1,
    sex: index % 2 === 0 ? 'M' : 'F',
    fullName: `Learner ${index + 1}`,
    enrollmentPeriods: [{ startInclusive: '2026-11-01', endExclusive: null }],
  }));
  const result = selectMonthlyRoster({ month: '2026-11', learners });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 60);
});

test('monthly roster blocks malformed or overlapping enrollment intervals', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [{
      studentId: 16,
      sex: 'F',
      fullName: 'Invalid History',
      enrollmentPeriods: [
        { startInclusive: '2026-11-01', endExclusive: '2026-11-20' },
        { startInclusive: '2026-11-15', endExclusive: null },
      ],
    }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((issue) => issue.code === 'OVERLAPPING_ENROLLMENT_PERIODS'));
});

test('non-overlapping historical learners do not require current month name/sex fields', () => {
  const result = selectMonthlyRoster({
    month: '2026-11',
    learners: [{
      studentId: 17,
      enrollmentPeriods: [{ startInclusive: '2026-09-01', endExclusive: '2026-10-01' }],
    }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.total, 0);
});
