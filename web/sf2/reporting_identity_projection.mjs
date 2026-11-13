/*
 * PROPOSED ONLY — join a prefix-pinned operational month roster with the
 * bitemporal school-reporting identity/movement history.
 *
 * Operational display names are diagnostic/UI data only. They are never used
 * as legal report names. This module has no storage, authentication, or XLSX
 * responsibilities.
 */

import { metadataRecordsAtRevision } from './metadata_history_model.mjs';
import { cloneSourcePrefix, isValidSourcePrefix } from './source_prefix.mjs';

const MAX_ID = 300;
const DAY_MS = 86400000;
const OPEN_DAY = 0x7fffffff;

function issue(code, path, message, studentId = null) {
  return { code, path, message, studentId };
}

function isObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function validMonth(month) {
  return typeof month === 'string' && /^\d{4}-(0[1-9]|1[0-2])$/.test(month);
}

function isoToDay(iso) {
  if (typeof iso !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(iso)) return null;
  const epoch = Date.parse(`${iso}T00:00:00Z`);
  if (!Number.isFinite(epoch) || new Date(epoch).toISOString().slice(0, 10) !== iso) return null;
  return Math.floor(epoch / DAY_MS);
}

function dayToIso(day) {
  if (!Number.isSafeInteger(day) || day < 0 || day >= OPEN_DAY) return null;
  return new Date(day * DAY_MS).toISOString().slice(0, 10);
}

function monthBounds(month) {
  if (!validMonth(month)) return null;
  const [year, monthNumber] = month.split('-').map(Number);
  const startIso = `${month}-01`;
  const next = monthNumber === 12
    ? `${year + 1}-01-01`
    : `${year}-${String(monthNumber + 1).padStart(2, '0')}-01`;
  return { startIso, endIso: next, startDay: isoToDay(startIso), endDay: isoToDay(next) };
}

function endOf(record) {
  return record.validToExclusive === null ? null : record.validToExclusive;
}

function overlaps(record, startIso, endIso) {
  return record.validFrom < endIso && (endOf(record) === null || endOf(record) > startIso);
}

function covers(record, startIso, endIso) {
  return record.validFrom <= startIso && (endOf(record) === null || endOf(record) >= endIso);
}

function identityKey(data) {
  const name = data.legalName ?? {};
  // Canonical field order: JSON object insertion order is not semantic identity.
  return JSON.stringify({
    legalName: {
      firstName: name.firstName ?? null,
      middleName: name.middleName ?? null,
      lastName: name.lastName ?? null,
      extensionName: name.extensionName ?? null,
    },
    lrn: data.lrn ?? null,
    sex: data.sex ?? null,
  });
}

function cloneLegalIdentity(data) {
  return {
    legalName: {
      firstName: data.legalName.firstName,
      middleName: data.legalName.middleName ?? null,
      lastName: data.legalName.lastName,
      extensionName: data.legalName.extensionName ?? null,
    },
    lrn: data.lrn ?? null,
    sex: data.sex ?? null,
    // This is a caller-supplied claim, not verified provenance.
    sourceStatusClaim: data.sourceStatus,
    sourceStatus: 'UNVERIFIED',
  };
}

