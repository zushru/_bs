import test from 'node:test';
import assert from 'node:assert/strict';
import { isValidOpenGeneration, isValidSourcePrefix, sameSourcePrefix } from '../source_prefix.mjs';

const MAX_U64 = '18446744073709551615';

test('accepts the full uint64 range as canonical decimal text', () => {
  assert.equal(isValidOpenGeneration('1'), true);
  assert.equal(isValidOpenGeneration('9007199254740993'), true);
  assert.equal(isValidOpenGeneration(MAX_U64), true);
});

test('rejects numeric, non-canonical and out-of-range open generations', () => {
  for (const value of [0, 1, Number.MAX_SAFE_INTEGER, '0', '01', '+1', '-1', '', '18446744073709551616']) {
    assert.equal(isValidOpenGeneration(value), false, `unexpectedly accepted ${String(value)}`);
  }
});

test('validates uint32 source-prefix fields separately from uint64 generation', () => {
  const prefix = { openGeneration: MAX_U64, recordCount: 0xffffffff,
    lastSeq: 0xffffffff, auditGeneration: 0xffffffff };
  assert.equal(isValidSourcePrefix(prefix), true);
  assert.equal(isValidSourcePrefix({ ...prefix, auditGeneration: 0x100000000 }), false);
  assert.equal(isValidSourcePrefix({ ...prefix, lastSeq: 0 }), false);
  assert.equal(isValidSourcePrefix({ ...prefix, recordCount: 0, lastSeq: 0 }), true);
});

test('compares source prefixes by exact generation text and bounded counters', () => {
  const a = { openGeneration: MAX_U64, recordCount: 12, lastSeq: 12, auditGeneration: 9 };
  assert.equal(sameSourcePrefix(a, { ...a }), true);
  assert.equal(sameSourcePrefix(a, { ...a, openGeneration: '18446744073709551614' }), false);
  assert.equal(sameSourcePrefix(a, { ...a, auditGeneration: 10 }), false);
});
