import { isValidOpenGeneration, isValidSourcePrefix, sameSourcePrefix } from './source_prefix.mjs';

const MAX_U32 = 0xFFFFFFFF;
const TOKEN_MAX = 128;
const REVISION_KEYS = Object.freeze([
  'logOpenGeneration', 'logRecordCount', 'logLastSeq', 'eventAuditGeneration',
  'configGeneration', 'rosterRevision', 'calendarRevision',
  'reportingProfileRevision', 'schoolProfileRevision',
  'learnerMetadataRevision', 'movementRevision',
]);
const COMPLETENESS_KEYS = Object.freeze([
  'rosterHistoryComplete', 'rosterTransitionAuditValid',
  'calendarHistoryComplete', 'scheduleHistoryComplete', 'metadataHistoryComplete',
]);

function object(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function token(value) {
  return typeof value === 'string' && value.trim().length > 0 && value.length <= TOKEN_MAX;
}

function safeNonNegative(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

function sameVector(a, b) {
  if (!object(a) || !object(b)) return false;
  for (const key of REVISION_KEYS) if (a[key] !== b[key]) return false;
  for (const key of COMPLETENESS_KEYS) if (a.completeness?.[key] !== b.completeness?.[key]) return false;
  return true;
}

function validateVector(vector) {
  const issues = [];
  if (!object(vector)) return { valid: false, issues: ['REVISION_VECTOR_REQUIRED'] };
  if (!isValidOpenGeneration(vector.logOpenGeneration)) issues.push('INVALID_LOG_OPENGENERATION');
  if (!Number.isSafeInteger(vector.eventAuditGeneration) || vector.eventAuditGeneration < 1
    || vector.eventAuditGeneration > MAX_U32) issues.push('INVALID_EVENTAUDIT_GENERATION');
  for (const key of ['logRecordCount', 'logLastSeq']) {
    if (!Number.isSafeInteger(vector[key]) || vector[key] < 0 || vector[key] > MAX_U32) {
      issues.push(`INVALID_${key.toUpperCase()}`);
    }
  }
  if (vector.logRecordCount !== vector.logLastSeq) issues.push('LOG_COUNT_SEQUENCE_MISMATCH');
  if (!safeNonNegative(vector.configGeneration)) issues.push('INVALID_CONFIG_GENERATION');
  for (const key of REVISION_KEYS.filter((key) => key.endsWith('Revision'))) {
    if (!token(vector[key])) issues.push(`INVALID_${key.toUpperCase()}`);
  }
  if (!object(vector.completeness)) issues.push('COMPLETENESS_REQUIRED');
  else for (const key of COMPLETENESS_KEYS) {
    if (typeof vector.completeness[key] !== 'boolean') issues.push(`INVALID_${key.toUpperCase()}`);
  }
  return { valid: issues.length === 0, issues };
}

/**
 * Host-side model of one serialized source-revision capture. It is not a device
 * lock, HTTP implementation, authentication boundary, or persistent ticket store.
 */
export function captureSnapshotRevisionVector(sources) {
  const issues = [];
  if (!object(sources) || !object(sources.log) || !object(sources.eventAudit)
    || !object(sources.roster) || !object(sources.config) || !object(sources.calendar)
    || !object(sources.reportingProfile) || !object(sources.schoolProfile)
    || !object(sources.learnerMetadata) || !object(sources.movements)) {
    return { state: 'INVALID', issues: ['ALL_SOURCE_SNAPSHOTS_REQUIRED'], vector: null };
  }

  const { log, eventAudit, roster } = sources;
  if (eventAudit.attached !== true || eventAudit.ready !== true) issues.push('EVENT_AUDIT_NOT_READY');
  if (!isValidOpenGeneration(log.openGeneration) || !isValidOpenGeneration(eventAudit.openGeneration)
    || eventAudit.openGeneration !== log.openGeneration) issues.push('AUDIT_OPEN_GENERATION_MISMATCH');
  if (eventAudit.recordCount !== log.recordCount || eventAudit.lastSeq !== log.lastSeq) {
    issues.push('AUDIT_LOG_PREFIX_MISMATCH');
  }
  const prefix = roster.prefix;
  const expectedPrefix = {
    openGeneration: log.openGeneration,
    recordCount: log.recordCount,
    lastSeq: log.lastSeq,
    auditGeneration: eventAudit.generation,
  };
  if (!isValidSourcePrefix(prefix) || !sameSourcePrefix(prefix, expectedPrefix)) {
    issues.push('ROSTER_PREFIX_MISMATCH');
  }
  if (roster.historyComplete !== true) issues.push('ROSTER_HISTORY_INCOMPLETE');
  if (roster.transitionAuditState !== 'VALID') issues.push('ROSTER_TRANSITION_AUDIT_NOT_VALID');

  const completeness = {
    rosterHistoryComplete: roster.historyComplete === true,
    rosterTransitionAuditValid: roster.transitionAuditState === 'VALID',
    calendarHistoryComplete: sources.calendar.historyComplete === true,
    scheduleHistoryComplete: sources.calendar.scheduleHistoryComplete === true,
    metadataHistoryComplete: sources.reportingProfile.complete === true
      && sources.schoolProfile.complete === true
      && sources.learnerMetadata.complete === true
      && sources.movements.complete === true,
  };
  const vector = {
    logOpenGeneration: log.openGeneration,
    logRecordCount: log.recordCount,
    logLastSeq: log.lastSeq,
    eventAuditGeneration: eventAudit.generation,
    configGeneration: sources.config.generation,
    rosterRevision: `log-${log.openGeneration}-${log.recordCount}-${log.lastSeq}-audit-${eventAudit.generation}`,
    calendarRevision: sources.calendar.revision,
    reportingProfileRevision: sources.reportingProfile.revision,
    schoolProfileRevision: sources.schoolProfile.revision,
    learnerMetadataRevision: sources.learnerMetadata.revision,
    movementRevision: sources.movements.revision,
    completeness,
  };
  const vectorValidation = validateVector(vector);
  issues.push(...vectorValidation.issues);
  if (issues.length) return { state: 'INVALID', issues: [...new Set(issues)], vector: null };
  const finalizable = Object.values(completeness).every(Boolean);
  return { state: finalizable ? 'READY' : 'INCOMPLETE', issues: [], vector, finalizable };
}

/** Compare two serialized captures; wall-clock timestamps are intentionally unused. */
export function compareSnapshotRevisionVectors(before, after) {
  const a = validateVector(before); const b = validateVector(after);
  if (!a.valid || !b.valid) return { state: 'INVALID', issues: [...a.issues, ...b.issues] };
  return sameVector(before, after)
    ? { state: 'UNCHANGED', issues: [] }
    : { state: 'STALE', issues: ['SOURCE_REVISION_CHANGED'] };
}

export function beginSnapshotTicket({ ticketId, month, revisionVector, totalItems, pageSize,
  createdMonotonicMs, ttlMs } = {}) {
  const issues = [];
  if (!token(ticketId)) issues.push('INVALID_TICKET_ID');
  if (typeof month !== 'string' || !/^\d{4}-(0[1-9]|1[0-2])$/.test(month)) issues.push('INVALID_MONTH');
  const vectorResult = validateVector(revisionVector);
  issues.push(...vectorResult.issues);
  if (!Number.isSafeInteger(totalItems) || totalItems < 0) issues.push('INVALID_TOTAL_ITEMS');
  if (!Number.isSafeInteger(pageSize) || pageSize < 1 || pageSize > 1024) issues.push('INVALID_PAGE_SIZE');
  if (!Number.isSafeInteger(createdMonotonicMs) || createdMonotonicMs < 0) issues.push('INVALID_MONOTONIC_CREATE_TIME');
  if (!Number.isSafeInteger(ttlMs) || ttlMs < 1 || ttlMs > 0x7FFFFFFF) issues.push('INVALID_TTL');
  if (issues.length) return { state: 'INVALID', issues: [...new Set(issues)], ticket: null };

  const expiresMonotonicMs = createdMonotonicMs + ttlMs;
  if (!Number.isSafeInteger(expiresMonotonicMs)) return { state: 'INVALID', issues: ['EXPIRY_OVERFLOW'], ticket: null };
  const pageCount = Math.ceil(totalItems / pageSize);
  const ticket = Object.freeze({
    ticketId, month, revisionVector: Object.freeze({ ...revisionVector,
      completeness: Object.freeze({ ...revisionVector.completeness }) }),
    totalItems, pageSize, pageCount, createdMonotonicMs, expiresMonotonicMs,
    finalizable: COMPLETENESS_KEYS.every((key) => revisionVector.completeness[key] === true),
  });
  return { state: 'READY', issues: [], ticket };
}

/**
 * Pure model of bounded page assembly. A caller must re-read the current vector
 * before every page acceptance and finalization. Any change invalidates all pages.
 */
export class SnapshotPageAssembler {
  constructor(ticket) {
    this.ticket = ticket;
    this.state = ticket ? 'OPEN' : 'INVALID';
    this.pages = new Map();
    this.itemKeys = new Set();
    this.failureCode = ticket ? null : 'TICKET_REQUIRED';
  }

  fail(state, code) {
    this.state = state;
    this.failureCode = code;
    this.pages.clear();
    this.itemKeys.clear();
    return { state, code };
  }

  checkCurrent(currentVector, nowMonotonicMs) {
    if (this.state !== 'OPEN') return { state: this.state, code: this.failureCode ?? 'NOT_OPEN' };
    if (!Number.isSafeInteger(nowMonotonicMs) || nowMonotonicMs < 0) return this.fail('INVALID', 'INVALID_MONOTONIC_NOW');
    if (nowMonotonicMs >= this.ticket.expiresMonotonicMs) return this.fail('EXPIRED', 'TICKET_EXPIRED');
    const comparison = compareSnapshotRevisionVectors(this.ticket.revisionVector, currentVector);
    if (comparison.state !== 'UNCHANGED') return this.fail('STALE', 'SOURCE_REVISION_CHANGED');
    return { state: 'OPEN', code: null };
  }

  addPage(page, { currentVector, nowMonotonicMs } = {}) {
    const current = this.checkCurrent(currentVector, nowMonotonicMs);
    if (current.state !== 'OPEN') return current;
    if (!object(page) || page.ticketId !== this.ticket.ticketId
      || page.pageCount !== this.ticket.pageCount
      || !Number.isSafeInteger(page.pageIndex) || page.pageIndex < 0
      || page.pageIndex >= this.ticket.pageCount
      || !sameVector(page.revisionVector, this.ticket.revisionVector)
      || !Array.isArray(page.items)) return this.fail('INVALID', 'INVALID_PAGE_ENVELOPE');
    if (this.pages.has(page.pageIndex)) return this.fail('INVALID', 'DUPLICATE_PAGE');

    const start = page.pageIndex * this.ticket.pageSize;
    const expectedItems = Math.min(this.ticket.pageSize, this.ticket.totalItems - start);
    if (page.items.length !== expectedItems) return this.fail('INVALID', 'PAGE_ITEM_COUNT_MISMATCH');
    const localKeys = new Set();
    for (const item of page.items) {
      if (!object(item) || !token(item.key)) return this.fail('INVALID', 'INVALID_PAGE_ITEM_KEY');
      if (localKeys.has(item.key) || this.itemKeys.has(item.key)) return this.fail('INVALID', 'DUPLICATE_ITEM_KEY');
      localKeys.add(item.key);
    }
    let copied;
    try {
      copied = structuredClone(page.items);
    } catch {
      return this.fail('INVALID', 'PAGE_ITEMS_NOT_CLONEABLE');
    }
    for (const key of localKeys) this.itemKeys.add(key);
    this.pages.set(page.pageIndex, copied);
    return { state: 'ACCEPTED', pageIndex: page.pageIndex };
  }

  finalize({ currentVector, nowMonotonicMs } = {}) {
    const current = this.checkCurrent(currentVector, nowMonotonicMs);
    if (current.state !== 'OPEN') return current;
    if (this.pages.size !== this.ticket.pageCount) return { state: 'INCOMPLETE', code: 'MISSING_PAGES' };
    const items = [];
    for (let index = 0; index < this.ticket.pageCount; index += 1) {
      const pageItems = this.pages.get(index);
      if (!pageItems) return { state: 'INCOMPLETE', code: 'MISSING_PAGES' };
      items.push(...pageItems);
    }
    if (items.length !== this.ticket.totalItems || this.itemKeys.size !== this.ticket.totalItems) {
      return this.fail('INVALID', 'ASSEMBLED_ITEM_COUNT_MISMATCH');
    }
    this.state = 'COMPLETE';
    return { state: 'COMPLETE', ticket: this.ticket, items, finalizable: this.ticket.finalizable };
  }
}

export const snapshotCoordinatorLimits = Object.freeze({ maxPageSize: 1024, maxTtlMs: 0x7FFFFFFF });
