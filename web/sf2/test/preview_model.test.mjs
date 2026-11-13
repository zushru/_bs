import test from 'node:test';
import assert from 'node:assert/strict';
import { buildSnapshotPreview } from '../preview_model.mjs';
import { cloneSnapshot, makeValidSnapshot } from '../fixtures/snapshot_fixtures.mjs';

const has = (preview, code) => preview.diagnostics.some((item) => item.code === code);

test('builds a clean preview from a complete synthetic snapshot', () => {
  const preview = buildSnapshotPreview(makeValidSnapshot());
  assert.equal(preview.state, 'PREVIEW_READY');
  assert.equal(preview.calculationState, 'PREVIEW_READY');
  assert.equal(preview.finalExportReady, false);
  assert.equal(preview.monthRosterCount, 3);
  assert.equal(preview.diagnostics.length, 0);
});

test('identifies the learner, date and session for an unresolved result', () => {
  const snapshot = cloneSnapshot();
  snapshot.attendance[0].days[0].pm = { status: 'UNRESOLVED', reasonCode: 'DEVICE_GAP' };
  const preview = buildSnapshotPreview(snapshot);
  const item = preview.diagnostics.find((diagnostic) => diagnostic.code === 'UNRESOLVED_SESSION');
  assert.equal(item.studentId, 1);
  assert.equal(item.date, '2026-11-02');
  assert.equal(item.session, 'PM');
});

test('identifies a missing learner/date/session row', () => {
  const snapshot = cloneSnapshot(); snapshot.attendance[0].days.pop();
  const preview = buildSnapshotPreview(snapshot);
  assert.ok(has(preview, 'MISSING_EXPECTED_ATTENDANCE'));
  const item = preview.diagnostics.find((diagnostic) => diagnostic.code === 'MISSING_EXPECTED_ATTENDANCE'
    && diagnostic.learnerName === 'Aldana, Adrian M.');
  assert.equal(item.session, 'AM+PM');
  assert.equal(item.learnerName, 'Aldana, Adrian M.');
});

test('identifies incomplete coverage by date and session', () => {
  const snapshot = cloneSnapshot();
  const affected = snapshot.coverage.find((item) => item.date === '2026-11-03' && item.session === 'PM');
  affected.gapAffected = true;
  affected.coverageComplete = false;
  affected.closeCoverageFlag = false;
  const preview = buildSnapshotPreview(snapshot);
  const item = preview.diagnostics.find((diagnostic) => diagnostic.code === 'COVERAGE_GAP');
  assert.equal(item.date, '2026-11-03');
  assert.equal(item.session, 'PM');
});

test('identifies a partial-day calendar policy blocker', () => {
  const snapshot = cloneSnapshot();
  snapshot.calendar.days.find((day) => day.date === '2026-11-02').scheduledSessions = ['AM'];
  for (const entry of snapshot.attendance) {
    const day = entry.days.find((item) => item.date === '2026-11-02');
    if (day) day.pm = { status: 'NOT_SCHEDULED', calendarConfirmed: true };
  }
  const preview = buildSnapshotPreview(snapshot);
  assert.ok(has(preview, 'PARTIAL_DAY_POLICY_OPEN'));
});

test('surfaces missing school reporting metadata as warnings', () => {
  const snapshot = cloneSnapshot(); snapshot.schoolProfile.schoolId = null;
  const preview = buildSnapshotPreview(snapshot);
  assert.ok(preview.warnings.some((item) => item.code === 'MISSING_SCHOOL_ID'));
});

test('does not use a supplied finalizable flag as authorization', () => {
  const snapshot = cloneSnapshot(); snapshot.finalizable = true;
  snapshot.completeness.rosterHistoryComplete = false;
  const preview = buildSnapshotPreview(snapshot);
  assert.notEqual(preview.calculationState, 'READY');
  assert.equal(preview.finalExportReady, false);
});

test('final export remains unavailable before a verified workbook adapter exists', () => {
  const preview = buildSnapshotPreview(makeValidSnapshot());
  assert.equal(preview.finalExportReady, false);
  assert.equal(preview.workbookWriterAvailable, false);
});

test('identifies an unclosed scheduled session in the preview', () => {
  const snapshot = cloneSnapshot();
  const coverage = snapshot.coverage.find((item) => item.date === '2026-11-02' && item.session === 'AM');
  coverage.sessionClosed = false;
  coverage.closeEventSeq = null;
  coverage.bitmapPayloadSeqs = null;
  coverage.sourceLastSeq = null;
  coverage.expectedStudentIds = null;
  coverage.expectedCount = null;
  const preview = buildSnapshotPreview(snapshot);
  const item = preview.diagnostics.find((diagnostic) => diagnostic.code === 'COVERAGE_NOT_FINAL');
  assert.equal(item.date, '2026-11-02');
  assert.equal(item.session, 'AM');
});

test('identifies a missing coverage row with its date and session', () => {
  const snapshot = cloneSnapshot(); snapshot.coverage.pop();
  const preview = buildSnapshotPreview(snapshot);
  const item = preview.diagnostics.find((diagnostic) => diagnostic.code === 'COVERAGE_ROW_MISSING');
  assert.equal(item.date, '2026-11-06');
  assert.equal(item.session, 'PM');
});