function intervalsForMonth(learner, bounds, studentId, issues) {
  if (!Array.isArray(learner.enrollmentIntervals) || learner.enrollmentIntervals.length === 0) {
    issues.push(issue('LEARNER_MONTH_INTERVAL_MISSING', `learners.${studentId}.enrollmentIntervals`,
      'The month roster has no enrollment interval for this learner.', studentId));
    return [];
  }
  const segments = [];
  const intervals = learner.enrollmentIntervals.map((interval, index) => ({ interval, index }))
    .sort((a, b) => (a.interval?.startDayInclusive ?? -1) - (b.interval?.startDayInclusive ?? -1));
  let previousEnd = null;
  for (const { interval, index } of intervals) {
    const start = interval?.startDayInclusive;
    const end = interval?.endDayExclusive ?? OPEN_DAY;
    if (!Number.isSafeInteger(start) || start < 0 || start >= OPEN_DAY
      || !(end === OPEN_DAY || (Number.isSafeInteger(end) && end >= 0 && end <= OPEN_DAY))
      || end <= start) {
      issues.push(issue('INVALID_MONTH_ROSTER_INTERVAL',
        `learners.${studentId}.enrollmentIntervals[${index}]`,
        'Enrollment interval is not a valid half-open day range.', studentId));
      continue;
    }
    if (previousEnd !== null && start < previousEnd) {
      issues.push(issue('OVERLAPPING_MONTH_ROSTER_INTERVAL',
        `learners.${studentId}.enrollmentIntervals[${index}]`,
        'The month-roster handoff contains overlapping intervals for one learner.', studentId));
    }
    previousEnd = end;
    const clippedStart = Math.max(start, bounds.startDay);
    const clippedEnd = Math.min(end, bounds.endDay);
    if (clippedStart < clippedEnd) segments.push({ startDay: clippedStart, endDay: clippedEnd });
  }
  return segments;
}

function identityForSegments(records, segments, studentId, warnings, issues) {
  const applicable = [];
  for (const [segmentIndex, segment] of segments.entries()) {
    const startIso = dayToIso(segment.startDay);
    const endIso = dayToIso(segment.endDay);
    if (!startIso || !endIso) {
      issues.push(issue('INVALID_ENROLLMENT_DAY_CONVERSION', `learners.${studentId}.enrollmentIntervals[${segmentIndex}]`,
        'Enrollment day could not be converted to a calendar date.', studentId));
      continue;
    }
    const candidates = records
      .filter((record) => record.kind === 'LEARNER_IDENTITY'
        && record.subjectId === String(studentId)
        && overlaps(record, startIso, endIso))
      .sort((a, b) => a.validFrom.localeCompare(b.validFrom));

    let cursor = startIso;
    for (const record of candidates) {
      const overlapStart = record.validFrom > startIso ? record.validFrom : startIso;
      const recordEnd = endOf(record) ?? endIso;
      const overlapEnd = recordEnd < endIso ? recordEnd : endIso;
      if (overlapEnd <= overlapStart) continue;
      if (overlapStart > cursor) {
        warnings.push(issue('REPORTING_IDENTITY_GAP', `learners.${studentId}.reportingIdentity`,
          `No legal reporting identity covers ${cursor} through the enrollment interval before ${overlapStart}.`, studentId));
      }
      if (overlapStart < cursor) {
        issues.push(issue('OVERLAPPING_LEGAL_IDENTITY', `learners.${studentId}.reportingIdentity`,
          'Legal reporting identity records overlap within an enrolled period.', studentId));
      }
      applicable.push(record);
      if (overlapEnd > cursor) cursor = overlapEnd;
    }
    if (cursor < endIso) {
      warnings.push(issue('REPORTING_IDENTITY_GAP', `learners.${studentId}.reportingIdentity`,
        `No legal reporting identity covers ${cursor} through ${endIso} for an enrolled interval.`, studentId));
    }
  }

  if (applicable.length === 0) return null;
  const unique = new Map();
  for (const record of applicable) unique.set(identityKey(record.data), record);
  if (unique.size > 1) {
    issues.push(issue('LEGAL_IDENTITY_CHANGED_IN_MONTH', `learners.${studentId}.reportingIdentity`,
      'Legal name, LRN or sex differs across enrolled dates in this month. The report must not choose one version automatically.', studentId));
    return null;
  }
  const selected = [...unique.values()][0];
  const identity = cloneLegalIdentity(selected.data);
  const allCheckedClaimed = applicable.every((record) => record.data.sourceStatus === 'SOURCE_CHECKED');
  identity.sourceStatusClaim = allCheckedClaimed ? 'SOURCE_CHECKED' : 'UNVERIFIED';
  // There is no authenticated reviewer identity, timestamp, source reference or
  // immutable verification event. Never let a boolean-like claim make this READY.
  warnings.push(issue('LEGAL_IDENTITY_SOURCE_ATTESTATION_MISSING', `learners.${studentId}.reportingIdentity`,
    'The stored source-status value is only a claim; no reviewer attribution or immutable verification evidence is present.', studentId));
  if (!identity.legalName.firstName || !identity.legalName.lastName) {
    warnings.push(issue('LEGAL_REPORTING_NAME_INCOMPLETE', `learners.${studentId}.reportingIdentity`,
      'Legal first name or family name is missing.', studentId));
  }
  if (!identity.lrn) warnings.push(issue('LRN_MISSING', `learners.${studentId}.reportingIdentity`,
    'LRN is missing.', studentId));
  if (!identity.sex) warnings.push(issue('SEX_MISSING', `learners.${studentId}.reportingIdentity`,
    'Sex is missing; sex-based subtotals cannot be finalized.', studentId));
  return identity;
}


