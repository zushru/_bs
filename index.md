# ATT project index and technical dictionary

**Baseline:** v0.2.95 development snapshot (source baseline dated 2026-11-12).  
**Purpose:** navigate the project and identify the right authority, modules, callers, tests and evidence.  
**Authority:** this is an index only. It does not define or approve product behavior.

## 1. Start and operating context

Every agent must read these files in one batch, in this exact order:

1. AGENTS.md — development rules, security constraints, RDC limits and
   multi-agent ownership.
2. WORKING_MEMORY.md — current state, evidence, blockers and the single next
   engineering task.
3. index.md — this project map and task router.
4. AI_COORDINATION.md — codename registry, current writer lock and handoffs.

Use AI_START.md for the one-line codename instruction. The user supplies only
the agent identity and task; the repository provides the durable project
context. No AI account is assumed to share another account's conversation or
filesystem state.

## 2. Authority and evidence map

| Source | Role | Important limitation |
|---|---|---|
| HANDOFF_FINAL.md | Original signed product handoff and baseline requirements | Later approved decisions may change specific clauses; preserve original provenance |
| docs/DECISIONS.md | Signed, delegated, open, measured, defective and superseded decisions | Preserve status; OPEN is not permission to guess |
| AGENTS.md | Universal agent and engineering operating rules | Does not grant product authority |
| ARCHITECTURE.md | Source-oriented subsystem boundaries and integration state; §4.1–4.6 proposes the single-loop application owner, route boundaries, write discipline and remaining gates | Proposal only; verify callers and exact ownership in code |
| docs/SPECS/ | Detailed module contracts | Read the relevant spec and decision entries before changing behavior |
| docs/CRITICAL.md | Active critical blockers and consequences | Not a substitute for inspecting implementation/evidence |
| docs/UNVERIFIED.md | Claims that still need proof | Not a general feature wish list |
| docs/DEVELOPMENT_ROADMAP.md | Engineering sequence and gates | Does not approve changing decision status |
| docs/TESTS.md | Test inventory and evidence ledger | Registration totals are not passing results |
| docs/TESTING_WORKFLOW.md | Current verification commands and evidence classes | Commands must still be authorized for the task |
| docs/NO_TEST_ASSURANCE.md | Required static-only assurance protocol when execution testing is unavailable | Risk reduction only; not a substitute for testing or release evidence |
| RELEASE.md and RELEASE_v0.2.95.md | Release/evidence summaries | Historical passes are bounded to their recorded source and environment |
| docs/CHANGELOG.md | Chronological source/document changes | Not the current specification |
| docs/ONE_DAY_EXECUTION.md | Historical graduation/deadline plan; superseded for current delivery scope | Does not weaken production requirements |
| docs/INDEX.md | Detailed documentation directory map | Secondary index; this file is the root project map |

**Authority rule:** signed handoff and later decision records govern requirements.
A delegated decision permits only its stated scope. Source describes what exists;
tests describe only what they exercise; neither retroactively approves policy.

## 3. Root-level files

| Path | Dictionary entry |
|---|---|
| AGENTS.md | Mandatory agent bootstrap, engineering rules, call discipline and single-writer policy |
| WORKING_MEMORY.md | Live development handoff; current implementation state and one next task |
| index.md | Root project dictionary, navigation and task routing |
| AI_COORDINATION.md | PISHDA sole-writer policy and SHRU inbox-only critique board |
| AI_START.md | Short codename-specific bootstrap instructions for PISHDA and SHRU |
| CHATGPT_START.md | Compatibility entry point that redirects to the universal AI starter |
| README.md | Human-facing project summary and repository entry point |
| HANDOFF_FINAL.md | Protected signed product handoff |
| ARCHITECTURE.md | Architecture and integration boundaries |
| MANIFEST.sha256 | Snapshot integrity manifest; currently stale and must not be treated as verified |
| RELEASE.md | Current release-level status pointer |
| RELEASE_v0.2.95.md | v0.2.95 release record and bounded evidence |
| platformio.ini | Main PlatformIO configuration, including native environment |
| platformio.attendance.ini | Attendance boot-shell target configuration |
| platformio.esp32-probe.ini | Diagnostic hardware-probe target configuration |
| platformio.esp32-smoke.ini | Inert adapter-link compile/link configuration |

Generated .pio contents are build artifacts, not project authority or source.
Do not index binaries or tool-generated dependency trees as meaningful modules.

## 4. Documentation map

### Product and engineering documents

