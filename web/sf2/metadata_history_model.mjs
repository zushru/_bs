/*
 * PROPOSED ONLY — bitemporal reporting-metadata reference model.
 * This validates an in-memory JSON representation. It is NOT a persistence
 * format, transaction journal, server API, or authorization boundary.
 */

const ENTITY_KINDS = new Set([
  'SCHOOL_PROFILE', 'SECTION_PROFILE', 'LEARNER_IDENTITY', 'MOVEMENT',
]);
const CURRICULA = new Set(['REGULAR_SHS', 'STRENGTHENED_SHS']);
const MOVEMENT_TYPES = new Set([
  'ENROLLED', 'TRANSFER_IN', 'TRANSFER_OUT', 'SHIFTED', 'WITHDRAWN', 'DROPPED', 'NLS', 'OTHER',
]);
const MOVEMENT_DATE_MEANINGS = new Set([
  'FIRST_ENROLLED_DAY', 'LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY',
  'SECTION_CHANGE_DAY', 'EVENT_DAY', 'UNCONFIRMED',
]);
const EXPECTED_MOVEMENT_DATE_MEANING = Object.freeze({
  ENROLLED: 'FIRST_ENROLLED_DAY', TRANSFER_IN: 'FIRST_ENROLLED_DAY',
  TRANSFER_OUT: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  WITHDRAWN: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  DROPPED: ['LAST_ENROLLED_DAY', 'FIRST_NOT_ENROLLED_DAY'],
  NLS: ['UNCONFIRMED'],
  SHIFTED: ['SECTION_CHANGE_DAY'], OTHER: ['EVENT_DAY'],
});

