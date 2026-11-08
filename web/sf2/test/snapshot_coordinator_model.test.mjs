import test from 'node:test';
import assert from 'node:assert/strict';
import {
  beginSnapshotTicket,
  captureSnapshotRevisionVector,
  compareSnapshotRevisionVectors,
  SnapshotPageAssembler,
} from '../snapshot_coordinator_model.mjs';

function sources(overrides = {}) {
  const log = { openGeneration: '2', recordCount: 40, lastSeq: 40 };
  const eventAudit = { attached: true, ready: true, openGeneration: '2', generation: 7, recordCount: 40, lastSeq: 40 };
  const roster = {
    historyComplete: true, transitionAuditState: 'VALID',
    prefix: { openGeneration: '2', recordCount: 40, lastSeq: 40, auditGeneration: 7 },
  };
  return {
    log, eventAudit, roster,
    config: { generation: 3 },
    calendar: { revision: 'cal-r4', historyComplete: true, scheduleHistoryComplete: true },
    reportingProfile: { revision: 'rp-r2', complete: true },
    schoolProfile: { revision: 'school-r1', complete: true },
    learnerMetadata: { revision: 'learner-r8', complete: true },
    movements: { revision: 'moves-r3', complete: true },
    ...overrides,
  };
}

function readyVector() {
  const result = captureSnapshotRevisionVector(sources());
  assert.equal(result.state, 'READY');
  return result.vector;
}

function ticket(totalItems = 5, pageSize = 2) {
  return beginSnapshotTicket({
    ticketId: 'ticket-2026-11-a', month: '2026-11', revisionVector: readyVector(),
    totalItems, pageSize, createdMonotonicMs: 1000, ttlMs: 5000,
  });
}

function page(t, index, items) {
  return {
    ticketId: t.ticket.ticketId,
    pageIndex: index,
    pageCount: t.ticket.pageCount,
    revisionVector: t.ticket.revisionVector,
    items,
  };
}

test('capture builds a roster revision from the exact audited log prefix', () => {
  const result = captureSnapshotRevisionVector(sources());
  assert.equal(result.state, 'READY');
  assert.equal(result.finalizable, true);
  assert.equal(result.vector.rosterRevision, 'log-2-40-40-audit-7');
});

test('capture rejects audit that is not attached and ready', () => {
  const input = sources({ eventAudit: { attached: true, ready: false, openGeneration: '2', generation: 7, recordCount: 40, lastSeq: 40 } });
  const result = captureSnapshotRevisionVector(input);
  assert.equal(result.state, 'INVALID');
  assert.ok(result.issues.includes('EVENT_AUDIT_NOT_READY'));
});

test('capture rejects an audit tied to a different log open generation', () => {
  const input = sources({ eventAudit: { ...sources().eventAudit, openGeneration: '1' } });
  const result = captureSnapshotRevisionVector(input);
  assert.ok(result.issues.includes('AUDIT_OPEN_GENERATION_MISMATCH'));
});

test('capture rejects roster projection from another prefix', () => {
  const input = sources({ roster: { ...sources().roster, prefix: { openGeneration: '2', recordCount: 39, lastSeq: 39, auditGeneration: 7 } } });
  const result = captureSnapshotRevisionVector(input);
  assert.ok(result.issues.includes('ROSTER_PREFIX_MISMATCH'));
});

test('capture allows incomplete historical calendar as preview-only', () => {
  const input = sources({ calendar: { revision: 'cal-r4', historyComplete: false, scheduleHistoryComplete: false } });
  const result = captureSnapshotRevisionVector(input);
  assert.equal(result.state, 'INCOMPLETE');
  assert.equal(result.finalizable, false);
});

test('revision comparison detects any source generation change', () => {
  const before = readyVector();
  const after = { ...before, configGeneration: before.configGeneration + 1 };
  assert.equal(compareSnapshotRevisionVectors(before, after).state, 'STALE');
});

test('ticket derives page count and carries a finalizable flag', () => {
  const result = ticket();
  assert.equal(result.state, 'READY');
  assert.equal(result.ticket.pageCount, 3);
  assert.equal(result.ticket.finalizable, true);
});

test('ticket rejects an invalid page size and monotonic expiry overflow', () => {
  const vector = readyVector();
  assert.equal(beginSnapshotTicket({ ticketId: 't', month: '2026-11', revisionVector: vector,
    totalItems: 1, pageSize: 0, createdMonotonicMs: 1, ttlMs: 1 }).state, 'INVALID');
  assert.ok(beginSnapshotTicket({ ticketId: 't', month: '2026-11', revisionVector: vector,
    totalItems: 1, pageSize: 1, createdMonotonicMs: Number.MAX_SAFE_INTEGER - 1, ttlMs: 5 }).issues.includes('EXPIRY_OVERFLOW'));
});

