import test from 'node:test';
import assert from 'node:assert/strict';
import { selectMonthlyRoster } from '../attendance_model.mjs';
import { calculateSectionMonth } from '../section_model.mjs';

const learner = (studentId, sex, fullName, startInclusive = '2026-11-01', endExclusive = null) => ({
  studentId,
  sex,
  fullName,
  enrollmentPeriods: [{ startInclusive, endExclusive }],
});
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

function makeRoster(learners, month = '2026-11') {
  return selectMonthlyRoster({ month, learners });
}

function calculate({
  month = '2026-11',
  learners,
  calendarDays = ['2026-11-02'],
  attendanceByStudent,
  rosterHistoryComplete = true,
  calendarComplete = true,
  snapshotConsistent = true,
} = {}) {
  const roster = makeRoster(learners ?? [], month);
  return calculateSectionMonth({
    month,
    roster,
    calendarDays,
    rosterHistoryComplete,
    calendarComplete,
    snapshotConsistent,
    attendanceByStudent,
  });
}

test('section calculation produces sex-separated and combined daily totals', () => {
  const result = calculate({
    learners: [learner(1, 'M', 'Male One'), learner(2, 'F', 'Female One')],
    calendarDays: ['2026-11-02', '2026-11-03'],
    attendanceByStudent: [
      { studentId: 1, days: [
        { date: '2026-11-02', am: present(), pm: present() },
        { date: '2026-11-03', am: absent(), pm: absent() },
      ] },
      { studentId: 2, days: [
        { date: '2026-11-02', am: excused(), pm: absent() },
        { date: '2026-11-03', am: present(), pm: present() },
      ] },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.dailyTotals.map((day) => [day.present, day.absent]), [[1, 1], [1, 1]]);
  assert.deepEqual(result.summary.learnerDayTotals, {
    male: { presentLearnerDays: 1, absentLearnerDays: 1 },
    female: { presentLearnerDays: 1, absentLearnerDays: 1 },
    total: { presentLearnerDays: 2, absentLearnerDays: 2 },
  });
});

test('section totals count Late+Present as Present without emitting Tardy output', () => {
  const result = calculate({
    learners: [learner(3, 'F', 'Female Three')],
    attendanceByStudent: [{ studentId: 3, days: [{ date: '2026-11-02', am: late(), pm: present() }] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.dailyTotals[0].femalePresent, 1);
  assert.equal(result.learners[0].monthly.days[0].result.code, 'BLANK');
  assert.equal('observedLateSessions' in result.summary, false);
  assert.equal('tardyTotal' in result.summary, false);
  assert.equal('lateSessions' in result.learners[0].monthly.days[0].result, false);
});

test('section totals count a both-sessions-Excused E day as Present', () => {
  const result = calculate({
    learners: [learner(18, 'F', 'Excused Both Sessions')],
    attendanceByStudent: [{ studentId: 18, days: [{ date: '2026-11-02', am: excused(), pm: excused() }] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].monthly.days[0].result.code, 'E');
  assert.equal(result.dailyTotals[0].femalePresent, 1);
  assert.equal(result.dailyTotals[0].femaleAbsent, 0);
});

test('section totals count a Present+Excused full day as Present', () => {
  const result = calculate({
    learners: [learner(19, 'F', 'Present Excused')],
    attendanceByStudent: [{ studentId: 19, days: [{ date: '2026-11-02', am: present(), pm: excused() }] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].monthly.days[0].result.code, 'BLANK');
  assert.equal(result.learners[0].monthly.summary.presentDays, 1);
  assert.equal(result.dailyTotals[0].femalePresent, 1);
  assert.equal(result.dailyTotals[0].femaleAbsent, 0);
});

test('a learner enrolling mid-month is expected only on class dates after enrollment begins', () => {
  const result = calculate({
    learners: [learner(4, 'M', 'Midmonth', '2026-11-03')],
    calendarDays: ['2026-11-02', '2026-11-03', '2026-11-04'],
    attendanceByStudent: [{ studentId: 4, days: [
      { date: '2026-11-03', am: present(), pm: present() },
      { date: '2026-11-04', am: present(), pm: present() },
    ] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.dailyTotals[0].enrolled, 0);
  assert.equal(result.dailyTotals[1].enrolled, 1);
  assert.equal(result.learners[0].monthly.summary.classDays, 2);
});

test('a transferred-out learner remains in the month roster but is no longer expected after endExclusive', () => {
  const result = calculate({
    learners: [learner(5, 'M', 'Transferred', '2026-11-01', '2026-11-04')],
    calendarDays: ['2026-11-02', '2026-11-03', '2026-11-04'],
    attendanceByStudent: [{ studentId: 5, days: [
      { date: '2026-11-02', am: present(), pm: present() },
      { date: '2026-11-03', am: present(), pm: absent() },
    ] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners.length, 1);
  assert.equal(result.learners[0].monthly.summary.classDays, 2);
  assert.equal(result.dailyTotals[2].enrolled, 0);
});

test('a missing expected learner/date result blocks the entire section export', () => {
  const result = calculate({
    learners: [learner(6, 'F', 'Missing Date')],
    calendarDays: ['2026-11-02', '2026-11-03'],
    attendanceByStudent: [{ studentId: 6, days: [{ date: '2026-11-02', am: present(), pm: present() }] }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'MISSING_EXPECTED_ATTENDANCE'));
});

test('attendance recorded outside a class date or enrollment period blocks export', () => {
  const result = calculate({
    learners: [learner(7, 'M', 'Out of Scope')],
    attendanceByStudent: [{ studentId: 7, days: [{ date: '2026-11-03', am: present(), pm: present() }] }],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'ATTENDANCE_OUTSIDE_EXPECTED_DAYS'));
});

test('a learner with no attendance envelope blocks rather than appearing as zero attendance', () => {
  const result = calculate({ learners: [learner(8, 'F', 'No Envelope')], attendanceByStudent: [] });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'MISSING_LEARNER_ATTENDANCE'));
});

test('duplicate attendance envelopes for one learner block export', () => {
  const entry = { studentId: 9, days: [{ date: '2026-11-02', am: present(), pm: present() }] };
  const result = calculate({
    learners: [learner(9, 'M', 'Duplicate Envelope')],
    attendanceByStudent: [entry, entry],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'DUPLICATE_ATTENDANCE_ENTRY'));
});

test('attendance for an ID outside the month roster blocks export', () => {
  const result = calculate({
    learners: [learner(10, 'F', 'Known')],
    attendanceByStudent: [
      { studentId: 10, days: [{ date: '2026-11-02', am: present(), pm: present() }] },
      { studentId: 11, days: [] },
    ],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'ATTENDANCE_FOR_UNKNOWN_LEARNER'));
});

test('missing complete roster-history evidence blocks section totals', () => {
  const result = calculate({
    learners: [learner(12, 'M', 'History Unknown')],
    attendanceByStudent: [{ studentId: 12, days: [{ date: '2026-11-02', am: present(), pm: present() }] }],
    rosterHistoryComplete: false,
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'ROSTER_HISTORY_INCOMPLETE'));
});

test('incomplete calendar evidence blocks section totals', () => {
  const result = calculate({
    learners: [learner(13, 'M', 'Calendar Unknown')],
    attendanceByStudent: [{ studentId: 13, days: [{ date: '2026-11-02', am: present(), pm: present() }] }],
    calendarComplete: false,
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'CALENDAR_INCOMPLETE'));
});

test('inconsistent source revisions block section totals', () => {
  const result = calculate({
    learners: [learner(14, 'F', 'Mixed Snapshot')],
    attendanceByStudent: [{ studentId: 14, days: [{ date: '2026-11-02', am: present(), pm: present() }] }],
    snapshotConsistent: false,
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'SNAPSHOT_INCONSISTENT'));
});

test('an unresolved result for one learner blocks all section totals', () => {
  const result = calculate({
    learners: [learner(15, 'M', 'Ready'), learner(16, 'F', 'Unresolved')],
    attendanceByStudent: [
      { studentId: 15, days: [{ date: '2026-11-02', am: present(), pm: present() }] },
      { studentId: 16, days: [{ date: '2026-11-02', am: present(), pm: { status: 'CLEAR' } }] },
    ],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.equal(result.summary, null);
});

test('duplicate calendar dates block section aggregation', () => {
  const result = calculate({
    learners: [],
    calendarDays: ['2026-11-02', '2026-11-02'],
    attendanceByStudent: [],
  });
  assert.equal(result.state, 'BLOCKED');
  assert.ok(result.issues.some((item) => item.code === 'DUPLICATE_CLASS_DATE'));
});

test('a complete month with no confirmed class dates reports zero learner-days without inventing absences', () => {
  const result = calculate({
    learners: [learner(17, 'F', 'No Scheduled Dates')],
    calendarDays: [],
    attendanceByStudent: [{ studentId: 17, days: [] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].monthly.summary.classDays, 0);
  assert.equal(result.summary.learnerDayTotals.total.absentLearnerDays, 0);
  assert.deepEqual(result.dailyTotals, []);
});

test('month-end registration counts only learners enrolled on the final calendar day', () => {
  const result = calculate({
    learners: [
      learner(20, 'M', 'Transferred Out', '2026-11-01', '2026-11-04'),
      learner(21, 'F', 'Enrolled on Month End', '2026-11-30'),
      learner(22, 'M', 'Exited on Month End', '2026-11-01', '2026-11-30'),
      learner(23, 'F', 'After Month End', '2026-12-01'),
    ],
    calendarDays: ['2026-11-02'],
    attendanceByStudent: [
      { studentId: 20, days: [{ date: '2026-11-02', am: present(), pm: present() }] },
      { studentId: 21, days: [] },
      { studentId: 22, days: [{ date: '2026-11-02', am: present(), pm: present() }] },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.summary.monthEndRegistration, { male: 0, female: 1, total: 1 });
  assert.equal(result.summary.monthRosterCount, 3);
});

test('month-end registration counts a re-entered learner once using the end-exclusive boundary', () => {
  const result = calculate({
    learners: [{
      studentId: 24, sex: 'M', fullName: 'Re-entered',
      enrollmentPeriods: [
        { startInclusive: '2026-11-01', endExclusive: '2026-11-10' },
        { startInclusive: '2026-11-20', endExclusive: '2026-12-01' },
      ],
    }],
    calendarDays: ['2026-11-02'],
    attendanceByStudent: [{ studentId: 24, days: [{ date: '2026-11-02', am: present(), pm: present() }] }],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.summary.monthEndRegistration, { male: 1, female: 0, total: 1 });
});

test('section summary aggregates learners with five consecutive absent class dates by sex', () => {
  const dates = ['2026-11-02', '2026-11-03', '2026-11-04', '2026-11-05', '2026-11-06', '2026-11-09', '2026-11-10'];
  const days = (statuses) => dates.map((date, index) => ({
    date,
    am: statuses[index] === 'ABSENT' ? absent() : present(),
    pm: statuses[index] === 'ABSENT' ? absent() : present(),
  }));
  const result = calculate({
    learners: [
      learner(25, 'M', 'Male Five'),
      learner(26, 'F', 'Female Five'),
      learner(27, 'F', 'Female Four'),
      learner(30, 'F', 'Six Scattered Absences'),
    ],
    calendarDays: dates,
    attendanceByStudent: [
      { studentId: 25, days: days(['ABSENT', 'ABSENT', 'ABSENT', 'ABSENT', 'ABSENT']) },
      { studentId: 26, days: days(['ABSENT', 'ABSENT', 'ABSENT', 'ABSENT', 'ABSENT']) },
      { studentId: 27, days: days(['ABSENT', 'ABSENT', 'ABSENT', 'ABSENT', 'PRESENT', 'PRESENT', 'PRESENT']) },
      { studentId: 30, days: days(['ABSENT', 'ABSENT', 'ABSENT', 'PRESENT', 'ABSENT', 'ABSENT', 'ABSENT']) },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.summary.fiveConsecutiveAbsenceLearners, { male: 1, female: 1, total: 2 });
});

test('month-end registration uses leap-year February 29 and its exclusive March 1 boundary', () => {
  const result = calculate({
    month: '2024-02',
    learners: [
      learner(28, 'F', 'Active on Leap Day', '2024-02-01', '2024-03-01'),
      learner(29, 'M', 'Exited on Leap Day', '2024-02-01', '2024-02-29'),
    ],
    calendarDays: ['2024-02-01'],
    attendanceByStudent: [
      { studentId: 28, days: [{ date: '2024-02-01', am: present(), pm: present() }] },
      { studentId: 29, days: [{ date: '2024-02-01', am: present(), pm: present() }] },
    ],
  });
  assert.equal(result.state, 'READY');
  assert.deepEqual(result.summary.monthEndRegistration, { male: 0, female: 1, total: 1 });
});

test('an enrollment gap breaks a consecutive-absence run', () => {
  const dates = ['2026-11-02', '2026-11-03', '2026-11-04', '2026-11-05', '2026-11-06', '2026-11-09', '2026-11-10'];
  const result = calculate({
    learners: [{
      studentId: 31, sex: 'F', fullName: 'Re-enrolled After Gap',
      enrollmentPeriods: [
        { startInclusive: '2026-11-02', endExclusive: '2026-11-04' },
        { startInclusive: '2026-11-06', endExclusive: null },
      ],
    }],
    calendarDays: dates,
    attendanceByStudent: [{ studentId: 31, days: [
      { date: '2026-11-02', am: absent(), pm: absent() },
      { date: '2026-11-03', am: absent(), pm: absent() },
      { date: '2026-11-06', am: absent(), pm: absent() },
      { date: '2026-11-09', am: absent(), pm: absent() },
      { date: '2026-11-10', am: absent(), pm: absent() },
    ] }],
  });
  assert.equal(result.state, 'READY');
  assert.equal(result.learners[0].monthly.summary.maxConsecutiveAbsenceDays, 3);
  assert.equal(result.learners[0].monthly.summary.hasFiveConsecutiveAbsences, false);
  assert.deepEqual(result.summary.fiveConsecutiveAbsenceLearners, { male: 0, female: 0, total: 0 });
});
