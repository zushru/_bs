# ATT coordination and SHRU critique register

**Project:** ATT — ESP32 fingerprint attendance system
**Repository baseline:** v0.2.95 development snapshot; current working-tree integrity is not established by MANIFEST.sha256.
**Effective coordination model (2026-11-13):** PISHDA is fully independent and is the sole implementer and repository writer. SHRU is 100% optional, user-invoked only, and may write critique notes exclusively to its inbox.

This file is the current role/permission register and a concise handoff history.
Historical assignments below do not override the current permission model.

## 1. Active roles

| Codename | Permanent role | Permissions |
|---|---|---|
| PISHDA | Sole project developer, technical coordinator, primary RDC operator and integrator | May read/write all project paths, subject to product authority and repository safeguards |
| SHRU | Optional, user-invoked read-only critic (outside PISHDA workflow) | May read project content; may write critique reports only inside comms/inbox/SHRU/; no other write or execution authority |

QYUPAL and SEI are retired from the project workflow. Their earlier reports,
assignments and contributions remain historical records only.

### PISHDA — sole developer and repository owner

- Owns all implementation, tests, documentation, architecture, integration,
  coordination and release-preparation work.
- Is the sole writer to every repository path outside SHRU's inbox.
- Performs source inspection, adversarial self-review, atomic edits and
  independent read-back of each changed file.
- Does not call self-review independent verification, and does not claim
  production readiness without the required evidence.

### SHRU — optional user-invoked read-only critic

- May inspect any repository file and submit counterexamples, risks,
  assumptions, specification conflicts and review findings.
- May create or revise report files only within comms/inbox/SHRU/.
- Must not edit source, tests, documentation, specifications, coordination,
  staging, build configuration, manifests or any other project path.
- Must not run tests, compilers, builds, PlatformIO, flashing, hardware actions
  or project-mutating commands.
- Must label each finding as confirmed issue, plausible risk, or not provable
  without execution; cite exact paths/symbols/lines and state inspection gaps.
- SHRU has no standing task or review schedule. The user may prompt it
  independently for any read-only critique.
- PISHDA does not assign, invoke, poll, wait for or routinely inspect SHRU or
  its inbox. PISHDA reads a report only when the user explicitly asks it to
  review or incorporate that report. SHRU never gates PISHDA's work.

## 2. Sole-writer and permission rules

PISHDA is the sole writer and integrator for the canonical repository. There
are no parallel implementation tasks, shared file locks or delegated source
write scopes.

SHRU's sole write exception is:
comms/inbox/SHRU/<TASK-ID>.md

That exception permits critique reports only. It does not permit staging
files, patches, pre-images, edits to an existing report outside its inbox,
source/test changes, shell commands or generated artifacts elsewhere.

All previous SHRU source/test write assignments are revoked effective
2026-11-13:
- ATT-P15-MONOTONIC-CROSS-OP-REG-01 — former verifier regression path.
- ATT-U61-ROSTER-WRITER-01 — former new roster-writer source/test paths.

PISHDA will inspect the current canonical contents of those paths before any
follow-on work. Completed SHRU contributions already reviewed and accepted
remain part of the baseline; revocation does not revert accepted changes.

QYUPAL and SEI have no active role, task, permission or inbox requirement.
Their prior project records are retained only as provenance. Do not assign
new work to them or wait for their reports.

## 3. Optional, user-invoked SHRU input

SHRU is not part of PISHDA's normal workflow and has no standing assignment.
The user may independently prompt SHRU for any read-only critique. SHRU may
inspect project content and submit notes only inside comms/inbox/SHRU/.

PISHDA does not assign, invoke, poll, wait for or routinely inspect SHRU or
its inbox. PISHDA reads or acts on a SHRU report only when the user explicitly
asks PISHDA to review or incorporate it. A report, an absent report, or an
unread inbox has no effect on task sequencing, implementation or release.
There is no scheduler, live notification or background reviewer.

For any report the user explicitly asks PISHDA to assess, check:
- Exact baseline and workspace/canonical-tree status.
- Exact files, symbols and line ranges inspected; material omissions.
- Findings distinguished as confirmed issue, plausible risk, or unprovable.
- Evidence and impact, with a proposed safe correction where appropriate.
- Explicit statement that tests/builds/hardware were not run.

## 4. Task and evidence discipline

PISHDA owns task sequencing, all implementation and integration. Product
requirements and OPEN decisions remain under the user's authority. No source
change can silently alter signed requirements or resolve an OPEN decision.

Preserve exact baselines and before-images for consequential edits. Stage
changes under comms/staging/PISHDA/<TASK-ID>/, inspect the complete diff,
read back every changed path and record hashes. Multi-file replacements are
not globally atomic; prepare complete replacements first and roll back any
reported partial write failure.

Do not infer test/build/runtime/physical evidence from source inspection.
Follow docs/NO_TEST_ASSURANCE.md whenever execution is unavailable or
prohibited. Tests, compilers, builds, PlatformIO, flashing and hardware actions
remain subject to explicit user authorization.

## 5. RDC resource policy

RDC calls are scarce. PISHDA is the only routine RDC operator and must batch
known-path reads, avoid redundant device checks and prefer one targeted source
search over repeated exploratory calls. Use additional calls whenever needed
for correctness; efficiency never justifies skipping required read-back.

SHRU inbox review is not part of PISHDA's routine RDC workflow. Do not
poll the inbox or claim realtime/background monitoring. Never claim an
agent is working in the background.

## 6. Historical assignment and review record

**Permission override:** records in this section preserve historical task context. They do not grant source-write permission. The only currently permitted SHRU activity is optional read-only critique, with reports written only to comms/inbox/SHRU/.


### PISHDA — ATT-P15-OLED-01 — IMPLEMENTED / AWAITING REVIEW

Implement the standalone OLED six-digit setup-code disclosure primitive.
Requirements: exact six ASCII digits; preserve leading zeroes; reject null,
wrong length and every non-digit; clear a previous frame on invalid input when
the bus lock is available; acquire the shared I2C lock before display access;
request display transfer and immediately clear the OLED software framebuffer;
do not create a second plaintext code buffer; do not conflate this with the
32-character AP password or a production disclosure acknowledgement.

