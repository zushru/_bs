# Deadline-first development workflow

**Status:** Active operational workflow, aligned 2026-11-12 to the verified v0.2.95 source tree.  
**Scope:** Development execution, verification cadence, blocker handling and reporting only. This file does not approve product changes, alter decision status, or supersede signed/delegated requirements.

## 1. Authority and conflict handling

Apply the project source order:

1. `HANDOFF_FINAL.md`
2. `docs/DECISIONS.md`
3. The relevant approved specification
4. Production implementation and public interface
5. Owning tests and directly affected dependent tests
6. `docs/CRITICAL.md`, `docs/UNVERIFIED.md`, `docs/TESTS.md`, `docs/CONTEXT.md`
7. `docs/WORKING_MEMORY.md` and `docs/DEVELOPMENT_ROADMAP.md`

Preserve `SIGNED`, `DELEGATED`, `OPEN`, and `SUPERSEDED` statuses. Never turn an OPEN question into an implementation assumption. D1–D11 in `docs/SPECS/littlefs_recovery_protocol.md` remain DELEGATED engineering choices, not signed product rules. This workflow is subordinate to all product, security, data-format, and recovery decisions.

Use the dated historical portions of `HANDOFF_FINAL.md` as product/architecture evidence; do not blindly repeat obsolete resume-time test commands or historical test counts. The current resume state and verification cadence are in `docs/WORKING_MEMORY.md`, `docs/DEVELOPMENT_ROADMAP.md`, and the newest release/evidence entries. This distinction changes no product decision.

## 2. Session opening: verify before touching anything

Before remote edits or builds:

1. Confirm the authorized RDC device ID and current connection.
2. Confirm `pwd`, the actual project root, release/version markers and whether Git metadata exists. Do not assume `/root/att` or a version from memory.
3. Verify every entry in `MANIFEST.sha256` and compare protected-file hashes with the last verified baseline.
4. Inspect active processes/sessions and their exact PIDs. Do not duplicate, kill, or overwrite a running job without confirming ownership and state.
5. Record the baseline and current blockers before selecting a batch.

**Stop rule:** Any manifest mismatch, unexplained protected-file change, unexpected source drift, unknown active job, or wrong root means stop editing/building that snapshot. Preserve logs and ask for the intended diffs/current hashes or a refreshed verified archive/manifest. Do not reconcile, restore, or overwrite unknown work. No Git metadata exists in the current checkout; do not imply Git-based change history.

Batch remote work to conserve the phone, RDC call quota, and build time. Prefer one coherent inspection, one source batch, and one focused verification pass. Keep full logs outside the release tree. Use `set -o pipefail`; never mask failures with `|| true`.

## 3. Priority and blocker classification

Follow the approved dependency order:

1. **C13.3 — production LittleFS owner and target evidence.** Complete safe production storage integration and the exact ESP32 VFS evidence. Host tests and the standalone diagnostic sketch are not production-owner or durability proof.
2. **C1/C34 — NVS/config boot order.** Require the real `ConfigBootCoordinator` readiness path; adapter-local guards are not production startup readiness or a complete flash-erase firewall.
3. **C3 — production application root.** Compose one authoritative root/scheduler and one owner for storage, config boot, AS608, clock, attendance, closure and health.
4. **C6 — production HTTP/auth.** Integrate request-bound authorization, PIN verifier/KDF, sessions and protected routes.
5. **C2 — backup/restore.** Start only after storage ownership and the approved recovery design/commit behavior are established.
6. Resolve remaining independent blockers in coherent batches: E18/U5 sensor page/index semantics, U61 roster-history enforcement, U112 RAM-only request tickets, and SF2 reporting provenance/metadata/export.
7. Run the agreed integrated host and physical-device verification checkpoint.

Classify every blocker as one of these:

- **Development blocker:** prevents safe progress on dependent implementation. Resolve it before that dependent work.
- **Release blocker:** does not prevent independent development, but must be closed before release/use.
- **Evidence blocker:** code may exist, but the required proof is missing. Keep it explicitly open; do not convert implementation into a pass.

Continue safe independent work around release/evidence blockers. Do not block all development waiting for unavailable hardware. Do not build dependent features on an unsafe foundation.

## 4. Coherent batch protocol

State the batch goal, relevant files, owning/dependent suites, and affected target once. Then:

1. Read the controlling spec, implementation, public interface and owning tests together.
2. Identify one meaningful behavior gap or required integration boundary. Avoid unrelated refactors, cosmetics, speculative features and micro-increments.
3. For behavior changes, add the smallest real regression first and run it against the unchanged implementation to prove the gap. Preserve the useful red evidence.
4. Implement the complete safe batch, including actual owners/callers and shared-interface dependents. Do not weaken assertions to get green.
5. Rerun the owning suite once, then only directly affected dependent suites where shared behavior, state, persistence or callers changed. Documentation/mechanical-only edits do not need ritual tests.
6. Build only the affected ESP32 target after the batch is stable. Use at most `-j 2`; never run concurrent PlatformIO builds because target configurations share `.pio/build`.
7. Inspect the exact final ELF before another target configuration can replace it. Track and poll the exact process PID; timeout, kill or incomplete log is not a pass.
8. Critically review partial writes, duplicate retries, dead ends, races, error paths, rollback, security bypasses, duplicate sources of truth and spec mismatches.
9. Update the current release note, test ledger, context, blocker/evidence ledgers, working memory, changelog and manifest as applicable.

## 5. Verification cadence

- Native command: `pio test -e native -f 'actual_suite_directory'`. `-f` selects suite directories, not Unity test cases. Repeat `-f` only for directly affected suites.
- **Do not run full N1 yet.** It is deferred until C13.3, C1/C34, C3, C6 and C2 are sufficiently integrated for meaningful cross-module coverage. Then run it at the agreed integration/RC checkpoint.
- SF2 changes: run owning/dependent Node tests; reserve full `npm test` for the SF2 integration/RC checkpoint.
- Build only affected ESP32 targets. Compile/link, ELF inspection, mounted-target runtime, and physical-device tests are separate evidence classes.
- Inventory counts are not test passes. Never add overlapping suite results and call them a unique full-suite count.
- A host fake/POSIX test does not prove ESP-IDF VFS behavior. A successful link does not prove runtime. An inert target is never attendance firmware and must never be flashed.
- Do not rerun expensive full suites merely for reassurance. Do rerun the exact required checks after relevant source changes and at integration/RC gates.

## 6. Two-pass deadline execution

Use these as planning targets, not permission to skip required proof:

- **Pass A — Build (about 60%):** complete the highest-priority safe roadmap batches; integrate real owners/callers; add focused regressions; continue independent source-only work when hardware is unavailable.
- **Pass B — Stabilize (about 25%):** freeze new features; fix only required-behavior failures, data-integrity risks, security defects and critical integration problems; run affected regressions and one critical review.
- **Verification reserve (about 15%):** execute the agreed integration/release checks and physical tests that are possible; document every unavailable proof as open.

When time shrinks, reduce feature scope and defer nonessential work. Do not reduce safety requirements, skip required decisions, or relabel missing evidence as a pass. If a critical test needs more time, take it from feature scope, not from the test.

Switch from Pass A to Pass B when the available build window closes, remaining approved work is blocked by decisions/hardware, or a critical integrity/security defect demands stabilization. Do not keep adding features until the final minute.

## 7. Non-negotiable safety boundaries

- **LittleFS/log/restore:** `LogStore::open()` remains read-only/fail-closed for anomalous tails. Preserve bytes; do not replay a healthy prefix after an anomaly. A false append may have written partial or complete bytes; reconcile before retry. Never production-call non-transactional `MonthSet::repair()`. No format, truncate, quarantine, repair, journal or restore swap without approved design, proven target commit point, authorization, preconditions and interruption matrix.
- **NVS/auth:** no deliberate NVS corruption/erase or partition changes on the only device without exact authorization and preconditions. The current erase guard is narrow. No production-auth claim until request-bound authorization, PIN verifier/KDF, sessions and fault gates are integrated.
- **DS3231/I2C:** preserve signed P10/PHT UTC+8 interpretation. The board owner initializes shared I2C once and serializes clients. OSF means untrusted time; set/validate before clearing. After uncertain clock commit, stop writes and re-read/reconcile before retry.
- **Sensor/physical:** E18/U5 semantics remain unresolved. Do not infer sensor enrollment/page behavior from mocks. Never flash the inert adapter-link image. Confirm device and exact operation before physical/destructive actions.
- **Power:** the device must be usable while USB-C charging; do not assume a charger-only module or unverified power bank meets load-sharing and ESP32 peak-current needs.

## 8. Required batch report

Report once per batch, without a long narrative:

- **Yield:** what usable behavior/integration changed.
- **Baseline:** exact starting and resulting version.
- **Files:** exact changed files.
- **Verification:** exact commands, suite counts, exit statuses and log paths.
- **Not run/evidence needed:** what was skipped and why; distinguish host, target, ELF, mounted-target and physical evidence.
- **Remaining:** blockers and required authority/evidence.
- **Next:** highest-value safe batch.

When the user says **CONTINUE**, select and execute the next highest-priority safe batch. Do not ask them to choose routine engineering tasks. Stop only for a decision, authorization, essential missing input, baseline mismatch, unsafe state or required hardware proof. Never promise background work or claim future completion.
