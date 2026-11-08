const CURRICULA = new Set(['REGULAR_SHS', 'STRENGTHENED_SHS']);
const CALENDAR_KINDS = new Set(['CLASS_DAY', 'NON_CLASS_DAY', 'OUTSIDE_TERM']);
const SESSIONS = new Set(['AM', 'PM']);
const SESSION_STATUSES = new Set([
  'PRESENT', 'LATE', 'EXCUSED', 'ABSENT_CONFIRMED', 'CLEAR', 'UNRESOLVED', 'NOT_SCHEDULED',
]);
const EVENT_TYPES = new Set(['SCAN', 'MARK']);
const MOVEMENT_TYPES = new Set(['ENROLLED', 'TRANSFER_IN', 'TRANSFER_OUT', 'SHIFTED', 'WITHDRAWN', 'DROPPED', 'NLS', 'OTHER']);
const MOVEMENT_DATE_MEANINGS = new Set(['FIRST_ENROLLED_DAY', 'LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY', 'SECTION_CHANGE_DAY', 'EVENT_DAY', 'UNCONFIRMED']);
const MOVEMENT_DATE_RULES = Object.freeze({
  ENROLLED: ['FIRST_ENROLLED_DAY'], TRANSFER_IN: ['FIRST_ENROLLED_DAY'],
  TRANSFER_OUT: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  WITHDRAWN: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  DROPPED: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  NLS: ['UNCONFIRMED'],
  SHIFTED: ['SECTION_CHANGE_DAY'], OTHER: ['EVENT_DAY'],
});
const MAX_EVENT_SEQ = 0xFFFFFFFF; // Event.seq is uint32_t in event_codec.h.
const MAX_CLOSE_PAYLOADS = Math.ceil(300 / 64); // MAX_ID=300; each EV_PAYLOAD stores 64 expected-ID bits.

function isObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function validMonth(value) {
  return typeof value === 'string' && /^\d{4}-(0[1-9]|1[0-2])$/.test(value);
}

export function isValidIsoDate(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
  const [year, month, day] = value.split('-').map(Number);
  const parsed = new Date(Date.UTC(year, month - 1, day));
  return parsed.getUTCFullYear() === year
    && parsed.getUTCMonth() + 1 === month
    && parsed.getUTCDate() === day;
}

function monthStart(month) {
  return `${month}-01`;
}

function nextMonthStart(month) {
  const [year, number] = month.split('-').map(Number);
  return number === 12 ? `${year + 1}-01-01` : `${year}-${String(number + 1).padStart(2, '0')}-01`;
}

function allMonthDates(month) {
  const start = new Date(`${month}-01T00:00:00Z`);
  const count = new Date(Date.UTC(start.getUTCFullYear(), start.getUTCMonth() + 1, 0)).getUTCDate();
  return Array.from({ length: count }, (_, index) => `${month}-${String(index + 1).padStart(2, '0')}`);
}

function overlapsMonth(period, month) {
  return period.startInclusive < nextMonthStart(month)
    && (period.endExclusive === null || period.endExclusive > monthStart(month));
}

function enrolledOn(learner, date) {
  return learner.enrollmentPeriods.some((period) => (
    period.startInclusive <= date
    && (period.endExclusive === null || date < period.endExclusive)
  ));
}

function addIssue(target, code, path, message, severity = 'ERROR') {
  target.push({ code, path, message, severity });
}

function nullableString(value) {
  return value === null || typeof value === 'string';
}

function isRevision(value) {
  return typeof value === 'string' && value.trim().length > 0 && value.length <= 128;
}

function validateRevision(snapshot, issues) {
  const revision = snapshot.sourceRevision;
  if (!isObject(revision)
    || !Number.isSafeInteger(revision.attendanceLastSeq) || revision.attendanceLastSeq < 0
    || revision.attendanceLastSeq > MAX_EVENT_SEQ
    || !isRevision(revision.rosterRevision)
    || !Number.isSafeInteger(revision.configGeneration) || revision.configGeneration < 0
    || !isRevision(revision.calendarRevision)
    || !isRevision(revision.reportingProfileRevision)
    || !isRevision(revision.schoolProfileRevision)
    || !isRevision(revision.learnerMetadataRevision)) {
    addIssue(issues, 'INVALID_SOURCE_REVISION', 'sourceRevision', 'All source revisions must be present and valid; attendanceLastSeq must fit the C++ uint32 event sequence and configGeneration must be a non-negative safe integer.');
    return false;
  }
  return true;
}