PISHDA held the sole-writer lock for this bounded patch and has released it
after source read-back. Seven regression cases were authored but not executed.
No tests, compiler checks, builds, PlatformIO, flash or hardware actions were
authorized.

### SHRU — ATT-P15-OLED-SHRU-REV-01 — REVIEWED / COMPLETE

SHRU submitted `comms/inbox/SHRU/ATT-P15-OLED-SHRU-REV-01.md` from the
canonical shared tree. PISHDA reconciled the findings against the current
source/spec/test context: no confirmed production-code defect. Accepted
findings: the host fake cannot prove pixel-level layout or actual driver
framebuffer zeroization; one punctuation-byte rejection regression is missing.
The report correctly preserves the distinction between transfer requested and
physical visibility. No source change was made by SHRU in this review.

### Retired QYUPAL / SEI OLED review assignments — NOT ACTIVE

The former QYUPAL and SEI OLED review requests are retired with those roles.
Their historical instructions are not current assignments and no reports are
awaited. Existing findings already received remain historical input only.

### SHRU implementation assignments — 2026-11-13

Three independent, test-only tasks were assigned. Their project write paths
were disjoint. PISHDA has independently reviewed and accepted all three at
source level; their test files remain NOT RUN and their write scopes are
released. The task details below remain as provenance. The other SHRU tasks
retain their exact-path ownership. No assignment authorizes production behavior
changes or verification execution.

#### ATT-P15-OLED-TST-PUNCT-01 — PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TEST NOT RUN)

- Read: `docs/SPECS/esp32_oled.md`,
  `lib/hal_esp32/src/esp32_oled.h`,
  `lib/hal_esp32/src/esp32_oled.cpp`,
  `test/test_esp32_oled/test_main.cpp`.
- Write: `test/test_esp32_oled/test_main.cpp` only.
- Add a focused regression proving punctuation (for example, '-') is rejected
  by `showSetupCode()`; assert the documented invalid-input result and blank
  frame behavior without claiming pixel-level or physical-panel evidence.
- Do not change production OLED code, stubs, specs or other tests.
- Preserve the existing file before editing at
  `comms/staging/SHRU/ATT-P15-OLED-TST-PUNCT-01/before/test_main.cpp`;
  save its unified diff at
  `comms/staging/SHRU/ATT-P15-OLED-TST-PUNCT-01/patch.diff`.
- Report to `comms/inbox/SHRU/ATT-P15-OLED-TST-PUNCT-01.md`.

#### ATT-E27-PROJECTION-REG-01 — PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TEST NOT RUN)

- Authority/read: E27 in `docs/DECISIONS.md`,
  `lib/core/src/month_roster_projection.h/.cpp`,
  `lib/core/src/roster_transition_audit.h/.cpp`,
  `test/test_month_roster/test_main.cpp`.
- Write: `test/test_month_roster/test_main.cpp` only.
- Add a regression for an already-enrolled learner who is deactivated, then
  reactivated and deactivated again on the same local day. Operational log
  appends must remain accepted; the authoritative monthly projection must fail
  closed with the appropriate transition finding. This specifically covers
  ACTIVATE-then-DEACTIVATE ordering, which is not the same as the existing
  SET-then-DEACTIVATE and DEACTIVATE-then-ACTIVATE tests.
- Do not change roster event admission, projection source, policy or any other
  test file.
- Preserve the existing file before editing at
  `comms/staging/SHRU/ATT-E27-PROJECTION-REG-01/before/test_main.cpp`;
  save its unified diff at
  `comms/staging/SHRU/ATT-E27-PROJECTION-REG-01/patch.diff`.
- Report to `comms/inbox/SHRU/ATT-E27-PROJECTION-REG-01.md`.

#### ATT-P15-AUTH-BOUNDARY-REG-01 — PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TEST NOT RUN)

- Read: `docs/SPECS/api_logic.md`, `lib/core/src/admin_session.h/.cpp`,
  `lib/core/src/pin_auth.h/.cpp`,
  `test/test_admin_session/test_main.cpp`.
- Write: `test/test_admin_session/test_main.cpp` only.
- Add adversarial regressions for the existing `AdminSetupCodeBoundary`
  contract, prioritizing: malformed setup-code inputs are rejected before
  persistence/verifier/action; verifier infrastructure error latches the
  setup-code coordinator and never runs the action; and a failure to persist
  the post-match counter reset never runs the privileged action. Extend only
  the local test fake in this same file if needed to fail a selected persist
  call. Assert counters, verifier/action call counts and fault state.
- Do not implement a concrete verifier, KDF, storage format, entropy source,
  lifetime policy, counter-domain mapping, HTTP route or production caller.
- Preserve the existing file before editing at
  `comms/staging/SHRU/ATT-P15-AUTH-BOUNDARY-REG-01/before/test_main.cpp`;
  save its unified diff at
  `comms/staging/SHRU/ATT-P15-AUTH-BOUNDARY-REG-01/patch.diff`.
- Report to `comms/inbox/SHRU/ATT-P15-AUTH-BOUNDARY-REG-01.md`.

For all three tasks: baseline v0.2.95 development snapshot; current manifest is
stale and is not integrity attestation. Exact listed project file is the only
project write path; all unlisted files are read-only. Do not run tests,
compilers, builds, PlatformIO, flashing or hardware actions. Authored tests
must be marked NOT RUN. Include exact changed paths and before/after SHA-256
checksums in each report. PISHDA independently reviews each diff and changed
file before acceptance.

### SHRU - ATT-P15-RAM-CODE-VERIFIER-01 - PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TESTS NOT RUN)

- Authority: E33 in `docs/DECISIONS.md`; specification/context:
  `docs/SPECS/api_logic.md`, `docs/SPECS/esp32_oled.md`.
- Read: `lib/core/src/pin_auth.h`,
  `lib/core/src/pin_auth_session.h/.cpp`,
  `lib/core/src/api_logic.h`,
  `lib/hal_esp32/src/esp32_secure_random.h/.cpp`,
  `lib/hal_esp32/src/esp32_clock_monotonic.h`,
  `lib/hal_esp32/src/esp32_oled.h/.cpp`,
  `lib/hal_esp32/src/esp32_wifi_ap.h/.cpp`,
  `test/test_esp32_oled/stubs/Adafruit_SSD1306.h`,
  `test/test_esp32_oled/stubs/Wire.h` and
  `test/test_esp32_secure_random/stubs/esp_random.h`.
