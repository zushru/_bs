const SESSION_STATUSES = new Set([
  'PRESENT',
  'LATE',
  'EXCUSED',
  'ABSENT_CONFIRMED',
  'CLEAR',
  'UNRESOLVED',
  'NOT_SCHEDULED',
]);

const ATTENDED = new Set(['PRESENT', 'LATE']);
const ABSENCE_EVIDENCE = [
  'sessionClosed',
  'coverageComplete',
  'studentExpected',
  'logHealthy',
  'snapshotConsistent',
];

function issue(code, message) {
  return { code, message };
}

function unresolved(code, message) {
  return {
    state: 'UNRESOLVED',
    code: null,
    countsAsPresentDay: null,
    countsAsAbsentDay: null,
    issues: [issue(code, message)],
  };
}

function reportStatus(status) {
  return status === 'LATE' ? 'PRESENT' : status;
}

function normalizeSession(value, sessionName) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return { error: issue('MISSING_SESSION_RESULT', `${sessionName} result is missing.`) };
  }

  const { status } = value;
  if (typeof status !== 'string' || !SESSION_STATUSES.has(status)) {
    return { error: issue('INVALID_SESSION_STATUS', `${sessionName} has an unsupported status.`) };
  }

  if (status === 'CLEAR') {
    return { error: issue('CLEAR_NOT_FINAL', `${sessionName} was cleared; it has no final attendance result.`) };
  }
  if (status === 'UNRESOLVED') {
    return { error: issue('SESSION_UNRESOLVED', `${sessionName} attendance is unresolved.`) };
  }
  if (status === 'NOT_SCHEDULED' && value.calendarConfirmed !== true) {
    return { error: issue('CALENDAR_NOT_CONFIRMED', `${sessionName} non-class status is not confirmed by the calendar.`) };
  }
  if (['PRESENT', 'LATE', 'EXCUSED'].includes(status) && value.eventCommitted !== true) {
    return { error: issue('SESSION_EVENT_NOT_COMMITTED', `${sessionName} result has no confirmed committed event.`) };
  }
  if (status === 'ABSENT_CONFIRMED') {
    const evidence = value.absenceEvidence;
    if (!evidence || ABSENCE_EVIDENCE.some((key) => evidence[key] !== true)) {
      return { error: issue('ABSENCE_EVIDENCE_INCOMPLETE', `${sessionName} absence lacks closed-session, complete-coverage, expected-student, healthy-log, or consistent-snapshot evidence.`) };
    }
  }

  return { status };
}

function dailyResult(code, amStatus, pmStatus) {
  const countsAsAbsentDay = ['X', 'AM_ONLY', 'PM_ONLY'].includes(code);
  const countsAsPresentDay = ['BLANK', 'E'].includes(code);
  return {
    state: 'READY',
    code,
    amStatus: reportStatus(amStatus),
    pmStatus: reportStatus(pmStatus),
    countsAsPresentDay,
    countsAsAbsentDay,
    issues: [],
  };
}

/**
 * Resolve one learner's AM/PM session pair into the selected local SF2 mark.
 * The visual adapter later maps AM_ONLY to upper-left shading and PM_ONLY to
 * lower-right shading. This function never writes to or changes device records.
 */
export function resolveDailyCell(amInput, pmInput) {
  const am = normalizeSession(amInput, 'AM');
  const pm = normalizeSession(pmInput, 'PM');
  const errors = [am.error, pm.error].filter(Boolean);
  if (errors.length) {
    return {
      state: 'UNRESOLVED',
      code: null,
      amStatus: reportStatus(am.status ?? null),
      pmStatus: reportStatus(pm.status ?? null),
      countsAsPresentDay: null,
      countsAsAbsentDay: null,
      issues: errors,
    };
  }

  if (am.status === 'NOT_SCHEDULED' && pm.status === 'NOT_SCHEDULED') {
    return {
      state: 'EXCLUDED',
      code: null,
      amStatus: reportStatus(am.status),
      pmStatus: reportStatus(pm.status),
      countsAsPresentDay: null,
      countsAsAbsentDay: null,
      issues: [],
    };
  }

  if (am.status === 'NOT_SCHEDULED' || pm.status === 'NOT_SCHEDULED') {
    return unresolved(
      'PARTIAL_DAY_SCHEDULE_POLICY_REQUIRED',
      'One session was not scheduled. The teacher-approved daily-cell rule for a one-session class day is still pending.',
    );
  }

  const amAttended = ATTENDED.has(am.status);
  const pmAttended = ATTENDED.has(pm.status);
  const amPresentOrExcused = amAttended || am.status === 'EXCUSED';
  const pmPresentOrExcused = pmAttended || pm.status === 'EXCUSED';

  // A full day without an absence is Present unless both sessions are
  // Excused: both Excused maps to E; any attended+Excused mix maps to BLANK.
  // A single attended/excused session paired with a confirmed absence is a
  // diagonal mark, and the user's monthly-total rule counts that day absent.
  // Late is treated as attended only in this report projection; the source
  // event log retains its original ST_LATE status.
  if (amPresentOrExcused && pmPresentOrExcused) {
    const bothExcused = am.status === 'EXCUSED' && pm.status === 'EXCUSED';
    return dailyResult(bothExcused ? 'E' : 'BLANK', am.status, pm.status);
  }
  if (amPresentOrExcused && pm.status === 'ABSENT_CONFIRMED') {
    return dailyResult('AM_ONLY', am.status, pm.status);
  }
  if (am.status === 'ABSENT_CONFIRMED' && pmPresentOrExcused) {
    return dailyResult('PM_ONLY', am.status, pm.status);
  }
  if (am.status === 'ABSENT_CONFIRMED' && pm.status === 'ABSENT_CONFIRMED') {
    return dailyResult('X', am.status, pm.status);
  }
  return unresolved('UNSUPPORTED_SESSION_PAIR', 'The AM/PM combination has no approved mapping.');
}

