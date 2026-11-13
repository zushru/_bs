# Project Instructions addendum — deadline-first execution

Paste this block into the _bs Project Instructions. It supplements, never replaces, existing authority and safety rules.

## Execution protocol
- Maximize verified progress under the deadline. Safety/data integrity and decision authority outrank speed.
- Authority: `HANDOFF_FINAL.md` → `docs/DECISIONS.md` → approved spec → implementation/interface → owning tests → `CRITICAL`/`UNVERIFIED`/`TESTS`/`CONTEXT` → `WORKING_MEMORY`/roadmap. Preserve SIGNED, DELEGATED, OPEN and SUPERSEDED status. Never infer OPEN decisions.
- At session start, verify authorized RDC device, `pwd`, project root/version, full `MANIFEST.sha256`, protected hashes and active PIDs. Any mismatch or unexplained drift means stop edits/builds and request intended diffs/current hashes or a refreshed verified snapshot. No Git assumptions.
- Follow `DEVELOPMENT_WORKFLOW.md` in the project root. Read `docs/WORKING_MEMORY.md` for the current baseline, then inspect the controlling spec, implementation/interface and owning tests for the selected batch.
- Roadmap order: C13.3 → C1/C34 → C3 → C6 → C2. C2 stays gated on storage ownership and approved recovery design. Continue safe independent work on E18/U5, U61, U112 and SF2 without changing their decision status.
- Classify blockers as DEVELOPMENT (blocks dependent work), RELEASE (must close before use/release), or EVIDENCE (implementation may exist, proof is missing). Continue independent safe work around release/evidence blockers.
- Work in coherent subsystem batches. For behavior changes, add the smallest real regression first, prove the gap, implement the complete safe path, rerun the owning suite and only directly affected dependents. Skip ritual tests for docs/mechanical edits. Avoid unrelated refactors and micro-increments.
- Run focused native suites after behavior batches. Do not run full native N1 until C13.3, C1/C34, C3, C6 and C2 are sufficiently integrated and the integration checkpoint is agreed. Run full SF2 only at its integration/RC checkpoint. Build affected ESP32 targets only, `-j 2` maximum, one PIO build at a time; inspect the exact ELF before another target build.
- Pass A: build the highest-value safe roadmap work (target about 60% of the time). Pass B: freeze features and fix only required-behavior, integrity, security and critical integration defects (about 25%). Reserve about 15% for agreed verification. These are planning targets, never grounds to skip required tests. Reduce feature scope, not safety or evidence.
- Keep host tests, target compile/link, ELF inspection, mounted-target runtime and physical-device evidence separate. Inventory is not a pass. Missing physical proof remains a release blocker.
- Preserve project safety boundaries: no destructive NVS/storage/partition/sensor operation without exact authorization and preconditions; no LittleFS format/truncate/quarantine/repair/journal/restore swap without approved design and proven target commit behavior; never flash the inert adapter-link image.
- On `CONTINUE`, select and execute the highest-priority safe complete batch without asking the user to choose routine tasks. Stop only for a decision, authorization, missing essential input, baseline mismatch or unsafe state.
- Report each batch once: yield, baseline, exact files, exact verification/results, not-run evidence, remaining blockers and next safe batch. Never claim more than the evidence proves.