function validateProfile(snapshot, issues) {
  const profile = snapshot.reportingProfile;
  if (!isObject(profile)) {
    addIssue(issues, 'INVALID_REPORTING_PROFILE', 'reportingProfile', 'A reporting profile object is required.');
    return false;
  }
  const requiredText = ['profileId', 'revision', 'schoolYear', 'gradeLevel', 'sectionName'];
  for (const key of requiredText) {
    if (typeof profile[key] !== 'string' || profile[key].trim() === '') {
      addIssue(issues, 'INVALID_REPORTING_PROFILE_FIELD', `reportingProfile.${key}`, `${key} must be a non-empty string.`);
    }
  }
  if (!CURRICULA.has(profile.curriculum)) {
    addIssue(issues, 'INVALID_CURRICULUM', 'reportingProfile.curriculum', 'curriculum must be REGULAR_SHS or STRENGTHENED_SHS.');
  }
  if (profile.gradeLevel !== '11' && profile.gradeLevel !== '12') {
    addIssue(issues, 'INVALID_GRADE_LEVEL', 'reportingProfile.gradeLevel', 'Senior High School gradeLevel must be "11" or "12".');
  }
  const schoolYear = typeof profile.schoolYear === 'string' ? /^(\d{4})-(\d{4})$/.exec(profile.schoolYear) : null;
  if (!schoolYear || Number(schoolYear[2]) !== Number(schoolYear[1]) + 1) {
    addIssue(issues, 'INVALID_SCHOOL_YEAR', 'reportingProfile.schoolYear', 'schoolYear must use consecutive YYYY-YYYY years.');
  }
  for (const key of ['track', 'strand', 'course', 'semesterLabel']) {
    if (!nullableString(profile[key])) {
      addIssue(issues, 'INVALID_REPORTING_PROFILE_FIELD', `reportingProfile.${key}`, `${key} must be a string or null.`);
    }
  }
  if (!isValidIsoDate(profile.effectiveFrom)
    || !(profile.effectiveToExclusive === null || isValidIsoDate(profile.effectiveToExclusive))
    || (profile.effectiveToExclusive && profile.effectiveToExclusive <= profile.effectiveFrom)) {
    addIssue(issues, 'INVALID_PROFILE_EFFECTIVE_RANGE', 'reportingProfile', 'Profile effective dates must form a valid half-open date interval.');
  }
  if (snapshot.sourceRevision && profile.revision !== snapshot.sourceRevision.reportingProfileRevision) {
    addIssue(issues, 'PROFILE_REVISION_MISMATCH', 'reportingProfile.revision', 'Reporting profile revision must match sourceRevision.reportingProfileRevision.');
  }
  if (validMonth(snapshot.month) && isValidIsoDate(profile.effectiveFrom)) {
    const lastDate = allMonthDates(snapshot.month).at(-1);
    if (profile.effectiveFrom > lastDate
      || (profile.effectiveToExclusive && profile.effectiveToExclusive <= monthStart(snapshot.month))) {
      addIssue(issues, 'PROFILE_OUTSIDE_REPORT_MONTH', 'reportingProfile', 'The configured single-section profile does not overlap the reporting month.');
    }
  }
  if (profile.semesterLabel === null) {
    // The semester/term label is a known policy question, not a malformed snapshot.
  }
  return true;
}

function validateRosterRevision(snapshot, issues) {
  if (!isObject(snapshot.roster) || !isRevision(snapshot.roster.revision)) {
    addIssue(issues, 'INVALID_ROSTER_REVISION', 'roster.revision', 'A roster revision token is required.');
    return;
  }
  if (snapshot.sourceRevision && snapshot.roster.revision !== snapshot.sourceRevision.rosterRevision) {
    addIssue(issues, 'ROSTER_REVISION_MISMATCH', 'roster.revision', 'Roster revision must match sourceRevision.rosterRevision.');
  }
}

function validateLearnerMetadataRevision(snapshot, issues) {
  if (!isObject(snapshot.learnerMetadata) || !isRevision(snapshot.learnerMetadata.revision)) {
    addIssue(issues, 'INVALID_LEARNER_METADATA_REVISION', 'learnerMetadata.revision', 'A learner reporting-metadata revision token is required.');
    return;
  }
  if (snapshot.sourceRevision && snapshot.learnerMetadata.revision !== snapshot.sourceRevision.learnerMetadataRevision) {
    addIssue(issues, 'LEARNER_METADATA_REVISION_MISMATCH', 'learnerMetadata.revision', 'Learner metadata revision must match sourceRevision.learnerMetadataRevision.');
  }
}

function validateSchoolProfile(snapshot, issues, warnings) {
  const profile = snapshot.schoolProfile;
  if (!isObject(profile)) {
    addIssue(issues, 'INVALID_SCHOOL_PROFILE', 'schoolProfile', 'A school reporting profile object is required.');
    return false;
  }
  if (!isRevision(profile.revision)) {
    addIssue(issues, 'INVALID_SCHOOL_PROFILE_REVISION', 'schoolProfile.revision', 'schoolProfile.revision must be a non-empty revision token.');
  }
  if (snapshot.sourceRevision && profile.revision !== snapshot.sourceRevision.schoolProfileRevision) {
    addIssue(issues, 'SCHOOL_PROFILE_REVISION_MISMATCH', 'schoolProfile.revision', 'School profile revision must match sourceRevision.schoolProfileRevision.');
  }
  const fields = ['schoolName', 'schoolId', 'district', 'division', 'region', 'adviserName', 'schoolHeadName'];
  for (const key of fields) {
    if (!nullableString(profile[key])) {
      addIssue(issues, 'INVALID_SCHOOL_METADATA', `schoolProfile.${key}`, `${key} must be a string or null.`);
    } else if (profile[key] === null || profile[key].trim() === '') {
      addIssue(warnings, `MISSING_${key.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toUpperCase()}`, `schoolProfile.${key}`, `${key} has not been supplied.`, 'WARNING');
    }
  }
  return true;
}