function isValidDate(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
  const [year, month, day] = value.split('-').map(Number);
  const date = new Date(Date.UTC(year, month - 1, day));
  return date.getUTCFullYear() === year
    && date.getUTCMonth() + 1 === month
    && date.getUTCDate() === day;
}

function isValidMonth(value) {
  return typeof value === 'string' && /^\d{4}-(0[1-9]|1[0-2])$/.test(value);
}

/**
 * Calculate one learner's monthly totals from a complete list of confirmed
 * class days. `classDays` must include every expected class day in the month;
 * omitted dates cannot be detected by this function and must be prevented by
 * the snapshot/schema validator before calling it.
 */
export function calculateLearnerMonth({ month, classDays }) {
  const issues = [];
  if (!isValidMonth(month)) {
    issues.push(issue('INVALID_MONTH', 'Month must use YYYY-MM and be a valid calendar month.'));
  }
  if (!Array.isArray(classDays) || classDays.length === 0) {
    issues.push(issue('NO_CLASS_DAYS', 'A monthly calculation requires a non-empty confirmed class-day list.'));
  }
  if (issues.length) return { state: 'BLOCKED', summary: null, days: [], issues };

  const seen = new Set();
  const ordered = [...classDays].sort((a, b) => String(a?.date ?? '').localeCompare(String(b?.date ?? '')));
  for (const day of ordered) {
    if (!day || !isValidDate(day.date) || !day.date.startsWith(`${month}-`)) {
      issues.push(issue('INVALID_CLASS_DATE', `Class date must be a valid date inside ${month}.`));
      continue;
    }
    if (seen.has(day.date)) {
      issues.push(issue('DUPLICATE_CLASS_DATE', `Class date ${day.date} appears more than once.`));
      continue;
    }
    seen.add(day.date);
  }
  if (issues.length) return { state: 'BLOCKED', summary: null, days: [], issues };

  const days = ordered.map((day) => ({
    date: day.date,
    result: resolveDailyCell(day.am, day.pm),
  }));

  days.forEach(({ date, result }) => {
    if (result.state === 'EXCLUDED') {
      issues.push(issue('CLASS_DAY_HAS_NO_SESSIONS', `${date} is listed as a class day but both sessions are marked not scheduled.`));
    } else if (result.state !== 'READY') {
      for (const detail of result.issues) {
        issues.push(issue(detail.code, `${date}: ${detail.message}`));
      }
    }
  });
  if (issues.length) return { state: 'BLOCKED', summary: null, days, issues };

  let presentDays = 0;
  let absentDays = 0;
  let excusedDays = 0;
  let morningOnlyDays = 0;
  let afternoonOnlyDays = 0;
  let currentAbsenceRun = 0;
  let maxConsecutiveAbsenceDays = 0;

  for (const { result } of days) {
    if (result.countsAsPresentDay) presentDays += 1;
    if (result.countsAsAbsentDay) absentDays += 1;
    if (result.code === 'E') excusedDays += 1;
    if (result.code === 'AM_ONLY') morningOnlyDays += 1;
    if (result.code === 'PM_ONLY') afternoonOnlyDays += 1;
    if (result.countsAsAbsentDay) {
      currentAbsenceRun += 1;
      maxConsecutiveAbsenceDays = Math.max(maxConsecutiveAbsenceDays, currentAbsenceRun);
    } else {
      currentAbsenceRun = 0;
    }
  }

  return {
    state: 'READY',
    days,
    issues: [],
    summary: {
      classDays: days.length,
      presentDays,
      absentDays,
      excusedDays,
      morningOnlyDays,
      afternoonOnlyDays,
      maxConsecutiveAbsenceDays,
      hasFiveConsecutiveAbsences: maxConsecutiveAbsenceDays >= 5,
    },
  };
}