function movementMatchesEnrollmentBoundary(movement, learner) {
  const type = movement.data.movementType;
  const meaning = movement.data.effectiveDateMeaning;
  if (meaning === 'UNCONFIRMED' || type === 'OTHER') return true;
  const intervals = Array.isArray(learner?.enrollmentIntervals) ? learner.enrollmentIntervals : [];
  const movementDay = isoToDay(movement.effectiveDate);
  if (movementDay === null) return false;
  if (meaning === 'FIRST_ENROLLED_DAY') {
    return intervals.some((interval) => interval.startDayInclusive === movementDay);
  }
  if (meaning === 'FIRST_NOT_ENROLLED_DAY') {
    return intervals.some((interval) => interval.endDayExclusive === movementDay);
  }
  if (meaning === 'LAST_ENROLLED_DAY') {
    return intervals.some((interval) => interval.endDayExclusive === movementDay + 1);
  }
  if (meaning === 'SECTION_CHANGE_DAY' && type === 'SHIFTED') {
    return intervals.some((interval) => interval.startDayInclusive === movementDay
      || interval.endDayExclusive === movementDay);
  }
  if (meaning === 'EVENT_DAY' && type === 'OTHER') return true;
  return false;
}

function profileForMonth(records, kind, bounds, warnings, issues) {
  const candidates = records.filter((record) => record.kind === kind
    && overlaps(record, bounds.startIso, bounds.endIso));
  if (candidates.length === 0) {
    warnings.push(issue(`${kind}_NOT_CONFIGURED_FOR_MONTH`, kind, `${kind} does not cover the complete reporting month.`));
    return null;
  }
  if (candidates.length > 1 || !covers(candidates[0], bounds.startIso, bounds.endIso)) {
    issues.push(issue(`${kind}_CHANGED_IN_MONTH`, kind,
      `${kind} changes or has multiple versions during the reporting month; no version is selected automatically.`));
    return null;
  }
  return candidates[0];
}

/**
 * Join one complete month roster with reporting metadata at a pinned revision.
 * Operational display names are returned separately and never copied into
 * legalName. Same-month legal identity changes and movement-date ambiguity are
 * surfaced instead of silently normalized.
 */