function validateCalendar(snapshot, issues, blockers) {
  const calendar = snapshot.calendar;
  if (!isObject(calendar) || !Array.isArray(calendar.days)) {
    addIssue(issues, 'INVALID_CALENDAR', 'calendar', 'calendar must contain a revision and a complete days array.');
    return { byDate: new Map(), classDays: [] };
  }
  if (!isRevision(calendar.revision)) {
    addIssue(issues, 'INVALID_CALENDAR_REVISION', 'calendar.revision', 'calendar.revision must be a non-empty revision token.');
  }
  if (snapshot.sourceRevision && calendar.revision !== snapshot.sourceRevision.calendarRevision) {
    addIssue(issues, 'CALENDAR_REVISION_MISMATCH', 'calendar.revision', 'Calendar revision must match sourceRevision.calendarRevision.');
  }
  if (!Number.isSafeInteger(calendar.configGeneration) || calendar.configGeneration < 0
    || calendar.configGeneration !== snapshot.sourceRevision?.configGeneration) {
    addIssue(issues, 'CALENDAR_CONFIG_GENERATION_MISMATCH', 'calendar.configGeneration', 'Calendar must be derived from the same config generation recorded in sourceRevision.');
  }
  const expectedDates = validMonth(snapshot.month) ? new Set(allMonthDates(snapshot.month)) : new Set();
  const byDate = new Map();
  for (let index = 0; index < calendar.days.length; index += 1) {
    const day = calendar.days[index];
    const path = `calendar.days[${index}]`;
    if (!isObject(day) || !isValidIsoDate(day.date) || !expectedDates.has(day.date)) {
      addIssue(issues, 'INVALID_CALENDAR_DATE', `${path}.date`, 'Calendar dates must be valid dates inside the requested month.');
      continue;
    }
    if (byDate.has(day.date)) {
      addIssue(issues, 'DUPLICATE_CALENDAR_DATE', `${path}.date`, `Calendar date ${day.date} appears more than once.`);
      continue;
    }
    if (!CALENDAR_KINDS.has(day.kind)) {
      addIssue(issues, 'INVALID_CALENDAR_KIND', `${path}.kind`, 'Calendar kind must be CLASS_DAY, NON_CLASS_DAY or OUTSIDE_TERM.');
      continue;
    }
    if (!Array.isArray(day.scheduledSessions)) {
      addIssue(issues, 'INVALID_SCHEDULED_SESSIONS', `${path}.scheduledSessions`, 'scheduledSessions must be an array.');
      continue;
    }
    const sessions = day.scheduledSessions;
    if (sessions.some((session) => !SESSIONS.has(session)) || new Set(sessions).size !== sessions.length) {
      addIssue(issues, 'INVALID_SCHEDULED_SESSIONS', `${path}.scheduledSessions`, 'scheduledSessions may contain AM and/or PM once each.');
      continue;
    }
    if (day.kind === 'CLASS_DAY' && sessions.length === 0) {
      addIssue(issues, 'CLASS_DAY_WITHOUT_SESSIONS', path, 'A class day must schedule at least one session.');
    }
    if (day.kind !== 'CLASS_DAY' && sessions.length !== 0) {
      addIssue(issues, 'NON_CLASS_DAY_HAS_SESSIONS', path, 'A non-class/outside-term day cannot schedule attendance sessions.');
    }
    if (!nullableString(day.reasonCode) || !nullableString(day.reasonText)) {
      addIssue(issues, 'INVALID_CALENDAR_REASON', path, 'Calendar reason fields must be strings or null.');
    }
    if (day.kind !== 'CLASS_DAY' && (typeof day.reasonCode !== 'string' || day.reasonCode.trim() === '')) {
      addIssue(issues, 'MISSING_NON_CLASS_REASON', `${path}.reasonCode`, 'A non-class day must identify why attendance is not scheduled.');
    }
    if (day.kind === 'CLASS_DAY' && (day.reasonCode !== null || day.reasonText !== null)) {
      addIssue(issues, 'CLASS_DAY_HAS_NON_CLASS_REASON', path, 'A class day must not carry a non-class reason.');
    }
    byDate.set(day.date, { ...day, scheduledSessions: [...sessions] });
  }
  for (const date of expectedDates) {
    if (!byDate.has(date)) addIssue(issues, 'CALENDAR_DAY_MISSING', 'calendar.days', `The snapshot omits ${date}; every calendar date in the month must be represented.`);
  }
  if (byDate.size !== expectedDates.size) {
    // More than one distinct row per expected date or an invalid row was already reported above.
  }
  if (!Array.isArray(calendar.days) || calendar.days.length !== expectedDates.size) {
    // Preserve the precise missing/extra diagnostics above; this is a secondary guard.
  }
  const classDays = [...byDate.values()].filter((day) => day.kind === 'CLASS_DAY').map((day) => day.date).sort();
  const profile = snapshot.reportingProfile;
  for (const date of classDays) {
    if (profile && isValidIsoDate(profile.effectiveFrom)
      && (date < profile.effectiveFrom
        || (isValidIsoDate(profile.effectiveToExclusive) && date >= profile.effectiveToExclusive))) {
      addIssue(issues, 'CLASS_DAY_OUTSIDE_PROFILE_RANGE', `calendar.days[date=${date}]`, 'A class day falls outside the effective range of the configured section/curriculum profile.');
    }
  }
  for (const day of byDate.values()) {
    if (day.kind === 'CLASS_DAY' && day.scheduledSessions.length === 1) {
      addIssue(blockers, 'PARTIAL_DAY_POLICY_OPEN', `calendar.days[date=${day.date}]`, `Only ${day.scheduledSessions[0]} is scheduled on ${day.date}; the teacher-approved one-session cell mapping is pending.`);
    }
  }
  if (snapshot.completeness?.calendarComplete !== true) {
    addIssue(blockers, 'CALENDAR_INCOMPLETE', 'completeness.calendarComplete', 'The device has not certified the entire reporting-month calendar.');
  }
  if (snapshot.completeness?.scheduleHistoryComplete !== true) {
    addIssue(blockers, 'SCHEDULE_HISTORY_INCOMPLETE', 'completeness.scheduleHistoryComplete', 'Historical timetable applicability is not confirmed for the entire reporting month.');
  }
  return { byDate, classDays };
}

