# AI development instructions

**Project:** ATT — ESP32 fingerprint attendance system  
**Baseline:** v0.2.95 development snapshot, dated 2026-11-12  
**Scope:** This file governs work in this repository. It does not grant authority to change product requirements.

## Mandatory AI bootstrap

Every agent instance must identify itself as PISHDA or SHRU, the only two
active codenames in AI_COORDINATION.md. The user's message supplies the
codename; the repository defines the role. Do not infer or change it.

1. Verify this is the ATT repository by checking AGENTS.md, HANDOFF_FINAL.md,
   platformio.ini and the expected lib/, docs/, test/ and web/ directories.
2. Read these four files in exactly this order, using one multi-file read:
   AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md. If a read is
   missing, failed or truncated, retrieve it before acting.
3. Resolve your role, active task, repository ownership and pending handoffs
   from AI_COORDINATION.md and WORKING_MEMORY.md. A role does not grant write
   ownership. If the user says "Continue", take the single next task in
   WORKING_MEMORY.md unless an active task/lock says otherwise.
4. Use index.md to locate task-specific authority, specifications, source,
   callers, tests and evidence. Do not read the whole repository by default.
5. Before changing product behavior, read HANDOFF_FINAL.md and relevant
   entries in docs/DECISIONS.md. Read ARCHITECTURE.md and the applicable
   critical/unverified/specification documents for the affected subsystem.
6. Inspect implementation, public headers, callers and owning tests before
   proposing a patch. Source presence is not proof of production integration.

## Efficient remote-tool workflow (quality first; no fixed call limit)

RDC tool calls are limited and must be treated as a scarce resource. The
remaining quota is not known and must never be guessed. Minimize redundant
RDC use without skipping required inspection or read-back.

1. Use one multi-file read for the four-file bootstrap. Do not make separate
   calls for files that can be read together. Avoid repeated access,
   device-status or connection checks without a concrete failure or need.
2. PISHDA is the sole implementation owner, coordinator and primary RDC
   operator. SHRU is a read-only critic and may write reports only under
   comms/inbox/SHRU/. SHRU must not repeat PISHDA's successful reads. No other
   agent role is active.
3. Batch known task-relevant file reads. Use one targeted search for unknown
   callers/symbols. Read only authority, specification, implementation, tests
   and direct callers that can affect the task.
4. Run the read-only manifest audit before edits when available. A mismatch
   means the manifest is stale; report it and do not regenerate it merely to
   make the audit green. Never claim manifest integrity from a failed audit.
5. Before editing, state intended behavior, exact files, governing authority,
   key risks and the smallest authorized verification plan. If a requirement
   is OPEN or sources materially conflict, stop that part instead of guessing.
6. Validate repository root, target paths and every edit anchor/count before
   the first write. Prepare all complete replacements first. Stage replacements
   on the same filesystem, preserve modes, atomically replace each file and
   roll back replaced files if a reported write fails. Multi-file commits are
   not globally atomic. Never run concurrent edits against the same file.
7. Inspect the proposed diff. Reject unexpected drift, broad rewrites,
   unrelated formatting churn, weakened tests or undeclared file changes.
8. Independently read back every changed file. Check callers, invariants,
   counts and stale references. Add reads/searches when output is ambiguous,
   truncated, asynchronous or indicates drift.
9. Execute tests, compilers, builds, PlatformIO, flashing or hardware actions
   only when the current user/task authorizes them. Authored-but-unrun tests
   are NOT RUN. Source inventory and static inspection are not test passes.
10. Four RDC calls is a planning heuristic for a normal bounded task
    (bootstrap, targeted inspection, patch, read-back), never a quota or cap.
    Add calls whenever needed for correctness. Never replay a pending
    side-effecting call; retrieve its result first.

## Single-owner development and SHRU critic protocol

- PISHDA is the sole implementation engineer, repository writer, coordinator,
  integrator and primary RDC operator. PISHDA owns every repository path.
- SHRU is an optional read-only critic. It may inspect repository content and
  may write critique reports only inside comms/inbox/SHRU/. It must not write
  source, tests, documentation, specifications, coordination files, staging
  files, build configuration, manifests or any other path.
- SHRU must not run tests, compilers, builds, PlatformIO, flashing, hardware
  actions or project-mutating commands. Its contribution is analysis only.
- SHRU is 100% optional and outside PISHDA's execution path. PISHDA does not
  assign, invoke, poll, wait for or routinely inspect SHRU or its inbox.
- The user may prompt SHRU independently. PISHDA reads or considers an SHRU
  report only when the user explicitly asks PISHDA to review or incorporate it.
  A report, missing report or unread inbox never gates implementation or release.
- SHRU has no standing assignment, schedule, live notification or background
  activity. Its only permitted project write location remains comms/inbox/SHRU/.
- All previous SHRU source/test write assignments are revoked effective
  2026-11-13. Completed SHRU contributions already accepted into the canonical
  tree remain part of the baseline; revocation does not revert them.
- QYUPAL and SEI are retired from this project workflow. Their prior records
  remain historical only and grant no current access or task authority.
- Product requirements and OPEN decisions remain under the user's authority.
  SHRU may identify conflicts but cannot change policy.
- PISHDA uses a distinct adversarial self-review and read-back for every
  increment. That is author self-review, not independent verification.
  SHRU findings may add useful counterexamples, but absent or unverified
  feedback never blocks work and is never represented as independent proof.

