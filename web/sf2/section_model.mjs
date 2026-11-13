import { calculateLearnerMonth, selectMonthlyRoster } from './attendance_model.mjs';
import { buildLearnerRows } from './learner_layout.mjs';

function issue(code, message) {
  return { code, message };
}

function validDate(value, month) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)
    || (month && !value.startsWith(`${month}-`))) return false;
  const [year, monthNumber, day] = value.split('-').map(Number);
  const parsed = new Date(Date.UTC(year, monthNumber - 1, day));
  return parsed.getUTCFullYear() === year
    && parsed.getUTCMonth() + 1 === monthNumber
    && parsed.getUTCDate() === day;
}

function isEnrolledOn(learner, date) {
  return learner.enrollmentPeriods.some((period) => (
    period.startInclusive <= date
    && (period.endExclusive === null || period.endExclusive === undefined || date < period.endExclusive)
  ));
}

function lastCalendarDateOfMonth(month) {
  const [year, monthNumber] = month.split('-').map(Number);
  const leapYear = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const daysByMonth = [31, leapYear ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  return `${month}-${String(daysByMonth[monthNumber - 1]).padStart(2, '0')}`;
}

function consecutiveAbsenceRun(learner, monthlyDays, classDates) {
  const resultByDate = new Map(monthlyDays.map(({ date, result }) => [date, result]));
  let current = 0;
  let maximum = 0;
  for (const date of classDates) {
    if (!isEnrolledOn(learner, date)) {
      current = 0;
      continue;
    }
    const result = resultByDate.get(date);
    if (result?.countsAsAbsentDay === true) {
      current += 1;
      maximum = Math.max(maximum, current);
    } else {
      current = 0;
    }
  }
  return maximum;
}

function zeroLearnerResult() {
  return {
    state: 'READY',
    days: [],
    issues: [],
    summary: {
      classDays: 0,
      presentDays: 0,
      absentDays: 0,
      excusedDays: 0,
      morningOnlyDays: 0,
      afternoonOnlyDays: 0,
      maxConsecutiveAbsenceDays: 0,
      hasFiveConsecutiveAbsences: false,
    },
  };
}

/**
 * Aggregate a complete month snapshot for the device's one configured section.
 * `calendarDays` is the complete confirmed list of class dates for that section.
 * Each learner must have exactly one AM/PM result for each class date on which
 * their half-open enrollment history says they were expected.
 */
export function calculateSectionMonth({
  month,
  roster,
  calendarDays,
  rosterHistoryComplete,
  calendarComplete,
  snapshotConsistent,
  attendanceByStudent,
}) {
  const issues = [];
  if (typeof month !== 'string' || !/^\d{4}-(0[1-9]|1[0-2])$/.test(month)) {
    issues.push(issue('INVALID_MONTH', 'Month must use YYYY-MM and be a valid calendar month.'));
  }
  let canonicalRoster = null;
  if (!roster || roster.state !== 'READY' || !Array.isArray(roster.male) || !Array.isArray(roster.female)) {
    issues.push(issue('INVALID_MONTH_ROSTER', 'A READY month-overlap roster is required.'));
  } else {
    const suppliedLearners = [...roster.male, ...roster.female];
    const checkedRoster = selectMonthlyRoster({ month, learners: suppliedLearners });
    if (checkedRoster.state !== 'READY') {
      issues.push(...checkedRoster.issues);
    } else {
      const sameGroup = (left, right) => left.length === right.length
        && left.every((learner, index) => learner.studentId === right[index].studentId);
      if (roster.total !== suppliedLearners.length
        || checkedRoster.total !== suppliedLearners.length
        || !sameGroup(roster.male, checkedRoster.male)
        || !sameGroup(roster.female, checkedRoster.female)) {
        issues.push(issue('ROSTER_SELECTION_MISMATCH', 'The supplied roster does not exactly match the validated month-overlap selection.'));
      } else {
        canonicalRoster = checkedRoster;
      }
    }
  }
  if (rosterHistoryComplete !== true) {
    issues.push(issue('ROSTER_HISTORY_INCOMPLETE', 'The device has not confirmed that all month-overlap enrollment history is present.'));
  }
  const rowPlan = canonicalRoster ? buildLearnerRows(canonicalRoster) : null;
  if (rowPlan && rowPlan.state !== 'READY') issues.push(...rowPlan.issues);
  if (!Array.isArray(calendarDays)) {
    issues.push(issue('INVALID_CALENDAR', 'calendarDays must be the complete confirmed class-date list.'));
  }
  if (calendarComplete !== true) {
    issues.push(issue('CALENDAR_INCOMPLETE', 'The school calendar is not confirmed complete for this reporting month.'));
  }
  if (snapshotConsistent !== true) {
    issues.push(issue('SNAPSHOT_INCONSISTENT', 'The reporting snapshot revisions are not consistent.'));
  }
  if (!Array.isArray(attendanceByStudent)) {
    issues.push(issue('INVALID_ATTENDANCE_SET', 'Attendance results must be supplied once per month-overlap learner.'));
  }
  if (issues.length) {
    return { state: 'BLOCKED', learners: [], dailyTotals: [], summary: null, issues };
  }

  const monthEndDate = lastCalendarDateOfMonth(month);
  const monthEndMale = canonicalRoster.male.filter((learner) => isEnrolledOn(learner, monthEndDate)).length;
  const monthEndFemale = canonicalRoster.female.filter((learner) => isEnrolledOn(learner, monthEndDate)).length;
  const monthEndRegistration = {
    male: monthEndMale,
    female: monthEndFemale,
    total: monthEndMale + monthEndFemale,
  };

  const sortedCalendar = [...calendarDays].sort();
  const calendarSet = new Set();
  for (const date of sortedCalendar) {
    if (!validDate(date, month)) {
      issues.push(issue('INVALID_CLASS_DATE', `Class date ${String(date)} is invalid or outside ${month}.`));
      continue;
    }
    if (calendarSet.has(date)) {
      issues.push(issue('DUPLICATE_CLASS_DATE', `Class date ${date} appears more than once in the calendar.`));
    }
    calendarSet.add(date);
  }
  if (issues.length) return { state: 'BLOCKED', learners: [], dailyTotals: [], summary: null, issues };

  const attendanceMap = new Map();
  for (const entry of attendanceByStudent) {
    if (!entry || !Number.isSafeInteger(entry.studentId) || entry.studentId <= 0
      || !Array.isArray(entry.days)) {
      issues.push(issue('INVALID_ATTENDANCE_ENTRY', 'An attendance entry has an invalid studentId or days array.'));
      continue;
    }
    if (attendanceMap.has(entry.studentId)) {
      issues.push(issue('DUPLICATE_ATTENDANCE_ENTRY', `Student ID ${entry.studentId} has more than one attendance entry.`));
      continue;
    }
    attendanceMap.set(entry.studentId, entry);
  }

  const rosterIds = new Set(rowPlan.rows.map((row) => row.studentId));
  for (const studentId of attendanceMap.keys()) {
    if (!rosterIds.has(studentId)) {
      issues.push(issue('ATTENDANCE_FOR_UNKNOWN_LEARNER', `Attendance contains student ID ${studentId}, which is not in the month-overlap roster.`));
    }
  }

  const daily = new Map(sortedCalendar.map((date) => [date, {
    date,
    maleEnrolled: 0,
    femaleEnrolled: 0,
    enrolled: 0,
    malePresent: 0,
    maleAbsent: 0,
    femalePresent: 0,
    femaleAbsent: 0,
    present: 0,
    absent: 0,
  }]));
  const learnerResults = [];

  for (const learnerRow of rowPlan.rows) {
    const learner = learnerRow.sex === 'M'
      ? canonicalRoster.male.find((candidate) => candidate.studentId === learnerRow.studentId)
      : canonicalRoster.female.find((candidate) => candidate.studentId === learnerRow.studentId);
    const attendance = attendanceMap.get(learnerRow.studentId);
    if (!attendance) {
      issues.push(issue('MISSING_LEARNER_ATTENDANCE', `Student ID ${learnerRow.studentId} has no attendance snapshot entry.`));
      continue;
    }
    if (!Array.isArray(learner.enrollmentPeriods) || learner.enrollmentPeriods.length === 0) {
      issues.push(issue('MISSING_ENROLLMENT_HISTORY', `Student ID ${learnerRow.studentId} has no enrollment periods.`));
      continue;
    }

    const expectedDates = sortedCalendar.filter((date) => isEnrolledOn(learner, date));
    const expectedSet = new Set(expectedDates);
    const actualMap = new Map();
    for (const day of attendance.days) {
      if (!day || !validDate(day.date, month)) {
        issues.push(issue('INVALID_LEARNER_ATTENDANCE_DATE', `Student ID ${learnerRow.studentId} has an invalid attendance date.`));
        continue;
      }
      if (actualMap.has(day.date)) {
        issues.push(issue('DUPLICATE_LEARNER_ATTENDANCE_DATE', `Student ID ${learnerRow.studentId} has duplicate attendance for ${day.date}.`));
        continue;
      }
      if (!expectedSet.has(day.date)) {
        issues.push(issue('ATTENDANCE_OUTSIDE_EXPECTED_DAYS', `Student ID ${learnerRow.studentId} has attendance for ${day.date} outside confirmed class/enrollment dates.`));
        continue;
      }
      actualMap.set(day.date, day);
    }

    for (const date of expectedDates) {
      if (!actualMap.has(date)) {
        issues.push(issue('MISSING_EXPECTED_ATTENDANCE', `Student ID ${learnerRow.studentId} has no finalized result for expected class date ${date}.`));
      }
    }
    if (expectedDates.length !== actualMap.size) continue;

    const monthly = expectedDates.length === 0
      ? zeroLearnerResult()
      : calculateLearnerMonth({
        month,
        classDays: expectedDates.map((date) => actualMap.get(date)),
      });
    if (monthly.state !== 'READY') {
      for (const detail of monthly.issues) {
        issues.push(issue(detail.code, `Student ID ${learnerRow.studentId}: ${detail.message}`));
      }
      continue;
    }

    // calculateLearnerMonth sees only expected dates. Re-evaluate continuity
    // against the full class calendar so a deactivation/re-entry gap resets a
    // streak instead of joining absences from separate enrollment periods.
    const maxConsecutiveAbsenceDays = consecutiveAbsenceRun(learner, monthly.days, sortedCalendar);
    monthly.summary.maxConsecutiveAbsenceDays = maxConsecutiveAbsenceDays;
    monthly.summary.hasFiveConsecutiveAbsences = maxConsecutiveAbsenceDays >= 5;

    for (const { date, result } of monthly.days) {
      const totals = daily.get(date);
      if (!totals) {
        issues.push(issue('DAILY_TOTAL_DATE_MISSING', `No section total bucket exists for ${date}.`));
        continue;
      }
      totals.enrolled += 1;
      if (learnerRow.sex === 'M') totals.maleEnrolled += 1;
      else totals.femaleEnrolled += 1;
      if (result.countsAsPresentDay) {
        totals.present += 1;
        if (learnerRow.sex === 'M') totals.malePresent += 1;
        else totals.femalePresent += 1;
      } else if (result.countsAsAbsentDay) {
        totals.absent += 1;
        if (learnerRow.sex === 'M') totals.maleAbsent += 1;
        else totals.femaleAbsent += 1;
      }
    }
    learnerResults.push({
      studentId: learnerRow.studentId,
      sex: learnerRow.sex,
      fullName: learnerRow.fullName,
      monthly,
    });
  }

  if (attendanceMap.size !== rowPlan.rows.length) {
    const absentEntryCount = rowPlan.rows.filter((row) => !attendanceMap.has(row.studentId)).length;
    if (absentEntryCount === 0) {
      issues.push(issue('ATTENDANCE_SET_CARDINALITY_MISMATCH', 'The attendance entry count does not match the month-overlap roster.'));
    }
  }
  if (issues.length) return { state: 'BLOCKED', learners: [], dailyTotals: [], summary: null, issues };

  const totals = {
    male: { presentLearnerDays: 0, absentLearnerDays: 0 },
    female: { presentLearnerDays: 0, absentLearnerDays: 0 },
    total: { presentLearnerDays: 0, absentLearnerDays: 0 },
  };
  const fiveConsecutiveAbsenceLearners = { male: 0, female: 0, total: 0 };
  for (const learner of learnerResults) {
    if (learner.monthly.summary.hasFiveConsecutiveAbsences) {
      fiveConsecutiveAbsenceLearners[learner.sex === 'M' ? 'male' : 'female'] += 1;
      fiveConsecutiveAbsenceLearners.total += 1;
    }
  }
  for (const day of daily.values()) {
    totals.male.presentLearnerDays += day.malePresent;
    totals.male.absentLearnerDays += day.maleAbsent;
    totals.female.presentLearnerDays += day.femalePresent;
    totals.female.absentLearnerDays += day.femaleAbsent;
    totals.total.presentLearnerDays += day.present;
    totals.total.absentLearnerDays += day.absent;
  }

  return {
    state: 'READY',
    learners: learnerResults,
    dailyTotals: [...daily.values()],
    summary: {
      monthRosterCount: rowPlan.counts.total,
      monthRosterMale: rowPlan.counts.male,
      monthRosterFemale: rowPlan.counts.female,
      monthEndRegistration,
      fiveConsecutiveAbsenceLearners,
      confirmedClassDates: sortedCalendar.length,
      learnerDayTotals: totals,
    },
    issues: [],
  };
}