function validateLearners(snapshot, issues, warnings) {
  if (!Array.isArray(snapshot.learners)) {
    addIssue(issues, 'INVALID_LEARNER_SET', 'learners', 'learners must be the complete month-overlap roster.');
    return [];
  }
  const seen = new Set();
  const seenLrn = new Map();
  const selected = [];
  for (let index = 0; index < snapshot.learners.length; index += 1) {
    const learner = snapshot.learners[index];
    const path = `learners[${index}]`;
    if (!isObject(learner) || !Number.isSafeInteger(learner.studentId)
      || learner.studentId < 1 || learner.studentId > 300) {
      addIssue(issues, 'INVALID_STUDENT_ID', `${path}.studentId`, 'studentId must be a never-reused device ID from 1 through 300.');
      continue;
    }
    if (seen.has(learner.studentId)) {
      addIssue(issues, 'DUPLICATE_STUDENT_ID', `${path}.studentId`, `Student ID ${learner.studentId} appears more than once.`);
      continue;
    }
    seen.add(learner.studentId);
    if (learner.sex !== 'M' && learner.sex !== 'F') {
      addIssue(issues, 'INVALID_LEARNER_SEX', `${path}.sex`, 'sex must be M or F for the required form subtotals.');
    }
    if (!isObject(learner.identity)) {
      addIssue(issues, 'INVALID_LEARNER_IDENTITY', `${path}.identity`, 'A reporting identity object is required.');
    } else {
      for (const key of ['firstName', 'lastName']) {
        if (typeof learner.identity[key] !== 'string' || learner.identity[key].trim() === '') {
          addIssue(issues, 'MISSING_LEARNER_NAME_COMPONENT', `${path}.identity.${key}`, `${key} is required for a reportable learner name.`);
        }
      }
      for (const key of ['middleName', 'extensionName', 'lrn']) {
        if (!nullableString(learner.identity[key])) {
          addIssue(issues, 'INVALID_LEARNER_IDENTITY_FIELD', `${path}.identity.${key}`, `${key} must be a string or null.`);
        }
      }
      if (learner.identity.lrn !== null && learner.identity.lrn !== undefined
        && !/^\d{12}$/.test(learner.identity.lrn)) {
        addIssue(issues, 'INVALID_LRN', `${path}.identity.lrn`, 'LRN must contain exactly 12 digits when supplied.');
      }
      if (!learner.identity.lrn) {
        addIssue(warnings, 'MISSING_LRN', `${path}.identity.lrn`, `Student ID ${learner.studentId} has no LRN; requiredness must be confirmed against the accepted form.`, 'WARNING');
      } else if (/^\d{12}$/.test(learner.identity.lrn)) {
        if (seenLrn.has(learner.identity.lrn)) {
          addIssue(issues, 'DUPLICATE_LRN', `${path}.identity.lrn`, `LRN is already assigned to student ID ${seenLrn.get(learner.identity.lrn)}.`);
        } else {
          seenLrn.set(learner.identity.lrn, learner.studentId);
        }
      }
    }
    if (!Array.isArray(learner.enrollmentPeriods) || learner.enrollmentPeriods.length === 0) {
      addIssue(issues, 'MISSING_ENROLLMENT_HISTORY', `${path}.enrollmentPeriods`, `Student ID ${learner.studentId} has no enrollment periods.`);
      continue;
    }
    const periods = [...learner.enrollmentPeriods].sort((a, b) => String(a?.startInclusive ?? '').localeCompare(String(b?.startInclusive ?? '')));
    let previous = null;
    let hasOverlap = false;
    let periodsValid = true;
    for (let periodIndex = 0; periodIndex < periods.length; periodIndex += 1) {
      const period = periods[periodIndex];
      const periodPath = `${path}.enrollmentPeriods[${periodIndex}]`;
      if (!isObject(period) || !isValidIsoDate(period.startInclusive)
        || !(period.endExclusive === null || isValidIsoDate(period.endExclusive))
        || (period.endExclusive && period.endExclusive <= period.startInclusive)) {
        addIssue(issues, 'INVALID_ENROLLMENT_PERIOD', periodPath, 'Enrollment must be a valid half-open [startInclusive, endExclusive) date interval.');
        periodsValid = false;
        continue;
      }
      if (period.profileId !== snapshot.reportingProfile?.profileId) {
        addIssue(issues, 'SECTION_PROFILE_MISMATCH', `${periodPath}.profileId`, 'Enrollment period must reference the configured single-section profile.');
      }
      if (previous && (previous.endExclusive === null || period.startInclusive < previous.endExclusive)) {
        addIssue(issues, 'OVERLAPPING_ENROLLMENT_PERIODS', periodPath, `Student ID ${learner.studentId} has overlapping enrollment intervals.`);
        periodsValid = false;
      }
      if (validMonth(snapshot.month) && overlapsMonth(period, snapshot.month)) hasOverlap = true;
      previous = period;
    }
    if (periodsValid && !hasOverlap) {
      addIssue(issues, 'ROSTER_LEARNER_OUTSIDE_MONTH', `${path}.enrollmentPeriods`, `Student ID ${learner.studentId} does not overlap the requested month.`);
    }
    if (periodsValid && hasOverlap && (learner.sex === 'M' || learner.sex === 'F') && isObject(learner.identity)
      && typeof learner.identity.firstName === 'string' && learner.identity.firstName.trim()
      && typeof learner.identity.lastName === 'string' && learner.identity.lastName.trim()) {
      const fullName = `${learner.identity.lastName.trim()}, ${[learner.identity.firstName, learner.identity.middleName, learner.identity.extensionName]
        .filter((part) => typeof part === 'string' && part.trim()).map((part) => part.trim()).join(' ')}`;
      selected.push({ ...learner, fullName, enrollmentPeriods: periods });
    }
  }
  const male = selected.filter((learner) => learner.sex === 'M');
  const female = selected.filter((learner) => learner.sex === 'F');
  return [...male, ...female];
}

