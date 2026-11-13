# Revision-pinned snapshot coordinator model — v0.4

Status: **pure browser/host reference model only**. It is not a device lock, server ticket store, HTTP route, authentication layer or proof of storage atomicity.

## 1. Capture vector

`snapshot_coordinator_model.mjs` models a serialized capture from:

- LogStore open generation, record count and last sequence;
- EventGroupAudit generation, open generation, record count, last sequence, attachment and readiness;
- roster prefix identity, `historyComplete`, and `RosterTransitionAudit` state;
- Config generation;
- calendar and historical schedule revision/completeness;
- reporting profile, school profile, learner metadata and movement revisions/completeness.

The roster revision token is derived from the exact log prefix and event-audit generation, not from RTC time. The model rejects count/sequence disagreement, stale log-open generation, mismatched audit prefix, mismatched roster prefix, incomplete roster history, or a non-VALID roster transition audit.

Calendar/schedule or metadata incompleteness may still produce an `INCOMPLETE` preview vector. It can never be marked finalizable.

## 2. Ticket and paging model

A ticket pins one month, source revision vector, total item count, page size and monotonic expiry. Page acceptance checks:

- same ticket identifier;
- same source revision vector;
- current vector still equals the pinned vector;
- exact page index/count and expected item count;
- unique page indices and unique item keys across all pages;
- expiry using caller-supplied monotonic milliseconds, never RTC/wall time.

Pages may arrive out of order. Finalization requires every page, the exact total item count, unique keys and one unchanged source vector. Any stale/expired/invalid state discards accepted pages.

The page-size ceiling and TTL range are **model limits only**, not approved production limits. They must be measured and approved against ESP32 memory, synchronous WebServer loop latency and browser memory before becoming protocol constants.

## 3. Security and lifecycle boundary

The model does not generate secure ticket IDs, store ticket data, authenticate a caller, encrypt PII, prevent concurrent writes, or make source stores immutable. The actual ESP32 coordinator must run in the serialized application loop, enforce the authenticated admin session, use a cryptographically strong opaque ID, avoid logging PII, set no-store behavior, and recheck source revisions on each page. A client-side model cannot prove any of those server guarantees.

No production endpoint or browser API has been implemented in v0.2.22. C22/C27/C28 remain open.


## v0.2.25 exact generation representation

`LogStore::openGeneration()` and `RosterLogPrefix.openGeneration` are C++ `uint64_t`. The JSON/browser contract now requires canonical unsigned decimal text, including values above `Number.MAX_SAFE_INTEGER` and up to `18446744073709551615`. `recordCount`, `lastSeq` and `EventGroupAudit.generation` remain `uint32_t` JSON integers. Numeric, signed, padded or out-of-range open-generation values are rejected. This removes floating-point rounding from exact prefix identity; it does not strengthen the host model into a production snapshot lock.
