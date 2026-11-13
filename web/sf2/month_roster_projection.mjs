/**
 * Reporting-month projection over a complete, exact-prefix roster history.
 * Enrollment intervals are half-open local-day ranges [start,endExclusive).
 * This module never edits operational roster data and never chooses a name
 * across conflicting revisions inside the reporting month.
 */

import { isValidSourcePrefix, sameSourcePrefix } from './source_prefix.mjs';

const MAX_ID = 300;
const OPEN_END = 0x7fffffff;

function issue(code, path, message, studentId = null) {
  return { code, path, message, studentId };
}

function isSafeDay(value) {
  return Number.isSafeInteger(value) && value >= 0 && value < OPEN_END;
}

function validPrefix(value) {
  return isValidSourcePrefix(value);
}

function samePrefix(a, b) {
  return sameSourcePrefix(a, b);
}

function utf8Length(value) {
  let bytes = 0;
  for (const character of value) {
    const point = character.codePointAt(0);
    if (point >= 0xd800 && point <= 0xdfff) return Infinity;
    if (point <= 0x7f) bytes += 1;
    else if (point <= 0x7ff) bytes += 2;
    else if (point <= 0xffff) bytes += 3;
    else bytes += 4;
  }
  return bytes;
}

function validDisplayName(name) {
  return typeof name === 'string' && name.length > 0
    && name[0] !== ' ' && name[name.length - 1] !== ' '
    && !/[\u0000-\u001f\u007f]/u.test(name)
    && utf8Length(name) <= 32;
}

function intervalOverlaps(interval, start, end) {
  const intervalEnd = interval.endDayExclusive ?? OPEN_END;
  return interval.startDayInclusive < end && intervalEnd > start;
}

function intervalCoversDay(interval, day) {
  const intervalEnd = interval.endDayExclusive ?? OPEN_END;
  return interval.startDayInclusive <= day && day < intervalEnd;
}

/**
 * Build one learner entry for every known student with at least one enrollment
 * interval intersecting the month. All intersecting intervals are preserved.
 * `nameHistory` includes the last pre-month operational display name (if any)
 * and every revision during the month. It is diagnostic only: legal reporting
 * names come from the separate bitemporal reporting-identity model.
 */