- docs/DECISIONS.md — decision ledger and authority classifications.
- docs/CRITICAL.md — critical blocker register.
- docs/UNVERIFIED.md — unverified claims and evidence gaps.
- docs/DEVELOPMENT_ROADMAP.md — engineering priorities, dependencies and gates.
- docs/ONE_DAY_EXECUTION.md — bounded demo/deadline strategy.
- docs/TESTS.md — suite inventory and test evidence.
- docs/TESTING_WORKFLOW.md — commands, target distinctions and evidence reporting.
- docs/NO_TEST_ASSURANCE.md — static-only assurance workflow, adversarial review checklist, evidence labels and release limits when execution tests cannot be run.
- docs/CHANGELOG.md — chronological changes.
- docs/BRINGUP.md and docs/ADAPTER_DESIGN.md — hardware bring-up observations and adapter design context.
- docs/PACKAGE_AUDIT_v0.2.95.md and docs/AUDIT_v0.2.95.md — baseline/package audit provenance.
- docs/evidence/ — retained command logs; each log applies only to its recorded tree, command and environment.

### Module specifications

Start with docs/SPECS/INDEX.md. It maps 21 module specifications to implementation
starting points and likely owning tests. The main contracts are:

| Domain | Specification entry points | Source starting points |
|---|---|---|
| API, PIN and sessions | api_logic.md | lib/core/src/api_logic.*, pin_auth.*, pin_auth_session.*, admin_session.*; related ESP32 PIN adapters |
| AP credential and setup | ap_credential_bootstrap.md, ap_credential_setup.md, esp32_wifi_ap.md | core AP credential modules; HAL NVS credential store and Wi-Fi AP |
| AS608 transport and attendance | as608_adapter.md, as608_attendance_pipeline.md | lib/as608/src/* and HAL AS608 runtime/owner/transport |
| Enrollment and roster | enroll_fsm.md, enrolled_roster.md, roster_store.md | core enrollment/roster modules and AS608 enrollment sensor |
| Clock and RTC | clock_policy.md, ds3231_clock.md | core clock/time policy and HAL DS3231/I2C/monotonic adapters |
| Configuration | config_store.md, config_boot.md, config_apply.md, config_runtime_guard.md | core config store/boot/provision/apply/guard and HAL NVS/runtime |
| Storage and recovery | month_set.md, backup_format.md, littlefs_recovery_protocol.md | core file-set/log/month/restore and LittleFS adapter |
| Display | esp32_oled.md | lib/hal_esp32/src/esp32_oled.* |
| Historical core fixes | core_fixes.md | event, log, closure and roster modules |
| Diagnostic sketch | diag_sketch.md | sketch/sketch_compact_minimal_fixed.ino |

Specification-to-test links in SPECS/INDEX.md are starting points, not claims
that every requirement has coverage. Inspect exact test bodies and callers.

### SF2 documentation

Start with docs/sf2/INDEX.md, then read the required policy and data-contract
documents. Useful map:

- REQUIREMENTS.md — SF2 workstream requirements and status distinctions.
- D4_DECISIONS.md — reviewer-evidence policy decisions.
- DATA_CONTRACT.md and CORE_SOURCE_MAPPING.md — proposed source/snapshot data
  contract and mapping to core structures.
- TEACHER_QUESTIONS.md — unresolved school-specific rules; never guess them.
- TEMPLATE_AUDIT.md and UPLOAD_TEMPLATE_AUDIT_2026-11-10.md — workbook
  observations, not proof of final approval.
- Versioned SNAPSHOT, REPORTING, ROSTER and REVIEW_EVIDENCE documents — design
  models/proposals; a version number does not confer approval.
- reference/ — supplied reference workbooks with their provenance; do not
  relabel a reference workbook as the approved final template.
- archive/ — superseded SF2 design/audit material.

### Historical material

docs/archive/README.md explains docs/archive/ subdivisions: audits, context,
patches, releases and tooling. Historical files are provenance only; do not
load them as first-read context or replay archived patch scripts on this tree.
tools/legacy/ contains version-specific historical verification scripts.
docs/evidence/ contains logs, not source code or general proof beyond each log.

## 5. Firmware source dictionary

### Hardware-independent core — lib/core/src/

| Responsibility | Main files |
|---|---|
| Event encoding and group validation | event_codec.*, event_group_audit.* |
| Append log, index and month/file coordination | log_store.*, log_index.*, month_set.*, fileset.h, istorage.h |
| Roster state, enrollment identity, history and mutation boundary | roster_store.*, roster_event_writer.*, enrolled_roster.*, roster_name_history.*, roster_snapshot.*, month_roster_projection.* |
| Attendance state, closure and health | status_derive.*, day_view.*, closure_engine.*, health_fsm.* |
| Time and clock policy | time_rules.*, clock_policy.* |
| Configuration and provisioning | config_store.*, config_boot.*, config_provision.*, config_apply.*, config_runtime_guard.* |
| PIN/API/session behavior | pin_auth.*, pin_auth_session.*, api_logic.*, admin_session.* |
| Backup and restore validation | backup_format.*, restore_replay.* |
| AP credential/setup contracts | ap_credential_bootstrap.*, ap_credential_setup.*, ap_credential_display.h, credential_soft_ap.h |

Core logic is hardware-independent by design. A source module does not imply a
production composition root or a live HTTP route.

### AS608 subsystem — lib/as608/src/

- as608_attendance_client.* — reusable scan/client behavior.
- as608_attendance_pipeline.* — scan outcome to attendance/admission pipeline.
- as608_enroll_sensor.* — enrollment operations against the sensor.
- as608_sensor_arbiter.* — ownership/arbitration for shared sensor operations.
- attendance_config_runtime.* — configuration-to-attendance runtime boundary.
- log_attendance_admission.cpp — log-backed attendance admission boundary.

Check AS608 adapter and pipeline specs plus all direct callers before changing
transport, mapping, cancellation or enrollment/attendance concurrency.

### ESP32 hardware abstraction — lib/hal_esp32/src/

| Area | Important files |
|---|---|
| AS608 runtime/ownership/startup | stream_as608_link.*, as608_runtime.*, as608_startup.*, as608_attendance_owner.*, as608_owner_config_runtime.*, application_attendance_gate.* |
| Authentication/PIN | esp32_pin_auth_runtime.*, esp32_mbedtls_pin_verifier.*, esp32_mbedtls_pin_writer.*, esp32_mbedtls_pin_config_runtime.*, nvs_pin_attempt_store.* |
| Random sources | esp32_secure_random.*, esp32_bootstrap_random.*; do not assume their lifecycle/use cases are interchangeable |
| AP and NVS | esp32_wifi_ap.*, nvs_ap_credential_store.*, nvs_config_backend.*, esp32_nvs_startup_guard.* |
| OLED | esp32_oled.* |
| RTC/time | ds3231_clock.*, esp32_ds3231_register_io.*, esp32_clock_monotonic.* |
| I2C ownership | esp32_i2c_bus_owner.*, i2c_bus_lock.* |
| LittleFS mount | esp32_littlefs_mount.* |
| Clock confirmation | clock_confirmation.*, admin_clock_confirmation.* |

HAL adapters are not proof of correct production startup ordering, complete
runtime composition or physical device behavior.

### LittleFS adapter — lib/littlefs_file_set/src/

- littlefs_file_set.* — file-set adapter.
- littlefs_mount_owner.* — no-format mount/storage ownership boundary.
- littlefs_serialized_fileset.* — serialized access wrapper.
- littlefs_file_set_test.h — test support surface; not production behavior.

Host/POSIX evidence does not prove mounted ESP-IDF LittleFS durability, metadata
durability or power-cut recovery. See C13/C13.3 in docs/CRITICAL.md.

## 6. Firmware targets and legacy sketch

| Path | Purpose | Restriction |
|---|---|---|
| target/esp32_attendance/ | Read-only production-oriented boot shell and diagnostics | Not a complete attendance runtime; privileged operations remain disabled |
| target/esp32_hardware_probe/ | Diagnostic-only hardware/VFS probe | Not attendance firmware; inspect wiring/source and obtain explicit authorization before hardware action |
| target/esp32_adapter_link/ | Inert compile/link smoke application | NEVER FLASH |
| sketch/ | Legacy diagnostic sketch | Known credential/duplicate-template cautions; do not assume sensor is clean |

Never infer end-to-end attendance functionality from a target name, compile,
link or diagnostic endpoint.

## 7. Native C++ test map

Current source inventory: 962 RUN_TEST registrations across 47 direct suites.
This is a registration count, not a test pass; authored but unrun tests remain NOT RUN. Test source and exact fixture
scope must be inspected before relying on a suite.

| Concern | Suite directories |
|---|---|
| API, sessions, PIN and hardening | test_admin_session, test_api, test_pin_auth_session, test_esp32_mbedtls_pin_verifier, test_hardening |
| AP credentials, provisioning and Wi-Fi | test_ap_credential, test_ap_credential_setup, test_nvs_ap_credential, test_esp32_bootstrap_random, test_esp32_wifi_ap |
| AS608, enrollment and admission | test_as608, test_as608_gate, test_as608_pipeline, test_enroll, test_attendance_admission |
| Core event, audit, closure and health | test_core, test_coreb, test_coreb2, test_event_group_audit |
| Configuration and NVS | test_config, test_config_apply, test_config_boot, test_config_guard, test_config_provision, test_nvs_config, test_nvs_startup_guard_policy |
| Clock, RTC and confirmation | test_clock_confirmation, test_ds3231, test_esp32_clock_monotonic |
| Storage, backup and restore | test_backup, test_littlefs_fileset, test_littlefs_mount_owner, test_littlefs_serialized_fileset, test_littlefs_vfs_probe, test_monthset, test_restore_replay |
| Roster and month projection | test_month_roster, test_roster, test_roster_event_writer, test_roster_history, test_roster_snapshot |
| Hardware adapter and bus | test_esp32_oled, test_esp32_ram_setup_code_verifier, test_esp32_secure_random, test_hardware_probe, test_i2c_bus_owner |

Use docs/TESTS.md and docs/SPECS/INDEX.md for exact suite inventory and
spec-to-test starting points. PlatformIO native tests are host evidence unless
the test explicitly establishes something stronger.

## 8. SF2 source and tests

- Source/reference models: web/sf2/*.mjs.
- Fixture inputs: web/sf2/fixtures/.
- Node tests: web/sf2/test/*.test.mjs (12 files, 270 registrations in this
  baseline).
- Package scripts/dependencies: web/sf2/package.json.
- Workstream status and source mapping: docs/sf2/INDEX.md and
  docs/sf2/CORE_SOURCE_MAPPING.md.

This is a browser-side reference model, not a production device API, dashboard,
trusted data service or final XLSX exporter.

## 9. Tools, configurations and evidence

- tools/host_compile_all.sh — host object-compilation helper for production
  library translation units; not an ESP32 target build.
- tools/run_host_littlefs_tests.sh — constrained POSIX fallback for selected
  storage/VFS-helper suites; not target LittleFS proof.
- tools/host_test_support/unity.h — limited host Unity-compatibility shim.
- tools/check_att_boot_shell.py — source/contract checker for boot-shell
  invariants; inspect its exact checks before interpreting its result.
- tools/inspect_probe_wrappers.sh and related tests — diagnostic probe wrapper
  inspection utilities.
- tools/legacy/ — historical, version-specific verification scripts only.
- docs/evidence/ — retained logs; check timestamp, source identity, command,
  target and evidence level before citing any result.

Current root PlatformIO files: platformio.ini, platformio.attendance.ini,
platformio.esp32-probe.ini and platformio.esp32-smoke.ini. Read their contents
before running a target; never flash the adapter-link smoke target.

## 10. Task router

| Task area | First authority/context reads | Implementation/test navigation |
|---|---|---|
| Product rule or requirement | HANDOFF_FINAL.md, relevant docs/DECISIONS.md entries | Relevant spec, source callers and tests |
| Authentication, sessions or API | C6 in docs/CRITICAL.md; docs/UNVERIFIED.md; docs/SPECS/api_logic.md | core PIN/API/session modules, HAL verifier/writer/runtime, related auth suites |
| AP onboarding or credential | AP decisions; bootstrap/setup specifications | Core AP setup, HAL Wi-Fi/NVS, AP credential suites |
| AS608 enrollment/attendance | AS608 decisions, adapter and pipeline specs, U5 status | lib/as608, HAL AS608 owners/transport, AS608/enrollment/admission suites |
| Clock, RTC or time zone | clock decisions, clock_policy.md, ds3231_clock.md | core time/clock, HAL RTC/I2C, clock/DS3231 suites |
| Storage, logging or recovery | C13/C13.3, C2, storage/recovery specs | core fileset/month/log/restore, LittleFS adapter, storage suites |
| Roster or identity | roster decisions, enrolled_roster.md and roster_store.md | core roster/history/snapshot, AS608 callers, roster suites |
| OLED/display | esp32_oled.md and applicable credential/display decision | HAL OLED interface/implementation and test_esp32_oled |
| SF2/reporting | docs/sf2/INDEX.md, REQUIREMENTS.md, TEACHER_QUESTIONS.md, root decisions | web/sf2 source, fixtures, Node tests and reference workbook audits |
| Build/test/tooling | docs/TESTING_WORKFLOW.md, docs/TESTS.md, target README | exact PlatformIO config, owning suite and evidence log |
| Agent workflow/context | AGENTS.md, WORKING_MEMORY.md, AI_COORDINATION.md | AI_START.md and this index |

## 11. Known navigation and integrity cautions

- WORKING_MEMORY.md is the live state pointer, not product authority.
- AI_COORDINATION.md is a cooperative communication/ownership record, not a
  technical lock or shared-memory service across separate filesystem copies.
- MANIFEST.sha256 failed its read-only audit before the 2026-11-13 context
  update; after these changes it has not been re-audited and must be considered
  stale.
- Existing documentation may contain historical counts or stale statements.
  Prefer the current source inventory and docs/TESTS.md; report conflicts and
  correct them through a bounded documentation update rather than guessing.
- No Git metadata was present in the inspected repository root. Do not claim
  branch, commit, clean-worktree or merge safety.
- Never treat boot-shell, smoke, diagnostic, host-test, mock, fixture or
  reference-model evidence as proof of full physical attendance operation.
