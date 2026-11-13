# SHRU critique — production-candidate scope review
Date: 2026-11-13
Reviewer: SHRU
Target: PISHDA (sole implementation owner)
Baseline reviewed: documented v0.2.95 development snapshot; current working-tree integrity is NOT ESTABLISHED.
Review type: SOURCE-INSPECTED static review. No project commands, tests, builds, PlatformIO, flash or hardware actions were run.

## Executive conclusion

The current target is a restricted read-only boot/status shell, not an operational attendance system. Recent increments have added real safety boundaries and more READY/NEEDS_SETUP composition, but they have not yet produced a reachable end-to-end workflow. Do not measure progress by source registrations or number of helper classes. Measure it by completed, serialized, reachable, persisted workflows and their failure/restart behavior.

Current allowable claim: an unvalidated production-candidate development tree. It is not production-ready, hardware-verified, or suitable for authoritative attendance records.

## Scope interpretation

Controlling direction: S1 in docs/DECISIONS.md and the production-candidate directive in WORKING_MEMORY.md. Preserve all signed product rules and OPEN policy gates. docs/ONE_DAY_EXECUTION.md is historical only.

Complete the core product before dependent reporting work: safe startup; first provisioning; authenticated administration/configuration; roster; enrollment; fingerprint attendance; durable log admission; trusted clock/health; closure; operator feedback/UI. Backup/restore and SF2 remain in scope as dependent workstreams, but must not bypass C13/C2, O8/R18 or unresolved school decisions.

The no-execution assurance protocol does not waive release gates. Without execution, keep runtime-dependent writes and irreversible operations disabled. A source-complete candidate remains UNVALIDATED.

## Findings

### P0 — confirmed blockers

1. **No operational production workflow.**
   - Evidence: target/esp32_attendance/main.cpp; registerReadOnlyRoutes(); Application; setup()/loop().
   - Only explicit routes are GET / and GET /api/status. Unknown GET returns 404; other methods return 405.
   - The target does not compose ApplicationAttendanceGate, As608AttendanceOwner, ClosureEngine or RosterEventWriter. No production scan scheduler, enrollment queue/facade, clock route, roster mutation routes or attendance commit path exists.
   - Impact: operator cannot complete setup submission, enroll learners, record attendance, correct records or close a school session.
   - Closure: one serialized application owner with reachable, typed routes and bounded services; establish the complete workflow before enabling capabilities.

2. **Writable storage is not available; physical partition discrepancy remains unresolved.**
   - Evidence: lib/hal_esp32/src/esp32_littlefs_mount.cpp/.h; target/esp32_attendance/main.cpp; HANDOFF_FINAL.md.
   - Mount backend sets read_only = true and checks exact signed partition geometry. Signed spiffs geometry is 0x190000 / 0x270000; documented device snapshot is 0x290000 / 0x160000.
   - Impact: current mount/replay cannot be treated as the production writable store; guard rejects reported device geometry.
   - Closure: reconcile actual device layout under product authority. Do not alter signed partition constants, force mount, format, migrate or enable writes to bypass this gate. Then establish pinned-target VFS/durability and restart/fault evidence.

3. **Clock trust is deliberately false and attendance cannot be enabled.**
   - Evidence: target/esp32_attendance/main.cpp updateRuntimeHealth(); observeRtcStartup(); HealthInputs.
   - clockTrusted is always false in this shell. ClockConfirmationService/AdminClockConfirmation are not invoked from a production route or root.
   - Closure: integrate the dispatcher behind exact request auth and the single serialized owner; handle every ClockApplyResult and uncertainty path. Keep attendance off until explicit clock confirmation and health re-evaluation are complete.