function validateCoverage(snapshot, calendar, monthRoster, issues, blockers) {
  if (!Array.isArray(snapshot.coverage)) {
    addIssue(issues, 'INVALID_COVERAGE_SET', 'coverage', 'coverage must enumerate every scheduled date/session pair.');
    return { byKey: new Map(), sourceSeqs: new Set() };
  }
  const byKey = new Map();
  const expected = new Map();
  const sourceSeqs = new Set();
  const lastSeq = snapshot.sourceRevision?.attendanceLastSeq ?? -1;
  for (const day of calendar.byDate.values()) {
    if (day.kind !== 'CLASS_DAY') continue;
    for (const session of day.scheduledSessions) expected.set(`${day.date}|${session}`, { date: day.date, session });
  }
  for (let index = 0; index < snapshot.coverage.length; index += 1) {
    const row = snapshot.coverage[index];
    const path = `coverage[${index}]`;
    if (!isObject(row) || !isValidIsoDate(row.date) || !SESSIONS.has(row.session)
      || typeof row.sessionClosed !== 'boolean' || typeof row.coverageComplete !== 'boolean'
      || typeof row.logHealthy !== 'boolean' || typeof row.gapAffected !== 'boolean') {
      addIssue(issues, 'INVALID_COVERAGE_ROW', path, 'Coverage row must contain date, session, closure, completeness, log-health and gap fields.');
      continue;
    }

    if (row.sessionClosed) {
      if (!Number.isSafeInteger(row.closeEventSeq) || row.closeEventSeq < 1
        || row.closeEventSeq > MAX_EVENT_SEQ
        || !Array.isArray(row.bitmapPayloadSeqs) || row.bitmapPayloadSeqs.length !== MAX_CLOSE_PAYLOADS
        || !Number.isSafeInteger(row.sourceLastSeq) || row.sourceLastSeq > MAX_EVENT_SEQ
        || !Array.isArray(row.expectedStudentIds)
        || typeof row.closeCoverageFlag !== 'boolean'
        || !Number.isSafeInteger(row.expectedCount) || row.expectedCount < 0) {
        addIssue(issues, 'INVALID_CLOSE_GROUP_PROVENANCE', path, 'A closed session must reference its EV_CLOSE sequence, contiguous bitmap PAYLOAD sequences, final group sequence, expected IDs and expected count.');
      } else {
        if (row.coverageComplete !== row.closeCoverageFlag) {
          addIssue(issues, 'CLOSE_COVERAGE_FLAG_MISMATCH', `${path}.coverageComplete`, 'coverageComplete must be projected from the EV_CLOSE coverage flag, not asserted independently.');
        }
        if (row.closeCoverageFlag && row.gapAffected) {
          addIssue(issues, 'CLOSE_COVERAGE_GAP_CONTRADICTION', path, 'The EV_CLOSE coverage flag cannot be true when an overlapping GAP is reported.');
        }
        const expectedPayloadSeqs = row.bitmapPayloadSeqs.map((_, payloadIndex) => row.closeEventSeq + payloadIndex + 1);
        const groupSequences = [row.closeEventSeq, ...row.bitmapPayloadSeqs];
        if (row.bitmapPayloadSeqs.some((seq, payloadIndex) => !Number.isSafeInteger(seq) || seq < 1
          || seq > MAX_EVENT_SEQ || seq !== expectedPayloadSeqs[payloadIndex])
          || row.sourceLastSeq !== row.bitmapPayloadSeqs.at(-1)
          || row.closeEventSeq > lastSeq || row.sourceLastSeq > lastSeq) {
          addIssue(issues, 'INVALID_CLOSE_GROUP_PROVENANCE', path, 'The EV_CLOSE record and bitmap PAYLOAD records must be contiguous and remain within attendanceLastSeq; sourceLastSeq must equal the final PAYLOAD sequence.');
        }
        for (const seq of groupSequences) {
          if (!Number.isSafeInteger(seq) || seq < 1) continue;
          if (sourceSeqs.has(seq)) {
            addIssue(issues, 'DUPLICATE_CLOSE_GROUP_SEQUENCE', path, `Close-group event sequence ${seq} is referenced more than once.`);
          } else {
            sourceSeqs.add(seq);
          }
        }

        const ids = row.expectedStudentIds;
        if (ids.some((id) => !Number.isSafeInteger(id) || id < 1 || id > 300)
          || new Set(ids).size !== ids.length || row.expectedCount !== ids.length) {
          addIssue(issues, 'INVALID_CLOSE_EXPECTED_SET', path, 'Closed-session expected IDs must be unique device IDs 1..300 and expectedCount must equal the array length.');
        }
      }
    } else if (row.closeCoverageFlag !== null || row.closeEventSeq !== null || row.bitmapPayloadSeqs !== null
      || row.sourceLastSeq !== null || row.expectedStudentIds !== null || row.expectedCount !== null) {
      addIssue(issues, 'UNTRUSTED_OPEN_EXPECTED_SET', path, 'An unclosed session cannot claim a finalized CLOSE/PAYLOAD sequence range or expected-student set.');
    }

    const key = `${row.date}|${row.session}`;
    if (byKey.has(key)) {
      addIssue(issues, 'DUPLICATE_COVERAGE_ROW', path, `Coverage for ${key} appears more than once.`);
      continue;
    }
    if (!expected.has(key)) {
      addIssue(issues, 'COVERAGE_FOR_UNSCHEDULED_SESSION', path, `Coverage ${key} does not correspond to a scheduled session.`);
      continue;
    }
    if (row.sessionClosed && Array.isArray(row.expectedStudentIds)) {
      const expectedIds = new Set(row.expectedStudentIds);
      const rosterIds = new Set(monthRoster.map((learner) => learner.studentId));
      for (const studentId of expectedIds) {
        if (!rosterIds.has(studentId)) {
          addIssue(issues, 'CLOSE_EXPECTED_UNKNOWN_LEARNER', `${path}.expectedStudentIds`, `Close bitmap includes student ID ${studentId}, which is absent from the month-overlap roster.`);
        }
      }
      for (const learner of monthRoster) {
        const enrolledByHistory = learner.enrollmentPeriods.some((period) => period.startInclusive <= row.date
          && (period.endExclusive === null || row.date < period.endExclusive));
        if (enrolledByHistory !== expectedIds.has(learner.studentId)) {
          addIssue(issues, 'CLOSE_ROSTER_EXPECTATION_MISMATCH', `${path}.expectedStudentIds`, `Student ID ${learner.studentId} enrollment history disagrees with the expected-student bitmap for ${row.date} ${row.session}.`);
        }
      }
    }
    byKey.set(key, row);
    if (!row.sessionClosed || !row.coverageComplete || !row.logHealthy || row.gapAffected) {
      addIssue(blockers, 'COVERAGE_INCOMPLETE', path, `Coverage for ${key} is not finalized and reliable.`);
      if (row.gapAffected) addIssue(blockers, 'COVERAGE_GAP', path, `A GAP or fault affects ${key}.`);
    }
  }
  for (const [key] of expected) {
    if (!byKey.has(key)) addIssue(blockers, 'COVERAGE_ROW_MISSING', 'coverage', `No coverage row exists for scheduled session ${key}.`);
  }
  return { byKey, sourceSeqs };
}