## Authority rules

- `HANDOFF_FINAL.md` is the signed handoff baseline. `docs/DECISIONS.md` records later explicit changes, delegated engineering choices, open matters and superseded decisions. Preserve each row's status.
- `SIGNED` and `SIGNED-HANDOFF` are product constraints. `DELEGATED` permits only the bounded choice described. `OPEN` means no permission to guess. `MEASURED` is evidence, not policy. `DEFECT` is not a desired behavior. `SUPERSEDED` is non-controlling.
- Source code describes what exists; it does not retroactively approve product behavior.
- Tests prove only the behavior and environment they actually exercise.
- When two current sources materially disagree, stop the affected change, report the exact conflict and ask for the smallest necessary decision. Never resolve it by silently choosing the easiest implementation.
- The 15-minute scan-lead statement in the handoff is superseded for the current baseline by decision P2 in `docs/DECISIONS.md` (CHANGED by chat on 2026-11-10): the current default is 30 minutes. Preserve the handoff text as historical evidence; do not edit the protected handoff.

## Development protocol

1. Verify the repository root and baseline. Run `sha256sum -c MANIFEST.sha256` before edits when the environment provides `sha256sum`.
2. Inspect the relevant source, headers, specifications, tests and callers. Search for all call sites of changed interfaces.
3. Add or strengthen a meaningful regression before implementation. Establish that the assertion detects the defect; do not weaken tests to get a pass.
4. Make the smallest safe patch. Preserve fail-closed behavior and persistent formats unless the relevant authority has approved a change.
5. Run the owning test suite first. Run directly affected dependent suites only when a shared contract or caller changed. Avoid repeated full-suite runs on a constrained phone.
6. Build only the affected ESP32 target when authorized. Never run duplicate or concurrent PlatformIO builds.

An explicit current-task user instruction prohibiting tests, compiler checks, builds, PlatformIO, flashing or hardware actions overrides steps 5–6 for that task. Still add the necessary regression source, independently read it back, and clearly report each prohibited check as `NOT RUN`.
7. Report exact commands, target, suite, count, exit status and environment. Separate source review, host tests, target compilation, linking, ELF inspection, runtime and physical-device evidence.
8. Update `WORKING_MEMORY.md` only when the baseline, next action, blockers or evidence changed. Keep it short and current.
9. Update the release record, test ledger, changelog, relevant specification and manifest as appropriate. Do not duplicate a requirement into multiple authoritative specifications.
10. At the end of an agent session, provide a compact checkpoint: changed paths, tests run, tests not run, known risks, next single task and exact baseline identity. Never claim background work.

## Hard safety and integrity constraints

- **Never flash `target/esp32_adapter_link/`.** It is inert compile/link smoke code, not attendance firmware.
- `target/esp32_hardware_probe/` is diagnostic-only and is not attendance firmware. Review its source and wiring before any authorized diagnostic flash.
- `sketch/` is legacy diagnostic material. It contains a demonstration SoftAP credential and known duplicate-template risk. Never reuse that credential or assume the sensor is clean.
- Do not erase NVS, format/repair storage, truncate/quarantine files, change partitions, delete sensor templates, or run destructive recovery without explicit authorization and documented preconditions.
- Do not turn a host fake, POSIX test, linker-retention anchor or browser reference model into a claim of physical correctness.
- An append returning `false` may still have persisted bytes. Never retry a logical attendance event blindly after an indeterminate append.
- Do not bypass authentication, clock health, roster validation, storage ownership, attendance admission or request reconciliation to make a demo appear to work.
- Do not add reporting PII persistence/export or claim an authoritative SF2 export while the O8 and related gates remain unresolved.

## Production-candidate delivery

The active objective is a production candidate, not a graduation/demo prototype. Do not use demo-only bypasses, mock success paths in the firmware target, fake attendance data, weakened authorization, or unverified persistence shortcuts. Preserve signed product requirements and keep school-dependent/open reporting policy gated.

Prioritize the complete operational workflow and its dependencies: safe startup and storage, trusted clock, authenticated administration, roster and enrollment, fingerprint attendance, durable event admission, closure, review, backup/restore and approved reporting. A component is not integrated until the actual production composition root calls it.

Production-candidate claims require evidence by class: owning native tests, full regression, actual ESP32 target build and ELF inspection, and physical end-to-end/restart/fault testing. Host tests and compile/link results do not prove mounted-target durability or sensor behavior. Keep unverified functions disabled; label gaps plainly. Do not run tests/builds/PIO/flash/hardware unless the current user/task authorizes them.

When controlled testing is unavailable or the user prohibits execution, follow `docs/NO_TEST_ASSURANCE.md`. Use source inspection, traceability, range/resource reasoning, available static analysis and independent adversarial review to reduce risk; these methods do not replace execution evidence. Never claim no-break guarantees, production readiness or validated hardware behavior. Keep high-risk writes, migrations, restore/repair and sensor mutation disabled where their safety cannot be established without execution. Every unrun check remains `NOT RUN`.


`docs/ONE_DAY_EXECUTION.md` is historical graduation planning only and no longer governs current scope. Use the one-line codename instruction in AI_START.md for every fresh agent session. A ZIP upload does not guarantee automatic reading of repository context.
