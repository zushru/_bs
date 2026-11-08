# SF2 month-overlap roster projection — v0.5

Status: **source implementation added in v0.2.23; v0.2.32 user run exposed and v0.2.33 corrects one test-fixture boundary error; rerun pending; not production-wired**.

## 1. Scope

This increment completes the first implementation of Batch D1. It projects a reporting-month roster from a scratch `RosterStore` rebuilt from an exact, audited event-log prefix. It does not add persistence, learner PII, a production API, or XLSX generation.

The signed month-overlap rule is unchanged: include each known learner with at least one enrollment interval intersecting any part of the report month, including a learner who exited before export day.

## 2. C++ source projection

`lib/core/src/month_roster_projection.{h,cpp}` adds `MonthRosterProjection`.

The constructor binds it to one `LogStore`, `EventGroupAudit`, copied `OpenInfo` and `RosterLogPrefix` identity descriptors, UTC offset, caller-owned scratch `RosterStore`, `RosterTransitionAudit`, and a non-empty half-open local-day range. Copying the small identity descriptors prevents later caller mutation from changing the projection identity and avoids dangling references when a temporary is passed. The projected month is the non-empty half-open local-day range `[monthStartDay, monthEndExclusive)`.

`begin()` re-runs `replayRosterPrefix()` itself. The projection does not trust a caller-supplied success boolean or an unrelated scratch roster. It requires `RPR_OK`, exact prefix currency, a matching UTC offset, and complete scratch history before it computes month totals.

Before exposing rows, it checks:

- every stored interval references a known ID in `1..MAX_ID`;
- interval start is a valid local day;
- each finite interval is non-empty and end-exclusive;
- no two intervals for the same ID overlap;
- every known ID has at least one interval;
- every result remains tied to the same source prefix.

It enumerates learners in ascending stable device ID order. This is a deterministic transport order only; it is **not** a signed school class-list or template row-order rule.

Each `MonthRosterStudent` reports:
- `studentId`;
- number of that learner's enrollment intervals intersecting the report month;
- whether the learner is enrolled on the final calendar day of the report month.

`intervalAt(studentId, ordinal, out)` streams the original, unclipped intervals that overlap the month. This preserves exit/re-entry boundaries. A learner appears once even if several intervals overlap the month.

No extra interval table is allocated. Projection uses the existing fixed `MAX_INTERVALS=512` storage and bounded scans. Interval validation is O(n²) in the maximum 512 intervals; each row-read rechecks the exact log prefix and fingerprints the bounded scratch roster, so repeated streaming is O(rows × (intervals + IDs + name bytes)). These are bounded but still require target timing measurement before placing them in a latency-sensitive loop.

The scratch fingerprint routine reports read failure separately from the numeric hash, so an invalid scratch read cannot accidentally compare equal to a zero hash. `begin()` clears prior fingerprint and replay-diagnostic state before replay, so a failed re-begin cannot expose an earlier `RPR_OK` as if it belonged to the new attempt. The scratch roster is caller-owned. The projection fingerprints its IDs, names, intervals and UTC offset after replay and checks for accidental mutation while reading. Student summary returns separate `MRP_VALUE`, `MRP_NOT_FOUND` and error statuses, so an interval-read failure is not conflated with “no month overlap” and cannot silently drop a learner. This FNV-1a fingerprint is a mutation-detection aid only; it is not cryptographic integrity protection. The caller must keep the scratch roster alive, must not attach it to a live log or mutate it while the projection is in use, and must discard it if `begin()` fails because replay may have partially reset/rebuilt it.

## 3. JavaScript name/interval reconciliation

`web/sf2/month_roster_projection.mjs` adds `projectMonthRoster()` for browser-side reconciliation of the C++ interval projection and the complete `RosterNameHistoryReader` stream.

Input must include:
- the report month's half-open local-day range;
- every known ID and all reconstructed enrollment intervals;
- all historical `RA_SET` display-name revisions from the same exact log prefix;
- complete-history flags for both streams;
- matching prefix identity: open generation, record count/last sequence and audit generation.

