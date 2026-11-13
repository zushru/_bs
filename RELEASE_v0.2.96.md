# Development release v0.2.96 — 2026-11-12

## Yield

Documentation-only workflow alignment. Added root `DEVELOPMENT_WORKFLOW.md` and aligned the read-first working memory, current roadmap/context, critical and unverified summaries, test ledger, changelog, README and release pointers. The workflow codifies baseline verification, development/release/evidence blocker classes, the approved subsystem order, coherent test-first batches, focused verification, Pass A/Pass B, stop conditions and the required batch report.

No firmware, production source, public header/interface, test source, product decision, approved specification, persisted format, API exposure, sensor mapping, term boundary or recovery behavior changed. The current product and engineering decision statuses are unchanged.

## Baseline

- Code baseline: v0.2.95 at `/root/att`; no Git metadata.
- Before this documentation batch, all **451/451** entries in `MANIFEST.sha256` passed.
- Protected hashes before the batch matched the prior verified values and remain unchanged: `HANDOFF_FINAL.md` `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`; `docs/DECISIONS.md` `52856953239e90c4e834e777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3`.
- RDC device: `f4f8b9b2-468c-4825-80fd-a9df69a7968e`.

## Files changed

- Root: `DEVELOPMENT_WORKFLOW.md`, `PROJECT_INSTRUCTIONS_ADDENDUM.md`, `README.md`, `RELEASE.md`, `RELEASE_v0.2.96.md`, `MANIFEST.sha256`.
- Docs: `WORKING_MEMORY.md`, `DEVELOPMENT_ROADMAP.md`, `CONTEXT.md`, `CRITICAL.md`, `UNVERIFIED.md`, `TESTS.md`, `CHANGELOG.md`, `TESTING_WORKFLOW.md`, `AUDIT_v0.2.96.md`.

## Verification

- All entries in the regenerated `MANIFEST.sha256` were checked with `sha256sum -c MANIFEST.sha256`; the exact final entry count and protected hashes are recorded by the final remote verification output.
- No production source or tests changed, so no native suite, SF2 suite or ESP32 build was run for this documentation-only increment.

## Not run / remaining evidence

- No full native N1, SF2 full suite, ESP32 target build, ELF inspection, flash, mounted-target VFS test or physical test was run in v0.2.96.
- v0.2.95's focused **100/100** native result remains host POSIX/fake-backend evidence only.
- C13.3 remains BLOCKING: mounted ESP-IDF LittleFS VFS/runtime faults, actual descriptors, low-space, file and directory durability, restart/interruption, and physical power-cut/brownout proof are absent.
- C1/C34, C3, C6 and C2 remain on the approved roadmap. E18/U5, U61, U112 and SF2 remain open/delegated as documented.

## Critical review

The key workflow risk was stale status propagation: current working memory/context/roadmap and test ledger did not all point to v0.2.95. This batch corrects the current entry points and adds one canonical workflow file. Historical records are retained and labeled as historical rather than rewritten as current evidence. Project Settings instructions are not directly editable through RDC; the project instruction block must be updated separately by the user using the text supplied in the chat.