export function projectReportingRoster({ month, monthRoster, metadataHistory, asOfRevision } = {}) {
  const issues = [];
  const warnings = [];
  const operationalDiagnostics = [];
  const bounds = monthBounds(month);
  if (!bounds) issues.push(issue('INVALID_REPORT_MONTH', 'month', 'month must be YYYY-MM.'));
  if (!isObject(monthRoster) || monthRoster.state !== 'READY'
    || !Array.isArray(monthRoster.learners)
    || monthRoster.learners.some((learner) => !isObject(learner))
    || !Number.isSafeInteger(monthRoster.monthStartDay)
    || !Number.isSafeInteger(monthRoster.monthEndExclusive)) {
    issues.push(issue('MONTH_ROSTER_NOT_READY', 'monthRoster',
      'A READY, exact-prefix month roster projection is required.'));
  } else if (bounds && (monthRoster.monthStartDay !== bounds.startDay
    || monthRoster.monthEndExclusive !== bounds.endDay)) {
    issues.push(issue('MONTH_ROSTER_RANGE_MISMATCH', 'monthRoster',
      'Month roster day boundaries do not match the requested calendar month.'));
  }
  const prefix = monthRoster?.sourcePrefix;
  if (!isValidSourcePrefix(prefix)) {
    issues.push(issue('MONTH_ROSTER_PREFIX_INVALID', 'monthRoster.sourcePrefix',
      'The report-month roster must carry a valid exact-source prefix identity.'));
  }
  if (issues.length) return {
    state: 'BLOCKED', month, metadataRevision: null, sourcePrefix: null,
    schoolProfile: null, sectionProfile: null, learners: [], movements: [],
    issues, warnings, operationalDiagnostics,
  };

  const ids = monthRoster.learners.map((learner) => learner.studentId);
  const seenIds = new Set();
  for (const id of ids) {
    if (!Number.isSafeInteger(id) || id < 1 || id > MAX_ID || seenIds.has(id)) {
      issues.push(issue('INVALID_MONTH_ROSTER_STUDENT_ID', 'monthRoster.learners',
        'Month roster student IDs must be unique integers in 1..300.', id ?? null));
    }
    seenIds.add(id);
  }
  if (!Number.isSafeInteger(asOfRevision) || asOfRevision < 0
    || asOfRevision > (metadataHistory?.revision ?? -1)) {
    issues.push(issue('INVALID_METADATA_REVISION', 'asOfRevision',
      'Metadata as-of revision must be an available non-negative transaction revision.'));
  }
  if (issues.length) return {
    state: 'BLOCKED', month, metadataRevision: asOfRevision ?? null,
    sourcePrefix: cloneSourcePrefix(monthRoster.sourcePrefix),
    schoolProfile: null, sectionProfile: null, learners: [], movements: [],
    issues, warnings, operationalDiagnostics,
  };

  const materialized = metadataRecordsAtRevision(metadataHistory, asOfRevision);
  if (materialized.state !== 'OK') {
    issues.push(issue('METADATA_HISTORY_UNTRUSTED', 'metadataHistory',
      'Metadata history is invalid or cannot be materialized at the requested revision.'));
    return {
      state: 'BLOCKED', month, metadataRevision: asOfRevision,
      sourcePrefix: cloneSourcePrefix(monthRoster.sourcePrefix), schoolProfile: null,
      sectionProfile: null, learners: [], movements: [], issues, warnings, operationalDiagnostics,
    };
  }
  // Do not leak or apply diagnostics from unrelated learners/movements outside
  // this report month. Recompute completeness only for selected profiles, the
  // month-overlap roster, and movement facts effective in this month.
  operationalDiagnostics.push(...(Array.isArray(monthRoster.warnings) ? monthRoster.warnings.map((entry) => ({ ...entry })) : []));

  const schoolProfile = profileForMonth(materialized.records, 'SCHOOL_PROFILE', bounds, warnings, issues);
  const sectionProfile = profileForMonth(materialized.records, 'SECTION_PROFILE', bounds, warnings, issues);
  if (schoolProfile) {
    for (const field of ['schoolName', 'schoolId', 'district', 'division', 'region', 'adviserName', 'schoolHeadName']) {
      if (!schoolProfile.data[field]) warnings.push(issue('SCHOOL_FIELD_MISSING', `schoolProfile.${field}`,
        `${field} is missing from the selected school profile.`));
    }
  }
  if (sectionProfile && !sectionProfile.data.academicPeriodLabel) {
    warnings.push(issue('ACADEMIC_PERIOD_LABEL_OPEN', 'sectionProfile.academicPeriodLabel',
      'The workbook Semester/term label has no approved mapping.'));
  }
  const learners = [];
  for (const source of monthRoster.learners) {
    const studentId = source.studentId;
    const segments = intervalsForMonth(source, bounds, studentId, issues);
    const reportingIdentity = identityForSegments(materialized.records, segments, studentId, warnings, issues);
    learners.push({
      studentId,
      enrollmentIntervals: Array.isArray(source.enrollmentIntervals)
        ? source.enrollmentIntervals.map((interval) => isObject(interval) ? { ...interval } : null)
        : [],
      enrolledOnLastDay: source.enrolledOnLastDay === true,
      operational: {
        displayName: source.displayName ?? null,
        state: source.nameState ?? 'UNKNOWN',
      },
      reportingIdentity,
    });
  }

  const monthMovements = materialized.records.filter((record) => record.kind === 'MOVEMENT'
    && record.effectiveDate >= bounds.startIso && record.effectiveDate < bounds.endIso);
  const movementDays = new Map();
  for (const movement of monthMovements) {
    const key = `${movement.subjectId}|${movement.effectiveDate}`;
    const previous = movementDays.get(key);
    if (previous) {
      issues.push(issue('MULTIPLE_MOVEMENTS_SAME_DAY', `movements.${movement.recordId}`,
        `Multiple movement records exist for one learner on ${movement.effectiveDate}; the daily enrollment transition cannot be inferred.`, Number(movement.subjectId)));
    } else {
      movementDays.set(key, movement.recordId);
    }
  }
  const rosterSet = new Set(ids);
  const movements = [];
  for (const movement of monthMovements) {
    const studentId = Number(movement.subjectId);
    if (!rosterSet.has(studentId)) {
      warnings.push(issue('MOVEMENT_FOR_NON_MONTH_LEARNER', `movements.${movement.recordId}`,
        'A movement effective in the month references a learner outside the month-overlap roster.', studentId));
      continue;
    }
    movements.push({
      recordId: movement.recordId,
      studentId,
      effectiveDate: movement.effectiveDate,
      movementType: movement.data.movementType,
      effectiveDateMeaning: movement.data.effectiveDateMeaning,
      reasonCode: movement.data.reasonCode ?? null,
      reasonText: movement.data.reasonText ?? null,
      destinationSchool: movement.data.destinationSchool ?? null,
      destinationSection: movement.data.destinationSection ?? null,
    });
    const rosterLearner = monthRoster.learners.find((item) => item.studentId === studentId);
    if (!movementMatchesEnrollmentBoundary(movement, rosterLearner)) {
      issues.push(issue('MOVEMENT_ROSTER_BOUNDARY_MISMATCH', `movements.${movement.recordId}`,
        'The explicitly stated movement date meaning does not match a stored enrollment interval boundary.', studentId));
    }
    if (movement.data.effectiveDateMeaning === 'UNCONFIRMED') {
      warnings.push(issue('MOVEMENT_DATE_MEANING_UNCONFIRMED', `movements.${movement.recordId}`,
        'Movement date meaning is not confirmed; it has not been reconciled against enrollment interval boundaries.', studentId));
    }
    if (movement.data.movementType === 'NLS') {
      warnings.push(issue('NLS_SEMANTICS_UNCONFIRMED', `movements.${movement.recordId}`,
        'NLS is retained verbatim; its meaning and final form mapping remain unconfirmed.', studentId));
    }
    if (!movement.data.reasonCode && !movement.data.reasonText) {
      warnings.push(issue('MOVEMENT_REASON_MISSING', `movements.${movement.recordId}`,
        'Movement reason is missing.', studentId));
    }
    if (['TRANSFER_OUT', 'TRANSFER_IN'].includes(movement.data.movementType)
      && !movement.data.destinationSchool) {
      warnings.push(issue('MOVEMENT_DESTINATION_MISSING', `movements.${movement.recordId}`,
        'Transfer destination/source school is missing.', studentId));
    }
  }
  movements.sort((a, b) => a.effectiveDate.localeCompare(b.effectiveDate)
    || a.recordId.localeCompare(b.recordId));

  return {
    state: issues.length ? 'BLOCKED' : warnings.length ? 'INCOMPLETE' : 'READY',
    month,
    metadataRevision: asOfRevision,
    sourcePrefix: cloneSourcePrefix(monthRoster.sourcePrefix),
    schoolProfile,
    sectionProfile,
    learners,
    movements,
    issues,
    warnings,
    operationalDiagnostics,
  };
}