- Write exactly:
  `lib/hal_esp32/src/esp32_ram_setup_code_verifier.h`,
  `lib/hal_esp32/src/esp32_ram_setup_code_verifier.cpp`, and
  `test/test_esp32_ram_setup_code_verifier/test_main.cpp`.
- Implement `Esp32RamSetupCodeVerifier : ISetupCodeVerifier, ISetupCodeIssuer`
  using `setup_code_sequence.h`. Inject the same RF-gated
  `Esp32SecureRandom` instance passed by reference to
  `Esp32WifiAp` and an `IClockMonotonicSource` (production
  `Esp32ClockMonotonicSource`). AP_START causes the AP controller to call
  `notifyRfStarted()` on that same random instance. It must
  issue only after RF is ready; implement `issue()` for `ISetupCodeIssuer` and
  use the existing unbiased `generateSetupCode`.
  Issue exactly one active code, display directly from its owned buffer via
  `Esp32Oled::showSetupCode()` (no caller copy), and start a 600-second lifetime
  using monotonic microseconds. Refuse duplicate issue while still active.
  Expire at elapsed >= 600,000,000 us; clock read failure or backwards time
  fails closed. At expiry, request a blank frame before rotating. Verify
  exactly six ASCII digits using constant-time comparison. A mismatch keeps
  the current code;
  a match consumes it and requests a blank frame. Clock/readiness/display
  infrastructure failures fail closed and clear internal code bytes using a
  non-elidable wipe routine (do not rely solely on optimizable memset).
- Never persist the code or a verifier/hash; do not add a KDF, HTTP route,
  logging, or a production caller. Transfer requested is not visible-display
  acknowledgement. A successful protected submission is the readback proof.
- The caller keeps the OLED alive longer than the verifier. The first-run
  sequence invokes issue only after a DHCP-assigned client IP and a still-READY
  `Esp32WifiAp`; AP credential disclosure remains owned by
  `ApCredentialSetupCoordinator` and happens before AP startup.
- Tests must cover RF-not-ready/random failure, leading zeroes, valid issue,
  duplicate issue while active, exact expiry boundary, mismatch retention,
  one-time success, display/clock failures and secure clearing. Add tests only;
  do not run them or any compiler/build/PlatformIO/flash/hardware command.
- New source/test files need no before-image. Report checksums and handoff to
  `comms/inbox/SHRU/ATT-P15-RAM-CODE-VERIFIER-01.md`.

#### ATT-P15-NVS-DOMAIN-REG-01 - PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TEST NOT RUN)

- Authority: E33 in `docs/DECISIONS.md`; read
  `docs/SPECS/api_logic.md`, `lib/hal_esp32/src/esp32_pin_auth_runtime.h/.cpp`,
  `lib/hal_esp32/src/nvs_pin_attempt_store.h/.cpp`,
  `lib/core/src/pin_auth.h` and the current
  `test/test_nvs_config/test_main.cpp` fake NVS namespace implementation.
- Write: `test/test_nvs_config/test_main.cpp` only. This exact assignment
  explicitly grants this shared test file; the common harness and all other
  paths remain read-only.
- Add host regressions for runtime-level E33 behavior, not just the already
  existing store-only namespace test:
  1. On a provisioned READY config, seed independent persisted counters in
     `attsec` and `attsetup`, start two `Esp32PinAuthRuntime` instances bound
     to the same `ConfigBootCoordinator`, and assert each coordinator loads
     only its own count.
  2. With only `attsec` present on a provisioned READY config, the `attsec`
     runtime may start but the `attsetup` runtime must fail closed with a
     missing-counter error, null coordinator, and zero NVS set/commit calls.
  3. Where practical, drive one runtime through a failed authentication and
     prove only its own namespace increments; do not weaken or duplicate the
     existing E12 coordinator tests.
- No production source, migration, namespace format, common fake or build
  configuration changes. Do not initialize a missing `attsetup` counter on a
  READY device. No tests, compiler, build, PlatformIO, flash or hardware runs.
- Preserve the existing test file before editing at
  `comms/staging/SHRU/ATT-P15-NVS-DOMAIN-REG-01/before/test_main.cpp`; save
  its unified diff at
  `comms/staging/SHRU/ATT-P15-NVS-DOMAIN-REG-01/patch.diff`.
- Report to `comms/inbox/SHRU/ATT-P15-NVS-DOMAIN-REG-01.md`.

#### ATT-P15-AP-START-EVENT-REG-01 - PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TEST NOT RUN)

- Authority: E28/E33; read lib/hal_esp32/src/esp32_wifi_ap.h/.cpp,
  lib/hal_esp32/src/esp32_secure_random.h/.cpp,
  test/test_esp32_wifi_ap/test_main.cpp and its existing stubs.
- Write: test/test_esp32_wifi_ap/test_main.cpp only.
- Add a fake-event regression proving an unrelated Wi-Fi event, wrong event
  base, or null callback context cannot satisfy the AP_START wait or open the
  Esp32SecureRandom RF gate. Suppress real AP_START, inject wrong event(s)
  while esp_wifi_start() executes, and assert startup times out/fails closed,
  random.rfStarted() remains false, no random bytes are filled, and cleanup
  follows the existing timeout path.
- Local fake changes are permitted only inside this exact test file. Do not
  change production Wi-Fi code, shared stubs, build files or other tests.
- Preserve before-image at
  comms/staging/SHRU/ATT-P15-AP-START-EVENT-REG-01/before/test_main.cpp;
  save diff at comms/staging/SHRU/ATT-P15-AP-START-EVENT-REG-01/patch.diff.
- Tests, compilers, builds, PlatformIO, flash and hardware: NOT RUN.
- Report to comms/inbox/SHRU/ATT-P15-AP-START-EVENT-REG-01.md.

#### ATT-P15-FIRST-RUN-SEQUENCE-REG-01 - PISHDA REVIEWED / ACCEPTED (SOURCE ONLY; TESTS NOT RUN)

