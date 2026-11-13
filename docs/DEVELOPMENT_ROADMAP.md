# Development roadmap

**Approved:** 2026-11-10  
**Original baseline at approval:** v0.2.71; current verified source baseline is v0.2.95 (2026-11-12)  
**Completed:** C13.1 — host-only append fault-injection coverage (v0.2.72).  
**C13.2 status:** D1–D11 engineering choices were delegated to assistant judgment on 2026-11-10 and are recorded in the proposal as DELEGATED, not user-signed product rules. v0.2.77 implements only read-only startup preservation; no destructive recovery transaction or journal is authorized.
**C34 status:** v0.2.74 audited the pinned erase surface; v0.2.75 gates Esp32NvsConfigBackend on the pre-setup guard snapshot; v0.2.76 makes low-level AP/PIN NVS adapters independently reject an unsafe snapshot. The shared erase wrapper remains narrow, `ConfigBootCoordinator` ordering is not enforced by the adapters, and production root integration is still blocking.  
**Status:** Approved sequence; each increment still requires its own source-level review and verification. This roadmap does not authorize changing signed or delegated product decisions.


## Current execution overlay — 2026-11-12

- Current code baseline: **v0.2.95**. The newest code evidence is in `RELEASE_v0.2.95.md`, `docs/AUDIT_v0.2.95.md` and the top of `docs/TESTS.md`; older v0.2.77–v0.2.94 statements below are historical evidence, not current-version claims.
- C13.3 remains the first priority and remains **BLOCKING**. v0.2.95 passed 100/100 focused native cases across `test_monthset`, `test_littlefs_fileset`, `test_littlefs_mount_owner` and `test_littlefs_serialized_fileset`; these use POSIX/fake backends. No mounted ESP-IDF VFS runtime, real target descriptor/low-space/directory durability, interruption matrix or physical power-cut/brownout evidence is established.
- Continue in the approved order: **C13.3 → C1/C34 → C3 → C6 → C2**. C2 remains gated on storage ownership and an approved recovery design. Continue safe independent work on E18/U5, U61, U112 and SF2 without changing their OPEN/DELEGATED status.
- Classify blockers as DEVELOPMENT, RELEASE or EVIDENCE blockers. Resolve a development blocker before dependent work; keep release/evidence blockers open while safe independent work proceeds.
- Use `../DEVELOPMENT_WORKFLOW.md` for the two-pass deadline workflow, session baseline guard, focused test cadence, freeze trigger and batch report. Do not run full native N1 before the approved integration gate.
- This overlay updates execution status only. It does not change product decisions, recovery policy, API exposure, persisted formats, sensor mapping, term boundaries or any decision status.

## Current execution overlay — 2026-11-13

- C13.3 source-only hardening: the `att-boot-shell` READY path already composes `Esp32LittleFsMountBackend` → `LittleFsStorageOwner` → serialized file set → `MonthSet`/`LogStore` read-only replay. The backend now requests `config.read_only = true`; `format_if_mount_failed` and `grow_on_mount` remain false. The existing `tools/check_att_boot_shell.py` assertion already required this setting, so the implementation now matches its guard.
- This is only a boot-shell access restriction. It does not establish exact pinned VFS enforcement, durable file/directory commits, low-space behavior, descriptor recovery, restart safety, or power-cut resilience. It does not provide the writable storage owner needed by production attendance. C13.3 remains BLOCKING.
- Partition reconciliation remains unresolved: the full read-only 0x1000-byte device partition-table capture and flash identity evidence are absent. Do not change signed geometry or flash the target.
- All tests, checkers, compilers, PlatformIO builds, flash operations and hardware tests remain NOT RUN for this increment.

## Governing constraints

