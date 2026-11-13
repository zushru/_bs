# Project working memory — read first

Project: _bs ESP32 fingerprint attendance device
Active root: /root/att
Baseline: v0.2.93 development snapshot; its 447-entry manifest and protected hashes were verified before v0.2.94 edits. No Git metadata exists.
Current increment: v0.2.94 extends the native-only LittleFsFileSet fault seam through addFile() open/fsync/close. Three regressions cover open failure and the indeterminate created-file outcomes after fsync/close errors. ESP32 direct syscalls are unchanged.
Protected files: HANDOFF_FINAL.md and docs/DECISIONS.md were not edited. SHA-256 remains 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d and 52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3 respectively.
Focused command: pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset' — 76/76 passed, exit 0 (50 + 23 + 3). Current inventory: 738 C++ registrations across 39 suites; the other 662 were not run. SF2 remains 270 Node registrations across 12 files and was not run.
ESP32 command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — SUCCESS, exit 0, 1,987.27 s. ELF SHA-256 18af01b5664e794b22f27e2d3acb383dbbbdfb3c00135a4a08d3198cfb677660; BIN SHA-256 40ce2e4416b2a59c524bf40d97b7d648566b0054d894e9aa7cdbcd88a689c909; RAM 67,892 B (20.7%), flash 1,010,340 B (77.1%). ELF retains affected fileset and mount-owner symbols. Compile/link only; NEVER flash.
C13.3 remains blocking: no mounted ESP-IDF VFS runtime/fault injection, low-space, real descriptor-close, parent-directory durability, proven commit point, restart interruption or physical power-cut/brownout proof exists. The production root is still absent; C3, C1/C34 and C6 remain blocking. No format, truncate, quarantine, repair, journal or commit primitive was enabled.

READ THIS FILE FIRST when resuming project work. Then read HANDOFF_FINAL.md, docs/DECISIONS.md, docs/CONTEXT.md, docs/CRITICAL.md, docs/UNVERIFIED.md, the newest release entry in docs/TESTS.md, and the directly relevant spec, implementation, public header and tests.

This is a navigation and working-memory aid. It is not authority to change signed or delegated product rules.

## 1. Source-of-truth order

1. HANDOFF_FINAL.md — signed product and architecture handoff.
2. docs/DECISIONS.md — decision ledger with SIGNED, DELEGATED and OPEN status.
3. Relevant approved module specification under docs/SPECS/ or docs/sf2/.
4. Current production implementation and public interface.
5. Owning tests and directly related tests.
6. docs/CRITICAL.md, docs/UNVERIFIED.md, docs/TESTS.md, docs/CONTEXT.md.
7. This working memory and the development roadmap.

Never resolve a DELEGATED choice by inference. Ask before changing it. Historical release notes explain past increments; their test counts are historical, not the current inventory.

## 2. Mandatory work protocol

- Verify root, release/version marker, archive checksum, manifest, protected hashes and running processes before editing.
- The user explicitly replaced `/root/att`; the current extracted tree and its manifest are the source of truth. This tree has no Git metadata. At every new task, recheck the version marker, manifest and protected hashes; do not infer Git history or reconcile unknown work.
- Inspect the implementation, public interface, owning tests, directly affected dependent tests and controlling specification before editing.
- Add a meaningful regression first. Run the owning suite against the unmodified implementation; establish that the assertion detects the gap. Preserve the first useful red log.
- Make the smallest implementation change that makes the test pass. Do not weaken assertions to obtain a pass.
- Run the owning suite again, then directly affected dependent suites only where a shared interface or behavior changed.
- Build only the affected ESP32 target. Do not run duplicate or concurrent PIO processes. Poll a running PID and inspect its log before further work.
- The phone/PRoot environment is slow. Full regression is a planned integration/release checkpoint, not a default after every increment.
- Never flash the inert adapter-link image. Do not erase NVS, format storage, alter partitions, erase sensor templates or run destructive recovery without explicit authorization and established safety conditions.
- Report source inspection, host tests, syntax checks, target compilation/linking, ELF inspection, mounted-target runtime and physical-device evidence separately.
- Preserve exact commands, suite/target names, test counts, exit status, full logs and checksums. State what was deliberately not run.
- Be critical of false confidence: inspect partial writes, retry duplication, persistent corruption, state-machine dead ends, race conditions, incomplete rollback, and fake backends that do not model the target.

## 3. Baseline and protected identity