export function projectMonthRoster({
  monthStartDay,
  monthEndExclusive,
  knownStudentIds,
  intervals,
  nameRevisions,
  sourcePrefix,
  expectedSourcePrefix,
  rosterHistoryComplete,
  nameHistoryComplete,
}) {
  const issues = [];
  if (!isSafeDay(monthStartDay) || !isSafeDay(monthEndExclusive)
      || monthEndExclusive <= monthStartDay) {
    issues.push(issue('INVALID_MONTH_RANGE', 'month', 'Month day range must be a non-empty half-open range of valid local days.'));
  }
  if (!samePrefix(sourcePrefix, expectedSourcePrefix)) {
    issues.push(issue('SOURCE_PREFIX_MISMATCH', 'sourcePrefix', 'Roster intervals and names must come from the same valid log prefix.'));
  }
  if (rosterHistoryComplete !== true) {
    issues.push(issue('ROSTER_HISTORY_INCOMPLETE', 'rosterHistoryComplete', 'Complete roster history is required.'));
  }
  const warnings = [];
  if (nameHistoryComplete !== true) {
    warnings.push(issue('OPERATIONAL_NAME_HISTORY_INCOMPLETE', 'nameHistoryComplete',
      'Operational display-name history is incomplete; it is not used as the legal reporting identity.'));
  }
  if (!Array.isArray(knownStudentIds) || !Array.isArray(intervals)
      || !Array.isArray(nameRevisions)) {
    issues.push(issue('INVALID_HISTORY_INPUT', 'history', 'Known IDs, intervals and name revisions must be arrays.'));
  }
  if (issues.length) return { state: 'BLOCKED', learners: [], total: 0, issues, warnings };

  const known = new Set();
  for (const [index, id] of knownStudentIds.entries()) {
    if (!Number.isSafeInteger(id) || id < 1 || id > MAX_ID) {
      issues.push(issue('INVALID_STUDENT_ID', `knownStudentIds[${index}]`, 'Known student ID must be in 1..300.', id));
    } else if (known.has(id)) {
      issues.push(issue('DUPLICATE_STUDENT_ID', `knownStudentIds[${index}]`, `Student ID ${id} is duplicated.`, id));
    } else {
      known.add(id);
    }
  }

  const byStudent = new Map([...known].map((id) => [id, []]));
  for (const [index, interval] of intervals.entries()) {
    const path = `intervals[${index}]`;
    if (!interval || !Number.isSafeInteger(interval.studentId)
        || !known.has(interval.studentId)) {
      issues.push(issue('INTERVAL_UNKNOWN_STUDENT', path, 'Enrollment interval references an unknown student ID.', interval?.studentId ?? null));
      continue;
    }
    const end = interval.endDayExclusive ?? OPEN_END;
    if (!isSafeDay(interval.startDayInclusive)
        || !(end === OPEN_END || isSafeDay(end))
        || end <= interval.startDayInclusive) {
      issues.push(issue('INVALID_ENROLLMENT_INTERVAL', path, 'Enrollment interval must be a valid, non-empty half-open interval.', interval.studentId));
      continue;
    }
    byStudent.get(interval.studentId).push({
      studentId: interval.studentId,
      startDayInclusive: interval.startDayInclusive,
      endDayExclusive: end === OPEN_END ? null : end,
    });
  }

  for (const [studentId, studentIntervals] of byStudent) {
    studentIntervals.sort((a, b) => a.startDayInclusive - b.startDayInclusive
      || (a.endDayExclusive ?? OPEN_END) - (b.endDayExclusive ?? OPEN_END));
    if (studentIntervals.length === 0) {
      issues.push(issue('STUDENT_WITHOUT_INTERVAL', 'intervals', `Known student ID ${studentId} has no enrollment interval.`, studentId));
      continue;
    }
    for (let index = 1; index < studentIntervals.length; index += 1) {
      const previous = studentIntervals[index - 1];
      const current = studentIntervals[index];
      if ((previous.endDayExclusive ?? OPEN_END) > current.startDayInclusive) {
        issues.push(issue('OVERLAPPING_ENROLLMENT_INTERVALS', `intervals.${studentId}`, `Student ID ${studentId} has overlapping enrollment intervals.`, studentId));
        break;
      }
    }
  }

  const sourceSeqs = new Set();
  const nameByStudent = new Map([...known].map((id) => [id, []]));
  const lastNameDay = new Map();
  let lastGlobalNameSeq = 0;
  for (const [index, revision] of nameRevisions.entries()) {
    const path = `nameRevisions[${index}]`;
    if (!revision || !Number.isSafeInteger(revision.studentId)
        || !known.has(revision.studentId) || !isSafeDay(revision.effectiveDay)
        || !Number.isSafeInteger(revision.sourceSeq) || revision.sourceSeq < 1
        || revision.sourceSeq > (sourcePrefix?.recordCount ?? 0)
        || !validDisplayName(revision.name)) {
      issues.push(issue('INVALID_NAME_REVISION', path, 'Name revision has an invalid student ID, day, sequence or 32-byte operational display name.', revision?.studentId ?? null));
      continue;
    }
    if (sourceSeqs.has(revision.sourceSeq)) {
      issues.push(issue('DUPLICATE_NAME_SOURCE_SEQUENCE', path, `Name source sequence ${revision.sourceSeq} is duplicated.`, revision.studentId));
      continue;
    }
    if (revision.sourceSeq <= lastGlobalNameSeq) {
      issues.push(issue('NAME_REVISIONS_OUT_OF_SOURCE_ORDER', path, 'Name revisions must remain in strictly increasing source-log sequence order.', revision.studentId));
    }
    lastGlobalNameSeq = revision.sourceSeq;
    sourceSeqs.add(revision.sourceSeq);
    const previousDay = lastNameDay.get(revision.studentId);
    if (previousDay !== undefined && revision.effectiveDay < previousDay) {
      warnings.push(issue('BACKDATED_OPERATIONAL_NAME_REVISION', path,
        `Student ID ${revision.studentId} has an operational display-name revision whose effective day moves backwards.`, revision.studentId));
    }
    lastNameDay.set(revision.studentId, revision.effectiveDay);
    nameByStudent.get(revision.studentId).push({
      studentId: revision.studentId,
      effectiveDay: revision.effectiveDay,
      sourceSeq: revision.sourceSeq,
      name: revision.name,
    });
  }

  if (issues.length) return { state: 'BLOCKED', learners: [], total: 0, issues, warnings };

  const monthLastDay = monthEndExclusive - 1;
  const learners = [];
  const orderedIds = [...known].sort((a, b) => a - b);
  for (const studentId of orderedIds) {
    const overlappingIntervals = byStudent.get(studentId)
      .filter((interval) => intervalOverlaps(interval, monthStartDay, monthEndExclusive));
    if (overlappingIntervals.length === 0) continue;

    const history = nameByStudent.get(studentId).sort((a, b) => a.sourceSeq - b.sourceSeq);
    const prior = history.filter((revision) => revision.effectiveDay < monthStartDay);
    const during = history.filter((revision) => revision.effectiveDay >= monthStartDay
      && revision.effectiveDay < monthEndExclusive);
    const baseline = prior.length ? prior[prior.length - 1] : null;
    const applicable = [...(baseline ? [baseline] : []), ...during];
    const distinctNames = [...new Set(applicable.map((revision) => revision.name))];
    let nameState = 'RESOLVED';
    let displayName = distinctNames[0] ?? null;
    if (distinctNames.length === 0) {
      nameState = 'MISSING';
      displayName = null;
      warnings.push(issue('MISSING_OPERATIONAL_DISPLAY_NAME', `learners.${studentId}.nameHistory`,
        `Student ID ${studentId} has no operational display name effective during or before the report month.`, studentId));
    } else if (distinctNames.length > 1) {
      nameState = 'CHANGED_IN_MONTH';
      displayName = null;
      warnings.push(issue('OPERATIONAL_DISPLAY_NAME_CHANGED', `learners.${studentId}.nameHistory`,
        `Student ID ${studentId} has multiple operational display names applicable during the report month; no single operational label is selected.`, studentId));
    }

    learners.push({
      studentId,
      enrollmentIntervals: overlappingIntervals,
      enrolledOnLastDay: byStudent.get(studentId).some((interval) => intervalCoversDay(interval, monthLastDay)),
      nameState,
      displayName,
      nameHistory: applicable,
    });
  }

  return {
    state: issues.length ? 'BLOCKED' : 'READY',
    monthStartDay,
    monthEndExclusive,
    learners,
    total: learners.length,
    sourcePrefix: { ...sourcePrefix },
    issues,
    warnings,
  };
}
