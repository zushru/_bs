import { calculateSectionMonth } from './section_model.mjs';
import { validateSnapshot } from './snapshot_schema.mjs';

function fullName(identity) {
  const rest = [identity.firstName, identity.middleName, identity.extensionName]
    .filter((part) => typeof part === 'string' && part.trim())
    .map((part) => part.trim())
    .join(' ');
  return `${identity.lastName.trim()}, ${rest}`;
}


function toCalculatorSession(result, { snapshot, learner, date, session, coverageByKey }) {
  if (!result) return undefined;
  const output = { status: result.status };
  if (result.status === 'PRESENT' || result.status === 'LATE' || result.status === 'EXCUSED' || result.status === 'CLEAR') {
    output.eventCommitted = Boolean(result.sourceEvent);
  }
  if (result.status === 'NOT_SCHEDULED') output.calendarConfirmed = result.calendarConfirmed === true;
  if (result.status === 'ABSENT_CONFIRMED') {
    const coverage = coverageByKey.get(`${date}|${session}`);
    output.absenceEvidence = {
      sessionClosed: coverage?.sessionClosed === true,
      coverageComplete: coverage?.coverageComplete === true && coverage?.gapAffected === false,
      studentExpected: Array.isArray(coverage?.expectedStudentIds)
        ? coverage.expectedStudentIds.includes(learner.studentId)
        : false,
      logHealthy: coverage?.logHealthy === true,
      snapshotConsistent: snapshot.completeness.attendanceSnapshotConsistent === true,
    };
  }
  return output;
}

/**
 * Translate the proposed, revision-bound snapshot envelope into the existing
 * pure section model. This is an in-browser adapter only; no HTTP or firmware
 * integration is implied.
 */
export function calculateSnapshotMonth(snapshot, { expectedMonth } = {}) {
  const validation = validateSnapshot(snapshot, { expectedMonth });
  if (validation.state === 'INVALID') {
    return {
      state: 'BLOCKED',
      authoritativeExportReady: false,
      authority: { state: 'NOT_ESTABLISHED', code: 'AUTHENTICATED_IDENTITY_REVIEW_MISSING' },
      validation,
      report: null,
      workbookGenerated: false,
      workbookWriterAvailable: false,
      issues: validation.issues,
      warnings: validation.warnings,
    };
  }

  const male = validation.monthRoster.filter((learner) => learner.sex === 'M');
  const female = validation.monthRoster.filter((learner) => learner.sex === 'F');
  const roster = {
    state: 'READY',
    total: validation.monthRoster.length,
    male: male.map((learner) => ({
      studentId: learner.studentId,
      sex: learner.sex,
      fullName: fullName(learner.identity),
      enrollmentPeriods: learner.enrollmentPeriods,
    })),
    female: female.map((learner) => ({
      studentId: learner.studentId,
      sex: learner.sex,
      fullName: fullName(learner.identity),
      enrollmentPeriods: learner.enrollmentPeriods,
    })),
  };

  const attendanceById = new Map(snapshot.attendance.map((entry) => [entry.studentId, entry]));
  const attendanceByStudent = validation.monthRoster.map((learner) => {
    const entry = attendanceById.get(learner.studentId);
    return {
      studentId: learner.studentId,
      days: Array.isArray(entry?.days) ? entry.days.map((day) => ({
        date: day.date,
        am: toCalculatorSession(day.am, {
          snapshot, learner, date: day.date, session: 'AM', coverageByKey: validation.coverageByKey,
        }),
        pm: toCalculatorSession(day.pm, {
          snapshot, learner, date: day.date, session: 'PM', coverageByKey: validation.coverageByKey,
        }),
      })) : [],
    };
  });

  const report = calculateSectionMonth({
    month: snapshot.month,
    roster,
    calendarDays: validation.classDays,
    rosterHistoryComplete: snapshot.completeness.rosterHistoryComplete === true,
    calendarComplete: snapshot.completeness.calendarComplete === true
      && snapshot.completeness.scheduleHistoryComplete === true,
    snapshotConsistent: snapshot.completeness.attendanceSnapshotConsistent === true,
    attendanceByStudent,
  });
  const state = validation.state === 'VALID' && report.state === 'READY' ? 'PREVIEW_READY' : 'BLOCKED';
  const mergedIssues = [...validation.blockers, ...report.issues];
  return {
    state,
    authoritativeExportReady: false,
    authority: { state: 'NOT_ESTABLISHED', code: 'AUTHENTICATED_IDENTITY_REVIEW_MISSING' },
    validation,
    report: state === 'PREVIEW_READY' ? report : null,
    workbookGenerated: false,
    workbookWriterAvailable: false,
    issues: mergedIssues,
    warnings: validation.warnings,
  };
}