Historical baseline v0.2.74:
- Authoritative prior release archive: /storage/emulated/0/Download/att_v0.2.74_2026-11-10.tar.gz; SHA-256 9412f4b1670b099b9fc5171a33da8bfdcb3b0a550edbec9e991f1ac6b811170b.
- Its sidecar verified; the extracted 385-entry manifest passed; `/root/att` was compared with `/tmp/att_v0274_extract_final` before v0.2.75 source edits.
- v0.2.74 was documentation-only: C34 audit, release/audit notes, changelog/test/context updates and targeted verifier; no implementation or tests changed.
- Protected HANDOFF_FINAL.md SHA-256: 4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d.
- Protected docs/DECISIONS.md SHA-256: d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44.
- No Git metadata exists in the checkout.
- Do not edit the two protected files unless explicitly requested and decision authority permits it.

For each release, record the exact archive name/SHA, release identifier, manifest count, protected hashes, changed files, tests/builds, exit statuses and independent extraction verification. The current v0.2.72 archive digest is external to this archive in the .tar.gz.sha256 sidecar to avoid a self-referential manifest/archive hash.

## 4. Project structure and file-purpose map

Root files:
- README.md — project overview, directory guide, verification status and limitations.
- RELEASE.md and RELEASE_v*.md — current release pointer and per-increment evidence.
- HANDOFF_FINAL.md — signed behavior and architecture contract; protected.
- MANIFEST.sha256 — source-file integrity inventory; regenerate last.
- platformio.ini — native PlatformIO environment.
- platformio.esp32-smoke.ini — isolated inert ESP32 adapter-link build.
- platformio.esp32-probe.ini — separate diagnostic hardware-probe build.

docs:
- docs/CONTEXT.md — read-first project/module status summary and directory navigation.
- docs/DECISIONS.md — decision authority ledger; protected.
- docs/CRITICAL.md — architectural blockers, impact and required remediation.
- docs/UNVERIFIED.md — claims still requiring proof and how to prove them.
- docs/TESTS.md — newest test evidence at top, historical increments below.
- docs/TESTING_WORKFLOW.md — resource-aware targeted test/build workflow.
- docs/CHANGELOG.md — chronological change history.
- docs/SPECS/ — module contracts; inspect the relevant one before implementation changes.
- docs/sf2/ — reporting/export proposals, policy notes and template references.
- docs/patches/ — historical patch artifacts. apply.sh was already used; do not rerun against the current tree.
- docs/DEVELOPMENT_ROADMAP.md — approved increment sequence and explicit full-suite gates.
- docs/WORKING_MEMORY.md — this file.

Implementation:
- lib/core/src/ — hardware-independent event codec/log/index, storage contracts/month sets, closure/day/health/time/clock, roster/history/audit/projection, config/boot, API/auth/session and backup/restore replay.
- lib/as608/src/ — AS608 command/sensor logic, attendance client/pipeline/admission, sensor arbiter, enrollment session/FSM and config runtime bridge.
- lib/hal_esp32/src/ — Arduino Stream UART, ESP32 AS608 runtime/startup/owner, NVS config/PIN/credential stores, NVS startup guard, Wi-Fi AP, secure/bootstrap RNG and OLED adapters. These are reusable components, not a complete production firmware root.
- lib/littlefs_file_set/src/ — POSIX/VFS IFileSet adapter. The filesystem must already be mounted and owned by the caller; operations require caller serialization. Destructive recovery methods remain disabled.

Firmware/test harness:
- target/esp32_adapter_link/ — inert compile/link smoke application. Compile/link only; NEVER FLASH.
- target/esp32_hardware_probe/ — diagnostic I2C/OLED/AP/DHCP probe; not attendance firmware.
- sketch/ — legacy diagnostic bring-up sketch.
- test/ — PlatformIO native Unity test suites. Filter by suite directory, not Unity case name.
- tools/ — manifest, targeted verification and exact ELF/link-wrapper inspection scripts. Preserve executable bits.

SF2 reference:
- web/sf2/ — separate browser-side reference model, not production device API, dashboard or final XLSX writer.
- attendance_model.mjs — normalized attendance calculation.
- learner_layout.mjs — learner row order/pagination.
- section_model.mjs — section monthly totals and consistency.
- preview_model.mjs — preview diagnostics, not export authorization.
- month_roster_projection.mjs — month-overlap roster projection.
- reporting_identity_projection.mjs — legal reporting identity and movement projection.
- reporting_snapshot_join.mjs — exact-prefix join of roster and identity.
- snapshot_schema.mjs — strict snapshot/provenance validation.
- snapshot_adapter.mjs — adapts validated source snapshots to pure calculation.
- snapshot_coordinator_model.mjs — browser reference ticket/page/revision model, not a durable server.
- metadata_history_model.mjs — proposed versioned reporting metadata history.
- source_prefix.mjs — canonical source-prefix rules.