function validateSessionResult({ result, date, session, learner, calendarDay, coverageMap, snapshot, usedEventSeqs, blockers, issues }) {
  const path = `attendance[studentId=${learner.studentId}].days[date=${date}].${session.toLowerCase()}`;
  if (result === undefined || result === null) {
    addIssue(blockers, 'MISSING_SESSION_RESULT', path, `Student ID ${learner.studentId} has no ${session} result for ${date}.`);
    return;
  }
  if (!isObject(result) || typeof result.status !== 'string' || !SESSION_STATUSES.has(result.status)) {
    addIssue(issues, 'INVALID_SESSION_RESULT', path, `Student ID ${learner.studentId} has a malformed ${session} result for ${date}.`);
    return;
  }
  const scheduled = calendarDay.scheduledSessions.includes(session);
  if (result.status === 'NOT_SCHEDULED') {
    if (result.sourceEvent !== null && result.sourceEvent !== undefined) {
      addIssue(issues, 'NOT_SCHEDULED_HAS_EVENT_REFERENCE', `${path}.sourceEvent`, 'A calendar-confirmed unscheduled session cannot reference an attendance event.');
    }
    if (scheduled || result.calendarConfirmed !== true) {
      addIssue(issues, 'SCHEDULE_STATUS_MISMATCH', path, `${session} is not confirmed as unscheduled by the calendar.`);
    }
    return;
  }
  if (!scheduled) {
    addIssue(issues, 'SCHEDULE_STATUS_MISMATCH', path, `${session} was marked ${result.status} although the calendar does not schedule it.`);
    return;
  }
  if (result.status === 'UNRESOLVED') {
    if (result.sourceEvent !== null && result.sourceEvent !== undefined) {
      addIssue(issues, 'UNRESOLVED_HAS_EVENT_REFERENCE', `${path}.sourceEvent`, 'An unresolved session cannot claim a final attendance event reference.');
    }
    addIssue(blockers, 'UNRESOLVED_SESSION', path, `Student ID ${learner.studentId} has unresolved ${session} attendance on ${date}.`);
    return;
  }
  if (!result.status || !SESSION_STATUSES.has(result.status)) return;

  if (['PRESENT', 'LATE', 'EXCUSED', 'CLEAR'].includes(result.status)) {
    const source = result.sourceEvent;
    const expectedType = result.status === 'EXCUSED' || result.status === 'CLEAR' ? 'MARK' : null;
    if (!isObject(source) || !Number.isSafeInteger(source.seq) || source.seq < 1
      || source.seq > MAX_EVENT_SEQ || !EVENT_TYPES.has(source.type)
      || (expectedType && source.type !== expectedType)) {
      addIssue(issues, 'INVALID_EVENT_PROVENANCE', `${path}.sourceEvent`, `${result.status} must reference a valid committed source event (EXCUSED/CLEAR require MARK).`);
    } else if (source.status !== result.status) {
      addIssue(issues, 'EVENT_STATUS_MISMATCH', `${path}.sourceEvent.status`, `Source event status ${String(source.status)} does not match projected result ${result.status}.`);
    } else {
      if (source.seq > (snapshot.sourceRevision?.attendanceLastSeq ?? -1)) {
        addIssue(issues, 'EVENT_SEQUENCE_OUT_OF_RANGE', `${path}.sourceEvent.seq`, 'Event sequence cannot exceed the snapshot attendanceLastSeq.');
      }
      if (usedEventSeqs.has(source.seq)) {
        addIssue(issues, 'DUPLICATE_EVENT_REFERENCE', `${path}.sourceEvent.seq`, `Event sequence ${source.seq} is referenced more than once.`);
      } else {
        usedEventSeqs.add(source.seq);
      }
    }
    if (result.status === 'CLEAR') {
      addIssue(blockers, 'CLEARED_SESSION_UNRESOLVED', path, `Student ID ${learner.studentId} has a Clear operation that has not resolved to a final attendance state.`);
    }
    return;
  }

  if (result.status === 'ABSENT_CONFIRMED') {
    if (result.sourceEvent !== null && result.sourceEvent !== undefined) {
      addIssue(issues, 'ABSENCE_HAS_EVENT_REFERENCE', `${path}.sourceEvent`, 'ABSENT_CONFIRMED is derived and must not point to a stored attendance mark.');
    }
    const coverage = coverageMap.get(`${date}|${session}`);
    const isExpected = learner.enrollmentPeriods.some((period) => period && period.startInclusive <= date
      && (period.endExclusive === null || date < period.endExclusive));
    if (!isExpected) {
      addIssue(issues, 'ABSENCE_FOR_UNEXPECTED_LEARNER', path, 'A learner outside the enrollment interval cannot be reported absent.');
    }
    if (!coverage || coverage.sessionClosed !== true || coverage.coverageComplete !== true
      || coverage.logHealthy !== true || coverage.gapAffected !== false
      || !Array.isArray(coverage.expectedStudentIds) || !coverage.expectedStudentIds.includes(learner.studentId)
      || snapshot.completeness?.attendanceSnapshotConsistent !== true) {
      addIssue(issues, 'FALSE_CONFIRMED_ABSENCE', path, 'ABSENT_CONFIRMED requires closed session, complete coverage, healthy log, no GAP, expected learner and consistent snapshot evidence.');
    }
    return;
  }

  if (result.status === 'NOT_SCHEDULED') return;
  if (result.status === 'UNRESOLVED') return;
  addIssue(issues, 'UNSUPPORTED_SESSION_RESULT', path, `Unsupported ${session} state ${result.status}.`);
}

