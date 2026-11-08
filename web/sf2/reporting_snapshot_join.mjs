/*
 * D1 -> D2 -> proposed snapshot roster join.
 *
 * This is a reference-model boundary only. It deliberately never returns
 * authoritativeReady=true: no authenticated reviewer evidence verifier or
 * trusted identity-attestation source exists in the current project.
 */

import { cloneSourcePrefix, isValidSourcePrefix, sameSourcePrefix } from './source_prefix.mjs';

const MAX_ID = 300;
const OPEN_DAY = 0x7fffffff;
const DAY_MS = 86400000;
const CURRICULA = new Set(['REGULAR_SHS', 'STRENGTHENED_SHS']);

function issue(code, path, message, studentId = null) {
  return { code, path, message, studentId };
}

function object(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function nonEmptyText(value) {
  return typeof value === 'string' && value.trim().length > 0;
}

function nullableText(value) {
  return value === null || value === undefined || nonEmptyText(value);
}

function validMonth(month) {
  return typeof month === 'string' && /^\d{4}-(0[1-9]|1[0-2])$/.test(month);
}

function validIsoDate(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
  const epoch = Date.parse(`${value}T00:00:00Z`);
  return Number.isFinite(epoch) && new Date(epoch).toISOString().slice(0, 10) === value;
}

function monthRange(month) {
  const [year, monthNumber] = month.split('-').map(Number);
  const start = `${month}-01`;
  const end = monthNumber === 12 ? `${year + 1}-01-01`
    : `${year}-${String(monthNumber + 1).padStart(2, '0')}-01`;
  return { start, end, startDay: Date.parse(`${start}T00:00:00Z`) / DAY_MS,
    endDay: Date.parse(`${end}T00:00:00Z`) / DAY_MS };
}

function dayToIso(day) {
  if (!Number.isSafeInteger(day) || day < 0 || day >= OPEN_DAY) return null;
  return new Date(day * DAY_MS).toISOString().slice(0, 10);
}

function normalizeIntervals(intervals, studentId, path, monthBounds, issues) {
  if (!Array.isArray(intervals) || intervals.length === 0) {
    issues.push(issue('ENROLLMENT_INTERVALS_MISSING', path,
      'Every month-overlap learner must retain at least one enrollment interval.', studentId));
    return [];
  }
  const result = [];
  for (const [index, interval] of intervals.entries()) {
    const start = interval?.startDayInclusive;
    const hasEnd = object(interval) && Object.hasOwn(interval, 'endDayExclusive');
    const end = interval?.endDayExclusive;
    const startIso = dayToIso(start);
    const endIso = end === null ? null : dayToIso(end);
    if (!object(interval) || !Object.hasOwn(interval, 'startDayInclusive') || !hasEnd
      || !startIso || !(end === null || (endIso && end > start))) {
      issues.push(issue('INVALID_JOINED_ENROLLMENT_INTERVAL', `${path}[${index}]`,
        'Enrollment interval is not a valid half-open local-day range.', studentId));
      continue;
    }
    const intervalEnd = end === null ? OPEN_DAY : end;
    if (!(start < monthBounds.endDay && intervalEnd > monthBounds.startDay)) {
      issues.push(issue('LEARNER_INTERVAL_NOT_MONTH_OVERLAP', `${path}[${index}]`,
        'Every interval retained in the month-overlap roster must intersect the requested report month.', studentId));
    }
    const previous = result.at(-1);
    if (previous && (start < previous.startDayInclusive
      || (previous.endDayExclusive ?? OPEN_DAY) > start)) {
      issues.push(issue('OVERLAPPING_OR_UNSORTED_JOINED_INTERVALS', path,
        'Intervals must be in start-day order and must not overlap; adjacent half-open intervals are allowed.', studentId));
    }
    result.push({ startDayInclusive: start, endDayExclusive: end,
      startInclusive: startIso, endExclusive: endIso });
  }
  return result;
}

function sameIntervals(a, b) {
  if (a.length !== b.length) return false;
  return a.every((interval, index) => interval.startDayInclusive === b[index].startDayInclusive
    && interval.endDayExclusive === b[index].endDayExclusive);
}

function mapProfile(profile, revision, kind, month, issues) {
  const range = monthRange(month);
  if (kind === 'SCHOOL' && profile === null) {
    return { revision, schoolName: null, schoolId: null, district: null,
      division: null, region: null, adviserName: null, schoolHeadName: null };
  }
  if (!object(profile) || !object(profile.data) || !nonEmptyText(profile.recordId)
    || !validIsoDate(profile.validFrom)
    || !(profile.validToExclusive === null || validIsoDate(profile.validToExclusive))
    || (profile.validToExclusive !== null && profile.validToExclusive <= profile.validFrom)
    || profile.validFrom > range.start
    || (profile.validToExclusive !== null && profile.validToExclusive < range.end)) {
    issues.push(issue(`${kind}_PROFILE_UNAVAILABLE`, kind,
      `A valid month-covering ${kind.toLowerCase()} profile is required for snapshot mapping.`));
    return null;
  }
  if (kind === 'SECTION') {
    const data = profile.data;
    const schoolYear = typeof data.schoolYear === 'string'
      ? /^(\d{4})-(\d{4})$/.exec(data.schoolYear) : null;
    const optionalFields = ['track', 'strand', 'course', 'academicPeriodLabel'];
    if (!nonEmptyText(data.profileId) || !CURRICULA.has(data.curriculum)
      || !schoolYear || Number(schoolYear[2]) !== Number(schoolYear[1]) + 1
      || !['11', '12'].includes(data.gradeLevel) || !nonEmptyText(data.sectionName)
      || optionalFields.some((field) => !nullableText(data[field]))) {
      issues.push(issue('SECTION_PROFILE_FIELDS_INVALID', 'reportingRoster.sectionProfile.data',
        'Section profile fields must satisfy the proposed snapshot contract before mapping.'));
      return null;
    }
    return {
      profileId: data.profileId,
      revision,
      curriculum: data.curriculum,
      schoolYear: data.schoolYear,
      gradeLevel: data.gradeLevel,
      sectionName: data.sectionName,
      track: data.track ?? null,
      strand: data.strand ?? null,
      course: data.course ?? null,
      semesterLabel: data.academicPeriodLabel ?? null,
      effectiveFrom: profile.validFrom,
      effectiveToExclusive: profile.validToExclusive ?? null,
    };
  }
  const schoolFields = ['schoolName', 'schoolId', 'district', 'division', 'region', 'adviserName', 'schoolHeadName'];
  if (schoolFields.some((field) => !nullableText(profile.data[field]))) {
    issues.push(issue('SCHOOL_PROFILE_FIELDS_INVALID', 'reportingRoster.schoolProfile.data',
      'School profile fields must be non-empty strings or null before mapping.'));
    return null;
  }
  return {
    revision,
    schoolName: profile.data.schoolName ?? null,
    schoolId: profile.data.schoolId ?? null,
    district: profile.data.district ?? null,
    division: profile.data.division ?? null,
    region: profile.data.region ?? null,
    adviserName: profile.data.adviserName ?? null,
    schoolHeadName: profile.data.schoolHeadName ?? null,
  };
}

/**
 * Join the exact D1 prefix projection with D2 identity/movement output and map
 * it to the learner/profile/movement portion of the proposed snapshot schema.
 * This function is intentionally fail-closed and preview-only.
 */
export function joinReportingRosterForSnapshot({ month, monthRoster, reportingRoster } = {}) {
  const issues = [];
  const warnings = [];
  if (!validMonth(month)) issues.push(issue('INVALID_REPORT_MONTH', 'month', 'month must be YYYY-MM.'));
  if (!object(monthRoster) || monthRoster.state !== 'READY' || !Array.isArray(monthRoster.learners)
    || !isValidSourcePrefix(monthRoster.sourcePrefix)) {
    issues.push(issue('D1_MONTH_ROSTER_NOT_READY', 'monthRoster',
      'A READY D1 month roster with a valid exact-source prefix is required.'));
  }
  if (!object(reportingRoster) || !['READY', 'INCOMPLETE'].includes(reportingRoster.state)
    || !Array.isArray(reportingRoster.learners) || !Array.isArray(reportingRoster.movements)
    || !Array.isArray(reportingRoster.issues) || !Array.isArray(reportingRoster.warnings)) {
    issues.push(issue('D2_REPORTING_ROSTER_INVALID', 'reportingRoster',
      'A structurally available D2 reporting roster is required.'));
  }
  if (issues.length) return {
    state: 'BLOCKED', authoritativeReady: false, month,
    snapshotFragment: null, issues, warnings,
  };

  if (reportingRoster.month !== month) {
    issues.push(issue('REPORT_MONTH_MISMATCH', 'reportingRoster.month',
      'D1 and D2 projections must refer to the same report month.'));
  }
  if (!sameSourcePrefix(monthRoster.sourcePrefix, reportingRoster.sourcePrefix)) {
    issues.push(issue('SOURCE_PREFIX_MISMATCH', 'reportingRoster.sourcePrefix',
      'D1 roster intervals and D2 identity/movement projection must carry the exact same C++ source prefix.'));
  }
  if (!Number.isSafeInteger(reportingRoster.metadataRevision) || reportingRoster.metadataRevision < 0) {
    issues.push(issue('METADATA_REVISION_INVALID', 'reportingRoster.metadataRevision',
      'D2 must carry one explicit non-negative metadata revision.'));
  }
  if (reportingRoster.issues?.length) {
    issues.push(issue('D2_HAS_BLOCKING_ISSUES', 'reportingRoster.issues',
      'D2 reports blocking identity or movement issues; no snapshot roster can be assembled.'));
  }

  const range = monthRange(month);
  if (monthRoster.monthStartDay !== range.startDay || monthRoster.monthEndExclusive !== range.endDay) {
    issues.push(issue('D1_MONTH_RANGE_MISMATCH', 'monthRoster',
      'D1 day boundaries must exactly match the requested calendar month.'));
  }

  const d1ById = new Map();
  for (const [index, learner] of monthRoster.learners.entries()) {
    const id = learner?.studentId;
    if (!Number.isSafeInteger(id) || id < 1 || id > MAX_ID || d1ById.has(id)) {
      issues.push(issue('D1_STUDENT_ID_INVALID', `monthRoster.learners[${index}]`,
        'D1 learner IDs must be unique integers in 1..300.', id ?? null));
      continue;
    }
    d1ById.set(id, learner);
  }

  const d2ById = new Map();
  for (const [index, learner] of reportingRoster.learners.entries()) {
    const id = learner?.studentId;
    if (!Number.isSafeInteger(id) || id < 1 || id > MAX_ID || d2ById.has(id)) {
      issues.push(issue('D2_STUDENT_ID_INVALID', `reportingRoster.learners[${index}]`,
        'D2 learner IDs must be unique integers in 1..300.', id ?? null));
      continue;
    }
    d2ById.set(id, learner);
  }
  if (d1ById.size !== d2ById.size || [...d1ById.keys()].some((id) => !d2ById.has(id))) {
    issues.push(issue('MONTH_ROSTER_ID_SET_MISMATCH', 'reportingRoster.learners',
      'D1 and D2 must contain exactly the same month-overlap learner IDs.'));
  }

  const metadataRevision = reportingRoster.metadataRevision;
  const revision = Number.isSafeInteger(metadataRevision) ? `metadata-${metadataRevision}` : null;
  const sectionProfile = mapProfile(reportingRoster.sectionProfile, revision, 'SECTION', month, issues);
  const schoolProfile = mapProfile(reportingRoster.schoolProfile, revision, 'SCHOOL', month, issues);
  const learners = [];
  const intervalsById = new Map();

  for (const [studentId, d1Learner] of d1ById) {
    const d2Learner = d2ById.get(studentId);
    if (!d2Learner) continue;
    const d1Intervals = normalizeIntervals(d1Learner.enrollmentIntervals, studentId,
      `monthRoster.learners.${studentId}.enrollmentIntervals`, range, issues);
    const d2Intervals = normalizeIntervals(d2Learner.enrollmentIntervals, studentId,
      `reportingRoster.learners.${studentId}.enrollmentIntervals`, range, issues);
    intervalsById.set(studentId, d1Intervals);
    if (!sameIntervals(d1Intervals, d2Intervals)) {
      issues.push(issue('ENROLLMENT_INTERVAL_JOIN_MISMATCH', `learners.${studentId}.enrollmentIntervals`,
        'D2 interval data must exactly reproduce D1 enrollment intervals; it cannot silently alter the roster.', studentId));
    }
    const identity = d2Learner.reportingIdentity;
    if (!object(identity) || !object(identity.legalName)) {
      issues.push(issue('LEGAL_REPORTING_IDENTITY_MISSING', `learners.${studentId}.identity`,
        'D2 legal reporting identity is required; operational display labels are never a fallback.', studentId));
      continue;
    }
    if (identity.sourceStatus !== 'UNVERIFIED') {
      issues.push(issue('UNSUPPORTED_IDENTITY_VERIFICATION_STATE', `learners.${studentId}.reportingIdentity.sourceStatus`,
        'No authenticated identity evidence verifier exists; caller-supplied VERIFIED states are rejected.', studentId));
    }
    const legalName = identity.legalName;
    if (!nonEmptyText(legalName.firstName) || !nonEmptyText(legalName.lastName)
      || !nullableText(legalName.middleName) || !nullableText(legalName.extensionName)
      || !(identity.lrn === null || identity.lrn === undefined
        || (typeof identity.lrn === 'string' && /^\d{12}$/.test(identity.lrn)))
      || !['M', 'F'].includes(identity.sex)) {
      issues.push(issue('LEGAL_IDENTITY_FIELDS_INVALID', `learners.${studentId}.reportingIdentity`,
        'Legal first/last name, optional name components, LRN and sex must satisfy the proposed snapshot schema.', studentId));
    }
    if (!sectionProfile?.profileId) {
      issues.push(issue('SECTION_PROFILE_ID_MISSING', 'reportingRoster.sectionProfile',
        'A configured section profile ID is required to bind enrollment intervals.', studentId));
    }
    learners.push({
      studentId,
      sex: identity.sex ?? null,
      identity: {
        firstName: identity.legalName.firstName ?? null,
        middleName: identity.legalName.middleName ?? null,
        lastName: identity.legalName.lastName ?? null,
        extensionName: identity.legalName.extensionName ?? null,
        lrn: identity.lrn ?? null,
      },
      enrollmentPeriods: d1Intervals.map((interval) => ({
        profileId: sectionProfile?.profileId ?? null,
        startInclusive: interval.startInclusive,
        endExclusive: interval.endExclusive,
      })),
    });
  }

  const movementHistory = [];
  const movementKeys = new Set();
  const movementTypes = new Set(['ENROLLED', 'TRANSFER_IN', 'TRANSFER_OUT', 'SHIFTED',
    'WITHDRAWN', 'DROPPED', 'NLS', 'OTHER']);
  const movementMeanings = new Set(['FIRST_ENROLLED_DAY', 'LAST_ENROLLED_DAY',
    'FIRST_NOT_ENROLLED_DAY', 'SECTION_CHANGE_DAY', 'EVENT_DAY', 'UNCONFIRMED']);
  const movementDateRules = {
    ENROLLED: ['FIRST_ENROLLED_DAY'], TRANSFER_IN: ['FIRST_ENROLLED_DAY'],
    TRANSFER_OUT: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
    WITHDRAWN: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
    DROPPED: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
    SHIFTED: ['SECTION_CHANGE_DAY'], NLS: ['UNCONFIRMED'], OTHER: ['EVENT_DAY'],
  };
  for (const [index, movement] of reportingRoster.movements.entries()) {
    const studentId = movement?.studentId;
    const date = movement?.effectiveDate;
    if (!object(movement) || !Number.isSafeInteger(studentId) || studentId < 1 || studentId > MAX_ID
      || !d1ById.has(studentId) || !validIsoDate(date) || !date.startsWith(`${month}-`)
      || !movementTypes.has(movement.movementType) || !movementMeanings.has(movement.effectiveDateMeaning)) {
      issues.push(issue('MOVEMENT_JOIN_INVALID', `reportingRoster.movements[${index}]`,
        'Movement must reference a month-overlap learner and a supported date/type/meaning inside the report month.',
        Number.isSafeInteger(studentId) ? studentId : null));
      continue;
    }
    const meaning = movement.effectiveDateMeaning;
    if (meaning !== 'UNCONFIRMED' && !movementDateRules[movement.movementType]?.includes(meaning)) {
      issues.push(issue('MOVEMENT_DATE_MEANING_MISMATCH', `reportingRoster.movements[${index}].effectiveDateMeaning`,
        'Movement date meaning is incompatible with its movement type.', studentId));
      continue;
    }
    const movementDay = Date.parse(`${date}T00:00:00Z`) / DAY_MS;
    const intervals = intervalsById.get(studentId) ?? [];
    const boundaryMatches = meaning === 'UNCONFIRMED' || meaning === 'EVENT_DAY'
      || (meaning === 'FIRST_ENROLLED_DAY' && intervals.some((interval) => interval.startDayInclusive === movementDay))
      || (meaning === 'FIRST_NOT_ENROLLED_DAY' && intervals.some((interval) => interval.endDayExclusive === movementDay))
      || (meaning === 'LAST_ENROLLED_DAY' && intervals.some((interval) => interval.endDayExclusive === movementDay + 1))
      || (meaning === 'SECTION_CHANGE_DAY' && intervals.some((interval) => interval.startDayInclusive === movementDay
        || interval.endDayExclusive === movementDay));
    if (!boundaryMatches) {
      issues.push(issue('MOVEMENT_ROSTER_BOUNDARY_MISMATCH', `reportingRoster.movements[${index}]`,
        'Movement effective-date meaning does not match a D1 enrollment interval boundary.', studentId));
      continue;
    }
    const key = `${studentId}|${date}`;
    if (movementKeys.has(key)) {
      issues.push(issue('MOVEMENT_JOIN_COLLISION', `reportingRoster.movements[${index}]`,
        'Multiple movements for one learner on one effective date remain ambiguous.', studentId));
      continue;
    }
    movementKeys.add(key);
    for (const field of ['reasonCode', 'reasonText', 'destinationSchool', 'destinationSection']) {
      if (!(movement[field] === null || movement[field] === undefined
        || (typeof movement[field] === 'string' && movement[field].trim().length > 0))) {
        issues.push(issue('MOVEMENT_JOIN_FIELD_INVALID', `reportingRoster.movements[${index}].${field}`,
          `${field} must be a string or null.`, studentId));
      }
    }
    movementHistory.push({
      studentId,
      date,
      type: movement.movementType,
      effectiveDateMeaning: movement.effectiveDateMeaning,
      reasonCode: movement.reasonCode ?? null,
      reasonText: movement.reasonText ?? null,
      destinationSchool: movement.destinationSchool ?? null,
      destinationSection: movement.destinationSection ?? null,
    });
  }

  if (issues.length) return {
    state: 'BLOCKED', authoritativeReady: false, month,
    snapshotFragment: null, issues, warnings,
  };

  warnings.push(issue('AUTHENTICATED_IDENTITY_REVIEW_MISSING', 'reportingRoster.learners',
    'The D2 model has no authenticated reviewer evidence, immutable verification event or trusted verifier. The joined data is preview-only.'));
  warnings.push(...(Array.isArray(reportingRoster.warnings) ? reportingRoster.warnings.map((entry) => ({ ...entry })) : []));

  return {
    state: 'INCOMPLETE',
    authoritativeReady: false,
    month,
    sourcePrefix: cloneSourcePrefix(monthRoster.sourcePrefix),
    metadataRevision,
    snapshotFragment: {
      reportingProfile: sectionProfile,
      schoolProfile,
      roster: {
        revision: `log-${monthRoster.sourcePrefix.openGeneration}-${monthRoster.sourcePrefix.recordCount}-${monthRoster.sourcePrefix.lastSeq}-audit-${monthRoster.sourcePrefix.auditGeneration}`,
      },
      learnerMetadata: { revision },
      sourceRevision: {
        rosterRevision: `log-${monthRoster.sourcePrefix.openGeneration}-${monthRoster.sourcePrefix.recordCount}-${monthRoster.sourcePrefix.lastSeq}-audit-${monthRoster.sourcePrefix.auditGeneration}`,
        reportingProfileRevision: sectionProfile.revision,
        schoolProfileRevision: schoolProfile.revision,
        learnerMetadataRevision: revision,
        movementRevision: revision,
      },
      learners,
      movementHistory,
    },
    issues: [],
    warnings,
  };
}
