/*
 * JSON-safe representation of the C++ RosterLogPrefix identity.
 * openGeneration is uint64_t and therefore travels as canonical decimal text;
 * the remaining source-prefix counters are uint32_t and remain JSON numbers.
 */

const MAX_U32 = 0xffffffff;
const MAX_U64_DECIMAL = '18446744073709551615';

export function isValidOpenGeneration(value) {
  if (typeof value !== 'string' || !/^[1-9][0-9]{0,19}$/.test(value)) return false;
  if (value.length < MAX_U64_DECIMAL.length) return true;
  if (value.length > MAX_U64_DECIMAL.length) return false;
  return value <= MAX_U64_DECIMAL;
}

function u32(value, { allowZero = true } = {}) {
  return Number.isSafeInteger(value) && value >= (allowZero ? 0 : 1) && value <= MAX_U32;
}

export function isValidSourcePrefix(value) {
  return Boolean(value && typeof value === 'object' && !Array.isArray(value)
    && isValidOpenGeneration(value.openGeneration)
    && u32(value.recordCount)
    && u32(value.lastSeq)
    && value.lastSeq === value.recordCount
    && u32(value.auditGeneration, { allowZero: false }));
}

export function sameSourcePrefix(a, b) {
  return isValidSourcePrefix(a) && isValidSourcePrefix(b)
    && a.openGeneration === b.openGeneration
    && a.recordCount === b.recordCount
    && a.lastSeq === b.lastSeq
    && a.auditGeneration === b.auditGeneration;
}

export function cloneSourcePrefix(value) {
  if (!isValidSourcePrefix(value)) return null;
  return {
    openGeneration: value.openGeneration,
    recordCount: value.recordCount,
    lastSeq: value.lastSeq,
    auditGeneration: value.auditGeneration,
  };
}