function validateAttendance(snapshot, calendar, monthRoster, coverageMap, closeGroupSeqs, issues, blockers) {
  if (!Array.isArray(snapshot.attendance)) {
    addIssue(issues, 'INVALID_ATTENDANCE_SET', 'attendance', 'attendance must provide one entry per month-overlap learner.');
    return;
  }
  const learnersById = new Map(monthRoster.map((learner) => [learner.studentId, learner]));
  const attendanceById = new Map();
  for (let index = 0; index < snapshot.attendance.length; index += 1) {
    const entry = snapshot.attendance[index];
    const path = `attendance[${index}]`;
    if (!isObject(entry) || !Number.isSafeInteger(entry.studentId) || !Array.isArray(entry.days)) {
      addIssue(issues, 'INVALID_ATTENDANCE_ENTRY', path, 'Each attendance entry requires a valid studentId and days array.');
      continue;
    }
    if (!learnersById.has(entry.studentId)) {
      addIssue(issues, 'ATTENDANCE_FOR_UNKNOWN_LEARNER', `${path}.studentId`, `Student ID ${entry.studentId} is not in the selected month-overlap roster.`);
      continue;
    }
    if (attendanceById.has(entry.studentId)) {
      addIssue(issues, 'DUPLICATE_ATTENDANCE_ENTRY', `${path}.studentId`, `Student ID ${entry.studentId} has multiple attendance entries.`);
      continue;
    }
    attendanceById.set(entry.studentId, entry);
  }
  const usedEventSeqs = new Set(closeGroupSeqs);
  for (const learner of monthRoster) {
    const entry = attendanceById.get(learner.studentId);
    if (!entry) {
      addIssue(blockers, 'MISSING_LEARNER_ATTENDANCE', 'attendance', `Student ID ${learner.studentId} has no attendance snapshot entry.`);
      continue;
    }
    const expectedDates = calendar.classDays.filter((date) => learner.enrollmentPeriods.some((period) => (
      period && period.startInclusive <= date && (period.endExclusive === null || date < period.endExclusive)
    )));
    const byDate = new Map();
    for (let dayIndex = 0; dayIndex < entry.days.length; dayIndex += 1) {
      const day = entry.days[dayIndex];
      const path = `attendance[studentId=${learner.studentId}].days[${dayIndex}]`;
      if (!isObject(day) || !isValidIsoDate(day.date) || !day.date.startsWith(`${snapshot.month}-`)) {
        addIssue(issues, 'INVALID_ATTENDANCE_DATE', path, 'Attendance dates must be valid and inside the report month.');
        continue;
      }
      if (byDate.has(day.date)) {
        addIssue(issues, 'DUPLICATE_LEARNER_ATTENDANCE_DATE', `${path}.date`, `Student ID ${learner.studentId} has duplicate attendance for ${day.date}.`);
        continue;
      }
      const calendarDay = calendar.byDate.get(day.date);
      if (!calendarDay || calendarDay.kind !== 'CLASS_DAY') {
        addIssue(issues, 'ATTENDANCE_ON_NON_CLASS_DAY', path, `Attendance exists on non-class date ${day.date}.`);
        continue;
      }
      if (!expectedDates.includes(day.date)) {
        addIssue(issues, 'ATTENDANCE_OUTSIDE_EXPECTED_DAYS', path, `Student ID ${learner.studentId} was not enrolled on ${day.date}.`);
        continue;
      }
      byDate.set(day.date, day);
      validateSessionResult({ result: day.am, date: day.date, session: 'AM', learner, calendarDay, coverageMap, snapshot, usedEventSeqs, blockers, issues });
      validateSessionResult({ result: day.pm, date: day.date, session: 'PM', learner, calendarDay, coverageMap, snapshot, usedEventSeqs, blockers, issues });
    }
    for (const date of expectedDates) {
      if (!byDate.has(date)) {
        addIssue(blockers, 'MISSING_EXPECTED_ATTENDANCE', `attendance[studentId=${learner.studentId}].days`, `Student ID ${learner.studentId} has no finalized attendance pair for expected class date ${date}.`);
      }
    }
  }
}