- Authority: E28/E33; PISHDA-owned implementation:
  lib/core/src/setup_code_sequence.h/.cpp and lib/core/src/ap_credential_setup.h.
- Read: docs/SPECS/ap_credential_setup.md,
  lib/core/src/setup_code_sequence.h/.cpp,
  lib/core/src/ap_credential_setup.h/.cpp,
  lib/core/src/ap_credential_display.h,
  lib/core/src/credential_soft_ap.h and
  test/test_ap_credential_setup/test_main.cpp.
- Write: test/test_ap_credential_setup/test_main.cpp only.
- Add regressions for FirstRunSetupCodeSequence:
  1. Assert AP credential display and secured AP start happen before code issue.
  2. No client lease means the passphrase remains visible, issuer is not called,
     and sequence/provisioning remain blocked.
  3. First observed client IP (with AP still READY) permits exactly one issuer
     call; repeated issue attempts cannot issue another code.
  4. AP setup/readiness failure or issuer failure keeps provisioning blocked.
  5. A repeated start() while waiting must not destroy the waiting state; after
     DHCP assignment a later issueAfterClientAssigned() must still succeed once.
  6. Existing DHCP plus request-auth gate still controls provisioning; sequence
     start/issue and blank-frame retry must not bypass authorization.
- Implement only local fakes in this test file. No production source, interface,
  spec, stub or build-configuration changes.
- Preserve before-image at
  comms/staging/SHRU/ATT-P15-FIRST-RUN-SEQUENCE-REG-01/before/test_main.cpp;
  save diff at comms/staging/SHRU/ATT-P15-FIRST-RUN-SEQUENCE-REG-01/patch.diff.
- Tests, compilers, builds, PlatformIO, flash and hardware: NOT RUN.
- Report to comms/inbox/SHRU/ATT-P15-FIRST-RUN-SEQUENCE-REG-01.md.

### Agent response/handoff rule

Assignments do not launch or message other AI products automatically. Start
or resume each agent with its codename from AI_START.md. Each agent writes
its completed report directly to its own `comms/inbox/<CODENAME>/` folder,
using the task ID as the filename. If it cannot access the canonical shared
tree, it must label the workspace ISOLATED_COPY/UNKNOWN and use its normal
chat channel; it must not claim the inbox was updated. PISHDA checks all three
inboxes in one batched read and records accepted findings here. Agents may
work independently; no one needs to wait for the others. Assignments remain
pending until reports are actually present and reviewed; availability alone
is not completion.

### PISHDA — ATT-C3-APP-OWNER-DESIGN-01 — DOCUMENTED / SOURCE-REVIEWED (PROPOSAL ONLY)

- Read the mandatory bootstrap and traced `target/esp32_attendance/main.cpp`,
  `ARCHITECTURE.md`, the request/session/setup/clock boundaries, storage graph,
  AS608 startup/attendance owner and config-apply ordering.
- Updated `ARCHITECTURE.md` §4.1–4.6 and its root index reference. Recommended
  one synchronous Arduino loop as the sole mutable application owner; mapped
  the exact long-lived storage/auth/attendance bindings, request families,
  append/observer failure rules and cooperative service steps.
- Explicitly identified absent production pieces: PIN KDF iteration policy,
  READY `attsetup` migration, periodic setup-code blanking, scan-window scheduler,
  enrollment façade, serialized closure tick source, full observer graph and
  production route registration. The current read-only boot shell is unchanged.
- No source behavior, target, route or product policy was implemented. Tests,
  compilers, builds, PlatformIO, flash and hardware: NOT RUN.

### PISHDA — ATT-U61-DUPLICATE-LIFECYCLE-01 — IMPLEMENTED / SOURCE READ-BACK VERIFIED

- Authority: U61 and E27 in `docs/DECISIONS.md`; `docs/SPECS/roster_store.md`.
- Read and traced `RosterStore` replay/planning, `RosterTransitionAudit`,
  `MonthRosterProjection`, `LittleFsFileSet` roster audit, attendance readers,
  pure API event builders and the existing roster tests. No production roster
  mutation route/caller is present in the read-only boot shell.
- Changed exactly `lib/core/src/roster_store.cpp`,
  `test/test_roster/test_main.cpp` and `docs/SPECS/roster_store.md`. Replay now
  marks duplicate DEACTIVATE-on-inactive and ACTIVATE-on-active records bad,
  making `historyComplete()` false. Three regressions cover both bad cases and
  preservation of valid same-day opposite transitions.
- E27 reporting-only rule is preserved: same-local-day opposite lifecycle
  events are still accepted operationally; authoritative monthly projection
  remains responsible for rejecting ambiguous day-granular history. Backdated
  rename semantics were not changed.
- Before/after copies and unified patch are in
  `comms/staging/PISHDA/ATT-U61-DUPLICATE-LIFECYCLE-01/`. Read-back matched the
  staged outputs and saved diff. Tests/compiler/build/PIO/flash/hardware: NOT RUN.
- U61 remains partially open until a production roster writer/consumer owner
  exists and its integrated paths can be audited.


### SHRU — ATT-U61-BACKDATE-AUDIT-01 — SUPERSEDED BY POLICY / IMPLEMENTATION TASK

The audit question was inspected by PISHDA and the policy is now selected under
E34. Do not continue the read-only task or submit a policy recommendation. Its
source findings are retained in the PISHDA task context. The replacement task
below owns only the two regression files; it does not overlap PISHDA source or
specification paths.

### PISHDA — ATT-U61-BACKDATED-ROSTER-01 — IMPLEMENTED / SOURCE READ-BACK VERIFIED; SHRU REGRESSION REVIEW ACCEPTED

