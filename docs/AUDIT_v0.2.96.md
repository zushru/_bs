# Audit v0.2.96 — deadline-first workflow alignment

**Type:** Documentation-only. **Code baseline:** v0.2.95. **Date:** 2026-11-12.

## Review findings addressed

1. `docs/WORKING_MEMORY.md` still led with v0.2.93/v0.2.94 status and a v0.2.94 test result even though v0.2.95 was the current code tree.
2. `docs/CONTEXT.md` still presented v0.2.94 and v0.2.91 as current snapshots.
3. `docs/DEVELOPMENT_ROADMAP.md` header still identified v0.2.77 as the current source.
4. `docs/TESTS.md` began with v0.2.94 and omitted the latest v0.2.95 100/100 focused run.
5. No single root-level file described the complete operational workflow, blocker classes, deadline freeze rule, verification cadence and batch report.

## Changes

- Added root `DEVELOPMENT_WORKFLOW.md` as the execution protocol and `PROJECT_INSTRUCTIONS_ADDENDUM.md` as the paste-ready Project Settings block (Project Settings itself cannot be edited through RDC). It is subordinate to signed/delegated decisions and approved specifications.
- Added a current v0.2.95 operational overlay to working memory, roadmap, context, critical and unverified summaries.
- Added exact v0.2.95 test evidence to `docs/TESTS.md` and updated current release/readme/changelog references.
- Preserved the dated historical entries. No product decision, approved spec, source implementation, public interface, test source, format, sensor mapping, term boundary, API exposure or recovery policy changed.

## Verification limits

This increment verifies documentation integrity only. A clean SHA-256 manifest does not prove firmware behavior, and no test/build was warranted because no code or tests changed. The v0.2.95 native result is **100/100 across four focused suites**, not full N1. The code remains v0.2.95; v0.2.96 is a documentation-only release marker.

## Open blockers

C13.3 target runtime/durability/physical interruption proof; C1/C34 production boot enforcement; C3 production composition root; C6 production auth/API; C2 approved backup/restore; E18/U5; U61; U112; and SF2 reporting provenance/metadata/export remain open as described by the controlling roadmap and decision ledger.