function nextMonthStart(month) {
  const [year, monthNumber] = month.split('-').map(Number);
  if (monthNumber === 12) return `${year + 1}-01-01`;
  return `${year}-${String(monthNumber + 1).padStart(2, '0')}-01`;
}

/**
 * Select the report-month roster by section-enrollment overlap.
 * Enrollment periods use the proposed half-open date interval
 * [startInclusive, endExclusive). This is a browser-domain representation;
 * it is not a change to the device's operational RosterStore.
 */
export function selectMonthlyRoster({ month, learners }) {
  const issues = [];
  if (!isValidMonth(month)) {
    issues.push(issue('INVALID_MONTH', 'Month must use YYYY-MM and be a valid calendar month.'));
  }
  if (!Array.isArray(learners)) {
    issues.push(issue('INVALID_ROSTER', 'Learners must be an array from a complete roster snapshot.'));
  }
  if (issues.length) return { state: 'BLOCKED', total: 0, male: [], female: [], issues };

  const monthStart = `${month}-01`;
  const monthEndExclusive = nextMonthStart(month);
  const seenIds = new Set();
  const male = [];
  const female = [];

  for (const learner of learners) {
    if (!learner || !Number.isSafeInteger(learner.studentId) || learner.studentId <= 0) {
      issues.push(issue('INVALID_STUDENT_ID', 'Every learner must have a positive stable integer studentId.'));
      continue;
    }
    if (seenIds.has(learner.studentId)) {
      issues.push(issue('DUPLICATE_STUDENT_ID', `Student ID ${learner.studentId} appears more than once.`));
      continue;
    }
    seenIds.add(learner.studentId);

    if (!Array.isArray(learner.enrollmentPeriods) || learner.enrollmentPeriods.length === 0) {
      issues.push(issue('MISSING_ENROLLMENT_HISTORY', `Student ID ${learner.studentId} has no section-enrollment history.`));
      continue;
    }

    const periods = [...learner.enrollmentPeriods].sort((a, b) => String(a?.startInclusive ?? '').localeCompare(String(b?.startInclusive ?? '')));
    let invalidPeriod = false;
    for (let index = 0; index < periods.length; index += 1) {
      const period = periods[index];
      if (!period || !isValidDate(period.startInclusive)
        || (period.endExclusive !== null && period.endExclusive !== undefined && !isValidDate(period.endExclusive))
        || (period.endExclusive && period.endExclusive <= period.startInclusive)) {
        issues.push(issue('INVALID_ENROLLMENT_PERIOD', `Student ID ${learner.studentId} has an invalid enrollment interval.`));
        invalidPeriod = true;
        break;
      }
      if (index > 0) {
        const previous = periods[index - 1];
        if (!previous.endExclusive || period.startInclusive < previous.endExclusive) {
          issues.push(issue('OVERLAPPING_ENROLLMENT_PERIODS', `Student ID ${learner.studentId} has overlapping section-enrollment intervals.`));
          invalidPeriod = true;
          break;
        }
      }
    }
    if (invalidPeriod) continue;

    const overlapsMonth = periods.some((period) => (
      period.startInclusive < monthEndExclusive
      && (period.endExclusive === null || period.endExclusive === undefined || period.endExclusive > monthStart)
    ));
    if (!overlapsMonth) continue;

    if (typeof learner.fullName !== 'string' || learner.fullName.trim() === '') {
      issues.push(issue('MISSING_LEARNER_NAME', `Student ID ${learner.studentId} has no reportable name.`));
      continue;
    }
    if (learner.sex !== 'M' && learner.sex !== 'F') {
      issues.push(issue('INVALID_LEARNER_SEX', `Student ID ${learner.studentId} has no valid M/F report grouping.`));
      continue;
    }

    // Keep incoming roster order; deterministic class-list ordering remains a
    // separate school-specific decision for the future template adapter.
    (learner.sex === 'M' ? male : female).push({ ...learner });
  }

  if (issues.length) return { state: 'BLOCKED', total: 0, male: [], female: [], issues };
  return {
    state: 'READY',
    total: male.length + female.length,
    male,
    female,
    sourceOrderPreserved: true,
    issues: [],
  };
}