test('pages may arrive out of order and are assembled in ordinal order', () => {
  const created = ticket(); const t = created.ticket; const currentVector = readyVector();
  const assembler = new SnapshotPageAssembler(t);
  assert.equal(assembler.addPage(page(created, 2, [{ key: 'e' }]), { currentVector, nowMonotonicMs: 1100 }).state, 'ACCEPTED');
  assert.equal(assembler.addPage(page(created, 0, [{ key: 'a' }, { key: 'b' }]), { currentVector, nowMonotonicMs: 1200 }).state, 'ACCEPTED');
  assert.equal(assembler.addPage(page(created, 1, [{ key: 'c' }, { key: 'd' }]), { currentVector, nowMonotonicMs: 1300 }).state, 'ACCEPTED');
  const result = assembler.finalize({ currentVector, nowMonotonicMs: 1400 });
  assert.equal(result.state, 'COMPLETE');
  assert.deepEqual(result.items.map((item) => item.key), ['a', 'b', 'c', 'd', 'e']);
});

test('missing page blocks finalization', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  assert.equal(assembler.addPage(page(created, 0, [{ key: 'a' }, { key: 'b' }]),
    { currentVector: readyVector(), nowMonotonicMs: 1100 }).state, 'ACCEPTED');
  assert.equal(assembler.finalize({ currentVector: readyVector(), nowMonotonicMs: 1200 }).code, 'MISSING_PAGES');
});

test('source change after one page invalidates and discards all accepted pages', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const current = readyVector();
  assembler.addPage(page(created, 0, [{ key: 'a' }, { key: 'b' }]), { currentVector: current, nowMonotonicMs: 1100 });
  const changed = { ...current, logOpenGeneration: String(Number(current.logOpenGeneration) + 1) };
  const result = assembler.addPage(page(created, 1, [{ key: 'c' }, { key: 'd' }]), { currentVector: changed, nowMonotonicMs: 1200 });
  assert.equal(result.state, 'STALE');
  assert.equal(assembler.pages.size, 0);
});

test('page carrying another source revision is rejected', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const badPage = page(created, 0, [{ key: 'a' }, { key: 'b' }]);
  badPage.revisionVector = { ...badPage.revisionVector, configGeneration: 999 };
  const result = assembler.addPage(badPage, { currentVector: readyVector(), nowMonotonicMs: 1100 });
  assert.equal(result.code, 'INVALID_PAGE_ENVELOPE');
});

test('duplicate page is rejected and invalidates the assembler', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const currentVector = readyVector(); const p = page(created, 0, [{ key: 'a' }, { key: 'b' }]);
  assembler.addPage(p, { currentVector, nowMonotonicMs: 1100 });
  const result = assembler.addPage(p, { currentVector, nowMonotonicMs: 1200 });
  assert.equal(result.code, 'DUPLICATE_PAGE');
  assert.equal(assembler.state, 'INVALID');
});

test('duplicate item key across pages is rejected', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const currentVector = readyVector();
  assembler.addPage(page(created, 0, [{ key: 'a' }, { key: 'b' }]), { currentVector, nowMonotonicMs: 1100 });
  const result = assembler.addPage(page(created, 1, [{ key: 'b' }, { key: 'd' }]), { currentVector, nowMonotonicMs: 1200 });
  assert.equal(result.code, 'DUPLICATE_ITEM_KEY');
});

test('ticket expiry uses monotonic time and discards pages', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const result = assembler.addPage(page(created, 0, [{ key: 'a' }, { key: 'b' }]),
    { currentVector: readyVector(), nowMonotonicMs: created.ticket.expiresMonotonicMs });
  assert.equal(result.state, 'EXPIRED');
});

test('page count and expected item count are enforced', () => {
  const created = ticket(); const assembler = new SnapshotPageAssembler(created.ticket);
  const wrongCount = page(created, 0, [{ key: 'a' }]);
  const result = assembler.addPage(wrongCount, { currentVector: readyVector(), nowMonotonicMs: 1100 });
  assert.equal(result.code, 'PAGE_ITEM_COUNT_MISMATCH');
});

test('empty snapshot can complete without phantom pages', () => {
  const created = ticket(0, 10); const assembler = new SnapshotPageAssembler(created.ticket);
  const result = assembler.finalize({ currentVector: readyVector(), nowMonotonicMs: 1100 });
  assert.equal(result.state, 'COMPLETE');
  assert.deepEqual(result.items, []);
});


test('source revision vector preserves a maximum-width uint64 open generation exactly', () => {
  const maxGeneration = '18446744073709551615';
  const input = sources({
    log: { openGeneration: maxGeneration, recordCount: 40, lastSeq: 40 },
    eventAudit: { ...sources().eventAudit, openGeneration: maxGeneration },
    roster: { ...sources().roster, prefix: { ...sources().roster.prefix, openGeneration: maxGeneration } },
  });
  const result = captureSnapshotRevisionVector(input);
  assert.equal(result.state, 'READY');
  assert.equal(result.vector.logOpenGeneration, maxGeneration);
  assert.equal(result.vector.rosterRevision, `log-${maxGeneration}-40-40-audit-7`);
});

test('source revision vector rejects numeric and out-of-range uint64 generations', () => {
  for (const generation of [9007199254740992, '18446744073709551616', '01']) {
    const input = sources({
      log: { openGeneration: generation, recordCount: 40, lastSeq: 40 },
      eventAudit: { ...sources().eventAudit, openGeneration: generation },
      roster: { ...sources().roster, prefix: { ...sources().roster.prefix, openGeneration: generation } },
    });
    assert.equal(captureSnapshotRevisionVector(input).state, 'INVALID');
  }
});
