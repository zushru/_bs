# D1/D2 to snapshot roster join — design v0.1

Status: **browser reference helper added in v0.2.25; 257/257 SF2 Node tests pass locally in v0.2.37; not production-wired**. The test suite validates synthetic host fixtures only. The D1/D2 join remains non-authoritative; calculation state is `PREVIEW_READY`, and no XLSX export is authorized.

## 1. Purpose

D1, D2 and the proposed snapshot schema previously had no single executable join. A caller could independently construct a schema-valid snapshot without proving that its learners, legal identity and movements came from the same D1/D2 projections. `snapshot_schema.mjs` validates the submitted envelope; it cannot establish its upstream provenance.

`reporting_snapshot_join.mjs` adds a narrow D1-to-D2-to-snapshot roster join. It:

- requires D1 `READY` and a valid exact source prefix;
- requires D2 to reference the same month, metadata revision and exact source prefix;
- requires the exact same learner ID set;
- rechecks D1 and D2 intervals for equality, month overlap, sorted order, non-overlap and half-open date validity;
- binds every enrollment period to the selected section profile;
- maps only the explicit legal identity object, never the operational display label;
- rechecks legal identity and section-profile field shapes, plus movement IDs, dates, types, date meanings, same-day collisions and non-empty nullable text fields;
- maps school/section profiles and revision identifiers into a snapshot-compatible fragment.

The helper returns `BLOCKED` for structural disagreement and `INCOMPLETE` for a clean join. It **never** returns `READY`: authenticated reviewer evidence and a trusted verifier do not exist. A caller-provided `state:READY`, `authenticatedReviewState:VERIFIED` or `SOURCE_CHECKED` claim cannot change this.

## 2. Snapshot schema is not an authority check

The proposed `validateSnapshot()` remains a structural/semantic envelope validator. A schema-valid payload can still contain caller-created identity data. v0.2.25 therefore changes `calculateSnapshotMonth()` to return `PREVIEW_READY`, not `READY`, when the pure calculation succeeds. It sets `authoritativeExportReady:false` and `authority.state:NOT_ESTABLISHED` for every result. This is a preview calculation only; it is not an export authorization or proof that the D1/D2 join was performed.

The integration fixture intentionally demonstrates both facts: the mapped fragment can be incorporated into a schema-valid synthetic snapshot, while the D3 join remains `INCOMPLETE`. This prevents structural validity from being confused with authenticated identity review.

## 3. Exact source-prefix JSON representation

The C++ `RosterLogPrefix.openGeneration` is `uint64_t`. JavaScript numbers cannot represent every `uint64_t` exactly. v0.2.25 defines one JSON representation:

- `openGeneration`: canonical unsigned decimal string, `"1"` through `"18446744073709551615"`; no sign, whitespace, leading zero, number or out-of-range value;
- `recordCount`, `lastSeq`, `auditGeneration`: JSON integers in `uint32_t` range;
- `lastSeq` must equal `recordCount`; `auditGeneration` must be positive.

D1, D2, and the snapshot revision-vector model share the same validator and exact equality comparison. The validator rejects JavaScript numeric generations, even safe integers, so there is only one wire representation. The future HTTP adapter must serialize the C++ `uint64_t` as a decimal string; it must not pass through a JavaScript floating-point number first.

## 4. Remaining limits

- D3 uses in-memory browser objects only; it does not authenticate or lock source stores.
- The C++ output is not directly linked to this JS fixture; the test uses the exact proposed JSON field contract and synthetic records, not a device-produced payload.
- `validateSnapshot()` still cannot prove that the server decoded C++ log bytes correctly or captured an immutable snapshot.
- The D2 metadata store remains hypothetical; no LRN/name/sex/movement data is persisted or exposed by firmware.
- `PREVIEW_READY` is not an approval to export. There is still no XLSX writer, approved template or school-accepted mark legend.
