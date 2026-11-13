import { calculateSnapshotMonth } from './snapshot_adapter.mjs';
import { validateSnapshot } from './snapshot_schema.mjs';

function diagnostic(code, message, details = {}) {
  return { code, message, severity: 'BLOCKER', ...details };
}

/** Build a record-oriented diagnostic view for a future dashboard preview. */
export function buildSnapshotPreview(snapshot, { expectedMonth } = {}) {
  const validation = validateSnapshot(snapshot, { expectedMonth });
  const calculation = calculateSnapshotMonth(snapshot, { expectedMonth });
  const diagnostics = [
    ...validation.issues.map((item) => ({ ...item, severity: 'ERROR' })),
    ...validation.blockers.map((item) => ({ ...item, severity: 'BLOCKER' })),
    ...calculation.issues.map((item) => ({ ...item, severity: 'BLOCKER' })),
  ];

  // Promote common record-level findings into structured learner/date/session
  // details. The validator remains the source for the exact code and message.
  const attendance = Array.isArray(snapshot?.attendance) ? new Map(snapshot.attendance
    .filter((entry) => entry && Number.isSafeInteger(entry.studentId))
    .map((entry) => [entry.studentId, entry])) : new Map();
  const learners = Array.isArray(snapshot?.learners) ? snapshot.learners : [];
  const learnerNames = new Map(learners.filter((learner) => learner && Number.isSafeInteger(learner.studentId))
    .map((learner) => {
      const identity = learner.identity ?? {};
      const given = [identity.firstName, identity.middleName, identity.extensionName]
        .filter((part) => typeof part === 'string' && part.trim())
        .map((part) => part.trim()).join(' ');
      const family = typeof identity.lastName === 'string' ? identity.lastName.trim() : '';
      return [learner.studentId, family && given ? `${family}, ${given}` : family || given || null];
    }));
  const calendarDays = Array.isArray(snapshot?.calendar?.days) ? snapshot.calendar.days : [];
  const diagnosticRecords = [];
  for (const learner of learners) {
    if (!learner || !Number.isSafeInteger(learner.studentId)) continue;
    const entry = attendance.get(learner.studentId);
    const byDate = new Map((Array.isArray(entry?.days) ? entry.days : [])
      .filter((day) => day && typeof day.date === 'string')
      .map((day) => [day.date, day]));
    for (const calendarDay of calendarDays) {
      if (calendarDay?.kind !== 'CLASS_DAY' || !Array.isArray(learner.enrollmentPeriods)) continue;
      const expected = learner.enrollmentPeriods.some((period) => (
        period && period.startInclusive <= calendarDay.date
        && (period.endExclusive === null || period.endExclusive === undefined || calendarDay.date < period.endExclusive)
      ));
      if (!expected) continue;
      const day = byDate.get(calendarDay.date);
      if (!day) {
        diagnosticRecords.push(diagnostic('MISSING_EXPECTED_ATTENDANCE', `Student ID ${learner.studentId} has no attendance pair for ${calendarDay.date}.`, {
          studentId: learner.studentId, learnerName: learnerNames.get(learner.studentId),
          date: calendarDay.date, session: 'AM+PM',
        }));
        continue;
      }
      for (const session of ['AM', 'PM']) {
        const result = day[session.toLowerCase()];
        if (!result) {
          diagnosticRecords.push(diagnostic('MISSING_SESSION_RESULT', `Student ID ${learner.studentId} has no ${session} result for ${calendarDay.date}.`, {
            studentId: learner.studentId, learnerName: learnerNames.get(learner.studentId),
            date: calendarDay.date, session,
          }));
        } else if (result.status === 'UNRESOLVED' || result.status === 'CLEAR') {
          diagnosticRecords.push(diagnostic('UNRESOLVED_SESSION', `Student ID ${learner.studentId} has ${result.status} in ${session} on ${calendarDay.date}.`, {
            studentId: learner.studentId, learnerName: learnerNames.get(learner.studentId),
            date: calendarDay.date, session,
          }));
        }
      }
    }
  }
  const coverageRows = new Map();
  for (const coverage of Array.isArray(snapshot?.coverage) ? snapshot.coverage : []) {
    if (!coverage || typeof coverage.date !== 'string' || typeof coverage.session !== 'string') continue;
    const key = `${coverage.date}|${coverage.session}`;
    coverageRows.set(key, coverage);
    if (coverage.gapAffected === true) {
      diagnosticRecords.push(diagnostic('COVERAGE_GAP', `A coverage gap affects ${coverage.date} ${coverage.session}.`, {
        date: coverage.date, session: coverage.session,
      }));
    } else if (coverage.sessionClosed !== true || coverage.coverageComplete !== true || coverage.logHealthy !== true) {
      diagnosticRecords.push(diagnostic('COVERAGE_NOT_FINAL', `Coverage for ${coverage.date} ${coverage.session} is not closed and reliable.`, {
        date: coverage.date, session: coverage.session,
      }));
    }
  }
  for (const day of calendarDays) {
    if (day?.kind !== 'CLASS_DAY' || !Array.isArray(day.scheduledSessions)) continue;
    for (const session of day.scheduledSessions) {
      if (!coverageRows.has(`${day.date}|${session}`)) {
        diagnosticRecords.push(diagnostic('COVERAGE_ROW_MISSING', `No coverage record exists for ${day.date} ${session}.`, {
          date: day.date, session,
        }));
      }
    }
    if (day.scheduledSessions.length === 1) {
      const unscheduled = day.scheduledSessions[0] === 'AM' ? 'PM' : 'AM';
      diagnosticRecords.push(diagnostic('PARTIAL_DAY_POLICY_OPEN', `The daily-cell treatment for ${day.date} (${unscheduled} not scheduled) is awaiting teacher confirmation.`, {
        date: day.date, session: unscheduled,
      }));
    }
  }

  // Prefer the structured records over generic validator/calculator copies.
  // These codes are fully enumerated above with learner/date/session context;
  // keeping the unstructured copies first made callers receive diagnostics
  // without the details the preview specifically exists to provide.
  const structuredCodes = new Set(diagnosticRecords.map((item) => item.code));
  const prioritizedDiagnostics = [
    ...diagnosticRecords,
    ...diagnostics.filter((item) => !structuredCodes.has(item.code)),
  ];
  const seen = new Set();
  const deduplicated = prioritizedDiagnostics.filter((item) => {
    const key = `${item.code}|${item.studentId ?? ''}|${item.date ?? ''}|${item.session ?? ''}|${item.path ?? ''}|${item.message}`;
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });

  const finalExportReady = false; // No approved/normalized XLSX template writer exists yet.
  const previewReady = validation.state === 'VALID' && calculation.state === 'PREVIEW_READY';
  return {
    state: previewReady ? 'PREVIEW_READY' : 'BLOCKED',
    calculationState: calculation.state,
    snapshotId: validation.snapshotId ?? null,
    month: validation.month ?? null,
    monthRosterCount: validation.monthRoster.length,
    diagnostics: deduplicated,
    warnings: validation.warnings,
    finalExportReady,
    workbookWriterAvailable: false,
    report: calculation.report,
  };
}