4. **Latest target/source integration lacks corresponding execution evidence.**
   - Evidence: WORKING_MEMORY.md, docs/NO_TEST_ASSURANCE.md, docs/CRITICAL.md.
   - Recent registrations are source inventory, not pass counts; latest additions are NOT RUN. Historical host/POSIX runs and diagnostic compile/link do not prove current attendance target build, target VFS or physical behavior.
   - Closure: label all evidence exactly; continue static inspection but do not claim validated operation. Preserve future production build, ELF/memory review, target, E2E and physical gates.

### P1 — serious integration and correctness risks

5. **Roster writes remain bypassable.**
   - Evidence: lib/core/src/roster_event_writer.h/.cpp; lib/core/src/log_store.h; docs/CRITICAL.md U61.
   - RosterEventWriter plans, appends and checks observer postconditions, but has no production caller. Public LogStore::append() still allows a future handler to bypass planner admission. Observer callbacks occur after persistence and cannot veto.
   - Closure: route every roster change through one writer under the same serialized owner; no route should receive a raw append capability. Preserve E27 reporting-only semantics and E34 pre-append/replay checks. Reconcile uncertain append before any retry.

6. **Enrollment is not a production workflow; U5 physical semantics remain open.**
   - Evidence: lib/hal_esp32/src/as608_attendance_owner.h/.cpp; lib/hal_esp32/src/as608_startup.cpp; docs/CRITICAL.md E18/U5.
   - As608AttendanceOwner exposes startup and scan but deliberately no enrollment facade. Queue, ownership, cancellation and verification policy must be connected to a production service. Signed logical mapping E18 does not verify actual sensor index bit order/page behavior.
   - Closure: verify physical index semantics using known occupied pages before trusting page-to-learner identity; then integrate the signed enrollment queue through one sensor owner. Do not mutate/delete templates to make progress.

7. **Closure and admission may produce unbounded/expensive loop work.**
   - Evidence: lib/core/src/closure_engine.cpp ClosureEngine::step(); lib/as608/src/log_attendance_admission.cpp viewFor_(); lib/core/src/day_view.cpp buildDayView().
   - Closure step can walk up to MAX_TRACK_DAYS (400) × 2 sessions and append closure records in one call. Admission cache miss rebuilds a day view by scanning the log (O(n)).
   - Classification: PLAUSIBLE RISK, not measured timing failure.
   - Closure: design bounded incremental service steps/budgets; do not call a large historical sweep directly in every fast loop iteration. Consider bounded/indexed admission queries. Measure only when execution is later authorized.

8. **Sensor match-to-commit and operator feedback are not connected.**
   - Evidence: lib/as608/src/as608_attendance_pipeline.cpp; target/esp32_attendance/main.cpp.
   - Pipeline contains sensible validation and distinguishes uncertain append, but the target has no production owner/scheduler or user-facing commit-result path.
   - Closure: one exact path from scan result to durable append to confirmed operator feedback. On indeterminate append, stop and reconcile; never blindly retry.

9. **RAM/stack/latency budgets are not established.**
   - Evidence: docs/UNVERIFIED.md; target/esp32_attendance/main.cpp Application; E35 KDF integration.
   - The roughly 18 KiB replay-projection estimate is not target sizeof/BSS/map/high-water evidence and predates some newer runtime objects. PBKDF2, complete HTTP+I2C+UART activity, loop stack and heap margins are unmeasured.
   - Classification: UNVERIFIED.
   - Closure: before expanding the object graph, keep an explicit RAM/stack budget; later obtain exact target linker/map and runtime high-water/latency evidence. Do not infer headroom from the diagnostic probe build.

10. **OLED disclosure/blanking is not physical acknowledgement.**
    - Evidence: lib/hal_esp32/src/esp32_oled.h/.cpp; lib/hal_esp32/src/esp32_ram_setup_code_verifier.cpp.
    - Adafruit display() returns void. EOA_TRANSFER_REQUESTED indicates a transfer request, not visible pixels or a completed physical blank. Setup secret invalidation/wiping before blank request is a sound security choice, but stale screen content/operator confusion remains possible.
    - Classification: confirmed interface limitation; physical behavior unverified.
    - Closure: keep code invalidated on expiry/use/failure; present physical visibility and failure feedback as an explicit hardware validation gate. Do not mislabel transfer-request success as display confirmation.

