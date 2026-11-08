# Specialized SF2/D4 audit — v0.2.30

Date: 2026-11-07  
Scope: D4 learner-identity review policy completion and documentation consistency.  
Base: verified user-uploaded `att_v0.2.29_2026-11-07.tar.gz`.

## Executive result

**D4 policy design is complete as delegated engineering decisions. Production implementation is not complete and remains blocked.** v0.2.30 is documentation-only: no C++/JavaScript production source, test source, test registration, firmware, storage adapter, API, dashboard, or workbook writer was changed.

The convenience-first workflow is now specified as one-time source-category configuration followed by one source selection and one confirmation per identity revision. Integrity is retained by binding review to immutable revisions, requiring a new review after correction, treating RTC time as advisory, and refusing to persist/report PII until an approved recovery path exists.

## D4 decisions

- **D4-1 / R11 — SIGNED:** protect against unauthorized Wi-Fi clients only. Physical flash access/modification and malicious authenticated administrators remain out of scope.
- **D4-2 / R12 — SIGNED:** one authorized administrator may enter and confirm the same learner identity. This is self-review.
- **D4-3 / R13 — DELEGATED:** actor role only (`ADMIN`) plus `SELF_REVIEW`; no self-entered operator identity label.
- **D4-4 / R14 — DELEGATED:** four broad source categories, school-configurable in one setup step. All four are initially selected; administrator confirms the set or disables categories not accepted locally. One or more categories can be selected per review. No images, document numbers, free-text locators, or claim of document authenticity/official acceptance.
- **D4-5 / R15 — DELEGATED:** authoritative order tuple is `(metadata lineage, committed transaction revision, change index)`. RTC observation is optional and explicitly untrusted; it cannot determine validity or report dates.
- **D4-6:** no device-signing key, secure element, or reviewer-key infrastructure under R11.
- **D4-7 / R16 — DELEGATED:** bind review to exact identity revision, capture source-policy revision, invalidate when identity changes or a selected source category is disabled, preserve prior reviews and snapshots, and require an explicit re-generation decision for affected reporting periods.
- **D4-8 / R17 — DELEGATED:** preserve signed O8. Do not persist or expose reporting PII until O8 is explicitly amended or a separate companion backup/recovery workflow is approved.

R13-R17 are engineering choices, not user-signed product rules. E27 was not changed.

## Critical design checks

1. **No false identity proof:** `SELF_REVIEWED` is never called `VERIFIED`; shared admin credentials do not identify a human.
2. **Minimal evidence:** source categories classify the admin's declaration; they do not prove source authenticity. No document uploads, IDs/numbers, unrestricted references or typed operator names.
3. **No per-field checkbox burden:** the server generates `reviewedFields` for all populated canonical identity fields. Report-required-field completeness is an independent axis; `SELF_REVIEWED` and `INCOMPLETE` can coexist.
4. **No stale review carry-forward:** a correction creates a new identity revision. The new revision begins `DRAFT`; the old review remains attached to its old revision.
5. **No false timestamps:** ordering uses store lineage + transaction revision + change index. RTC timestamps are optional `UNTRUSTED_RTC` observations only.
6. **No O8 bypass:** reporting PII is excluded from the signed config + roster + log backup. No metadata is persisted, exposed by HTTP, cached in browser storage or exported to XLSX while no approved recovery path exists.
7. **No D3 bypass:** browser-supplied `verified`, review sequence, timestamp or commit flags are not trusted. The future server must construct the source data and complete the review gate.

## Important remaining blockers

- C6: HTTP adapter does not yet enforce route authentication, lockout persistence, old-PIN checks and request limits.
- C13: LittleFS and power-loss transaction behavior are unverified.
- C24/C25/C30/E27: operational roster/event history has reporting limits; same-day lifecycle ambiguity remains fail-closed.
- C26: no persistent reporting-metadata store, capacity budget, transaction replay, migration or commit/read-back contract.
- C27: no production PII route enforcement, `no-store`, redaction or logging controls.
- C28: no target-side bounded snapshot pager or loop-latency measurement.
- C29/O8: no approved metadata recovery artifact. This prevents safe reporting PII persistence.
- C31: no server-side exact-prefix source builder/review gate.
- The current official SHS workbook is not verified, and teacher-dependent reporting semantics remain open.

## Scope and verification

- Existing documentation files updated: 12.
- New D4 specification: `REVIEW_EVIDENCE_DESIGN_v0.6.md`.
- New test registrations: 0.
- C++ source registrations: 376 across 23 suites, unchanged.
- SF2 Node source registrations: 256, unchanged.
- Functional tests run: 0.
- Static source checks run: 0 (documentation-only changes).
- C++ compiler/PlatformIO/ESP32/device verification: not performed.
- User-reported 318/318 native pass applies to v0.2.17 only. The 58 later C++ registrations and all 256 SF2 Node registrations remain unrun.

## Critical limitation

This release finishes the **policy design**, not the product feature. Because O8 is preserved, the device still cannot safely persist learner legal names/LRN/sex or issue authoritative SF2 output. Do not treat this documentation release as authorization to collect live learner PII.