function object(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function date(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
  const [year, month, day] = value.split('-').map(Number);
  const parsed = new Date(Date.UTC(year, month - 1, day));
  return parsed.getUTCFullYear() === year
    && parsed.getUTCMonth() + 1 === month
    && parsed.getUTCDate() === day;
}

function textOrNull(value) {
  return value === null || value === undefined
    || (typeof value === 'string' && value.trim().length > 0);
}

function add(list, code, path, message) {
  list.push({ code, path, message });
}

function rangeValid(record, path, issues) {
  if (!date(record.validFrom)
    || !(record.validToExclusive === null || date(record.validToExclusive))
    || (record.validToExclusive !== null && record.validToExclusive <= record.validFrom)) {
    add(issues, 'INVALID_VALID_TIME_RANGE', path,
      'validFrom/validToExclusive must form a valid half-open date interval.');
    return false;
  }
  return true;
}

function validateRecord(record, path, issues, warnings) {
  if (!object(record) || typeof record.recordId !== 'string' || !record.recordId.trim()
    || !ENTITY_KINDS.has(record.kind) || typeof record.subjectId !== 'string'
    || !record.subjectId.trim() || !object(record.data)) {
    add(issues, 'INVALID_METADATA_RECORD', path,
      'Each PUT requires recordId, supported kind, subjectId, and data object.');
    return false;
  }

  if (record.kind === 'MOVEMENT') {
    if (!date(record.effectiveDate)) {
      add(issues, 'INVALID_MOVEMENT_DATE', `${path}.effectiveDate`,
        'Movement records require a valid effectiveDate.');
    }
    if (!/^[1-9]\d{0,2}$/.test(record.subjectId)
      || Number(record.subjectId) > 300
      || !MOVEMENT_TYPES.has(record.data.movementType)) {
      add(issues, 'INVALID_MOVEMENT_SUBJECT', path,
        'Movement must reference a device student ID (1..300) and a supported movement type.');
    }
    const meaning = record.data.effectiveDateMeaning;
    if (!MOVEMENT_DATE_MEANINGS.has(meaning)) {
      add(issues, 'INVALID_MOVEMENT_DATE_MEANING', `${path}.data.effectiveDateMeaning`,
        'Movement date meaning must be explicit; use UNCONFIRMED until the school confirms its semantics.');
    } else if (meaning === 'UNCONFIRMED') {
      warnings.push({ code: 'MOVEMENT_DATE_MEANING_UNCONFIRMED', path,
        message: 'Movement effective-date interpretation is not confirmed; do not reconcile it to an enrollment interval automatically.' });
    } else if (meaning !== 'UNCONFIRMED'
      && EXPECTED_MOVEMENT_DATE_MEANING[record.data.movementType]
      && !EXPECTED_MOVEMENT_DATE_MEANING[record.data.movementType].includes(meaning)) {
      add(issues, 'MOVEMENT_DATE_MEANING_MISMATCH', `${path}.data.effectiveDateMeaning`,
        `The selected date meaning does not match the configured movement type ${record.data.movementType}.`);
    }
    if (record.data.movementType === 'NLS') {
      warnings.push({ code: 'NLS_SEMANTICS_UNCONFIRMED', path,
        message: 'NLS is preserved as a raw school-reported category; its exact school meaning and SF2 mapping are not defined.' });
    }
    for (const field of ['reasonCode', 'reasonText', 'destinationSchool', 'destinationSection']) {
      if (!textOrNull(record.data[field])) {
        add(issues, 'INVALID_MOVEMENT_FIELD', `${path}.data.${field}`,
          `${field} must be a non-empty string or null.`);
      }
    }
    if (!record.data.reasonCode && !record.data.reasonText) {
      warnings.push({ code: 'MOVEMENT_REASON_MISSING', path, message: 'Movement reason is not recorded.' });
    }
    if (record.data.movementType === 'TRANSFER_OUT' && !record.data.destinationSchool) {
      warnings.push({ code: 'TRANSFER_DESTINATION_MISSING', path,
        message: 'Transfer-out destination is not recorded.' });
    }
    return true;
  }

  if (!rangeValid(record, path, issues)) return false;

  if (record.kind === 'SCHOOL_PROFILE') {
    for (const field of [
      'schoolName', 'schoolId', 'district', 'division', 'region', 'adviserName', 'schoolHeadName',
    ]) {
      if (!textOrNull(record.data[field])) {
        add(issues, 'INVALID_SCHOOL_FIELD', `${path}.data.${field}`,
          `${field} must be a non-empty string or null.`);
      } else if (!record.data[field]) {
        warnings.push({ code: 'SCHOOL_FIELD_MISSING', path: `${path}.data.${field}`,
          message: `${field} has not been supplied.` });
      }
    }
  } else if (record.kind === 'SECTION_PROFILE') {
    const required = ['profileId', 'schoolYear', 'gradeLevel', 'sectionName'];
    for (const field of required) {
      if (typeof record.data[field] !== 'string' || !record.data[field].trim()) {
        add(issues, 'INVALID_SECTION_FIELD', `${path}.data.${field}`,
          `${field} must be a non-empty string.`);
      }
    }
    if (record.data.profileId !== record.subjectId) {
      add(issues, 'SECTION_SUBJECT_MISMATCH', `${path}.subjectId`,
        'SECTION_PROFILE subjectId must equal data.profileId.');
    }
    if (!CURRICULA.has(record.data.curriculum)) {
      add(issues, 'INVALID_CURRICULUM', `${path}.data.curriculum`,
        'curriculum must be REGULAR_SHS or STRENGTHENED_SHS.');
    }
    if (record.data.gradeLevel !== '11' && record.data.gradeLevel !== '12') {
      add(issues, 'INVALID_GRADE_LEVEL', `${path}.data.gradeLevel`,
        'gradeLevel must be 11 or 12.');
    }
    const schoolYear = typeof record.data.schoolYear === 'string'
      ? /^(\d{4})-(\d{4})$/.exec(record.data.schoolYear) : null;
    if (!schoolYear || Number(schoolYear[2]) !== Number(schoolYear[1]) + 1) {
      add(issues, 'INVALID_SCHOOL_YEAR', `${path}.data.schoolYear`,
        'schoolYear must contain consecutive YYYY-YYYY years.');
    }
    for (const field of ['track', 'strand', 'course', 'academicPeriodLabel']) {
      if (!textOrNull(record.data[field])) {
        add(issues, 'INVALID_SECTION_OPTIONAL_FIELD', `${path}.data.${field}`,
          `${field} must be a non-empty string or null.`);
      }
    }
    if (!record.data.academicPeriodLabel) {
      warnings.push({ code: 'ACADEMIC_PERIOD_LABEL_OPEN', path: `${path}.data.academicPeriodLabel`,
        message: 'Semester/term label remains unresolved; no default is inferred.' });
    }
  } else if (record.kind === 'LEARNER_IDENTITY') {
    const studentId = Number(record.subjectId);
    if (!/^[1-9]\d{0,2}$/.test(record.subjectId) || studentId > 300) {
      add(issues, 'INVALID_LEARNER_SUBJECT', `${path}.subjectId`,
        'LEARNER_IDENTITY subjectId must be an existing device student ID from 1 through 300.');
    }
    if (!object(record.data.legalName)) {
      add(issues, 'LEGAL_REPORTING_NAME_REQUIRED', `${path}.data.legalName`,
        'Reporting identity must use a distinct legalName object; operational roster display names are never a substitute.');
    } else {
      for (const field of ['firstName', 'middleName', 'lastName', 'extensionName']) {
        if (!textOrNull(record.data.legalName[field])) {
          add(issues, 'INVALID_LEGAL_NAME_FIELD', `${path}.data.legalName.${field}`,
            `${field} must be a non-empty string or null.`);
        }
      }
      for (const field of ['firstName', 'lastName']) {
        if (!record.data.legalName[field]) {
          warnings.push({ code: 'LEGAL_REPORTING_NAME_INCOMPLETE', path: `${path}.data.legalName.${field}`,
            message: `${field} is missing; attendance operation remains unaffected, but final reporting is incomplete.` });
        }
      }
    }
    if (!['UNVERIFIED', 'SOURCE_CHECKED'].includes(record.data.sourceStatus)) {
      add(issues, 'INVALID_IDENTITY_SOURCE_STATUS', `${path}.data.sourceStatus`,
        'sourceStatus must be UNVERIFIED or SOURCE_CHECKED.');
    } else if (record.data.sourceStatus === 'UNVERIFIED') {
      warnings.push({ code: 'LEGAL_IDENTITY_SOURCE_UNVERIFIED', path,
        message: 'The reporting identity has not been marked as checked against a school source.' });
    } else {
      warnings.push({ code: 'SOURCE_CHECKED_CLAIM_UNATTESTED', path,
        message: 'SOURCE_CHECKED is a caller-supplied claim; no reviewer identity, timestamp or immutable source-verification evidence is represented.' });
    }
    if (record.data.lrn !== null && record.data.lrn !== undefined
      && typeof record.data.lrn !== 'string') {
      add(issues, 'INVALID_LRN_TYPE', `${path}.data.lrn`,
        'LRN must be text, not a numeric value, to preserve leading zeroes.');
    } else if (record.data.lrn && !/^\d{12}$/.test(record.data.lrn)) {
      add(issues, 'INVALID_LRN', `${path}.data.lrn`,
        'LRN must be stored as a 12-digit string to preserve leading zeroes.');
    }
    if (record.data.sex !== null && record.data.sex !== undefined
      && !['M', 'F'].includes(record.data.sex)) {
      add(issues, 'INVALID_SEX', `${path}.data.sex`, 'sex must be M, F, or null.');
    }
    if (!record.data.lrn) {
      warnings.push({ code: 'LRN_MISSING', path: `${path}.data.lrn`,
        message: 'LRN is missing; final export remains incomplete.' });
    }
    if (!record.data.sex) {
      warnings.push({ code: 'SEX_MISSING', path: `${path}.data.sex`,
        message: 'Sex is missing; male/female report subtotals cannot be finalized.' });
    }
  }
  return true;
}

function activeAtRevision(history, revision, issues) {
  const records = new Map();
  const recordRevision = new Map();
  const revoked = new Set();
  for (let txIndex = 0; txIndex < history.transactions.length; txIndex += 1) {
    const tx = history.transactions[txIndex];
    if (!object(tx) || !Number.isSafeInteger(tx.revision) || tx.committed !== true || !Array.isArray(tx.changes)) continue;
    if (tx.revision > revision) break;
    for (let changeIndex = 0; changeIndex < tx.changes.length; changeIndex += 1) {
      const change = tx.changes[changeIndex];
      const path = `transactions[${txIndex}].changes[${changeIndex}]`;
      if (change?.op === 'PUT' && object(change.record)) {
        records.set(change.record.recordId, change.record);
        recordRevision.set(change.record.recordId, tx.revision);
      } else if (change?.op === 'REVOKE' && typeof change.targetRecordId === 'string') {
        if (!records.has(change.targetRecordId) || revoked.has(change.targetRecordId)
          || recordRevision.get(change.targetRecordId) >= tx.revision) {
          add(issues, 'INVALID_REVOKE_TARGET', `${path}.targetRecordId`,
            'REVOKE must reference a prior, currently active record.');
        } else {
          revoked.add(change.targetRecordId);
        }
      }
    }
  }
  return [...records.values()].filter((record) => !revoked.has(record.recordId));
}

function intersects(aStart, aEnd, bStart, bEnd) {
  const aAfterB = bEnd === null || aStart < bEnd;
  const bAfterA = aEnd === null || bStart < aEnd;
  return aAfterB && bAfterA;
}

function checkActiveIntervals(records, issues) {
  const intervalRecords = records.filter((record) => record.kind !== 'MOVEMENT');
  for (let i = 0; i < intervalRecords.length; i += 1) {
    for (let j = i + 1; j < intervalRecords.length; j += 1) {
      const a = intervalRecords[i]; const b = intervalRecords[j];
      const sameEntity = a.kind === b.kind
        && (a.subjectId === b.subjectId || a.kind === 'SCHOOL_PROFILE' || a.kind === 'SECTION_PROFILE');
      if (sameEntity && intersects(a.validFrom, a.validToExclusive, b.validFrom, b.validToExclusive)) {
        add(issues, 'OVERLAPPING_ACTIVE_METADATA', `records.${b.recordId}`,
          `Active ${b.kind} records for ${b.subjectId} overlap in effective time.`);
      }
      if (a.kind === 'SECTION_PROFILE' && b.kind === 'SECTION_PROFILE'
        && intersects(a.validFrom, a.validToExclusive, b.validFrom, b.validToExclusive)) {
        add(issues, 'MULTIPLE_CONFIGURED_SECTIONS', `records.${b.recordId}`,
          'This device supports only one section profile at a time.');
      }
      if (a.kind === 'LEARNER_IDENTITY' && b.kind === 'LEARNER_IDENTITY'
        && a.subjectId !== b.subjectId && a.data.lrn && a.data.lrn === b.data.lrn) {
        add(issues, 'LRN_REUSED_ACROSS_DEVICE_IDS', `records.${b.recordId}`,
          'The same LRN is assigned to different never-reused device student IDs. LRN identity must not be reassigned across learners.');
      }
    }
  }
}

/** Validate the proposed append-only, bitemporal logical model. */
export function validateMetadataHistory(history, { expectedStudentIds = [] } = {}) {
  const issues = []; const warnings = [];
  if (!object(history) || history.schemaVersion !== 2
    || !Number.isSafeInteger(history.revision) || history.revision < 0
    || !Array.isArray(history.transactions)) {
    return { state: 'INVALID', issues: [{ code: 'INVALID_METADATA_ENVELOPE', path: '',
      message: 'Expected metadata schemaVersion 2, non-negative revision and transactions array.' }], warnings, activeRecords: [] };
  }
  let expectedRevision = 1;
  const recordIds = new Set();
  for (let txIndex = 0; txIndex < history.transactions.length; txIndex += 1) {
    const tx = history.transactions[txIndex]; const path = `transactions[${txIndex}]`;
    if (!object(tx) || tx.revision !== expectedRevision || tx.committed !== true
      || !Array.isArray(tx.changes) || tx.changes.length === 0) {
      add(issues, 'INVALID_METADATA_TRANSACTION', path,
        'Transactions must be non-empty and sequential from revision 1 with no gaps.');
      expectedRevision = Number.isSafeInteger(tx?.revision) ? tx.revision + 1 : expectedRevision + 1;
      continue;
    }
    for (let changeIndex = 0; changeIndex < tx.changes.length; changeIndex += 1) {
      const change = tx.changes[changeIndex]; const changePath = `${path}.changes[${changeIndex}]`;
      if (change?.op === 'PUT') {
        if (validateRecord(change.record, `${changePath}.record`, issues, warnings)) {
          const id = change.record.recordId;
          if (recordIds.has(id)) add(issues, 'DUPLICATE_METADATA_RECORD_ID', `${changePath}.record.recordId`, 'recordId must be globally unique.');
          else recordIds.add(id);
        }
      } else if (change?.op !== 'REVOKE' || typeof change.targetRecordId !== 'string' || !change.targetRecordId.trim()) {
        add(issues, 'INVALID_METADATA_CHANGE', changePath, 'Change must be PUT or REVOKE with a targetRecordId.');
      }
    }
    expectedRevision += 1;
  }
  if (history.revision !== history.transactions.length) {
    add(issues, 'METADATA_REVISION_MISMATCH', 'revision',
      'revision must equal the last committed transaction revision; incomplete transactions are not included.');
  }
  if (!Array.isArray(expectedStudentIds)) {
    add(issues, 'INVALID_EXPECTED_STUDENT_SET', 'expectedStudentIds', 'expectedStudentIds must be an array.');
  } else {
    const seenExpectedStudentIds = new Set();
    for (const studentId of expectedStudentIds) {
      if (Number.isSafeInteger(studentId) && studentId >= 1 && studentId <= 300) {
        if (seenExpectedStudentIds.has(studentId)) {
          add(issues, 'DUPLICATE_EXPECTED_STUDENT_ID', 'expectedStudentIds',
            `Expected student ID ${studentId} appears more than once.`);
        }
        seenExpectedStudentIds.add(studentId);
      }
    }
  }
  const activeRecords = activeAtRevision(history, history.revision, issues);
  checkActiveIntervals(activeRecords, issues);
  const schoolProfiles = activeRecords.filter((record) => record.kind === 'SCHOOL_PROFILE');
  const sectionProfiles = activeRecords.filter((record) => record.kind === 'SECTION_PROFILE');
  if (schoolProfiles.length === 0) warnings.push({ code: 'SCHOOL_PROFILE_NOT_CONFIGURED', path: 'records', message: 'No active school reporting profile exists.' });
  if (sectionProfiles.length === 0) warnings.push({ code: 'SECTION_PROFILE_NOT_CONFIGURED', path: 'records', message: 'The required single-section profile has not been configured.' });
  const activeLearners = new Set(activeRecords.filter((record) => record.kind === 'LEARNER_IDENTITY').map((record) => Number(record.subjectId)));
  for (const studentId of Array.isArray(expectedStudentIds) ? expectedStudentIds : []) {
    if (!Number.isSafeInteger(studentId) || studentId < 1 || studentId > 300) {
      add(issues, 'INVALID_EXPECTED_STUDENT_ID', 'expectedStudentIds', 'Expected student IDs must be integers in 1..300.');
    } else if (!activeLearners.has(studentId)) {
      warnings.push({ code: 'REPORTING_IDENTITY_MISSING', path: `studentId:${studentId}`,
        message: `Student ID ${studentId} has no active reporting-identity record.` });
    }
  }
  const state = issues.length ? 'INVALID' : warnings.length ? 'INCOMPLETE' : 'VALID';
  return { state, issues, warnings, revision: history.revision, activeRecords };
}


/** Materialize active metadata records once at an explicit revision. */
export function metadataRecordsAtRevision(history, revision = history?.revision) {
  if (!Number.isSafeInteger(revision) || revision < 0
    || !Number.isSafeInteger(history?.revision) || revision > history.revision) {
    return { state: 'INVALID_QUERY', records: [], validation: null };
  }
  const validation = validateMetadataHistory(history);
  if (validation.state === 'INVALID') {
    return { state: 'INVALID_HISTORY', records: [], validation };
  }
  const issues = [];
  const records = activeAtRevision(history, revision, issues);
  // Validate uniqueness/overlap against the selected historical revision, not
  // only the latest state. A later REVOKE must not make an older report revision
  // appear safe if it contained duplicate LRNs or overlapping profiles.
  checkActiveIntervals(records, issues);
  if (issues.length) {
    return { state: 'INVALID_HISTORY', records: [], validation: { ...validation, issues: [...validation.issues, ...issues] } };
  }
  return { state: 'OK', records, validation };
}

/** Resolve one entity's valid-time record at an explicit transaction revision. */
export function resolveMetadataAt(history, { kind, subjectId, effectiveDate, asOfRevision = history?.revision } = {}) {
  const validSubject = (typeof subjectId === 'string' && subjectId.trim().length > 0)
    || Number.isSafeInteger(subjectId);
  if (!ENTITY_KINDS.has(kind) || kind === 'MOVEMENT' || !validSubject
    || !date(effectiveDate) || !Number.isSafeInteger(asOfRevision)
    || asOfRevision < 0 || asOfRevision > (history?.revision ?? -1)) {
    return { state: 'INVALID_QUERY', record: null };
  }
  const materialized = metadataRecordsAtRevision(history, asOfRevision);
  if (materialized.state !== 'OK') {
    return { state: 'INVALID_HISTORY', record: null, validation: materialized.validation };
  }
  const matches = materialized.records.filter((record) => record.kind === kind && record.subjectId === String(subjectId)
    && record.kind !== 'MOVEMENT' && record.validFrom <= effectiveDate
    && (record.validToExclusive === null || effectiveDate < record.validToExclusive));
  if (matches.length > 1) return { state: 'AMBIGUOUS', record: null };
  return { state: matches.length ? 'FOUND' : 'MISSING', record: matches[0] ?? null };
}

/** List movement events effective within [monthStart, nextMonthStart) as of a revision. */
export function movementHistoryForMonth(history, { studentIds, month, asOfRevision = history?.revision } = {}) {
  if (!/^\d{4}-(0[1-9]|1[0-2])$/.test(month ?? '')
    || !Number.isSafeInteger(asOfRevision) || asOfRevision < 0
    || asOfRevision > (history?.revision ?? -1) || !Array.isArray(studentIds)
    || studentIds.some((id) => !(Number.isSafeInteger(id) && id >= 1 && id <= 300))) {
    return { state: 'INVALID_QUERY', movements: [] };
  }
  const [year, number] = month.split('-').map(Number);
  const nextMonth = number === 12 ? `${year + 1}-01-01` : `${year}-${String(number + 1).padStart(2, '0')}-01`;
  const allowed = new Set(studentIds.map(String));
  const materialized = metadataRecordsAtRevision(history, asOfRevision);
  if (materialized.state !== 'OK') {
    return { state: 'INVALID_HISTORY', movements: [], validation: materialized.validation };
  }
  const movements = materialized.records
    .filter((record) => record.kind === 'MOVEMENT' && allowed.has(record.subjectId)
      && record.effectiveDate >= `${month}-01` && record.effectiveDate < nextMonth)
    .sort((a, b) => a.effectiveDate.localeCompare(b.effectiveDate)
      || a.recordId.localeCompare(b.recordId));
  return { state: 'OK', movements };
}