The model verifies prefix identity, ID uniqueness/range, interval validity/non-overlap, presence of at least one interval per known ID, valid operational display-name UTF-8 byte length (1..32), globally increasing name source sequence, unique sequence values and non-backward per-student name effective days.

The output contains each month-overlap learner once, every interval intersecting the month, `enrolledOnLastDay`, the latest pre-month display-name revision if present, and every display-name revision effective during the month.

If more than one distinct display name applies during the report month, the learner is returned with `nameState=CHANGED_IN_MONTH`, `displayName=null`, and the result is `BLOCKED`. The projection does not silently choose an old or new label. Identical repeated name values are preserved as source revisions but do not create a false conflict. A selected learner with no applicable name is also blocking.

The model returns an empty `READY` roster when no known learner's intervals overlap the month; the later report/template layer may decide whether an empty section report is useful, but this projection does not fabricate rows.

## 4. E27 — same-day activation/deactivation risk

E27 is **DELEGATED**. Same-local-day `RA_DEACTIVATE`/`RA_ACTIVATE` remains fail-closed for authoritative monthly reporting only. The restriction does not reject or rewrite committed operational events. D1 consumes only a successful prefix replay, so it does not bypass E27.

The concern is not event order alone: event records carry timestamps and sequence numbers, but `RosterStore` reduces them to local-day boundaries. An activate-then-deactivate pair on one day can create a zero-length day interval. Deactivate-then-reactivate on one day can create adjacent day intervals that make the learner appear expected for the entire date, even if enrollment was absent for part of it. This can disagree with attendance scans and the expected-student bitmap stored when sessions close.

A future time-granular reporting history could preserve intraday event ordering using the original epoch and sequence, but it must also define whether each attendance session overlaps the enrollment interval. The day-only roster must not be relaxed while that session-level eligibility rule is missing. Same-day timestamps also need protection against RTC corrections that move the event epoch backwards.

For now, same-day lifecycle transitions remain report-blocking. No committed log event is deleted or rewritten. D1 does not solve time-granular enrollment eligibility.

## 5. Known limitations

- The operational display name is not legal first/middle/last name data.
- LRN, sex, curriculum, section, transfer reason/destination and NLS data remain absent from the durable device model.
- `RosterTransitionAudit` still fails closed on same-day transitions and backward effective days.
- Same-day name revisions are ordered by source sequence but conflicting values during a month block the projection.
- The source prefix can still be bypassed by direct writes to the underlying storage driver; a future serialized firmware owner must prohibit those writes during projection.
- No production firmware composition root or HTTP endpoint calls this class yet.
- No official template capacity, report row order, or XLSX writer is defined by this module.
- The FNV fingerprint is not a MAC, hash-chain, or substitute for authenticated storage.

## 6. Test inventory

Added 16 C++ regression registrations in `test/test_month_roster` and 19 Node registrations in `web/sf2/test/month_roster_projection.test.mjs`. They cover month boundaries, multi-interval learners, exit/re-entry, end-of-month enrollment, ID ordering, empty rosters, source-prefix mismatch, history completeness, malformed/overlapping intervals, changed/missing names, same-day name revisions, source sequence ordering, both same-day lifecycle transition directions under E27 fail-closed behavior, descriptor-copy protection and stale replay-diagnostic reset. The v0.2.32 user run executed all 16 C++ cases: 15 passed, while one fixture failed because it used day 131 as the exit date for a month ending exclusively at 131. v0.2.33 corrects that fixture and adds a Node regression for the 130/131 boundary; no rerun has yet verified the correction.

Tests were authored but intentionally not executed. JavaScript syntax checks were run only. No C++ compiler, PlatformIO, ESP32 build, device test, or timing measurement is claimed.


## v0.2.25 JSON prefix width

The browser prefix contract now represents C++ `RosterLogPrefix.openGeneration` as canonical unsigned decimal text across the entire `uint64_t` range. The other prefix counters remain bounded `uint32_t` integers. D1 and D2 use the shared `source_prefix.mjs` validator; a JavaScript number is rejected even when it happens to be within the safe-integer range, keeping one unambiguous JSON representation.