## 5. Native suites — semantic map

Always check actual directory names with:
find test -mindepth 1 -maxdepth 1 -type d | sort

Historical v0.2.77 inventory: 635 C++ RUN_TEST registrations across 35 suites; SF2 then had 257 Node registrations across 12 test files. Current v0.2.82 inventory: 667 C++ registrations across 37 suites and 270 SF2 Node registrations across 12 files (already present in the v0.2.81 baseline archive). Focused v0.2.82 runs passed 122/122 across test_monthset (23), test_littlefs_mount_owner (22), test_littlefs_fileset (28), test_event_group_audit (20), test_roster_snapshot (26) and test_littlefs_serialized_fileset (3). The runs were separate; the other 545 C++ registrations were not run. Full native N1 and SF2 remain deferred.

Suite groupings:
- AS608, attendance and enrollment: test_as608, test_as608_gate, test_as608_pipeline, test_attendance_admission, test_enroll, test_hardware_probe.
- Core log/event/storage/restore: test_core, test_coreb, test_coreb2, test_event_group_audit, test_littlefs_fileset, test_monthset, test_restore_replay, test_backup.
- Configuration, NVS, AP, OLED and RNG: test_config, test_config_apply, test_config_boot, test_config_guard, test_config_provision, test_nvs_config, test_nvs_ap_credential, test_nvs_startup_guard_policy, test_ap_credential, test_ap_credential_setup, test_esp32_bootstrap_random, test_esp32_oled, test_esp32_secure_random, test_esp32_wifi_ap.
- Roster: test_roster, test_roster_history, test_roster_snapshot, test_month_roster.
- API/auth/session: test_api, test_hardening, test_pin_auth_session.

There are 37 suite directories in v0.2.82; this semantic grouping is only an index and should be checked against the live listing. PlatformIO -f selects suite paths relative to test_dir, not individual Unity registrations.

Focused command:
pio test -e native -f 'test_relevant_suite'

Use repeated -f only for directly affected dependent suites. Do not total overlapping runs as unique tests.

## 6. SF2 suite plan

Historical baseline source inventory at v0.2.71: 257 Node registrations across 12 test files. Current v0.2.81/v0.2.82 source inventory is 270/12; the 13 later registrations have not been rerun as a full SF2 suite.

S0: before the first SF2 code edit, run from /root/att/web/sf2:
npm test

During SF2 implementation, run owning and directly dependent files using:
node --test test/actual_module.test.mjs