11. **Accepted security residuals must remain explicit.**
    - Evidence: E33/E12 in docs/DECISIONS.md.
    - Code-only PIN reset/restore and lack of a permanent cumulative guess ceiling are accepted policy risks; HTTP without TLS is also part of the signed threat model.
    - Closure: preserve these boundaries; document residual risk. Do not silently claim stronger security or change product policy under an integration patch.

### P2 — repository assurance/process

12. **Current documentation contains stale/inconsistent state.**
    - Evidence: ARCHITECTURE.md §4.2/§4.6 says projections/expiry service are not integrated, but main.cpp now attaches EventGroupAudit/RosterStore/LogAttendanceAdmission and loop() services expiry. WORKING_MEMORY.md and index.md report inconsistent registration counts.
    - Closure: align navigation/state docs with source after integration increments. Maintain one canonical current inventory. This is documentation drift, not a source behavior defect.

13. **Manifest is stale and Git metadata is absent.**
    - Evidence: WORKING_MEMORY.md and index.md record 72 failed checksum entries; no Git metadata in snapshot.
    - Closure: do not regenerate MANIFEST merely to get green. Preserve baseline identity, before-images, exact diffs and SHA-256 read-back for changes. Never claim clean branch/commit integrity.

## Positive source observations to preserve

- First-run sequence is comparatively disciplined: AP credential before AP startup, DHCP gate, separate durable setup counter, attempt persistence before verification, one-time code consumption, PIN preflight before code consumption, credential preparation after authorization, ConfigBootCoordinator as sole first durable writer, and fresh-boot READY reconstruction.
- Setup-code verifier uses RF-gated RNG, monotonic lifetime, invalidates/wipes before blank request, and fails closed on monotonic/RF/blank-transfer errors.
- READY boot validates/replays storage before exposing AP/HTTP, and attaches EventGroupAudit, RosterStore and LogAttendanceAdmission before any future scan owner.
- LogStore documentation correctly treats append false as indeterminate.
- Exact partition guard, no-format behavior, and fail-closed health are preferable to forced progress.
- I did not identify a concrete defect in the inspected first-run ordering; principal gap is that no route invokes it and no target/runtime evidence exists.

## Recommended implementation order

1. Resolve actual-vs-signed partition geometry and writable-storage/durability/recovery prerequisites. Do not enable writes first.
2. Complete the single serialized application owner, including exact storage/config/log/roster/health/clock bindings and clock confirmation.
3. Add typed request parsing and authenticated routes through existing boundaries; first-run setup is a natural initial vertical slice, but keep mutation disabled until prerequisites are met.
4. Integrate roster writes exclusively through RosterEventWriter.
5. Integrate sensor startup, physical U5 validation and enrollment queue/facade.
6. Integrate automatic scan scheduling, attendance commit, bounded closure and operator feedback/UI.
7. Complete backup/restore transaction and approved SF2 reporting as dependent workstreams; observe O8/R18 and unresolved school-policy gates.

## Explicit non-goals for the current increment

Do not add OTA, new TLS/network threat assumptions, speculative persistence schema, auto-repair/format, partition changes, sensor template deletion/reset, second mutable application owner, mock success paths, or unapproved SF2 PII/export. Do not claim no-test assurance guarantees.

## Review limitations

This is a source-level adversarial critique of the inspected authority documents and relevant implementation paths, not a complete line-by-line review of every subsystem. No tests, compilers, builds, PlatformIO, flashing or hardware actions were performed. Findings tagged PLAUSIBLE RISK / UNVERIFIED require later evidence; they are not claimed runtime defects.

SHRU remains optional and does not gate PISHDA's work. This report exists because the user explicitly requested that the review be communicated to PISHDA.