function validateMovementHistory(snapshot, monthRoster, issues, warnings) {
  if (!Array.isArray(snapshot.movementHistory)) {
    addIssue(issues, 'INVALID_MOVEMENT_HISTORY', 'movementHistory', 'movementHistory must be an array, even when empty.');
    return;
  }
  const learnerIds = new Set(monthRoster.map((learner) => learner.studentId));
  for (let index = 0; index < snapshot.movementHistory.length; index += 1) {
    const movement = snapshot.movementHistory[index];
    const path = `movementHistory[${index}]`;
    if (!isObject(movement) || !Number.isSafeInteger(movement.studentId) || !isValidIsoDate(movement.date)
      || !MOVEMENT_TYPES.has(movement.type) || !MOVEMENT_DATE_MEANINGS.has(movement.effectiveDateMeaning)) {
      addIssue(issues, 'INVALID_MOVEMENT_RECORD', path, 'Movement record requires a known studentId, valid date, supported type and explicit date meaning.');
      continue;
    }
    if (movement.effectiveDateMeaning === 'UNCONFIRMED') {
      addIssue(warnings, 'MOVEMENT_DATE_MEANING_UNCONFIRMED', path,
        `Movement date meaning for student ID ${movement.studentId} is not confirmed; no enrollment-boundary inference is made.`, 'WARNING');
    }
    if (movement.type === 'NLS') {
      addIssue(warnings, 'NLS_SEMANTICS_UNCONFIRMED', path,
        `NLS for student ID ${movement.studentId} is preserved verbatim; its meaning and form mapping are not defined.`, 'WARNING');
    }
    if (movement.effectiveDateMeaning !== 'UNCONFIRMED'
      && !MOVEMENT_DATE_RULES[movement.type]?.includes(movement.effectiveDateMeaning)) {
      addIssue(issues, 'MOVEMENT_DATE_MEANING_MISMATCH', `${path}.effectiveDateMeaning`,
        `Movement date meaning does not match movement type ${movement.type}.`);
    }
    if (!learnerIds.has(movement.studentId)) {
      addIssue(issues, 'MOVEMENT_UNKNOWN_LEARNER', `${path}.studentId`, 'Movement record references a learner outside the month-overlap roster.');
    }
    if (!movement.date.startsWith(`${snapshot.month}-`)) {
      addIssue(issues, 'INVALID_MOVEMENT_DATE', `${path}.date`, 'Movement date must fall inside the selected report month.');
    }
    for (const key of ['reasonCode', 'reasonText', 'destinationSchool', 'destinationSection']) {
      if (!nullableString(movement[key])) addIssue(issues, 'INVALID_MOVEMENT_FIELD', `${path}.${key}`, `${key} must be a string or null.`);
    }
    if (movement.type.startsWith('TRANSFER') && (!movement.reasonCode || !movement.destinationSchool)) {
      addIssue(warnings, 'INCOMPLETE_TRANSFER_DETAILS', path, `Transfer for student ID ${movement.studentId} lacks a reason or destination; NLS/remarks may be incomplete.`, 'WARNING');
    }
  }
}

/**
 * Validate the proposed browser reporting snapshot, not a production API payload.
 * `VALID` means the data is structurally complete enough for calculation only;
 * it does not certify DepEd acceptance or authorize XLSX generation.
 */
export function validateSnapshot(snapshot, { expectedMonth } = {}) {
  const issues = [];
  const blockers = [];
  const warnings = [];
  if (!isObject(snapshot)) {
    addIssue(issues, 'INVALID_SNAPSHOT', 'snapshot', 'A JSON object is required.');
    return { state: 'INVALID', valid: false, monthRoster: [], classDays: [], issues, blockers, warnings };
  }
  if (snapshot.schemaVersion !== 1) {
    addIssue(issues, 'UNSUPPORTED_SCHEMA_VERSION', 'schemaVersion', 'Only proposed snapshot schema version 1 is supported.');
  }
  if (!validMonth(snapshot.month)) {
    addIssue(issues, 'INVALID_MONTH', 'month', 'month must use a valid YYYY-MM value.');
  }
  if (expectedMonth !== undefined && snapshot.month !== expectedMonth) {
    addIssue(issues, 'SNAPSHOT_MONTH_MISMATCH', 'month', `Snapshot month ${String(snapshot.month)} does not match requested month ${String(expectedMonth)}.`);
  }
  if (typeof snapshot.snapshotId !== 'string' || snapshot.snapshotId.trim() === '' || snapshot.snapshotId.length > 128) {
    addIssue(issues, 'INVALID_SNAPSHOT_ID', 'snapshotId', 'snapshotId must be a non-empty opaque token no longer than 128 characters.');
  }
  validateRevision(snapshot, issues);
  validateRosterRevision(snapshot, issues);
  validateLearnerMetadataRevision(snapshot, issues);
  validateProfile(snapshot, issues);
  validateSchoolProfile(snapshot, issues, warnings);

  if (!isObject(snapshot.completeness)) {
    addIssue(issues, 'INVALID_COMPLETENESS', 'completeness', 'Completeness evidence object is required.');
  } else {
    for (const key of ['rosterHistoryComplete', 'calendarComplete', 'scheduleHistoryComplete', 'attendanceSnapshotConsistent']) {
      if (typeof snapshot.completeness[key] !== 'boolean') {
        addIssue(issues, 'INVALID_COMPLETENESS_FLAG', `completeness.${key}`, `${key} must be an explicit boolean.`);
      }
    }
    if (snapshot.completeness.rosterHistoryComplete === false) {
      addIssue(blockers, 'ROSTER_HISTORY_INCOMPLETE', 'completeness.rosterHistoryComplete', 'Month-overlap enrollment history is not confirmed complete.');
    }
    if (snapshot.completeness.attendanceSnapshotConsistent === false) {
      addIssue(blockers, 'SNAPSHOT_INCONSISTENT', 'completeness.attendanceSnapshotConsistent', 'Source revisions changed or could not be proven consistent during snapshot assembly.');
    }
  }

  const calendar = validateCalendar(snapshot, issues, blockers);
  const monthRoster = validateLearners(snapshot, issues, warnings);
  for (const learner of monthRoster) {
    if (!learner.identity?.lrn) {
      // Already emitted as a warning while validating the learner.
    }
  }
  if (monthRoster.length === 0 && snapshot.learners?.length) {
    // More precise identity/period issues were emitted during selection.
  }
  const coverage = validateCoverage(snapshot, calendar, monthRoster, issues, blockers);
  validateAttendance(snapshot, calendar, monthRoster, coverage.byKey, coverage.sourceSeqs, issues, blockers);
  validateMovementHistory(snapshot, monthRoster, issues, warnings);

  const state = issues.length ? 'INVALID' : blockers.length ? 'INCOMPLETE' : 'VALID';
  return {
    state,
    valid: state === 'VALID',
    month: snapshot.month,
    snapshotId: snapshot.snapshotId,
    monthRoster,
    classDays: calendar.classDays,
    calendarByDate: calendar.byDate,
    coverageByKey: coverage.byKey,
    issues,
    blockers,
    warnings,
  };
}
