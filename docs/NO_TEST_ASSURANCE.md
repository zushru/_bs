# No-execution assurance protocol for ATT

**Status:** Active engineering workflow under the user's 2026-11-13 direction. This document does not change signed product requirements or authorize a release.

## 1. The hard limit

There is no defensible way to guarantee that a non-trivial embedded system will not break if its executable behavior is never exercised. Analysis can establish source-level facts and prove bounded properties under explicit assumptions; it cannot establish that the ESP32 toolchain, scheduler, peripheral timing, sensor module, NVS, mounted LittleFS, power behavior or complete production composition behaves as expected.

Accordingly, use this protocol when controlled execution is unavailable:

- Develop a **production candidate**, but label unexercised behavior `UNTESTED` / `NOT RUN`.
- Never translate source inspection, a passing text checker, authored test cases, a host fake, or a compile/link result into a runtime or hardware claim.
- Keep any capability whose safety depends on unverified runtime/persistence behavior disabled or read-only. Do not bypass a gate just to complete the feature list.
- Do not call the candidate production-ready, validated, reliable, durable or hardware-compatible until the corresponding evidence exists.

NASA distinguishes verification from validation and lists analysis, review, inspection and testing as different verification methods. It also identifies testing as the primary means for verifying implementation behavior, while static analysis, memory analysis and complexity analysis are useful supporting techniques. NIST SSDF similarly separates code review/analysis (PW.7) from executable-code testing (PW.8). These references support using static assurance when execution is impossible; they do **not** claim that static assurance replaces testing.

## 2. Required change protocol

Every source change follows these steps in order. A change that cannot satisfy a step is narrowed, deferred or left disabled.

### A. Establish authority and baseline

1. Read the mandatory project context in its required order, then the signed handoff, applicable decision rows, module specification, architecture, critical/unverified register and direct callers.
2. Identify the exact source baseline and changed-path ownership. Preserve before-images, a bounded unified diff and SHA-256 read-back evidence. Never infer Git state; this snapshot has no Git metadata.
3. Record the governing requirement and decision status (`SIGNED`, `DELEGATED`, `OPEN`, etc.). Source code is not permission to change policy. Stop when authority conflicts.
4. Check the existing integrity manifest read-only. A stale manifest is a warning, not permission to regenerate it just to obtain a clean result.

### B. Write the proof obligations before code

For each change, record:

- **Claim:** what one behavior is being changed, and what remains unchanged.
- **Preconditions:** required boot state, valid config, ownership, clock/time source, authentication, storage health, sensor readiness and caller serialization.
- **Postconditions:** exact observable state and durable result required on success.
- **Invariants:** facts that must remain true before, during and after every operation.
- **Failure semantics:** every error, timeout, partial I/O, power interruption, stale request, duplicate request, reset and recovery path; identify whether a failed return can still mean data was committed.
- **Assumptions:** toolchain, library, hardware, storage and operator assumptions that cannot be established from source.
- **Evidence method:** inspection, traceability, range/resource analysis, static analyzer, formal proof, compile-only evidence or execution test. Mark unavailable methods `NOT RUN`, not passed.

Requirements must be analysed as a related set, not as isolated sentences. Trace each affected requirement to the implementation, all callers, relevant observers, persistence format and failure/recovery owner.

### C. Minimize the change surface

1. Prefer one bounded owner and a small additive patch. Do not combine refactoring, feature work, data-format changes and dependency upgrades.
2. Preserve public interfaces and persistent encodings unless the governing decision explicitly authorizes change.
3. Do not add concurrency, callbacks, timers, background tasks, dynamic allocation or a second owner for mutable state without a complete ownership and cancellation design.
4. Prefer compile-time bounds, fixed-size buffers, explicit enum/state transitions and typed results. Reject unknown values and invalid states.
5. Keep previous known-good state intact until a replacement has been validated and committed. Never auto-format, silently initialize missing security state, truncate, erase, overwrite or retry an ambiguous write.
6. If a transaction can partially commit, specify the exact commit point and recovery behavior. A false/timeout result must be treated as indeterminate unless the storage contract proves otherwise.

### D. Perform static verification

At minimum, inspect the actual diff and reason through the following for every changed path:

| Area | Required source-level analysis |
|---|---|
| Authority and requirements | Bidirectional traceability; no contradictory or invented policy; caller and interface contract preserved. |
| Call graph and integration | All call sites, constructors, observers, event consumers, route registrations, scheduler callers and teardown/reboot paths. Prove the production root really owns and invokes the component before claiming integration. |
| State machines | Every state and enum value; legal and illegal transitions; terminal states; fault latching; reset/re-entry; timeout and repeated-call behavior. |
| Memory safety | Array index and length bounds; nullability; object lifetime; initialization order; placement-new alignment; buffer termination; stack versus static/heap placement; integer promotion and signedness. |
| Arithmetic and time | Overflow/underflow; narrowing casts; multiplication/addition bounds; epoch floor; local-time conversion; monotonic-clock regression and wrap; inclusive/exclusive boundaries. |
| Persistence | Exact bytes and ordering; schema compatibility; sequence numbers; group framing; observer order; partial append; read-back; replay; reset; power-loss assumptions; no blind retry. |
| Concurrency and ownership | One mutable owner; reentrancy; callback context; shared objects; leases; lock scope; cancellation; observer lifetimes. Never infer thread-safety from a narrow mutex. |
| Security | Exact request-bound token; authorization before side effects; durable lockout ordering; entropy lifecycle; secret clearing; fail-closed defaults; no sensitive public fields or accidental logs. |
| Resource use | Static object growth; bounded temporary storage; heap/stack ownership; observer capacity; worst-case record counts; CPU/timing claims explicitly marked unmeasured. |
| Recovery and operator behavior | What remains usable after each fault; visible error state; no silent data loss; safe restart/reopen path; no recovery action that can worsen corruption. |

Use manual line-by-line inspection plus available source-only analyzers or coding-standard checks when authorized. Do not invent analyzer results. Complexity and memory estimates are estimates until produced by the exact target toolchain; a successful compile is not a memory or timing measurement.

For the current READY-only replay increment, a structural estimate puts the additional `RosterStore`, `DayView` inside `LogAttendanceAdmission`, and `EventGroupAudit` storage at roughly 18 KiB of static RAM. This is an ABI/layout estimate from the declared arrays, not a measured `sizeof`, linker map or free-heap result. Alignment, actual target type sizes, total application BSS, task stack headroom and runtime high-water are still UNVERIFIED; do not infer available headroom from this estimate.

### E. Adversarial review (PISHDA-owned; optional user-invoked SHRU input)

PISHDA must perform a distinct adversarial self-review of the authority, full diff, before/after source, callers and failure paths. This is author self-review, not independent verification. SHRU is entirely optional and user-invoked; the user may independently prompt it to submit a read-only critique to comms/inbox/SHRU/. PISHDA does not assign, invoke, poll or routinely inspect SHRU or its inbox, and reviews a report only when the user explicitly asks. SHRU feedback is never a prerequisite for progress and its absence is not evidence of approval. Ask the review process to find counterexamples, not to approve the author's summary. Try to falsify each claim:

- Can a caller bypass the new boundary?
- Is there another writer or observer that sees a different order?
- Can a failure occur after a side effect but before acknowledgement?
- Can reboot expose a partial or incompatible persistent state?
- Can time move backwards, inputs be empty/truncated, capacities be zero/maximum, or an enum contain an unknown value?
- Does an existing device/state behave differently from a fresh one?
- Is the claimed safety property conditional on an assumption that was never recorded?

Record findings and unresolved objections. A PISHDA self-review must never be labeled independent. A SHRU critique is useful adversarial input but is not formal verification and does not replace tests, target evidence or the production release gates.

## 3. ATT-specific hard boundaries

These are mandatory checks because the system combines event-sourced records, authentication, clock policy and a physical fingerprint sensor.

| Subsystem | Never assume | Required conservative behavior without runtime evidence |
|---|---|---|
| NVS/config boot | Missing data means blank/new device; A/B writes are power-loss atomic just because read-back exists. | Preserve old slots, fail closed on malformed/missing READY state, and do not silently initialize or erase. |
| LittleFS/event log | POSIX behavior proves ESP-IDF VFS behavior; `append(false)` means nothing was written; a valid prefix proves a valid complete log. | No-format mount, read-only validation, full replay, reject ambiguous tails/groups, stop on uncertain append, reopen/reconcile before any retry. Keep writes disabled until target durability and recovery are established. |
| Clock | A parseable RTC calendar is trusted time; wall-clock time is monotonic. | Keep `clockTrusted=false` until explicit policy confirmation; use monotonic time for expiry/lockout and UTC/local-day rules only through the signed conversion path. |
| Authentication | A cached token-valid boolean or client-provided health flag is authoritative. | Authenticate the exact current request before side effects; keep PIN and setup-code attempt domains separate; fail closed on missing READY security state. |
| Roster | A raw event append is equivalent to a planner-approved mutation. | Plan before append, check the exact live observer postcondition, and stop on uncertain commit. Preserve E27 reporting-only semantics and E34 per-learner day ordering. |
| AS608 | Logical page arithmetic proves physical index-bit ordering; sensor UART success proves enrollment/attendance state. | One owner, one lease, bounded protocol parsing, no destructive template operation without a separately approved workflow; keep attendance off until physical index and end-to-end behavior are verified. |
| Attendance | Sensor match alone authorizes an attendance event. | Require trusted time, complete roster, active/expected learner, valid occupancy, current session/window, no-class/closure admission and exactly one durable event path. |
| Closure/backup/restore/SF2 | Source models imply production scheduling, atomic restore, data provenance or approved school rules. | Do not enable until their owners, persistence/recovery, PII policy and school-specific decisions are complete. |