- Baseline: v0.2.95 canonical tree at `/root/prototype/att`.
- Authority: delegated user judgment recorded as E34; preserve E27 exactly.
- Read: `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, `docs/SPECS/roster_store.md`,
  `docs/SPECS/enrolled_roster.md`, `lib/core/src/roster_store.h/.cpp`,
  `lib/core/src/roster_snapshot.h/.cpp`, `lib/core/src/log_store.h/.cpp`,
  `lib/core/src/api_logic.h/.cpp`, `test/test_roster/test_main.cpp` and
  `test/test_api/test_main.cpp`.
- Write exactly: `lib/core/src/roster_store.h`,
  `lib/core/src/roster_store.cpp`, `lib/core/src/api_logic.h`,
  `lib/core/src/api_logic.cpp`, `docs/DECISIONS.md` and
  `docs/SPECS/roster_store.md`.
- Rule: for an existing learner, a changed SET/rename, DEACTIVATE or ACTIVATE
  whose effective PHT-local day is strictly earlier than that learner's last
  roster event is rejected by the planner before the caller can append it and
  rejected during replay. Track this per learner. Invalid pre-epoch day is a
  separate planning error. Preserve RE_NOOP for operations that emit no event.
  Same-day SET/rename and same-day opposite lifecycle events remain operationally
  accepted; monthly projection retains E27's stricter ambiguity rejection.
- Excluded: no LogStore format/observer redesign, no clock rewrite, no historical
  correction endpoint, no production route/composition root, no same-day policy
  change and no changes to any file assigned to SHRU.
- Stage before-images and patch under
  `comms/staging/PISHDA/ATT-U61-BACKDATED-ROSTER-01/`. Re-read every edited
  source/spec/authority file and verify exact diffs before accepting.
- Tests, compilers, builds, PlatformIO, flashing and hardware: NOT RUN.

### SHRU — ATT-U61-BACKDATED-ROSTER-01 — PISHDA REVIEWED / ACCEPTED; OWNERSHIP RELEASED

- Baseline: v0.2.95 canonical shared tree at `/root/prototype/att`.
- Read exactly: `HANDOFF_FINAL.md`, `docs/DECISIONS.md`,
  `docs/SPECS/roster_store.md`, `lib/core/src/roster_store.h/.cpp`,
  `lib/core/src/api_logic.h/.cpp`, `test/test_roster/test_main.cpp` and
  `test/test_api/test_main.cpp`.
- Write exactly `test/test_roster/test_main.cpp` and
  `test/test_api/test_main.cpp`. No source, spec, authority, coordination,
  working-memory, build-config or other test files.
- Add regressions for: earlier-day rename rejection; earlier-day deactivation
  and activation rejection; per-learner isolation; pre-epoch planning rejection;
  same-day rename and same-day opposite lifecycle acceptance; raw replay of
  backdated roster records becoming untrusted; API mapping to `A_CONFLICT` /
  `R_BACKDATED`; invalid-day mapping to `A_INVALID`; and preservation of no-op
  behavior. Assert zero output events on rejected plans. Keep E27 reporting-only.
- Use the updated result names `RE_BAD_DAY`, `RE_BACKDATED` and `R_BACKDATED`.
  Preserve each existing test file before editing under
  `comms/staging/SHRU/ATT-U61-BACKDATED-ROSTER-01/before/` and write the unified
  diff to that task's `patch.diff`. Report to
  `comms/inbox/SHRU/ATT-U61-BACKDATED-ROSTER-01.md`.
- PISHDA independently inspected the two diffs, raw-replay fixtures, date
  construction, API mappings, pre-images and checksums. The test-only additions
  are ACCEPTED at source level. Tests/builds/PIO/flash/hardware remain NOT RUN.


### SHRU — ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01 — PISHDA REVIEWED / ACCEPTED; OWNERSHIP RELEASED

- Baseline: v0.2.95 canonical shared tree at `/root/prototype/att`.
- Authority: signed E33; this implements its 600-second monotonic expiry without
  changing the lifetime, code format, single-use rule, RNG lifecycle or any route.
- Read exactly: `HANDOFF_FINAL.md`, `docs/DECISIONS.md`,
  `docs/SPECS/api_logic.md`, `docs/SPECS/esp32_oled.md`,
  `lib/hal_esp32/src/clock_confirmation.h`,
  `lib/hal_esp32/src/esp32_ram_setup_code_verifier.h/.cpp`,
  `lib/hal_esp32/src/esp32_oled.h/.cpp`,
  `lib/core/src/setup_code_sequence.h/.cpp` and
  `test/test_esp32_ram_setup_code_verifier/test_main.cpp`.
- Write exactly:
  `lib/hal_esp32/src/esp32_ram_setup_code_verifier.h`,
  `lib/hal_esp32/src/esp32_ram_setup_code_verifier.cpp` and
  `test/test_esp32_ram_setup_code_verifier/test_main.cpp`.
  This task is disjoint from `ATT-U61-BACKDATED-ROSTER-01`'s two test paths.
- Add a bounded public `serviceExpiry()` (or equivalently narrow named method)
  callable by the future serialized application loop. Required contract:
  1. Faulted verifier returns failure; an inactive verifier is an idempotent
     healthy no-op; an active code before 600,000,000 us remains active.
  2. At elapsed >= 600,000,000 us, invalidate and securely wipe the code before
     requesting a blank OLED frame. Never rotate or issue a replacement code
     from this service method.
  3. Clock read failure, monotonic regression, lost RF readiness while a code
     is active, or blank-frame transfer failure latches fail-closed state,
     clears internal code bytes, and prevents future verification/issuance.
  4. Repeated service calls after expiry/consumption are harmless. Preserve
     `issue()` and `verify()`'s independent expiry enforcement so a caller that
     omits the periodic service cannot authenticate an expired code.
  5. Distinguish transfer requested from physical display acknowledgement.
     The service is synchronous, bounded, and intended for the same one-loop
     serialized owner; do not add timers, FreeRTOS tasks, mutexes or callbacks.
- Add regressions for inactive/no-op, before-boundary retention, exact 600-second
  expiry, beyond-boundary expiry, exactly-once blank request, clock read failure,
  backward monotonic time, RF-gate loss, blank-transfer failure, consumed-code
  idempotence and repeated post-expiry service. Assert code cannot verify after
  expiry/fault and no random bytes are requested by the service.
- No production caller, route, `target/esp32_attendance/main.cpp`, spec,
  coordination/working-memory or build-config changes. The separate C3 design
  proposal already identifies the required future loop call; do not wire it into
  the read-only boot shell.
- Preserve before-images under
  `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/before/`; save the
  unified diff as `comms/staging/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01/patch.diff`;
  report to `comms/inbox/SHRU/ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01.md`.
- No tests, compilers, builds, PlatformIO, flashing or hardware. Tests are
  source additions only and must be reported NOT RUN. Include exact registrations,
  before/after SHA-256 values and canonical-tree/isolated-workspace status.
  PISHDA independently reviewed the service implementation, failure paths,
  tests, pre-images, exact diff scope and checksums. Source is ACCEPTED; tests,
  builds, PIO, flash and hardware remain NOT RUN.


### Revoked SHRU implementation assignments — 2026-11-13

The owner revoked ATT-P15-MONOTONIC-CROSS-OP-REG-01 and
ATT-U61-ROSTER-WRITER-01. Their former source/test write scopes are closed;
SHRU must make no further changes to those paths. PISHDA will inspect the
canonical files before deciding whether or how to continue the work. The
uncompleted task descriptions are retained in the recent handoff history as
provenance, not as active instructions.

### PISHDA — ATT-C3-READONLY-REPLAY-AUDITS-01 — IMPLEMENTED / SOURCE REVIEWED

- Baseline: current canonical v0.2.95 development tree. Write scope: `target/esp32_attendance/main.cpp`, `tools/check_att_boot_shell.py`, `ARCHITECTURE.md`, `WORKING_MEMORY.md`, `docs/CHANGELOG.md` and this board.
- Added READY-only startup requirements for `EventGroupAudit`, live `RosterStore` replay and the exact-config `LogAttendanceAdmission` observer attachment. Any missing observer, malformed event group or incomplete roster now blocks `storageReady` and AP/HTTP startup. All components remain read-only in this shell.
- Explicitly kept E27 `RosterTransitionAudit` out of operational boot readiness: E27 applies to monthly-reporting projection, not same-day live lifecycle changes.
- Updated the local boot-shell source checker. PISHDA independently reviewed the source diff and readback. No compiler/tests/PlatformIO/build/flash/hardware run; source checker NOT RUN.
- Staged pre-images, after-images and unified patch at `comms/staging/PISHDA/ATT-C3-READONLY-REPLAY-AUDITS-01/`.


### ATT-NO-TEST-ASSURANCE-REVIEW-01 — RETIRED AS A STANDING REQUEST

This was previously recorded as a possible optional critique. It is not an
active SHRU assignment; no report is expected or required. The user may
independently prompt SHRU to review the no-test assurance protocol or any
other topic. Any SHRU report may be written only inside comms/inbox/SHRU/.
PISHDA consults such a report only when the user explicitly asks.


## 7. Current operating state

PISHDA is the sole implementation owner and sole writer to the project tree.
No parallel source/test work is active. All prior SHRU implementation locks
are revoked; completed SHRU changes already accepted remain part of the
canonical baseline.

SHRU is 100% optional and user-invoked, outside PISHDA's workflow. Its only
write destination is comms/inbox/SHRU/. The user may prompt it independently
for any read-only critique. PISHDA does not assign, invoke, poll or routinely
check the inbox; it reads a report only on the user's explicit request. No
SHRU report is required, awaited or a condition of progress. QYUPAL and SEI
remain retired.

The target remains a read-only status shell; C3/C6, U61, storage durability,
attendance and production workflow remain open. P15 expiry, separate attsetup
runtime, READY-only PIN/session boundaries and the session-free NEEDS_SETUP
boundary are source-composed. The first-run setup sequence issues/reissues its
RAM code only after DHCP and expiry service; the exact AP-start credential is
retained and checked against NVS before validation/commit. No HTTP route
consumes the setup boundary. The strict 768-byte JSON decoder and fixed-buffer
raw-body setup handler candidate remain source-only, with 17 NOT RUN
regressions. A hash-pinned PlatformIO pre-build patch is authored to require
explicit raw handlers for all POST/PUT/PATCH/DELETE requests, reject duplicate
Content-Length and any Transfer-Encoding, and cap raw bodies at 768 bytes /
5,000 ms. It restores the installed framework source on normal process exit
and retains a checked backup for recovery after abnormal termination.
The framework package is shared: concurrent PlatformIO builds are prohibited
while active; recover a retained backup before unrelated builds after forced
termination. The script has NOT RUN; the package is unmodified, main.cpp remains GET-only and
the setup handler is unregistered. The reboot gate remains unreachable until
a route invokes the setup boundary; fresh boot alone constructs READY owners.
E35 initial PIN derivation remains RAM-only until ConfigBootCoordinator's
first durable provision.
Tests, builds, PIO, flashing and hardware remain NOT RUN under the current
user restriction. The manifest remains stale.

A source-only ClosureEngine increment now caps each successful step at
one local day, avoids repeated work before the next enabled session ends and
uses LogStore observer replay to reconcile a committed CLOSE after uncertain
append. Three test registrations are authored NOT RUN. C17 and the production
closure scheduler/tick owner remain open; no route or mutable target graph was
enabled.

The CLOSE index now stores its existing fixed-capacity references sorted by
(day, session); insertion-point search and closed() lookup are binary searches.
Chronological inserts append without shifting, while out-of-order inserts shift
at most the existing MAX_CLOSES=800 references. No index RAM was added. One
out-of-order/duplicate regression was added and is NOT RUN. No target scheduler
or route is enabled.

The LogStore boot validation and replay passes now read at most 16 records per
fixed 256-byte batch. MonthSet may split a batch across month files; record
validation and observer callbacks remain in original sequence order. The
reverse trailing-group check remains record-at-a-time. One cross-month read
count regression is authored NOT RUN; no persistent format or target route
changed.

A null event-group pointer is rejected by LogStore::append() before
dereference or storage access. Invalid arguments remain non-faulting; only
an attempted append with an indeterminate storage result latches the log.
One no-mutation regression is authored NOT RUN.

An indexed-read integrity increment now makes LogStore::at() verify the
firstSeq + index sequence after decoding. Read, decode or sequence failure
faults and invalidates the in-memory log, clears its count/baseline and resets
attached projections; the caller Event remains unchanged. Out-of-range indexes
stay non-faulting. Two regressions are authored NOT RUN; no persistent format
or production route changed.

### Recent handoff history

| Date | Task | Result | Notes |
| 2026-11-13 | ATT-LOG-AT-READ-INTEGRITY-01 — sequence-checked indexed reads | PISHDA authored; source-only, staged | LogStore::at() now validates sequence before publishing; read/decode/sequence failure invalidates log/projections and blocks append. Two regressions NOT RUN. |
| 2026-11-13 | ATT-LOG-NULL-INPUT-GUARD-01 — reject null LogStore append input | PISHDA authored; source-only, staged | append(nullptr, n>0) is rejected before storage access; invalid-argument failure remains non-faulting. One no-mutation regression NOT RUN. |
| 2026-11-13 | ATT-LOGSTORE-BOUNDED-READ-01 — fixed-batch boot replay reads | PISHDA authored; source-only, staged | Validation and observer replay read up to 16 records per bounded 256-byte buffer; month-boundary splitting is preserved; tail group check remains single-record. One regression NOT RUN; no format or target route change. |
| 2026-11-13 | ATT-LOG-INDEX-BINARY-LOOKUP-01 — sorted CLOSE index / binary lookup | PISHDA authored; source-only, staged | Existing 800-entry CLOSE array stays fixed-RAM and sorted by (day, session); both insertion-point discovery and closed() use lower-bound binary search; chronological appends avoid shifts, while out-of-order shifts remain bounded by 800; duplicate first-CLOSE-wins preserved. One out-of-order/duplicate regression NOT RUN. No production caller or target behavior changed. |
| 2026-11-13 | ATT-CLOSURE-BOUNDED-STEP-01 — one-day resumable closure scan | PISHDA authored; source-only, staged | ClosureEngine::step() processes at most one local day per successful call, retains a high-water cursor, and waits for the next session end; three regressions NOT RUN. C17 schedule history and production scheduler remain unresolved; target remains read-only. |
| 2026-11-13 | AI-20261113-003 — sole-writer PISHDA / inbox-only SHRU critic model | PISHDA implemented; source-write scopes revoked | QYUPAL/SEI retired; SHRU is user-invoked only and may write critique reports solely to its inbox; PISHDA has no inbox-check obligation. No tests/builds/PIO/flash/hardware run. |
|---|---|---|---|
| 2026-11-13 | ATT-P15-WEBSERVER-RAW-BOUNDARY-01 — hash-pinned WebServer raw-only body guard | PISHDA authored; source-only, not applied | Adds 768-byte / 5,000 ms bounded raw parsing, rejects non-raw body methods and ambiguous framing; target remains GET-only. Pre-build script, checker and docs authored; all execution NOT RUN. |
| 2026-11-13 | ATT-P15-FIRST-RUN-REQUEST-PARSER-01 — strict JSON decoder and bounded HTTP handler candidate | PISHDA implemented; source read-back verified and staged | Added 768-byte strict Config decoder, fixed raw-body adapter candidate and 17 regression registrations. Not composed: pinned WebServer multipart pre-parser ignores total Content-Length; route remains disabled until bounded parser/replacement. Source inventory 954/47; tests/checker/compiler/build/PIO/flash/hardware NOT RUN. |
| 2026-11-13 | ATT-P15-READY-REBOOT-01 — post-provision fresh-boot handoff | PISHDA implemented; source read-back verified and staged | Added one-shot restart gate requiring committed first provision and ACSS_PROVISIONED OLED blank-frame state; retries pending blanking at a throttled cadence and restarts only after synchronous HTTP dispatch returns. READY owners are reconstructed on fresh boot, not hot-switched. One regression authored NOT RUN; source inventory 937/46. No setup route invokes the gate; tests/checker/compiler/build/PIO/flash/hardware NOT RUN. |
| 2026-11-13 | ATT-P15-INITIAL-PIN-CREDENTIAL-01 — E35 first-run PIN credential preparation | PISHDA implemented; source read-back verified and staged | Added PIN-exempt non-PIN Config preflight, RF-gated 8-byte salt plus shared PBKDF2-HMAC-SHA256 derivation at fixed 2,048 rounds, and first-run boundary sequencing after setup-code match/reset/H_UNPROVISIONED. Preparer changes only temporary Config PIN fields and never writes storage; ConfigBootCoordinator remains sole first writer. Eleven regressions authored NOT RUN; current native source inventory is 936 across 46 suites (pre-increment reconstruction: 925; prior ledger undercounted by one); tests/checker/compiler/build/PIO/flash/hardware NOT RUN. No HTTP setup route or post-provision READY transition. |
| 2026-11-13 | ATT-P15-FIRST-RUN-CODE-BOUNDARY-01 — session-free EP_SETUP boundary and DHCP-gated issuance | PISHDA implemented; source read-back verified and staged | Added `FirstRunSetupCodeBoundary`, exact `attsetup`/RAM verifier/sequence bindings, durable attempt/reset ordering, H_UNPROVISIONED gate, normalized-config preflight and constant-time AP-start credential identity checks. Target advances issue/reissue after DHCP and expiry. No HTTP setup submission route, initial E35 PIN credential preparer or post-provision READY transition. Thirteen regressions authored NOT RUN; no tests/checker/compiler/build/PIO/flash/hardware. |
| 2026-11-13 | ATT-C6-READY-AUTH-OWNER-01 — compose READY-only PIN/session and P15 boundary owners | PISHDA implemented; source read-back verified | E35 PBKDF2 config runtime, AdminSessionBoundary and READY-only AdminSetupCodeBoundary now bind exact long-lived owners; no HTTP routes, code issue/verify, tests/builds/PIO/flash/hardware. NEEDS_SETUP still lacks a session-free code-only provisioning boundary. |
| 2026-11-13 | ATT-U61-PLANNER-WRITER-01 — planner-before-append roster mutation boundary | PISHDA implemented and source-read-back reviewed; NOT RUN | New core writer binds exact observer/open generation, plans before append, checks log/roster postconditions, latches uncertain writes; six test registrations authored but not executed. U61 remains open until production routes use it. |
| 2026-11-13 | ATT-U61-BACKDATED-ROSTER-01 — reject strictly earlier-day roster events consistently | PISHDA source-reviewed and ACCEPTED SHRU's test-only additions; ownership released | Three roster and one API regression additions; exact diffs/hashes checked. Tests NOT RUN. E34/E27 preserved. |
| 2026-11-13 | ATT-C3-APP-OWNER-DESIGN-01 — serialized application owner and route boundaries | PISHDA documented source-grounded proposal | Recommends one cooperative Arduino loop; identifies auth/storage/AS608 graph and remaining C3/C6/U61 gates. No implementation or tests. |
| 2026-11-13 | ATT-U61-BACKDATE-AUDIT-01 — backdated roster admission/replay consistency | Superseded by E34 and implementation task | Source gap inspected by PISHDA; no separate SHRU report required. |
| 2026-11-13 | ATT-P15-RAM-CODE-VERIFIER-01 — RAM-only single-use setup-code verifier | SHRU authored; PISHDA source-reviewed and accepted | New HAL verifier/issuer and 16 regressions; monotonic checks on issue/verify, no periodic expiry service or production caller; tests NOT RUN. |
| 2026-11-13 | ATT-P15-NVS-DOMAIN-REG-01 — runtime-level PIN/setup counter isolation | SHRU authored; PISHDA source-reviewed and accepted | Two runtime-bound regressions prove distinct `attsec`/`attsetup` counts and fail-closed missing `attsetup` on READY; tests NOT RUN. |
| 2026-11-13 | ATT-P15-AP-START-EVENT-REG-01 — AP_START-only RF entropy gate | SHRU authored; PISHDA source-reviewed and accepted | Test-only wrong-event/mismatched-base/null-context regression verifies timeout cleanup and no RF-gate opening; NOT RUN. |
| 2026-11-13 | ATT-U61-DUPLICATE-LIFECYCLE-01 — fail-closed roster replay for redundant lifecycle events | PISHDA implemented; staged diff/read-back verified | Duplicate DEACTIVATE/ACTIVATE records now invalidate roster history; valid same-day opposite transitions remain accepted under E27. Three regressions authored; NOT RUN. U61 remains partially open pending production writer/consumer integration. |
| 2026-11-13 | ATT-P15-FIRST-RUN-SEQUENCE-REG-01 — credential then setup-code display order | SHRU authored; PISHDA source-reviewed and accepted | Eight test-only registrations cover AP credential-before-AP-before-code ordering, DHCP gate, exactly-once issuance, readiness/issuer faults, retry and per-request auth; tests NOT RUN. |
| 2026-11-13 | ATT-P15-AUTH-BOUNDARY-REG-01 — setup-code boundary regressions | SHRU authored; PISHDA source-reviewed and accepted | Three test-only registrations cover malformed input, verifier error and counter-reset persistence failure; NOT RUN. |
| 2026-11-13 | ATT-E27-PROJECTION-REG-01 — same-day lifecycle projection regression | SHRU authored; PISHDA source-reviewed and accepted | Test-only E27 ACTIVATE-then-DEACTIVATE case; NOT RUN. |
| 2026-11-13 | ATT-P15-OLED-TST-PUNCT-01 — punctuation rejection regression | SHRU authored; PISHDA source-reviewed and accepted | Test-only OLED punctuation rejection and blank-frame assertion; NOT RUN. |
| 2026-11-13 | ATT-STATUS-ALLOWLIST-01 — public status payload guard | PISHDA authored; source read-back verified | Added exact JSON key allowlists for both unauthenticated boot-state status payloads in the boot-shell checker. Checker NOT RUN; no builds/tests/PIO/flash/hardware. |
| 2026-11-13 | ATT-DOC-BOOT-README-01 — boot-shell README/source alignment | PISHDA completed; read-back verified | Corrected stale LittleFS scope and documented READY-only no-format mount, read-only validation/replay, RTC diagnostic and persistent PIN-attempt startup from current source. No firmware behavior changed; no tests/builds/PIO/flash/hardware run. |
| 2026-11-13 | AI-20261113-002 — SHRU parallel coder role | PISHDA implemented; read-back verified | Role changed to bounded parallel implementation engineer; exact-path ownership, per-file exclusivity and diff/pre-image handoff added. Current SHRU OLED audit remains read-only. No tests/builds/PIO/flash/hardware run. |
| 2026-11-13 | AI-20261113-001 — multi-agent context workflow | PISHDA implemented; ownership released; pending user review | Documentation-only. Eleven paths changed; 44 test directories/850 registrations mapped. No tests/builds/PIO/flash/hardware run. Manifest was already failing before this update. |
| 2026-11-13 | ATT-P15-OLED-01 — standalone setup-code display | PISHDA implemented; ownership released; reviews pending | Seven source regressions authored NOT RUN; native inventory 857/44. No production caller, entropy lifecycle, verifier/storage or visible-display acknowledgement. No tests/builds/PIO/flash/hardware run. |

| 2026-11-13 | ATT-AS608-READONLY-WEB-DIAG-01 — isolated read-only AS608 web diagnostic | PISHDA authored; staged and read-back verified | New sketch/README under `sketch/as608_readonly_web/`; AP `att`, downloadable RAM log, sensor parameters/index map/search only; no template mutation commands. UART pre-TX stale-byte race corrected during review. Not compiled, flashed or hardware-tested. |
| 2026-11-13 | ATT-P15-MONOTONIC-CROSS-OP-REG-01 — cross-operation monotonic regression test | SHRU assignment REVOKED by owner | One test file only; ensure a backwards clock observation between `verify()` calls faults even when time remains after the original issue timestamp. Tests NOT RUN. |
| 2026-11-13 | ATT-U61-ROSTER-WRITER-01 — planner-before-append roster write boundary | SHRU assignment REVOKED by owner | New isolated core writer + dedicated tests; preserve E27/E34, fail closed on uncertain append and observer mismatch. Tests NOT RUN. |
| 2026-11-13 | ATT-C3-READONLY-REPLAY-AUDITS-01 — validate event groups, roster and admission at READY boot | PISHDA implemented; source reviewed | READY-only read-only target composition; event-group audit, operational roster replay and log-backed admission observer required before AP/HTTP. Source checker updated but NOT RUN. No privileged/write paths enabled. |
| 2026-11-13 | ATT-NO-TEST-ASSURANCE-01 — researched testless assurance workflow | PISHDA documented; no active SHRU assignment | New static-only assurance protocol and agent rules; does not replace tests or authorize production release. SHRU review is user-invoked only. |