1. Read `docs/WORKING_MEMORY.md` first, then `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, the relevant specification, owning implementation and tests.
2. Verify the project root, version, release archive/manifest identity, protected hashes and absence of another active build before remote edits.
3. Add or extend the smallest relevant regression before implementation. Run the owning suite first; then only directly affected dependent suites.
4. Keep the phone/PRoot workload proportional to the increment. Do not run the full native suite after every patch. Do not run duplicate or concurrent PlatformIO builds.
5. Do not change a signed/delegated rule, persisted format, identity/page mapping, term boundary, recovery policy, or API exposure without the required explicit decision.
6. Never flash the inert adapter-link image. Do not erase NVS, format storage, alter partitions, or perform destructive sensor/storage operations without explicit authorization and preconditions.
7. Report source inspection, host assertions, target compile/link, ELF inspection, mounted-target runtime, and physical-device evidence as separate evidence classes.

## Ordered increments

### Phase 1 — C13: storage integrity and recovery

**C13.1 — Host append fault injection (completed in v0.2.72).**

- Add a per-instance native-only syscall seam for `LittleFsFileSet::appendFile()`; ESP32 builds must retain the direct syscall path and must not expose the test hooks.
- Exercise short successful writes, `EINTR`, a prefix followed by a hard write error, `fsync` failure, verification `pread` failure, read-back mismatch, and `close` failure.
- Assert the key contract: a false result can coexist with a partial or complete visible append. Never treat false as proof that no bytes changed.
- Keep `truncateFile()`, `quarantineCopy()`, and `quarantineFrom()` fail-closed.
- Verification: `pio test -e native -f 'test_littlefs_fileset'`. Run `test_monthset` only if a shared storage contract or behavior is changed; this increment does not change that contract.
- Target build: rebuild `esp32-adapter-link` once because the conditional class declaration/source path changes. Compile/link only; never flash.

**C13.2 — Recovery protocol design (design before code).**

- Proposal: [SPECS/littlefs_recovery_protocol.md](SPECS/littlefs_recovery_protocol.md). The v0.2.73 proposal identified a dangerous LogStore::open() suffix-truncation ambiguity and the non-transactional MonthSet::repair() sequence.
- On 2026-11-10 the user delegated D1–D11 engineering choices to assistant judgment. The selected conservative policies are recorded as DELEGATED, not as signed product rules.
- v0.2.77 implements the first safe slice: LogStore::open() performs read-only validation, preserves every byte on anomalies, returns failure, leaves the log faulted, and does not replay a valid-looking prefix. Focused owning/dependent host suites verify partial tails, invalid complete EOF records, unfinished groups, clock groups, observer replay and clean group-boundary reopen.
- This change does not implement or enable truncation, quarantine, format, a recovery journal, an evidence export, or manual repair. The existing MonthSet::repair() sequence remains non-transactional and must not be called by production.
- C13.3 must establish the production LittleFS mount/storage owner and validate exact pinned-target VFS behavior: file data, directory-entry durability, fsync/close, rename/truncate semantics, low-space handling and interruption/restart behavior.
- Do not choose a commit point based on host POSIX behavior. If no single durable commit primitive can be proven on target, automatic in-device recovery remains unavailable.
- A future transaction must preserve originals, validate staging byte-for-byte and semantically, have a restart-idempotent journal, and be tested at every interruption boundary. Physical power-cut evidence is mandatory before any durability claim.

**C13.3 — Mounted-target VFS integration and fault validation.**

- Establish the production LittleFS mount owner and the serialized storage boundary. The current adapter does not mount or own the filesystem.
- Inject errors on the exact ESP-IDF VFS path for mount/enumeration/stat/read/write/sync/close where the platform permits it.
- v0.2.89 extends host-only fault injection through `scan_()` and `countChecked()`; focused native suites passed 65/65 across `test_littlefs_fileset` and `test_littlefs_mount_owner`. The changed storage TU compiled/archived for Xtensa, but full target linking and mounted VFS runtime remain unverified.
- v0.2.90 adds a POSIX-host integration case from `LittleFsStorageOwner` through `SerializedFileSet`, `MonthSet` and `LogStore`, then reopens with a fresh owner. The three focused C13 suites pass 69/69. This validates host composition only; it does not close C13.3. The owner is still not adopted by a production firmware root, and target VFS/runtime, low-space, persistence/interruption and physical power-cut/brownout gates remain open. v0.2.93 adds four native-only readFile open/pread/close/EINTR fault regressions; the same three focused C13 suites pass 73/73. The inert ESP32 smoke image also compiles/links and final ELF inspection retains the fileset/mount-owner methods. v0.2.94 adds native-only addFile() open/fsync/close fault routing and three regressions; the three focused C13 suites pass 76/76, and the inert target build/link retains the changed addFile() plus mount-owner/backend methods. The new fsync/close regressions preserve the created empty file after a false result. These are host and compile/link checks only; target VFS/runtime, low-space, descriptor, parent-directory durability, persistence/interruption and physical power-cut/brownout gates remain open.
- Verify actual LittleFS directory-entry and file-data behavior, descriptor lifecycle, full/near-full storage, and restart/replay.
- Only after a separately reviewed recovery transaction, an experimentally proven commit primitive, and a restart-safe journal are available: validate quarantine/repair against interruption at every state transition.
- Physical power-cut/brownout testing is required for durability claims. Host POSIX and inert target linking do not substitute.

### Phase 2 — production startup and application boundary

**C1/C34 — startup and NVS safety.**

- Re-audit the exact pinned ESP-IDF startup and NVS erase paths before production startup composition.
- v0.2.75 adds a focused gate at Esp32NvsConfigBackend::begin(): it requires exactly one successful Arduino pre-setup NVS init and no blocked startup erase attempt; the backend no longer calls nvs_flash_init() a second time. Focused suites passed 55/55 and the inert adapter-link target compiled/linked with its exact ELF inspected.
- v0.2.91 binds `Esp32NvsApCredentialStore` and `Esp32NvsPinAttemptStore` to a required `ConfigBootCoordinator` reference and rejects direct store calls unless the same coordinator is `READY` or `NEEDS_SETUP`. `Esp32PinAuthRuntime` uses that same bound reference. Focused NVS suites passed 65/65. The inert adapter-link target compiled its changed `main.cpp` object and generated bootloader/partition images, but the build was stopped during third-party Adafruit GFX compilation before changed HAL TUs/final link; no ELF exists.
- Preserve the current fail-closed setup behavior and do not widen the existing erase guard's claims: C34 is not a complete flash-erase firewall. This is an API-level guard against accidental early calls, not protection from raw NVS calls or a caller deliberately supplying another coordinator. The production root must pass the one authoritative coordinator instance and serialize all NVS owners; no production root exists.
- Test boot/config/guard state ordering with focused native suites, then build and inspect the affected production target once a real production target exists.
- Do not deliberately induce NVS corruption on the only device.

**C3 — production application root and serialized owner.**

- Create the actual firmware entry point and one serialized application loop/scheduler.
- Compose exactly one AS608 runtime/owner, storage owner, configuration boot, clock source/caller, attendance pipeline, closure and health flow.
- Define startup readiness and fault-latching before attendance is enabled.
- v0.2.92 adds mandatory ApplicationAttendanceGate wiring to the preferred owner: validated boot, exact no-format storage/log/roster/closure graph, config equivalence, sensor-only startup allowance and H_OK attendance/commit permission. The config runtime rejects a closure instance not bound by the gate. Focused pipeline tests passed 16/16; inert ESP32 smoke target linked with the methods retained in the final ELF. This is a prerequisite only; no production root/caller was added, so C3 remains BLOCKING.
- Enrollment remains blocked until E18 ID-to-page mapping and U5 physical index semantics are resolved. Do not infer those from mocked UART tests.
- The future clock caller must handle every `ClockApplyResult` and stop/reopen/reconcile after uncertainty.

**C6 — production API and authentication boundary.**

- Integrate actual HTTP routes, request-bound authentication/authorization, PIN verifier/KDF and lockout, session lifetime, protected administrative operations, and PII-safe status/reporting boundaries.
- Bind each authorization gate to the current request; do not cache a stale authorization reference in a coordinator.
- Enforce serialized access to request/session tables and stop handlers after timeout/expiry before retrying side effects.
- Verify route-level denied/allowed cases, stale tokens, lockout, request replay, partial response writes and PII redaction.
- Do not expose reporting data or enrollment merely because an adapter compiles.

**C2 — backup/restore transaction and recovery.**

- Integrate backup/restore only after the C13 storage owner and approved recovery protocol exist.
- Prove staging, authentication, semantic replay, replacement ordering, rollback, restart recovery and idempotent result reporting.
- Preserve the signed O8 backup scope unless the user explicitly amends it.
- Include reporting metadata only through a separately approved, versioned and recoverable design; do not silently claim complete SF2 recovery without it.

### Phase 3 — integrated behavior and release verification

**Operational E2E integration.**

- Start only after the production root, storage, clock, attendance, auth/API and restore flows are actually composed.
- Use a host integration harness first for deterministic workflow and failure-state tests.
- Then verify on real ESP32 hardware: sensor enrollment/identification, OLED, RTC, EEPROM if used, Wi-Fi/AP/DHCP, mounted LittleFS, reboot/replay, storage faults, and power interruption.
- Keep host simulation, target compile/link, and physical tests separately reported.

## Mandatory verification gates

### Focused increment gate

- Add the regression first.
- Run the owning suite with `pio test -e native -f '<suite-directory>'`.
- Add dependent suites only when a shared interface, persisted format, state machine or caller behavior is changed.
- Build only the affected ESP32 target when target source, conditional compilation, shared ABI, linker routing or hardware adapter dependencies are affected.
- Do not sum overlapping suite runs as unique tests.

### N1 — first complete native regression checkpoint

**Do not run this yet.** Run only after C13 storage, C1/C34 startup, C3 production composition, C6 auth/API and C2 backup/restore increments are integrated far enough that their cross-module interactions are present.

Exact command from `/root/att`:

```sh
pio test -e native
```

Record total registered tests, passed/failed counts, suite names, exit status and full log. Any change after N1 invalidates the result for the changed source; repeat N1 at release candidate.

### SF2 Node suite plan

SF2 is a separate browser reference model, not device integration.

**S0 — baseline before the first SF2 source edit.** No SF2 source edit is part of C13.1, so do not spend a test run now. Before touching SF2 code, run:

```sh
cd /root/att/web/sf2
npm test
```

This records the exact current baseline. The current source inventory is 257 Node registrations across 12 test files; the historical 257/257 result is not a current v0.2.71/0.2.72 pass.

**Per-increment SF2 check.** Run the owning file and direct dependents only:

```sh
node --test test/affected_module.test.mjs
```

Use the actual filenames in `web/sf2/test/`; do not invent a suite name. Run syntax checks for changed modules when useful:

```sh
node --check path/to/changed_module.mjs
```

**S1 — integrated SF2 model checkpoint.** After the affected model/schema/adapter/coordinator work is integrated, run:

```sh
cd /root/att/web/sf2
npm test
for f in ./*.mjs ./test/*.test.mjs; do node --check "$f" || exit 1; done
```

Repeat the full SF2 suite at release candidate if any SF2 code changes after S1. A passing browser reference-model suite does not prove ESP32 API integration, legal DepEd acceptance, workbook fidelity, or device persistence.

### Release-candidate gate

At an explicit RC checkpoint, run the full native suite, full SF2 suite if SF2 changed, the actual production ESP32 build and exact ELF inspection, integration/E2E suites, and the approved physical-device test plan. Agree on this expensive checkpoint before running it on the phone.

## Current known blockers

- **C13:** no production LittleFS mount owner, target VFS runtime/fault injection, proven commit primitive, recovery journal/transaction, truncate/quarantine implementation, or power-cut/brownout evidence.
- **C3:** no production firmware root/scheduler, HTTP routes, admin UI, production enrollment service or clock caller. v0.2.92 hardens the preferred attendance-owner boundary but does not compose it into firmware.
- **C6:** no integrated HTTP/session/auth boundary, PIN verifier/KDF writer or protected production routes.
- **C2:** no storage-swap transaction/recovery journal/reboot recovery.
- **U61:** roster semantic lifecycle audit is not enforced at every operational writer/consumer boundary; further enforcement may intersect delegated E27.
- **U112:** request tickets remain RAM-only; durable exactly-once semantics and cancellation/reconciliation of expired handlers are unresolved.
- **C34:** NVS erase guard is intentionally limited, not a complete erase firewall.
- **E18/U5:** sensor ID/page and physical index semantics remain unresolved.
- **SF2:** browser reference models are not production API integration or final XLSX export; reporting metadata, trusted snapshot provenance, policy acceptance and backup/recovery remain blockers.
- Hardware: no physical ESP32/peripheral, powerbank, brownout, power-cut, soak or production firmware verification is implied by host tests or compile/link smoke builds.

## v0.2.95 evidence update
The host-side addFile failure propagation path is now covered through MonthSet/LogStore. Remaining C13.3 gates are unchanged: production LittleFS owner wiring, pinned ESP-IDF VFS runtime/descriptor faults, low-space, file and directory durability, restart/interruption, and physical power-cut/brownout proof. Do not advance C13.3 on native evidence alone.