## 4. Data protection and rollback rules

- Treat existing attendance, roster, credentials, AP configuration and sensor templates as irreplaceable. A migration must be non-destructive, versioned, explicitly authorized and read-back verified; if its recovery cannot be justified, do not run it.
- Do not promise rollback unless both the previous firmware and its compatibility with the current persistent data are established. A source snapshot is not a proven recovery image. The current target partition mismatch and lack of demonstrated target recovery mean firmware/data rollback must not be assumed.
- No destructive repair, format, NVS erase, partition change, sensor-template reset or bulk rewrite is part of a testless assurance task.
- Prefer fail-closed read-only degradation over accepting uncertain identity, time, log, roster or sensor state.
- Do not use a live deployment as an improvised canary when controlled testing, monitoring and rollback are unavailable. Canary releases reduce impact only when observation and rollback actually exist.

## 5. Evidence labels and release claims

Use precise evidence labels; they are not interchangeable:

- `SOURCE-INSPECTED`: the specified source and diff were read; no execution claim.
- `STATIC-ANALYZED`: named static checks/analyses ran on the specified source and their outputs were reviewed.
- `FORMALLY-PROVED`: a proof artifact exists, names its assumptions and applies to the exact code/model/version.
- `COMPILED` / `TARGET-COMPILED`: the exact named target compilation completed; this is not runtime evidence.
- `HOST-TESTED`, `TARGET-TESTED`, `HARDWARE-VERIFIED`: only for the exact tests actually executed in the named environment.
- `UNTESTED` / `NOT RUN`: execution evidence is absent. Authored test source is still `NOT RUN`.

Every claim in a release note or status report must identify its evidence, assumptions and unverified conditions. If no tests were run, say so plainly. A source-complete system with no execution evidence remains an **unvalidated production candidate**, not a production-ready release.

## 6. Release decision under a no-testing constraint

When controlled testing is unavailable, the allowable result is limited to a source-reviewed candidate whose unproven capabilities remain disabled or explicitly non-operational. Do not enable live attendance writes, credential migration, storage repair, restore, destructive sensor operations or other irreversible actions solely on static confidence.

The following evidence is still required before production release and cannot be replaced by this protocol:

1. Exact production ESP32 compile/link and memory/ELF inspection.
2. Target-specific startup, NVS and mounted filesystem behavior.
3. End-to-end authenticated setup, roster, fingerprint enrollment, attendance commit and closure.
4. Reboot/replay persistence and power-interruption recovery.
5. Physical AS608 index/page behavior, UART failure handling and OLED/operator feedback.
6. Security, resource, long-run and failure-recovery evidence in the intended environment.

If none of those can be obtained, preserve the candidate as source only and do not deploy it for authoritative attendance records.

## 7. Research basis

This protocol adapts the following primary guidance; it is not a claim of certification or compliance:

- [NASA-STD-8739.8B — Software Assurance and Software Safety Standard](https://standards.nasa.gov/standard/nasa/nasa-std-87398), active standard for systematic assurance and independent verification/validation.
- [NASA SWE-067 — Verify Implementation](https://swehb.nasa.gov/spaces/SWEHBVB/pages/32604539/SWE-067%2B-%2BVerify%2BImplementation), which distinguishes verification from validation and identifies traceability, review, analysis, static analysis, memory analysis and complexity analysis while stating testing is the primary implementation-verification method.
- [NASA SWE-028 — Verification Planning](https://swehb.nasa.gov/spaces/7150/pages/16450576/SWE-028%2B-%2BVerification%2BPlanning), which calls for planned methods, environments, success criteria and recorded results across the lifecycle.
- [NIST SP 800-218 — Secure Software Development Framework v1.1](https://csrc.nist.gov/pubs/sp/800/218/final), especially PW.7 (review/analyze human-readable code) and PW.8 (test executable code), separate practices.
- [ISO/IEC/IEEE 15026-2:2022 — Assurance Case](https://www.iso.org/obp/ui?_escaped_fragment_=iso%3Astd%3Aiso-iec-ieee%3A15026%3A-2%3Aed-2%3Av1%3Aen), for structuring claims, arguments and evidence. The standard explicitly does not guarantee the quality of an assurance case's contents; the argument still has to be sound.
- [Google SRE — Production Services Best Practices](https://sre.google/sre-book/service-best-practices/) and [Reliable Product Launches](https://sre.google/sre-book/reliable-product-launches/), for conservative configuration validation, staged release, observation and rollback. These controls cannot be claimed where the project lacks dependable monitoring or rollback.

**Bottom line:** without execution, we can improve source correctness and limit blast radius. We cannot know that the complete ESP32 attendance system works. Preserve that distinction in every engineering decision and every release claim.