S1: after integrated SF2 model changes, run:
npm test
for f in ./*.mjs ./test/*.test.mjs; do node --check "$f" || exit 1; done

Repeat full SF2 test suite at the release-candidate checkpoint if any SF2 code changes after S1. The historical 257/257 result does not cover the current 270 registrations and does not mean the current tree was freshly tested. A Node reference-model pass does not prove DepEd acceptance, workbook fidelity, device API integration or persistent metadata recovery.

## 7. Full-suite and E2E checkpoints

Do not run pio test -e native yet.

N1 full native checkpoint is deferred until C13 storage, C1/C34 startup, C3 production composition, C6 authentication/API and C2 backup/restore are integrated sufficiently to exercise their cross-module interactions.

At N1, exact command from /root/att:
pio test -e native

Record exact suite list, registration count, pass/fail count, exit status and full log. Repeat at release candidate if source changes occur after N1.

E2E starts only after there is an actual production composition root, storage owner, clock caller, attendance scheduler, enrollment policy, auth/API and backup/restore integration. Start with a deterministic host integration harness, then perform real-device tests. Host simulation and inert smoke links are not physical E2E evidence.

## 8. Active roadmap and blockers

Read docs/DEVELOPMENT_ROADMAP.md for the approved increment order:
C13.1 host fault injection (complete); C13.2 recovery protocol proposal (drafted, not approved); C13.3 mounted-target VFS and power-cut verification; C1/C34 startup/NVS safety; C3 production root; C6 production API/auth; C2 backup/restore; then integrated host E2E and physical validation.

Critical blockers:
- C13: v0.2.82 adds a reusable no-format LittleFS mount owner, serialized fileset, read-only record validator and cross-month roster semantic preflight. It is not wired into a production composition root, and the inert smoke target never calls begin(). Mounted-target VFS runtime/fault injection, a proven commit primitive, recovery journal/transaction, truncate/quarantine implementation, crash-recovery and physical power-cut/brownout evidence are still absent. D1–D11 are delegated engineering choices in docs/SPECS/littlefs_recovery_protocol.md, not user-signed product rules.
- C3: no production firmware main/scheduler, integrated HTTP routes/admin UI, enrollment service or production clock caller.
- C6: no integrated HTTP/session/auth boundary, chosen PIN KDF/verifier writer or protected production routes.
- C2: no storage-swap transaction, recovery journal or reboot recovery.
- U61: v0.2.82 now checks roster lifecycle transitions across month files during the reusable LittleFS startup preflight, but this audit is not integrated at every operational writer/consumer boundary or a production boot path; do not cross delegated E27.
- U112: request tickets are RAM-only; no durable exactly-once semantics or cancellation/reconciliation of expired handlers.
- C34: v0.2.75 makes Esp32NvsConfigBackend fail closed unless the pre-setup guard reports exactly one successful Arduino NVS init and no blocked whole-partition erase. v0.2.76 adds the same unsafe-snapshot rejection to every AP credential/PIN attempt store operation. This still does not enforce ConfigBootCoordinator READY/NEEDS_SETUP ordering; a safe Arduino snapshot allows direct low-level calls before config boot. The erase wrapper still does not cover chunked/alternate/direct flash erase paths, and no production root proves any application-owner order.
- E18/U5: physical sensor ID/page mapping and index semantics unresolved; do not implement production enrollment using mock assumptions.
- SF2: browser reference code is not final client export integration; reporting metadata durability, trusted source/snapshot, policy acceptance and backup/recovery remain open.
- Hardware: no physical sensor/display/RTC/EEPROM/AP/DHCP/LittleFS runtime/powerbank/soak/brownout/power-cut claim may be inferred from host or link tests.

## 9. Product context to preserve

- Use “term”, not “semester”, in product-facing terminology.
- Physical direction selected: handheld, powerbank-powered, project-style enclosure; exact material not finalized.
- User prefers critical direct implementation, resource-aware verification and complete replacement archives with changelog.
- Check library documentation before planning hardware upload tests where possible.
- Continue safe host/source work if hardware is unavailable, but label all physical behavior unverified.

## 10. Current C13.2 proposal status

Read docs/SPECS/littlefs_recovery_protocol.md before any storage-recovery change. On 2026-11-10 the user delegated D1–D11 engineering choices to assistant judgment. They are recorded as DELEGATED, not user-signed product rules. D1–D3 require no automatic suffix discard; D4 preserves evidence without automatic expiry; D5/D6 select no commit primitive or persistent journal until target durability is proven; D7 fails closed on insufficient space; D8 requires one authenticated ADMIN, fresh typed confirmation and physical service presence; D9 leaves EV_REPAIR unused; D10 preserves signed O8; D11 prohibits automatic formatting.

v0.2.77 changes LogStore::open() to inspect without truncation, preserve all bytes on partial/corrupt/incomplete tails, return failure, remain faulted, and avoid replaying a valid-looking prefix. Focused host suites cover the owner and dependent contracts. It does not enable truncate, quarantine, format, repair, evidence export, a journal, or a destructive recovery workflow. The month_set spec remains DELEGATED, and MonthSet::repair() remains non-transactional. The pinned ESP32 LittleFS VFS and physical interruption behavior remain unverified.

## 11. Historical C34 erase-surface audit (v0.2.74)

Read `docs/AUDIT_v0.2.74.md` before making any NVS guard or flash-erase change. The installed package versions are Espressif32 8.0.0 / Arduino-ESP32 and ESP32 libs 3.3.12 / ESP-IDF 5.5.5; LittleFS component metadata is 1.22.3.

The shared guard blocks only an exact full DATA_NVS `esp_partition_erase_range()` call. It does not cover chunked erases, DATA_NVS_KEYS, or direct calls to public `esp_flash_erase_region()` / `esp_flash_erase_chip()`. The prior v0.2.72 smoke ELF SHA-256 is `bfbb50a538582d53d101a80ad0e01b2dcefc7e7d4f1e84f61bfe012af0b8154b`; its exact inspector passes 12 NVS route checks and 9 LittleFsFileSet symbol checks. In that image the only observed `esp_flash_erase_region()` caller is `esp_partition_erase_range()`; no direct application call was observed, and `esp_flash_erase_chip()` is not linked. This is static evidence about that inert image, not a complete flash firewall.

NVS erase APIs deinitialize an initialized partition before the lower-level erase attempt; a blocked runtime erase can still leave NVS handles closed. Do not assume denial is side-effect-free.

The installed ESP32 libs package sdkconfig confirms `CONFIG_VFS_SUPPORT_DIR=y`, LittleFS page size 256, max partitions 3, read/write size 128, block cycles 512, with `CONFIG_LITTLEFS_FLUSH_FILE_EVERY_WRITE` disabled. The pinned `esp_littlefs.h` exposes `ESP_LITTLEFS_ENABLE_FTRUNCATE`. This is package/header evidence only; no LittleFS component link/mount/ftruncate or power-loss test was run. The current adapter's destructive methods remain disabled.

v0.2.74 is documentation-only. Exact prior ELF was re-inspected; no PIO, SF2, target build, hardware or physical-device test was run.

## 12. Historical C34/C1 startup gate (v0.2.75)

Read `docs/AUDIT_v0.2.75.md` before further NVS startup-boundary changes. The config backend now checks `Esp32NvsStartupGuard::status()` and reuses Arduino's pre-setup initialization result. It rejects any snapshot other than exactly one successful init with no blocked whole-partition erase attempt. It does not call `nvs_flash_init()` again. `ConfigBootCoordinator::start()` latches fault before invoking the backend; AP setup and PIN runtime coordinators require the relevant successful config boot state.

Focused tests:
- `pio test -e native -f 'test_nvs_config'`: 46/46, exit 0.
- `pio test -e native -f 'test_nvs_startup_guard_policy'`: 9/9, exit 0.
- Red phase reproduced four intended backend failures. PlatformIO's failure wrapper emitted SIGILL after Unity failure output; the red run is not described as a normal completed pass.
- Current native source inventory: 630 registrations / 35 suites; 575 were not run. Full N1 remains deferred.
- `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2`: success, 2,086.96 s. RAM 67,356/327,680 B (20.6%); app flash 935,040/1,310,720 B (71.3%).
- ELF SHA-256 `1e959f325be97ee57c13eb852d40bdc40666cbbaceace8933f56a83d9c0ac57e`; BIN SHA-256 `dfc8c198d2dbd90aa1be6281cf99d6f4179ffbd14fd7072e8232ef95c968420d`.
- Final ELF checks: 12/12 NVS wrapper routes; `Esp32NvsConfigBackend::begin()` and `Esp32NvsStartupGuard::status()` retained; 9/9 LittleFsFileSet methods retained.

Limits at v0.2.75: this was host and inert target compile/link/static-ELF evidence, not runtime proof. The target was not flashed. The erase guard remains narrow. v0.2.76 adds independent unsafe-snapshot checks to AP credential and PIN-attempt adapters, but those checks are not a ConfigBootCoordinator readiness token; the future production root must still enforce config boot before application storage owners. There is no production root. C34 remains blocking. No runtime NVS, LittleFS mount, flash, hardware, power-cut or physical-device test was run.

## 13. Historical C13.1 verification note

Current code adds a native-only per-instance syscall hook for appendFile write/fsync/read-back/close. The ESP32 branch retains direct syscalls. Seven regressions cover short writes, write EINTR, partial prefix then hard error, fsync failure, read-back I/O failure, read-back mismatch and close error. The destructive operations still fail closed.

The first red PlatformIO run logged seven expected failures but the wrapper then emitted SIGBUS and an incorrect case total. Running the direct native binary isolated the result: 27 registrations, 7 failures, 20 passes, normal exit 7; no SIGBUS. The final green PlatformIO owning suite then passed cleanly: 27/27, exit 0.

These are host POSIX tests. They do not prove ESP32 LittleFS VFS behavior, real descriptor-close failure semantics, directory-entry durability, physical power-loss recovery or production storage ownership. The close-fault fake actually closes the host descriptor and then synthesizes EIO; it proves false-result propagation only.

C13.1 target build completed successfully:
- Command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
- Exit: 0; build time 1,769.31 s.
- Environment: Espressif32 8.0.0 / ESP32 Dev Module, Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121.
- RAM 67,356/327,680 B (20.6%); application flash 934,976/1,310,720 B (71.3%).
- Exact ELF inspection: ./tools/verify_v0.2.72.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf" — exit 0, 12/12 NVS wrapper routing and 9/9 LittleFsFileSet method retention.
- Log: /tmp/att_v0272_esp32_build.log.

The target is inert and was not flashed. No filesystem mount/operation, VFS fault, runtime NVS, power-cut or physical-device check was performed. Do not run another target build for this increment.


## 14. Current v0.2.76 adapter startup-boundary increment

Files changed:
- lib/hal_esp32/src/nvs_ap_credential_store.cpp/.h
- lib/hal_esp32/src/nvs_pin_attempt_store.cpp/.h
- target/esp32_adapter_link/main.cpp (smoke-only non-invoking PIN initializer symbol-retention anchor)
- test/test_nvs_ap_credential/test_main.cpp
- test/test_nvs_config/test_main.cpp
- tools/verify_v0.2.76.sh
- docs/SPECS/config_boot.md
- docs/AUDIT_v0.2.76.md
- RELEASE_v0.2.76.md
- README.md, RELEASE.md, docs/CHANGELOG.md, docs/CONTEXT.md, docs/CRITICAL.md, docs/UNVERIFIED.md, docs/TESTS.md, docs/DEVELOPMENT_ROADMAP.md, this file and MANIFEST.sha256.

Behavior: every public AP credential and PIN attempt store operation checks Esp32NvsStartupGuard::status() before nvs_open(). Unsafe startup fails closed. A non-ESP_OK first init error is preserved; otherwise ESP_FAIL is used. AP load clears its output. PIN load leaves output untouched. This snapshot check does not prove ConfigBootCoordinator has reached READY/NEEDS_SETUP and does not enforce production owner order.

Test evidence:
- Red: /tmp/att_v0276_red_test_nvs_ap_credential.log and /tmp/att_v0276_red_test_nvs_config.log reproduce the two new assertions. PlatformIO emitted SIGHUP after each failing Unity summary; do not describe either as a normal completed process.
- Green: pio test -e native -f 'test_nvs_ap_credential' — 16/16 passed, exit 0; log /tmp/att_v0276_green_test_nvs_ap_credential.log.
- Green: pio test -e native -f 'test_nvs_config' — 47/47 passed, exit 0; log /tmp/att_v0276_green_test_nvs_config.log.
- Selected total 63/63. Inventory 632 C++ registrations across 35 suites; 569 were not run. SF2 remains 257/12 and was not run. N1 remains deferred.

Target command (run twice: full rebuild, then incremental relink after correcting the smoke link probe):
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

- First build `/tmp/att_v0276_esp32_build.log`: SUCCESS, 1,076.82 s; RAM 67,356/327,680 B (20.6%); application flash 935,336/1,310,720 B (71.4%). Its first ELF check failed because initializeFailureCount() was garbage-collected: the compiled object contained the method, but its discarded-value anchor did not keep the pointer section live.
- Corrected target/esp32_adapter_link/main.cpp to force a volatile pointer read and nullptr comparison without invoking the method.
- Incremental build `/tmp/att_v0276_esp32_build2.log`: SUCCESS, 100.83 s; RAM 67,356/327,680 B (20.6%); application flash 935,792/1,310,720 B (71.4%).
- Final ELF SHA-256: 3e5989be6ef00ed708159e7a17bd70618f7c8b2955a6e597bc5465301bf359ec. Final BIN SHA-256: 20138030906d1f619be41736c5303b091c0ab8462906d56be52ecd17a6f252c7.
- Final `./tools/verify_v0.2.76.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"`: exit 0; 12/12 NVS wrapper routes, all seven config/startup/AP/PIN symbols, 9/9 LittleFsFileSet symbols. Log `/tmp/att_v0276_elf_verify2.log`; initial failure log `/tmp/att_v0276_elf_verify.log` retained for audit.
- Target is inert and was not flashed. This is static link evidence only.

Critical: the startup snapshot check is not a ConfigBootCoordinator readiness token. A safe Arduino snapshot still permits a direct store call before config boot; only a future production root can enforce owner order. C34 remains blocking because the erase guard is narrow and no production root/recovery workflow exists. No physical/runtime evidence is present.

## Latest verified increment — v0.2.95 (2026-11-12)
See `RELEASE_v0.2.95.md` and `docs/AUDIT_v0.2.95.md`. The only source/test change is a native MonthSet/LogStore regression for an uncertain `addFile()` result after empty-file creation. A new `open()` is required before retry. This does not close C13.3.
