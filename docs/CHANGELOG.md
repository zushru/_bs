## 2026-11-13 — ATT-E26-INCLUSIVE-TERM-01 — align inclusive term-end source contract (source only)

- Recorded E26 as DELEGATED: the final configured local calendar day is included; `0 <= termStart <= termEnd <= 31446`; one-day terms are valid.
- Aligned Config validation, first-run parsing, API mark/no-class, AS608 scan begin/commit, ClosureEngine tick/CLOSE/sweep/outage clipping, restore replay and both config-runtime rebinding paths. Added shared `apiDayInInclusiveTerm()` for future read/day-view routes.
- Added owning/dependent regression source. NOT RUN. Historical Nov 12 193/193 output belongs to another source state and is not credited to this increment. C18 remains unverified; C17/E2 still block schedule changes.

## 2026-11-13 — ATT-NVS-FAILED-INIT-ERASE-01 — block chunked DATA_NVS erase after failed init (source only)

- Preserved the unconditional exact-full-DATA_NVS erase block and extended the guard to deny every non-empty DATA_NVS `esp_partition_erase_range()` request after the first Arduino NVS init fails. Normal sector maintenance remains allowed after successful startup.
- Renamed the status bit to `nvsEraseBlocked` and added a pure-policy regression for first/later chunks, unknown partition size, non-NVS partitions and zero-size requests.
- This does not cover raw esp_flash erase APIs, DATA_NVS_KEYS, alternate raw NVS callers, deliberate chunked erases after successful init or production owner order. C34 remains blocking. No tests/checkers/build/ELF inspection/hardware were run.

## 2026-11-13 — ATT-READONLY-VFS-MOUNT-01 — enforce boot-shell LittleFS read-only mode (source only)

- Changed the ESP32 LittleFS mount configuration from `read_only=false` to `read_only=true`. Formatting and grow-on-mount remain disabled. The boot shell now requests VFS-level write protection in addition to its source-level read-only replay path.
- The existing `tools/check_att_boot_shell.py` already required `config.read_only = true`; this increment resolves the implementation/guard mismatch without changing its assertion. Espressif documents the flag and its incompatibility with format-on-failure and grow-on-mount; exact target runtime rejection is still unverified.
- Updated the target README, recovery spec, blocker summary, roadmap and working memory. This does not prove VFS durability or authorize the future writer. C13.3 and the partition mismatch remain blocking.
- No tests, checker, compiler, build, PlatformIO, flash or hardware operation was run.

## v0.2.96 (2026-11-12) — deadline-first workflow and status alignment (documentation only)

- Added root `DEVELOPMENT_WORKFLOW.md` as the canonical operational protocol for baseline guards, blocker classes, coherent batches, two-pass execution, verification cadence and batch reporting.
- Aligned working memory, roadmap, context, critical/unverified summaries, test ledger, README and release pointers to v0.2.95 and its exact 100/100 focused native result.
- No firmware, production source, public interface, test source, product decision, specification, persisted format, API exposure or recovery behavior changed. C13.3 and all other blockers remain as recorded.

## v0.2.94 (2026-11-12) — LittleFS month-creation fault coverage

- Extends the native-only LittleFsFileSet syscall seam through addFile() open/fsync/close. The test openFn now receives mode_t so it can wrap both read-only opens and exclusive file creation. ESP32 keeps direct syscalls.
- Adds three regressions: open failure creates no file; fsync/close failure returns false while preserving the already-created empty month file and preventing a blind retry.
- Focused C13 host suites pass 76/76. The inert ESP32 smoke build/link succeeded and its ELF retains the changed fileset plus mount-owner/backend methods. No mounted target VFS, low-space, directory durability, restart, power-cut or physical test was run. C13.3 remains BLOCKING.
- No file-format, recovery-policy, destructive operation, production root, or SF2 behavior changed. Never flash esp32-adapter-link.

## v0.2.93 (2026-11-11) — LittleFS read-path native fault injection

- Routes LittleFsFileSet::readFile() open/pread/close through the existing per-instance test hooks in native builds; ESP32 retains direct syscalls.
- Adds four registered regressions for open failure, pread failure, close failure and interrupted-pread retry. Updates post-fault byte-preservation checks to use a fresh uninjected reader.
- Test-first red reproduced all four gaps. Final focused C13 command passed 73/73: fileset 47, mount owner 23, serialized fileset 3.
- The inert ESP32 smoke target compiled/linked successfully; final ELF SHA-256 is be804edcc9fc096f9c27ff3bb27994c9928eb295b5cf402ed982395e85d34405. RAM 67,892 B (20.7%), flash 1,010,340 B (77.1%). NEVER flash this target.
- C13.3 remains blocking. No mounted target VFS, low-space, restart/interruption, directory durability, physical power-cut/brownout, production-root or device test was performed. D1–D11 and all destructive-operation prohibitions remain unchanged.

## v0.2.92 (2026-11-11) — C3 attendance admission and owner-graph gate

- Adds ApplicationAttendanceGate as the mandatory admission dependency of As608AttendanceOwner. It binds validated config boot, READY no-format LittleFsStorageOwner, exact IFileSet → MonthSet → LogStore, complete/attached roster, log-backed admission observer, ready/non-pending closure index and exact log/roster identities.
- Sensor auth/capacity/index startup may proceed only when config, storage, clock and replay prerequisites are sound; sensor-only H_SENSOR_FAULT is allowed for bounded read-only initialization. Attendance and commit require H_OK; the pipeline rechecks admission at match commit, so a health fault during an outstanding scan drains the sensor response but does not append EV_SCAN.
- The owner rejects mismatched log/roster/config wiring. As608OwnerConfigRuntime binds the exact closure instance from the gate and refuses a mismatched closure before any config-slot write.
- Two test-first regressions reproduced config-mismatch startup and wrong-closure config-apply bypasses. Final focused command pio test -e native -f test_as608_pipeline passed 16/16, exit 0. The NEEDS_SETUP fixture constructs no storage/runtime services before boot reaches READY.
- The inert esp32-adapter-link smoke target gained link-only anchors for the new methods. pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 passed; final ELF inspection confirms required methods survived linker garbage collection. PlatformIO reports 67,892 B RAM (20.7%) and 1,010,340 B flash (77.1%). The smoke target was not flashed.
- C3 remains blocking: no production root, scheduler, RTC caller, attendance service or physical sensor-to-event path exists. E18/U5, C17 and C18 remain unresolved. Target compile/link does not prove mounted LittleFS durability, RTC correctness, sensor runtime or power-cut safety. C13.3, C1/C34 and C6 remain blocking.

## v0.2.91 (2026-11-11) — bind AP/PIN NVS access to validated config boot

- `Esp32NvsApCredentialStore` and `Esp32NvsPinAttemptStore` now require a non-owning reference to the authoritative `ConfigBootCoordinator`. Every public operation rejects `COLD`/`FAULT` before `nvs_open()` and requires the pre-setup Arduino NVS snapshot to be safe; only `READY`/`NEEDS_SETUP` is accepted.
- `Esp32PinAuthRuntime` binds its internal attempt store to the same coordinator and no longer accepts a different coordinator at `start()`. The inert smoke composition now constructs the config backend/coordinator before its NVS adapters.
- Test-first compile-time red reproduced both constructor bypasses. Green focused suites: `test_nvs_ap_credential` 17/17 and `test_nvs_config` 48/48 (65/65, exit 0).
- The ESP32 smoke `main.cpp` object compiled and bootloader/partition artifacts were generated. The build was stopped during third-party Adafruit GFX compilation before changed HAL TUs/final link; no ELF/bin exists. No flash or physical NVS test was performed.
- This closes accidental direct use through these adapter APIs before config boot; it is not a complete NVS firewall and does not replace production-root composition. C34 remains blocking. No NVS schema, signed rule, or delegated E12 choice changed.

## v0.2.90 (2026-11-11) — owner-to-log host integration coverage

- Adds one host integration regression that exercises `LittleFsStorageOwner` through its serialized `IFileSet`, `MonthSet`, and `LogStore`; appends one event, then constructs a fresh owner and verifies read-only startup validation and log replay over the same POSIX files.
- Focused command: `pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'` — **69/69 passed**, exit 0 (43 + 23 + 3). Source inventory: **726 C++ registrations across 39 suites**.
- This is host/POSIX evidence with a fake mount backend. It does not exercise ESP-IDF LittleFS, VFS error injection, free-space limits, persistence after power loss, or physical reboot behavior. No production firmware root was added; D1–D11 and all destructive-operation prohibitions remain unchanged. C13.3 is not complete.

## v0.2.89 (2026-11-11) — fail-closed scan metadata and enumeration

- Extends the existing native-only `LittleFsFileSetTestOps` callbacks into `scan_()` and `ensureLogDirectory_()`; target `ESP_PLATFORM` paths still call the real VFS/POSIX functions directly.
- Removes a fail-open double-stat path: root metadata failures now fail; `/log` is considered valid-empty only when its single metadata lookup returns `ENOENT`; non-directory `/log`, other errors, and directory enumeration/close failures fail closed.
- Adds six regressions for transient root and `/log` stat errors, month-file stat error, `opendir`, `readdir` after a real entry (no partial count publication), and `closedir` failure.
- `test_littlefs_fileset`: 43/43 passed. `test_littlefs_mount_owner`: 22/22 passed, exit 0. Total inventory: 725 C++ registrations / 39 suites; SF2 inventory remains 270 / 12 files and was not run.
- The changed `littlefs_file_set.cpp` compiled and `liblittlefs_file_set.a` archived with the pinned Xtensa toolchain. The full `esp32-adapter-link` build was stopped during unrelated HAL recompilation; no final ELF/link is claimed. No VFS runtime or physical-device test was run. See `RELEASE_v0.2.89.md` and `docs/AUDIT_v0.2.89.md`.

## v0.2.88 (2026-11-11) — validator metadata and directory fault injection

- Extends the native-only test seam in `validateRecordFiles()` with `statFn`, `openFn`, `fstatFn`, `opendirFn`, `readdirFn` and `closedirFn`. The callbacks wrap real POSIX file and directory handles; the ESP32 branch remains direct system calls.
- Adds seven fault regressions for metadata, open, descriptor metadata and directory enumeration/close errors. Byte-preservation assertions are present, but the seven tests were not executed. The v0.2.86 read-fault and v0.2.87 close-fault tests remain pending.
- Host build: 57/57 production translation units compiled; changed source and host-preprocessed `ESP_PLATFORM` branch passed `-Werror`; owning test translation unit compiled with a temporary Unity syntax shim; 32-object core/LittleFS host link passed. No Unity tests, PlatformIO target, ESP-IDF/Arduino build, ELF, VFS runtime or physical test was run.
- The injection hooks are scoped to `validateRecordFiles()` only; `scan_()` and ordinary IFileSet metadata/enumeration calls remain direct. C13.3/U110 remain open. Source inventory is 719 C++ registrations across 39 suites; `test_littlefs_fileset` has 37. See `RELEASE_v0.2.88.md` and `docs/AUDIT_v0.2.88.md`.

## v0.2.87 (2026-11-11) — startup validator close-fault injection

- Reuses the existing native-only `LittleFsFileSetTestOps::closeFn` hook when `validateRecordFiles()` closes its read-only month-file descriptor, including the descriptor-mismatch cleanup path. The `ESP_PLATFORM` branch continues to call `close()` directly; no target runtime behavior or public production API changes.
- Adds `test_record_validation_fails_closed_on_close_error_without_mutation`. Its injected callback closes the POSIX descriptor and then reports `EIO`; validation must return false while the canonical month-file size and bytes remain unchanged. This tests propagation of a close error, not a target descriptor leak or LittleFS close semantics.
- Test execution is deferred as requested. The v0.2.86 read-fault regression and this close-fault regression remain pending. Local host compilation passed for all 57 production library translation units; the changed storage TU also passed `-Werror`, and the 32-object core/LittleFS host link passed with a dummy main. No PlatformIO, Xtensa, ESP32 target, ELF, or physical test was available/performed. See `RELEASE_v0.2.87.md` and `docs/AUDIT_v0.2.87.md`.
- Source inventory is 712 active `RUN_TEST` registrations across 39 suites; `test_littlefs_fileset` has 30. This is registration inventory, not executed test evidence.

## v0.2.86 (2026-11-11) — startup validator read-fault injection

- Reuses the existing native-only `LittleFsFileSetTestOps::preadFn` hook in the read-only `ReadOnlyFileStorage` used by `validateRecordFiles()`. The ESP32 branch remains a direct `pread()` call; no firmware behavior or public production interface changes.
- Adds `test_record_validation_fails_closed_on_pread_error_without_mutation`: a valid one-record month file first exercises `EINTR` retry, then receives an injected `EIO`; validation must fail and the stored file size and record bytes must remain unchanged.
- Verification is pending by explicit instruction while RDC is unavailable. The test was not run; no red/green result, target build, ELF, VFS runtime or physical result is claimed. The modified source passed a host-only C++11 syntax check; see `RELEASE_v0.2.86.md` for the exact command and scope. Owning command when the host is available: `pio test -e native -f 'test_littlefs_fileset'`.
- Source inventory was reconciled from the archive: 710 active registrations existed across 39 suites before this change, although prior v0.2.85 documentation claimed 704; this adds one, for 711. See `RELEASE_v0.2.86.md` and `docs/AUDIT_v0.2.86.md`.

## v0.2.85 (2026-11-11) — unique LittleFS mount backend ownership

- Deletes copy and move operations on `Esp32LittleFsMountBackend`. Its `mounted_` state and partition label represent one ESP-IDF VFS registration; duplicating them could let one object unregister a mount still claimed by another.
- Added four compile-time type-trait regressions. Red reproduction failed all four assertions before the patch; the owning `test_littlefs_mount_owner` suite then passed 22/22, exit 0. The three C13.3 storage suites had passed 53/53 before this ownership change.
- The pinned ESP32 target compiled `target/esp32_adapter_link/main.cpp` and archived the complete `hal_esp32` library, including `esp32_littlefs_mount.cpp`. The full smoke build was manually stopped during Arduino framework compilation before final link; no ELF exists. It is incomplete, not a pass or compiler failure.
- No mount runtime, format/repair, power-cut, flash, hardware, full N1 or SF2 test was run. C13 remains blocking. See `RELEASE_v0.2.85.md` and `docs/AUDIT_v0.2.85.md`.

## v0.2.84 (2026-11-11) — shared I2C owner and read-only RTC probe

- Guards the ESP-IDF-only LittleFS mount backend with `ESP_PLATFORM`, keeping ESP-IDF VFS headers out of native HAL builds while preserving the real target implementation.
- Adds `Esp32I2cBusOwner`: one `TwoWire::begin()` owner, one shared mutex, a readiness-gated lock for OLED/DS3231/future EEPROM clients, idempotent success and terminal failure semantics.
- Integrates the owner into the hardware probe and inert smoke composition. The hardware probe now reads only DS3231 status/OSF after bus startup; it does not set time or clear OSF.
- Focused native suites passed 56/56: `test_i2c_bus_owner` 4/4, `test_ds3231` 36/36, `test_esp32_oled` 16/16. Exact command and scope are in `RELEASE_v0.2.84.md`.
- The pinned ESP32 smoke build was attempted but its remote process session ended before a final summary and no ELF was produced. It is **not verified**. The hardware-probe build, flash, and physical I2C/RTC tests were not run.
- No production root, RTC time policy, OSF-clearing policy, system-clock application or bus-recovery mechanism was added. See `docs/AUDIT_v0.2.84.md`; C3 remains blocking.

## v0.2.82 (2026-11-11) — LittleFS read-only startup owner and cross-month roster audit

- Adds a reusable no-format LittleFS mount owner and serialized IFileSet boundary. Storage remains hidden until structural validation and read-only record preflight pass; destructive recovery methods remain disabled.
- Extends LogStore and EventGroupAudit with an explicit first-sequence base. Monthly files are read in chronological order and checked for continuous global sequence, CRC/type validity, complete per-file commit groups and no group crossing a month boundary.
- Carries one heap-allocated RosterStore and RosterTransitionAudit across month-scoped logs. Startup rejects invalid roster transitions across files, invalid UTF-8 names, nonzero post-NUL padding, malformed SET groups and untrusted roster history. The configured UTC offset is explicit; P10 remains signed PHT (UTC+8).
- Test-first regression reproduced a cross-month duplicate deactivation accepted by the previous implementation. Final focused native results: monthset 23/23, littlefs_mount_owner 22/22, littlefs_fileset 28/28, event_group_audit 20/20, roster_snapshot 26/26 and littlefs_serialized_fileset 3/3 (122/122 across six separate runs). Exact commands and scope are in RELEASE_v0.2.82.md.
- Current inventory is 667 C++ registrations across 37 suites and 270 SF2 registrations across 12 files. Full native N1 and SF2 were not run; 545 C++ registrations were not run, and the historical 257/257 SF2 pass does not cover the later 13 registrations.
- The inert ESP32 adapter-link target built successfully; its exact ELF passed 12 NVS routing, 6 LittleFS symbol and 7 NVS/config/AP/PIN symbol checks. No target flash or VFS runtime was performed.
- No production root calls the storage owner. No mounted-target VFS, power-cut, hardware, format, repair, truncate, quarantine, journal or recovery transaction is added. The inert ESP32 adapter-link target must never be flashed.

## v0.2.78 (2026-11-10) — 30-minute scan lead and Present-only SF2 projection

- Changed the scan-lead default to 30 minutes and normalized stored schema-v1 values of 15 to 30; non-15 configured values remain configurable. The old schema cannot distinguish an intentional 15-minute setting from the former default, and a custom timetable whose scan windows would overlap can fail closed after normalization. See `RELEASE_v0.2.78.md`.
- Kept internal `ST_LATE` behavior. The SF2 projection maps `LATE` to `PRESENT` and no longer emits late-session/tardy counters or fields. Focused Node suites passed 57/57; the targeted C++ suites were not run because `pio` was unavailable (exit 127).
- Pinned the 2026-11-10 uploaded SHSF-2 workbook and documented its 26 `In` markers, four-row capacity, calendar alignment, summary-calculation gaps, Tardy-legend conflict, and movement/NLS limitations in `docs/sf2/UPLOAD_TEMPLATE_AUDIT_2026-11-10.md`.
- No XLSX writer, production snapshot API, identity/PII persistence, O8 amendment, companion recovery workflow, or authoritative SF2 export was added. `finalExportReady` remains false. No target build or physical hardware verification was performed.

## v0.2.77 (2026-11-10) — read-only fail-closed log startup

- Changed `LogStore::open()` so startup never truncates or discards an anomalous suffix. Partial record bytes, invalid complete records, sequence errors and unfinished `FL_MORE` groups cause failure; the original storage bytes remain unchanged and no valid-looking prefix is replayed to observers.
- If a storage read/decode/sequence check fails during the second replay pass after some records have reached observers, the store resets its internal replay counters and calls `onReset()` on observers again before returning failure. This prevents partially rebuilt roster/index/coverage projections from remaining visible.
- Added/updated tests in `test_monthset`, `test_coreb`, `test_coreb2` and `test_event_group_audit`; `test_restore_replay` verifies the staging replay consumer. Detailed commands and results are in `docs/TESTS.md` and `RELEASE_v0.2.77.md`.
- Recorded the delegated D1–D11 engineering choices in `docs/SPECS/littlefs_recovery_protocol.md`. These are engineering decisions, not signed product rules. Signed O8 and the delegated month_set status are preserved.
- No destructive recovery, truncate, quarantine, format, journal, evidence export, ordinary backup-scope change, or on-device mount behavior was added. `MonthSet::repair()` remains non-transactional and must not be called by production.
- The target build and exact ELF inspection are recorded in `RELEASE_v0.2.77.md`. This is a development snapshot, not production firmware. No flash or physical-device test was performed.

## v0.2.76 (2026-11-10) — fail-closed AP/PIN NVS adapter startup boundary

- Added an `Esp32NvsStartupGuard::status()` check to every public operation of `Esp32NvsApCredentialStore` and `Esp32NvsPinAttemptStore`. Unsafe snapshots now fail before `nvs_open()`; startup errors are preserved where available, otherwise `ESP_FAIL` is used.
- Added regression coverage proving AP credential reads/creation and PIN attempt load/initialize/persist cannot reach NVS with an unsafe startup snapshot. AP read output is cleared; PIN output remains unchanged.
- Test-first red runs reproduced both bypasses. Final focused suites passed: `test_nvs_ap_credential` 16/16 and `test_nvs_config` 47/47. PlatformIO's red runner emitted SIGHUP after each intentional Unity failure; neither red run is represented as a normal completed test exit.
- The adapter checks do not prove `ConfigBootCoordinator` has run. The production root must still enforce successful config boot before any AP/PIN store or other application storage owner is used. The NVS erase wrapper remains narrow; C34 remains blocking.
- The inert ESP32 adapter-link target built successfully. Its first ELF check found the non-virtual PIN initializer had been garbage-collected by the original smoke anchor; `target/esp32_adapter_link/main.cpp` was corrected to force a live, non-invoking pointer read. The incremental rebuild and final ELF verifier then passed 12 NVS wrapper routes, 7 NVS symbol checks and 9 LittleFsFileSet symbol checks. Exact hashes/resources/logs are in `RELEASE_v0.2.76.md` and `docs/AUDIT_v0.2.76.md`.
- No flash, runtime NVS, physical hardware or destructive operation was performed.

## v0.2.75 (2026-11-10) — fail-closed NVS config boot gate

- Updated Esp32NvsConfigBackend::begin() to consume the Arduino pre-setup Esp32NvsStartupGuard snapshot. It refuses readiness unless exactly one successful initialization occurred and no whole-partition erase attempt was blocked.
- Removed the backend's redundant nvs_flash_init() call. ConfigBootCoordinator already latches CFG_BOOT_FAULT before backend initialization, so its owning AP setup and PIN runtime flows cannot proceed after an unsafe config startup.
- Added two native regressions for ambiguous repeated initialization and successful reuse of Arduino's initialized NVS driver. The corrected red run reproduced four intended failures; the first red attempt also exposed and corrected a fake-fixture initialization leak.
- Focused verification: test_nvs_config 46/46 and test_nvs_startup_guard_policy 9/9, both final runs exit 0. The red PlatformIO wrapper emitted SIGILL after its expected failure summary; it is not treated as a green or normal red completion.
- The inert esp32-adapter-link target build succeeded (2,086.96 s); RAM 67,356 B (20.6%), application flash 935,040 B (71.3%). Final ELF passed 12/12 NVS wrapper-routing, retained Esp32NvsConfigBackend::begin() and Esp32NvsStartupGuard::status(), and retained 9/9 LittleFsFileSet methods.
- Added a v0.2.75 verifier with the updated 630/35 C++ inventory and ELF symbol checks. No full native suite, SF2 test, hardware probe, flash, NVS runtime, LittleFS runtime, or physical-device test was run. C34 remains blocking because there is no production root and the erase guard remains narrow.

## v0.2.74 (2026-11-10) — C34 NVS erase-surface and pinned LittleFS configuration audit (documentation-only)

- Audited the installed Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 package headers and selected package sdkconfig.
- Confirmed the shared NVS guard blocks only one exact whole-DATA_NVS `esp_partition_erase_range()` request. Smaller/chunked ranges, DATA_NVS_KEYS, and direct lower-level `esp_flash_erase_region()` / `esp_flash_erase_chip()` paths are not generally covered.
- Re-inspected the prior v0.2.72 inert adapter-link ELF (SHA-256 `bfbb50a538582d53d101a80ad0e01b2dcefc7e7d4f1e84f61bfe012af0b8154b`): the 12/12 NVS wrapper-routing and 9/9 LittleFsFileSet symbol-retention checks pass. The only observed call site for raw `esp_flash_erase_region()` in that ELF is `esp_partition_erase_range()`; no direct app call was observed. `esp_flash_erase_chip()` is not retained in that image.
- Confirmed NVS erase APIs deinitialize initialized handles before attempting the erase; a blocked runtime erase can still leave NVS offline.
- Corrected C13 package-level configuration: `CONFIG_VFS_SUPPORT_DIR=y` and LittleFS geometry settings are present in the installed ESP32 libs package sdkconfig; `ESP_LITTLEFS_ENABLE_FTRUNCATE` is exposed by the pinned header. This does not establish runtime ftruncate or LittleFS durability.
- Updated C34/U103 and the C13 recovery proposal with explicit evidence limits. No source, tests, API, persistent format, or runtime behavior changed. No PIO suite, target rebuild, SF2 test, flash, or physical-device test was run.

## v0.2.73 (2026-11-10) — C13.2 recovery protocol proposal (documentation-only)

- Added docs/SPECS/littlefs_recovery_protocol.md as a proposal only, explicitly NOT APPROVED FOR IMPLEMENTATION.
- Audited the current LogStore::open() suffix-truncation behavior: a complete invalid EOF record can be treated as a torn suffix when the IStorage backend permits truncation, although it may be previously committed data corruption.
- Recorded that MonthSet::repair() is a quarantine-copy / later-file-move / truncate sequence with no durable journal, commit point, byte-verification contract, or restart protocol. Existing fake tests do not prove interruption safety.
- Defined proposed inspection classifications, recovery invariants, a phased transaction concept, restart states, interruption matrix, target/physical verification plan, and D1–D11 open decisions. No commit primitive or journal format was selected.
- Updated docs/SPECS/month_set.md while preserving its DELEGATED status. No source code, test registration, API, event format, persistent format, or runtime behavior changed. Destructive methods remain fail-closed.
- Documentation-only: no PIO, SF2, target build, ELF inspection, flash, or physical-device test was run.

## v0.2.72 (2026-11-10) — host-only LittleFsFileSet append fault injection

- Added a native-only, per-instance syscall callback seam for LittleFsFileSet append write, fsync, verification pread and close operations. The ESP_PLATFORM branch retains the original direct syscalls; no ESP32 product API or persistent format changed.
- Added seven test_littlefs_fileset regressions for short writes, EINTR, a partial prefix followed by hard write failure, fsync failure, read-back I/O failure, read-back mismatch and close error.
- The tests explicitly demonstrate indeterminate outcomes: false can be returned after a partial or complete append is visible. The close fault callback closes the POSIX descriptor then reports synthetic EIO; it does not model target descriptor leakage.
- Owning host suite passed 27/27. The red phase showed all seven new assertions failing before callback wiring; PlatformIO wrapper emitted SIGBUS after the intentionally nonzero test exit, while the direct binary isolated 27 tests / 7 failures / 20 passes and exited normally with status 7. Final green PlatformIO run completed 27/27, exit 0.
- ESP32 adapter-link rebuild succeeded (1,769.31 s; Espressif32 8.0.0, Arduino-ESP32/ESP-IDF libs 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121); RAM 67,356 B (20.6%), application flash 934,976 B (71.3%). Exact ELF inspection passed 12/12 NVS wrapper route checks and retained all nine LittleFsFileSet methods. No mounted LittleFS runtime, target VFS fault injection, destructive recovery, power cut, physical device, full native or SF2 run was performed.
- Added the approved roadmap and read-first working-memory map. No signed or delegated product decision changed.

## v0.2.71 (2026-11-10) — LittleFsFileSet ESP32 compile/link compatibility

- Added LittleFsFileSet to the inert ESP32 adapter smoke composition and retained all nine public methods through final ELF linking.
- Fixed the target compile blocker: pinned ESP-IDF/newlib does not declare lstat(). The adapter now uses stat() under ESP_PLATFORM; host POSIX builds retain lstat() and symlink-rejection behavior. No target filesystem operation was added to the smoke target.
- test_littlefs_fileset passed 20/20. The final esp32-adapter-link target build passed with 67,356 B RAM (20.6%) and 934,976 B app flash (71.3%).
- Final-ELF verification passed 12 NVS wrapper-routing checks and 9 LittleFsFileSet symbol-retention checks. This is static linked-image evidence only; C13 remains blocking because no mounted-target VFS fault injection, truncate/quarantine recovery, or power-cut test exists.
- No full native/SF2 suite, hardware-probe build, flash or physical-device test was performed.

## v0.2.70 (2026-11-10) — repair ESP32 smoke-link anchors and measure roster-audit BSS

- Updated the inert ESP32 smoke application's stale AS608 member-function anchors to match the current const public API; removed anchors to private friend-only methods without weakening production access controls.
- Retained a `RosterTransitionAudit` instance in the smoke target. The exact Xtensa ELF measures the object at 3,628 bytes in BSS.
- Pinned Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 smoke build succeeded: RAM 67,308/327,680 bytes (20.5%); application flash 927,120/1,310,720 bytes (70.7%).
- Direct final-ELF NVS wrapper inspection passed 12/12 checks. Fixed `tools/inspect_probe_wrappers.sh` mode from 0640 to 0755 because the existing links workflow executes it directly.
- No native suite was rerun; the v0.2.69 43/43 roster result is historical. The other 578 of 621 registrations remain unrun. No hardware probe, flash or physical test was performed.

## v0.2.69 (2026-11-10) — separate roster chronology from lifecycle boundaries

- Fixed a false rejection in `RosterTransitionAudit`: an existing-ID `RA_SET` is a name revision, not a lifecycle transition, and must not advance the last lifecycle boundary.
- The audit now tracks both the latest effective day across all roster events and the last lifecycle boundary. Lifecycle actions remain strictly later than the previous lifecycle boundary and cannot move backwards relative to any later event.
- Added regressions for same-day rename plus deactivate/reactivate, backdated transition rejection, preservation of the E27 same-day lifecycle guard, and all currently defined audit rejection codes.
- Final focused verification passed 43/43 across `test_roster_snapshot` (26) and `test_month_roster` (17). The other 578 of 621 C++ registrations were not run. No event format, operational `RosterStore` behavior, delegated decision, target or hardware behavior changed.

## v0.2.66 (2026-11-09) — checked API input/output boundaries

- Replaced reqFingerprint()'s unconditional uint32 return with a bool result plus output reference. Null input with nonzero length now fails without writing a digest; zero-length null input remains valid. This avoids an invalid-input sentinel colliding with a legitimate fingerprint.
- API event builders reject null output buffers with A_INVALID/R_BAD_FIELD and leave count zero.
- Appended RE_BAD_OUTPUT to RosterErr without renumbering existing values. All three RosterStore plan methods reject null output buffers with count zero; API wrappers map the new result explicitly.
- RosterStore::nameOf() returns zero without touching memory when the output destination is null.
- Final focused verification passed 77/77 across test_api (39), test_roster (15), test_roster_history (7) and test_month_roster (16). The other 532 of 609 registrations were not run. No persistence format or product decision changed.

## v0.2.65 (2026-11-09) — null-safe token API boundaries

- `TokenTable::check(nullptr, nowS)` now returns false without refreshing session timers; `revoke(nullptr)` is a no-op.
- `ctEqual()` now returns false for a null operand with nonzero length and true for zero-length comparisons, preserving the previous zero-length result.
- Added a regression that previously terminated `test_api` with SIGSEGV; it now verifies both live sessions survive null operations and that LRU eviction still removes the least-recently-used session.
- Final focused verification passed 49/49 across `test_api` (38) and `test_pin_auth_session` (11). The other 558 of 607 C++ registrations were not run. No persistence format, product decision, target or hardware behavior changed.

## v0.2.64 (2026-11-09) — wrap-safe PIN lockout deadline state

- Fixed a lockout bypass when uint32 seconds arithmetic produced an exact deadline of zero, which the old implementation also used as the unlocked sentinel.
- Added an independent locked_ state flag. Boot restoration, failure deadlines, exact-expiry handling and successful reset now distinguish an active lock from the deadline's numeric value.
- Added a red-to-green regression for both a fifth-failure lock and reboot restoration when the deadline wraps to zero. test_api passed 37/37; test_pin_auth_session passed 11/11. The other 558 of 606 C++ registrations were not run.
- Persisted failure counter format and 60/120/240/300-second policy are unchanged. No production caller, target build or hardware evidence was added; C6/U80 remain open.

## v0.2.63 (2026-11-09) — overflow-safe GAP encoding and all-or-reject preflight

- Replaced uint32 (remaining + 59) / 60 rounding with division/remainder and bounded emitted GAP chunks to the remaining representable epoch domain.
- Added uint64 gapSpanEncodable() and require a full-span preflight before appendGaps() or applyClockSet() writes. Unrepresentable rounded endpoints are rejected without a GAP prefix or CLOCK_SET mutation.
- Added regression cases covering full-range/near-ceiling chunk conversion and zero-write rejection by both callers. The final dependent batch test_coreb2, test_coreb, test_hardening, test_restore_replay passed **91/91**, exit 0.
- Cumulative current focused verification: **246/246 selected registrations across 12 suites** after the final rerun; **359/605 registrations were not run**. No full native, SF2, target or hardware test.

## v0.2.62 (2026-11-09) — explicit clock commit status; includes roster-history WIP completion

- Replaced bool applyClockSet() with ClockApplyResult and enforced CA_APPLY, source validity and ordered GAP bounds at the write boundary.
- Distinguishes rejected/no-write, pre-faulted log/no-write, committed, and uncertain write outcomes. Host regressions cover before-write failure, complete-write-then-false replay, and a long-gap prefix persisted without its final CLOCK_SET pair.
- Clock and restore focused batch: test_coreb + test_restore_replay, 58/58; test_hardening, 15/15. The final clock suite passes 43/43.
- The current 601-registration inventory has 242 selected registrations passing across 12 affected suites; 359 were not run. See RELEASE_v0.2.62.md and docs/AUDIT_v0.2.62.md.
- No persistent event format change, target build, production clock caller, full native/SF2 run or hardware test.

## v0.2.61 (2026-11-09) — roster-history integrity gates

- Completed the carried roster-history WIP: RosterStore historyComplete() now includes pending RA_SET groups; roster planning, API marks/enrollment, enrollment FSM, attendance pipeline and closure snapshots fail closed on untrusted history.
- Added state-based retry no-op behavior for complete roster writes reported failed and subsequently observed after reopen/replay. This does not add durable request identity.
- Strengthened tests to exercise a pending RA_SET header with one payload and bad_ == 0. Targeted WIP batches passed; see docs/AUDIT_v0.2.61.md.
- U61 semantic lifecycle audit, C3 production composition and C13 partial-write recovery remain unresolved.

## v0.2.60 (2026-11-09) — explicit uncertain attendance commit result

- Added `ATT_PIPE_COMMIT_UNCERTAIN` for a false `LogStore::append()` result. The attendance pipeline no longer claims the event definitely was not saved; `ATT_PIPE_NOT_SAVED` is retained for pre-append failures.
- Added a host fault-injection regression proving a fully persisted EV_SCAN returned as failed is replayed after reopen and a repeated student/session scan is rejected without another append.
- Exact command: `pio test -e native -f 'test_as608_pipeline' -f 'test_attendance_admission'` — **24/24 passed**, exit 0. The other 561 of 585 registrations were not run.
- Partial-write repair remains blocked by C13; request idempotency for clock, roster, GAP and future API writers remains unresolved (U112). No production caller, target build or device test.

## v0.2.59 (2026-11-09) — coalesced GAP coverage index

- Fixed U113 in `LogIndex`: overlapping and adjacent GAP intervals are merged into a disjoint coverage union, preventing indeterminate append retries and adjacent chunks from consuming multiple `MAX_GAPS` slots.
- Invalid zero-length or epoch-overflowing GAP spans now fail closed by setting `gapOverflow_`; an end exactly at `UINT32_MAX` remains representable.
- Added regressions for a complete GAP record reported failed, reopen and 512 retries, transitive overlap/adjacency merging, malformed bounds, genuine disjoint-span overflow and downstream ClosureEngine coverage. Original GAP records and the persistent format remain unchanged.
- `pio test -e native -f 'test_coreb2' -f 'test_restore_replay'`: **29/29 passed**, exit 0. The other 555 of 584 registrations were not run. No target, physical-device, power-cut or full-regression test.
- Residual: `appendGaps()` still is not exactly-once; duplicate records consume log/storage capacity, and request idempotency remains U112.

## v0.2.58 (2026-11-09) — explicit append-failure semantics and recovery tests

- Added four test_monthset regressions for partial writes, in-session fault latching/no retry, incomplete commit-group rejection when tail repair is unavailable, and a complete record that remains visible after the backend reports failure.
- Clarified the IFileSet, IStorage and LogStore contracts: false append status is indeterminate and may follow partial or complete storage mutation. The existing LogStore implementation already latches the fault and suppresses observer notification; no runtime behavior changed.
- Targeted test_monthset passed **18/18**, exit 0. Inventory is **581 C++ registrations across 35 suites**; **563 were not run** in this increment.
- Static review corrected the appendGaps retry comment and recorded U113: duplicate GAP retries consume the non-deduplicating 512-entry LogIndex capacity. No behavior change or retry/overflow regression was added.
- No actual VFS fault injection, ESP32 target build, production request idempotency, truncate/quarantine, physical device, power-cut or SF2 test was performed. C3/C13 remain blocking. Destructive recovery semantics remain DELEGATED and unchanged.

## v0.2.57 (2026-11-09) — non-destructive LittleFS VFS adapter subset

- Added `LittleFsFileSet` as a dedicated library over the mounted POSIX VFS path. It implements checked month-file enumeration/metadata, ranged reads, aligned append with `fsync()` and read-back verification, and canonical empty-month creation. It rejects malformed entries and symlinked month files.
- Destructive recovery methods (`truncateFile`, `quarantineCopy`, `quarantineFrom`) explicitly refuse operations. The caller owns mount lifetime and must serialize all access; there is no internal mutex or production composition root.
- Added 20 native adapter cases, including real `MonthSet`/`LogStore` rollover and reopen on a host filesystem and fail-closed torn-tail behavior. Targeted `test_littlefs_fileset` + `test_monthset` passed **34/34**, exit 0.
- Inventory is **577 C++ registrations across 35 suites**; **543 were not run** in this increment. Host filesystem tests do not prove ESP32 LittleFS behavior. No target build, physical mount, power-cut, full native or SF2 run. C3/C13 remain blocking.

## v0.2.56 (2026-11-09) — mandatory checked storage metadata

- Replaced value-only `IStorage::size()` and `IFileSet` metadata contracts with mandatory checked virtual methods. Every storage/file-set implementation must now report metadata-query failure explicitly; only the base `IStorage::size()` convenience helper returns `SIZE_MAX` on failure.
- Updated `MonthSet` metadata, range and rollover operations to use checked results, detect size arithmetic overflow, and verify a newly created month entry before appending.
- Updated `LogStore::open()` and `RestoreReplayAudit` to propagate size-query failures distinctly and fail closed. Added a restore replay status for stage-size query failure.
- Added regressions for file-set count/key/size failures and restore staging-size failure. The initial month-set test-first run failed all four new cases; final focused verification passed **264/264 across 15 affected suites**.
- Inventory is 557 C++ registrations across 34 suites; 293 were not run. No LittleFS driver, production firmware, target build, ELF inspection, flash, hardware, or power-cut verification was added. C3/C13 remain blocking. No signed or delegated decision changed.

## v0.2.51 (2026-11-09) — attendance scan-to-log integration tests

- Added a six-case native integration suite exercising completed sensor matches through As608Runtime, the injected page resolver, roster/admission rules and durable EV_SCAN writes. It covers Present, Late, duplicate, unresolved mapping, append failure and no-finger paths.
- Added tools/verify_v0.2.51.sh; targeted verification passed 108/108 across seven suites and inspector fixtures passed 11/11.
- Restored executable modes required by the inspector fixture harness and release verifier.
- No production runtime, product decisions, E18 mapping, SF2 source, ESP32 target or hardware behavior changed.

## v0.2.50 (2026-11-09) — AS608 target composition and verification hardening

- Added an ESP32-only compile guard around the IDF-specific NVS wrapper implementation. The platform-independent policy remains available to native tests.
- Expanded the inert adapter smoke target to compile/link the raw AS608 transport, occupancy reader, attendance client, enrollment session/FSM and single-owner runtime. It constructs in setup() after Arduino initialization and performs no sensor operation.
- Corrected the Xtensa ELF inspector to inspect internal branch targets; added a branch-target fixture and expanded positive/negative route coverage.
- Added a targeted verification workflow with source fingerprinting. Each target ELF is inspected immediately after its build because the two PlatformIO configs share .pio/build and the later build can replace the earlier output.
- Five affected native suites passed 90/90; inspector fixtures passed 11/11. Both ESP32 targets compiled/linked and both final ELFs passed NVS wrapper route inspection.
- No hardware was flashed or tested. C3/C34 remain blockers; no product decision or signed handoff rule changed.

## v0.2.49 (2026-11-09) — AS608 target link coverage and NVS ELF verification

- Extended the inert ESP32 adapter target with link references to the AS608 Stream bridge, raw adapter, shared runtime, attendance client, occupancy reader, enrollment session and FSM. Construction is function-local static in setup() after Arduino initialization; no UART command or sensor action is called.
- Guarded the ESP-IDF-only NVS wrapper implementation with ARDUINO_ARCH_ESP32; the pure policy remains native-testable.
- Fixed the ELF inspector's Xtensa branch-target false negative by following internal labels inside wrapper functions. Added 11 positive/negative fixtures; all passed.
- Targeted native verification: 90/90 across AS608, enrollment, NVS-policy and probe suites. The other 443 of 533 registrations were not run.
- Both ESP32 targets compiled and linked. Adapter smoke: 63,652 B RAM / 926,248 B flash. Hardware probe: 46,764 B RAM / 928,880 B flash.
- Final-ELF NVS routing passed for both images. This is call-routing evidence only, not NVS runtime/power-loss or hardware proof.
- Added the targeted/links/full verification script and documented the need to preserve the first ELF because the PlatformIO configurations reuse the same build directory. No device was flashed; no product decision changed.

## v0.2.50 (2026-11-09) — AS608 target composition and verification hardening
- Added an ESP32-only compile guard around the IDF-specific NVS wrapper implementation. The pure policy header remains available to native tests.
- Expanded the inert ESP32 adapter smoke target to compile/link the AS608 Stream transport, raw adapter, occupancy, enrollment FSM/session, attendance client and single-owner runtime. It does not start Serial2 or call sensor methods.
- Corrected Xtensa ELF inspection to re-disassemble internal branch targets; added regression coverage for a delegate reached through a branch.
- Fixed verification order: inspect each final ELF immediately after its build because both PlatformIO configs share `.pio/build`.
- Added incremental targeted/links/full verification modes, source-fingerprint gating, and phone/Desktop Commander workflow instructions.
- Targeted native suites passed 90/90; inspector fixtures passed 11/11. Both ESP32 target builds succeeded and both final ELFs passed NVS wrapper routing inspection. No flash or device test was performed.
- No signed/delegated decisions, handoff rules, hardware mapping or production behavior changed.

## v0.2.48 (2026-11-09) — shared NVS startup guard and route audit

- Moved the NVS startup wrappers from the diagnostic sketch into `lib/hal_esp32/src/esp32_nvs_startup_guard.*`; both ESP32 target configurations now link the same guard.
- Added a pure policy header and nine host-test registrations, including fail-closed handling for zero or multiple pre-setup NVS-init calls. The current source inventory is 533 registrations across 33 suites; the 13 new probe/policy registrations remain unrun.
- Added link-only references to the standard ESP-IDF NVS bulk-erase APIs in both target images. The final-ELF inspector now checks Arduino startup routing, wrapper delegation, and the `nvs_flash_erase()`, named-partition and partition-pointer erase call chains.
- Added `tools/verify_v0.2.48.sh` to batch native tests, both ESP32 target builds and final-ELF route inspection. It never uploads or flashes.
- No v0.2.48 compile, PlatformIO test, target build, ELF inspection or device test was performed here. The user-provided 520/520 result and smoke link apply to v0.2.46 only. C34 remains blocking until the verification gate passes and a production boot owner adopts the guard.
- Pinned IDF 5.5.5 source confirms that the standard NVS erase APIs delegate to `esp_partition_erase_range`; the guard remains incomplete because smaller range erases and alternate/direct flash routes are not blocked. The standard APIs may deinitialize open handles before their wrapped erase call is rejected; they are not a recovery workflow.

## v0.2.47 (2026-11-08) — isolated hardware probe and NVS erase guard

- Added a separate `esp32-hardware-probe` target for shared I2C ACK scan, OLED credential rendering/clear request, ephemeral WPA2 AP configuration verification and AP-netif DHCP-event reporting. It is diagnostic-only and is not production firmware.
- Added an isolated helper test suite with eight registrations for 128-bit-to-hex passphrase encoding/output clearing and exact whole-DATA_NVS erase-guard predicate behavior. The total source inventory is 528 registrations across 32 suites; the eight new tests have not been run.
- Added a probe-only linker wrapper around `nvs_flash_init()` and `esp_partition_erase_range()` in response to C34. It is intended to block Arduino-ESP32 3.3.12's whole-default-NVS erase fallback before `setup()` while permitting normal sector erases. Added `tools/inspect_probe_wrappers.sh`, which checks the final ELF's `initArduino()` call targets and each wrapper's call to the real IDF function. Target linker routing and device behavior remain unverified; do not flash until the build and final ELF inspection pass and are reviewed. This is not a production fix.
- Probe password is temporary and emitted over USB serial as fallback as well as shown on OLED. Treat captured serial logs as secret; do not publish or share the unredacted `PROBE_PASSWORD` line. Do not reuse the password.
- No existing core module, sensor path, SF2 code, signed handoff rule or delegated product choice changed. The probe does not mount attendance storage, run attendance scans, start HTTP or write application config/attendance namespaces. Wi-Fi PHY may use NVS namespace `phy`.
- User-provided v0.2.46 verification remains 520/520 native cases and successful inert adapter smoke compile/link. No v0.2.47 tests or target builds have been executed by the assistant.

## v0.2.46 (2026-11-08) — fixed first-provision AP identity and conservative credential policy

- Applied E29: production first-provision startup now uses fixed SSID `att`. E31 delegates channel 1 as the fixed engineering default. `ApCredentialSetupCoordinator::start()` no longer accepts caller-supplied SSID or channel, preventing accidental per-device identity leakage or configuration drift. Channel 1 is fixed under delegated E31; regulatory acceptance remains unverified. The isolated `Esp32WifiAp` adapter remains parameterized for tests/other explicitly scoped uses.
- Updated the OLED first-provision screen label from `Wi-Fi setup` to `Wi-Fi: att`, so the admin sees the fixed network name beside the password. Updated the OLED rendering assertion and write-count expectation. Added regression coverage asserting the coordinator starts exactly `att` on channel 1. Updated the target smoke member-function pointer for the new no-argument `start()` API. The suite registration count is unchanged.
- Applied delegated E30 judgment conservatively: no remote passphrase rotation, reset endpoint, boot/touch reset gesture, or automatic NVS erase/overwrite. Credential loss, mismatch, or corruption remains fail-closed. A physically supervised USB service recovery procedure is a future requirement and is not implemented; no attendance/config data may be erased as an AP recovery side effect. Rotation must wait for an atomic transaction spanning the NVS credential and authoritative Config slot.
- No production boot/HTTP integration was added. C6 remains blocking: six-digit setup-code verification, request/session-bound authorization construction, PIN KDF/verifier/writer, protected routes, and production composition are still absent.
- Verification status: no compiler or PlatformIO run was available in this environment. Full v0.2.46 native suite and ESP32 smoke target build are required before this archive is accepted. Hardware behavior remains unverified.

## v0.2.45 (2026-11-08) — ordered first-provision coordinator and DHCP assignment gate

- Added hardware-independent `ApCredentialSetupCoordinator`: validates caller SSID/channel before reading or disclosing the credential; requests the E28 OLED frame before starting the WPA2 SoftAP; requires AP readiness, this AP netif's DHCP assignment and an authorization gate supplied to the current `provision()` call; then provisions the configuration and requests a blank frame.
- Split the orchestration ports into `ap_credential_display.h` and `credential_soft_ap.h`, so the OLED and Wi-Fi HAL adapters implement narrow interfaces without including the complete setup coordinator header.
- Fixed a credential-consistency hazard: the coordinator re-reads the durable bootstrap secret and replaces only `Config::wifiPass` in a private config copy before first provision. A caller-supplied different passphrase can no longer be committed beside the generated AP credential.
- Authorization is now per-call rather than retained in a long-lived coordinator member. A short-lived composite gate passes the same request gate into `ConfigBootCoordinator::provision()`. The production caller must bind it to the current authenticated request/session; no HTTP route exists yet.
- Extended `Esp32WifiAp` with `IP_EVENT_AP_STAIPASSIGNED` observation, filtered by the exact AP netif. It stores only a latched assignment bit, not client MAC/IP data. This bit is historical for the AP run, not proof of a currently connected or authenticated station.
- Added 18 coordinator tests; the AP suite increases from 20 to 22 registrations. Under local ASan/UBSan with a temporary Unity-compatible stand-in: core **18/18 tests, 145 assertions**; AP **22/22 tests, 315 assertions**; unchanged OLED suite **13/13 tests, 67 assertions**. This is not PlatformIO Unity execution.
- Current inventory: **520 C++ registrations across 31 suites**. The user reports all **500/500** v0.2.44 native cases passed. The 20 added v0.2.45 registrations require PlatformIO verification.
- The user reports v0.2.44 expanded target smoke succeeded with Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. v0.2.45 target rebuild is pending. No hardware run was performed. SF2 source is unchanged; its 257/257 Node suite was rerun locally in this increment and all 25 `.mjs` files passed `node --check`.
- E28 unchanged (SIGNED). E29 SSID naming and E30 credential reset/rotation/recovery remain OPEN. No production boot, setup-code verifier, HTTP route, PIN verifier/KDF or request/session integration is claimed.

## v0.2.43 (2026-11-08) — pinned ESP32 adapter link-smoke project

- Added `platformio.esp32-smoke.ini`, isolated from the native `platformio.ini`, pinning Espressif32 platform 8.0.0, board `esp32dev`, and the Arduino framework. The platform release documents Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5; exact resolved package revisions must be recorded from the target build output.
- Added a compile/link-only smoke application that retains references to the ESP32 AP lifecycle, RF-gated RNG, pre-radio bootstrap RNG, NVS AP credential store, NVS config backend and NVS PIN-attempt store methods. It intentionally performs no AP start, entropy read, NVS initialization or storage write.
- The smoke translation unit passes local `g++ -std=gnu++11 -Wall -Wextra -Werror -fsyntax-only` against existing test stubs. This is not target compilation or linking; the user's PlatformIO target run remains pending.
- The user reports all 29 v0.2.42 PlatformIO native suites passed. The 487 C++ registration inventory is unchanged. SF2 independently reran 257/257 Node tests; all 25 `.mjs` files pass syntax checks.

## v0.2.42 (2026-11-08) — AP test executable link fix

- Fixed the `test_esp32_wifi_ap` native test executable's unresolved `esp_fill_random` symbol. Each PlatformIO test suite is a separate executable, so the fake defined by `test_esp32_secure_random` was not shared with the AP suite.
- Added a deterministic test-local `esp_fill_random` fake and extended the existing integration case to check RNG refusal before AP start, successful fill only after AP_START, and refusal again after stop.
- No production source or test registration count changed. Current inventory remains 487 C++ registrations across 29 suites.
- Local sanitizer-enabled g++ stand-in: AP suite 20/20 tests, 267 assertions, 0 failures. Leak detection was disabled for the existing stop-failure test, which intentionally retains callback context until reboot. PlatformIO rerun pending; no ESP32 target build or hardware test.
- Post-release verification: the user subsequently reports all 29 PlatformIO native suites passed in v0.2.42, including the corrected AP suite. This does not establish an ESP32 target build or device behavior.

## v0.2.41 (2026-11-08) — random first-provision AP credential bootstrap
- Added hardware-independent `ApCredentialBootstrap`: on `CFG_BOOT_NEEDS_SETUP`, obtains 16 random bytes, encodes 32 lower-case hex characters, persists create-only, verifies by fresh read-back and requests OLED disclosure on every boot that remains in setup. On `CFG_BOOT_READY`, validated `Config::wifiPass` remains authoritative; it backfills the separate store only when absent and fails closed on mismatch.
- Added `Esp32NvsApCredentialStore` using NVS namespace `attwifi` and key `ap_pass_v1`. It distinguishes missing keys from NVS faults, never overwrites a malformed/existing record, calls `nvs_commit()`, and provides the read-back that the core requires.
- Added `Esp32BootstrapRandom`, which temporarily enables the ESP-IDF SAR-ADC entropy source, fills the requested bytes with `esp_fill_random()`, then disables the source before Wi-Fi/ADC use. This is separate from the RF-gated RNG used for session tokens.
- E28 is now SIGNED for the source and disclosure design. E29 production SSID naming and E30 passphrase rotation/reset/recovery remain OPEN. Production boot/OLED/AP wiring is not implemented.
- Added three suites with 36 registrations (18 core, 15 NVS, 3 RNG). All 36 passed under a sanitizer-enabled g++ stand-in Unity harness; no PlatformIO run or ESP32 build was run. Current inventory: 487 registrations/29 suites; 74 post-v0.2.36 registrations remain without PlatformIO execution.
- Reran unchanged SF2 suite: 257/257 passed; all 25 `.mjs` syntax checks passed.

## v0.2.40 (2026-11-08) — checked SoftAP netif teardown
- Replaced the void `esp_netif_destroy_default_wifi()` wrapper with explicit `esp_wifi_clear_default_wifi_driver_and_handlers()` result checking followed by `esp_netif_destroy()` only on success. A failed clear now retains the netif and latches a cleanup fault.
- Strengthened the existing `test_esp32_wifi_ap` teardown assertions for deinit/clear/destroy ordering and no destroy after a clear failure. No new test registration was added.
- Corrected source accounting: the AP suite has 20 registrations, so current inventory is 451 across 26 suites; 38 registrations were added after the v0.2.36 baseline. Earlier v0.2.39 accounting of 450/19/37 was incorrect.
- Supplemental local C++ run: AP suite 20/20 and RF-gated RNG suite 5/5 compiled/executed with `g++ -std=c++11 -Wall -Wextra -Werror` and a temporary Unity-compatible stand-in harness. This is not PlatformIO/Unity verification; 13 token-session/API registrations remain unexecuted here, and all 38 post-baseline registrations still require PlatformIO.
- Re-ran unchanged SF2 browser model from this v0.2.40 working tree: **257/257 Node tests passed**, and all 25 `.mjs` modules passed `node --check`.
- No ESP32 target build or hardware test was run. E28/E29 remain OPEN; C6 remains blocking.

## v0.2.39 (2026-11-08) — direct ESP-IDF SoftAP lifecycle adapter
- Added `Esp32WifiAp` as the intended sole ESP-IDF SoftAP lifecycle owner and caller of `Esp32SecureRandom::notifyRfStarted/Stopped()`.
- Installs caller-supplied SSID/passphrase and explicit WPA2-PSK/CCMP config in `wifi_config_t` before `esp_wifi_start()`. The RNG gate opens only after `WIFI_EVENT_AP_START`.
- Rejected Arduino `WiFi.softAP()` for the production path after source audit showed the Arduino-ESP32 3.3.12 wrapper starts AP mode and waits for AP_START before applying caller config. Uses checked lower-level netif creation/attach/default-handler APIs to avoid the convenience constructor's assert/abort path. AP_START handler context is a retained EventGroup rather than the owner pointer, so unregister failure cannot leave a dangling callback.
- Added 20 native C++ registrations in `test_esp32_wifi_ap`; inventory is 451 registrations across 26 suites. At the v0.2.39 release point, the 38 registrations after v0.2.36 (18 v0.2.38 + 20 v0.2.39) had not been executed; v0.2.40 records later supplemental stand-in-harness execution for 20 AP + 5 RNG cases, but all 38 still lack PlatformIO verification. The original v0.2.39 audit undercounted this suite by one; corrected in v0.2.40. The 413/413 user pass is still v0.2.36 baseline-only.
- E28 remains OPEN for first-provision WPA2 passphrase source/rotation; E29 remains OPEN for production SSID naming. No credential or SSID default is invented.
- No target build or hardware AP test; no production caller, boot composition, HTTP routes, PBKDF2 verifier/KDF selection or E12 counter-domain decision. SF2 tree unchanged.
## v0.2.38 (2026-11-08) — PIN token-session action and RF-gated ESP32 RNG
- Added `ISecureRandomSource` and `TokenSessionSuccessAction` in the core. It requests 16 random bytes after durable attempt-counter reset, and uses read-only `TokenTable::canIssue()` to preflight all-zero/duplicate rejection without mutating existing sessions.
- The action allows one token copy for the HTTP response and requires `confirmResponseSent(monotonicNowS)` after a complete successful response write. The idle-expiry clock begins at this activation timestamp. Only then does it insert into `TokenTable` and apply LRU eviction; an abandoned response leaves all existing sessions unchanged. A stronger `test_api` regression now preflights a candidate then confirms the subsequent issue still evicts the originally least-recently-used session, proving preflight did not reorder LRU state. The route must serialize TokenTable mutations across the preflight/write/confirm sequence. Temporary response copies remain the handler's responsibility to erase.
- Added `Esp32SecureRandom` around ESP-IDF `esp_fill_random()`. It refuses use until the application reports successful RF startup and refuses use after RF is stopped. The application caller is not implemented; this does not independently query Wi-Fi state.
- Added 11 core token-session, 5 RNG-adapter and 2 TokenTable-preflight C++ registrations. Current inventory: 431 registrations across 25 suites. The 18 new tests are not run. The historical 413/413 v0.2.36 user run covers only the prior baseline.
- No PBKDF2 implementation, PIN KDF choice, E12 counter-domain decision, production Wi-Fi startup, route adapter, production firmware composition, target build or hardware test.
- No signed product rule, delegated choice, storage format, event schema or O8 backup scope changed.

## v0.2.37 (2026-11-08) — SF2 reference-model regression repair
- Reproduced the user's v0.2.36 SF2 test result: 238/257 passed, 19 failed. The independent run against the v0.2.36 archive reproduced the same 19 failures.
- Corrected seven categories of bad fixtures/expectations: month-roster source-sequence order; reading validation blockers separately from issues; male-then-female/source-order roster expectation; use of an actually out-of-range event sequence; malformed projected-roster fixture; consistent section profile IDs in the D1/D2 join fixture; and consistent gap/coverage evidence in the coverage-gap fixture. Validation was not loosened.
- Fixed `buildSnapshotPreview()` to surface `PREVIEW_READY` when schema validation is `VALID` and the calculation is `PREVIEW_READY`, without ever setting final export readiness. Structured learner/date/session diagnostics now take precedence over unstructured copies.
- Split source-event status disagreement into `EVENT_STATUS_MISMATCH`; malformed event structure/type remains `INVALID_EVENT_PROVENANCE`. The mismatch is still a blocking issue.
- Full SF2 Node suite passes locally: 257/257; all 25 SF2 `.mjs` files pass `node --check`. The user's v0.2.36 native summary reports 413/413 cases across 23 suites. No C++ source or native C++ test file changed in v0.2.37.
- Documentation updated to record current test evidence and retain older failures as historical. No signed/delegated decision, hardware mapping, event format, PII persistence rule, HTTP route or production firmware was changed. No ESP32 target build or device validation was performed.

## v0.2.36 (2026-11-07) — NVS runtime read-error test fixture correction
- The user supplied a v0.2.35 focused `test_nvs_config` run with 44 tests, one failure, and zero ignored. The failure expected `EPAR_COUNTER_READ_FAILED` (4) but received `EPAR_COUNTER_INITIALIZE_FAILED` (5).
- Root cause: the fake `attsec` namespace was absent, so `nvs_open(NVS_READONLY)` returned `ESP_ERR_NVS_NOT_FOUND` before the injected `nvs_get_u16()` error was reached. Runtime initialization on this missing-first-setup state followed the current contract.
- The fixture now keeps the namespace present and key absent, then injects the read error; it verifies one open, one get, no set and no commit. Test renamed for specificity.
- No production code, test registrations, policy or hardware mapping changed. Subsequent user-supplied v0.2.36 verification passed all 413/413 native cases across 23 suites; that summary did not show a SIGHUP. The earlier v0.2.35 SIGHUP remains historically unexplained and is not evidence of a persistent runner fault.

# Historical releases

## v0.2.44 (2026-11-08) — 128x32 SSD1306 OLED adapter

- Added `Esp32Oled`, a standalone Adafruit SSD1306 adapter for the project's 128x32 I2C panel at `0x3C`. It receives an injected shared `TwoWire` instance, does not call `Wire.begin()`, and uses a conservative 100 kHz transfer clock so the future board-level I2C owner can share the bus with RTC/EEPROM devices.
- Added the E28 setup credential screen: `Wi-Fi setup`, `WPA2 password:`, and the exact 32-character lowercase-hex credential split into two 16-character rows. It accepts only `APCB_OK` with `disclose=true`, rejects malformed/other-length/non-hex secrets, and requests a blank frame on READY/failure/non-disclosure.
- The adapter wipes its framebuffer after the credential transfer request and never formats the secret into a second plaintext string. The caller's passphrase buffer is not modified. `Adafruit_SSD1306::display()` returns `void`; transfer request is not proof that I2C writes succeeded or that the physical panel displayed the key. There is no production caller/boot flow yet.
- Pinned smoke dependencies: Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6 and Adafruit BusIO 1.17.4. Post-release verification: the user reports the expanded v0.2.44 target smoke build (including OLED/Adafruit dependencies) succeeded with Espressif32 8.0.0 / Arduino-ESP32 3.3.12; v0.2.45 adds coordinator/DHCP code and still needs a fresh target build.
- Added 13 C++ registrations in `test_esp32_oled`, bringing source inventory to **500 registrations across 30 suites**. The new suite passed 13/13 with 67 assertions in a local sanitizer-enabled Unity-compatible stand-in harness. This is not PlatformIO's Unity runner.
- SF2 remained unchanged and passed 257/257 Node tests; all 25 `.mjs` files passed syntax checks. No existing core source, signed decision or existing test suite was changed.
- No PlatformIO native full-suite run, expanded ESP32 target build, OLED bus/panel test, or physical-device verification was performed for this release.


## v0.2.35 (2026-11-07) — NVS test translation-unit compile correction
- Reviewed the user's v0.2.34 compiler output. `test_nvs_config/test_main.cpp` failed at two compile-time non-copyability assertions because `Esp32NvsPinAttemptStore` and `Esp32PinAuthRuntime` were referenced before `using namespace att;` and without namespace qualification.
- Qualified both assertion template arguments as `att::Esp32NvsPinAttemptStore` and `att::Esp32PinAuthRuntime`. The assertions remain compile-time checks; no production class or runtime behavior changed.
- The pasted diagnostics show the native run reached other suites, but the complete final summary and exit status were not included. The new NVS suite did not compile in that run, so its 24 test registrations did not execute.
- The separately uploaded `/mnt/data/native-test.log` does not match the pasted diagnostics: it records the older month-roster assertion failure and `SIGHUP`, while marking `test_nvs_config` as passed. It is treated as stale/inconsistent evidence and is not used to claim a v0.2.34 result.
- Current source inventory remains 413 C++ registrations across 23 suites and 257 SF2 Node registrations. v0.2.35 adds no registrations. No production implementation, signed policy, delegated decision, or hardware mapping changed.
- No compiler or test was run for v0.2.35. The namespace correction is source-reviewed only; focused/full v0.2.35 rerun is required.

## v0.2.34 (2026-11-07) — fail-closed NVS PIN-attempt storage and boot ownership
- Added `Esp32NvsPinAttemptStore`, a typed `uint16_t` counter store using a caller-selected namespace and `pin_fails` key. It rejects the reserved `attcfg` namespace, validates namespace length, refuses missing/wrong-type records during normal operations, and requires `nvs_commit()` plus fresh-handle read-back for successful writes.
- Added `Esp32PinAuthRuntime`, which owns the store and the matching `PinAuthCoordinator`. It starts only after config boot reaches `READY` or `NEEDS_SETUP`; on `NEEDS_SETUP`, it initializes only after an explicit `ESP_ERR_NVS_NOT_FOUND`, preserving an existing counter if initial setup was interrupted. On `READY`, a missing counter fails closed and is never silently recreated. The runtime is one-shot and non-copyable.
- Added 24 C++ regression registrations (16 NVS-store cases and 8 boot-owner cases), bringing the source inventory to 413 C++ registrations across 23 suites. SF2 Node registrations remain 257. At the v0.2.34 release point, the new tests had not yet been run; the later user-supplied v0.2.36 full native run passed all 413/413 cases, including these NVS tests.
- The host fake now models the documented ESP-IDF `ESP_ERR_NVS_TYPE_MISMATCH` value (`0x1103`). The NVS API signatures/error semantics and 15-character namespace limit were checked against the ESP-IDF v5.5 documentation.
- No production firmware entry point, PBKDF2 verifier/writer, PIN KDF parameter, secure RNG, token/session action or HTTP route was added. E12 remains DELEGATED; E21 remains OPEN. No signed or delegated product policy changed.
- No compiler, PlatformIO test, Node test, ESP32 build or device test was run. Commit/read-back is API-level verification only, not proof of power-cut durability.

## v0.2.33 (2026-11-07) — month-roster test boundary correction
- Reviewed the user's v0.2.32 PlatformIO log: all 23 suites reached testing; 22 suites passed; `test_month_roster` had one assertion failure (`Expected 1 Was 2`) and then received `SIGHUP`, resulting in suite status `ERRORED`.
- Confirmed the projection logic follows the half-open month contract `[monthStartDay, monthEndExclusive)`: the final included day is `monthEndExclusive - 1`. The failing fixture deactivated a learner on day 131 for month `[100, 131)`, which still makes the learner enrolled on day 130.
- Corrected the C++ fixture to distinguish an interval ending on day 130 from one ending on the exclusive boundary day 131. Added one matching Node regression.
- Removed all warning sources shown in the supplied log: unused `argc`/`argv`, implicit zero initialization of two unused `Event::b` fields, and an unused staging-helper parameter. The explicit zero values preserve aggregate initialization behavior.
- No production implementation or policy changed. C++ registrations remain 389; SF2 Node registrations rise from 256 to 257. No functional test or compiler was run for this correction; both follow-up runs are pending.
- The PlatformIO summary's 390 total does not match the 389 source `RUN_TEST` registrations. The mismatch and the SIGHUP cause remain unresolved pending rerun.
- Subsequent status update: the user reported that all v0.2.33 tests passed. The exact terminal summary/log was not supplied, so the historical count discrepancy and signal cause remain unresolved.

## v0.2.32 (2026-11-07) — native compile dependency correction
- Inspected the user's v0.2.31 PlatformIO log against the exact archive SHA. All 23 native suites stopped during compilation in `event_group_audit.cpp`; no suite reached test assertions.
- Added direct includes for `day_view.h` (`MAX_ID`) and `roster_store.h` (`RA_SET`, `RA_DEACTIVATE`, `RA_ACTIVATE`) to that implementation file. No runtime logic, event format, test registration or policy changed.
- This is a source-level correction only. The assistant did not compile or execute the patch; the user must rerun the focused event-audit suite and the full native suite. Other compiler issues may be revealed once this first shared blocker is removed.

## v0.2.27 (2026-11-07) — D4 network-only threat model
- Recorded the user's decision to protect only against unauthorized Wi-Fi clients; physical flash access and malicious authenticated administrators are out of scope.
- Preserved the signed handoff's plain-HTTP behavior and accepted risk if the WPA2 AP passphrase leaks. Added a concrete future HTTP-adapter acceptance boundary.
- Removed the need for a device signing key/reviewer public-key trust root under this threat model; a durable storage receipt is not anti-tamper proof.
- Recorded U79: AP passphrase strength/provisioning is unverified; 8–63 printable characters is only shape validation.
- Kept reviewer-role/source/time/revision policy OPEN, C6/C13/C26-C29 blockers, and D3 preview-only. O8 backup scope unchanged. No code or test changes.

## v0.2.26 (2026-11-07) — SF2 D4 reviewer-evidence architecture review
- Documentation-only review separates reviewer attestation from device commit receipt and identifies the missing threat model, individual reviewer credential, trust anchor, source policy, trusted-time, PII and backup decisions.
- Added the OPEN D4 decision matrix and U75-U78. No product decisions or implementation status were silently changed.
- No source code or tests changed; E27 remains DELEGATED.

## v0.2.25 (2026-11-07) — SF2 D3 cross-layer snapshot join
- Added a shared source-prefix contract: C++ `uint64_t openGeneration` is canonical unsigned decimal text in the browser/JSON layer; remaining prefix counters are bounded `uint32_t` values.
- Updated D1 month-roster, D2 reporting-identity and the snapshot coordinator to use exact string equality for the open generation.
- Added `joinReportingRosterForSnapshot()` to align D1/D2 month, exact source prefix, learner ID set, month-overlap enrollment intervals (including order/overlap rechecks), legal identity, profiles and movement boundaries before mapping a snapshot-compatible fragment. The join always remains `INCOMPLETE` until authenticated reviewer evidence is actually verified.
- Changed the browser calculation adapter to `PREVIEW_READY` only; schema validity and caller-supplied `SOURCE_CHECKED`/`VERIFIED` flags cannot authorize export.
- Added a reviewer-evidence design proposal. No persistence, API, authentication verifier, crypto key, firmware or C++ code was added.
- Added 16 Node test registrations; all SF2 functional tests remain unrun. Static syntax checking only.

## v0.2.24 (2026-11-07) — SF2 legal reporting identity projection
- E27 is now DELEGATED to keep same-local-day ACTIVATE/DEACTIVATE fail-closed for authoritative monthly reporting. This is a projection restriction only; it does not reject or rewrite operational roster events.
- Corrected D1's dependency: `RA_SET` operational display labels are diagnostics only. Missing, changing or backdated display names no longer block a legally complete report or substitute for the legal school-record name.
- Bumped the proposed metadata-history model to schemaVersion 2. Learner identity now uses a distinct `legalName` object, LRN, sex, and an explicit `UNVERIFIED`/`SOURCE_CHECKED` status.
- Added `metadataRecordsAtRevision()` and `projectReportingRoster()` to join an exact-prefix month-overlap roster with metadata at a pinned revision. Identity coverage is required only across enrolled days. Legal name/LRN/sex changes during enrolled days in one month block automatic selection; metadata gaps remain incomplete, not absent.
- Expanded explicit movement taxonomy with `WITHDRAWN` and raw `NLS`; added effective-date meaning and conservative interval-boundary checks. NLS and unknown date meanings are preserved and flagged, not normalized.
- Aligned the proposed snapshot validator with movement types and date-meaning fields.
- Added 34 Node test registrations; tests not run. Static syntax check only. No C++ source or test registrations changed.
- Critical audit: `SOURCE_CHECKED` has no reviewer identity/time/source attestation and therefore cannot make legal identity verified; historical revision materialization also validates LRN/profile uniqueness at the selected revision, not only at the latest state; projected status remains `UNVERIFIED`. Duplicate LRN is rejected across different never-reused device IDs even if valid-time periods do not overlap.
- Multiple movement records for the same learner and effective date block the report projection. `SHIFTED` must match an enrollment interval boundary; otherwise it is rejected. Malformed learner objects no longer throw during projection.
- D2 final audit hardening: legal-name identity keys use canonical field order; C++ `uint64_t openGeneration` is not incorrectly capped at `uint32_t`, while `auditGeneration` remains explicitly `uint32_t`. Prefix values outside JavaScript safe-integer representation are rejected.
- No metadata persistence, encryption, backup/restore, production API, dashboard, XLSX writer, C++ compiler, PlatformIO, ESP32 build, or device verification.

## v0.2.23 (2026-11-07) — SF2 month-overlap roster projection
- Added `MonthRosterProjection`, bound to one exact audited log prefix and a caller-owned scratch roster; it replays the prefix itself before projecting.
- Added validation for interval IDs, interval boundaries, overlap per student, known IDs without intervals, and source/scratch changes during iteration.
- Emits each month-overlap learner once, preserves every intersecting original interval, and reports enrollment on the month's final day separately.
- Added browser reconciliation of month-overlap intervals with full historical display-name revisions from the same prefix. Distinct name changes within the month and missing reportable names block finalization rather than silently choosing a value.
- E27 is DELEGATED; same-day activate/deactivate transitions still fail closed for reporting projection only. No P9, live roster or attendance-rule change.
- Added 16 C++ and 18 Node test registrations. The projection copies its open/prefix descriptors, distinguishes scratch-read failure from a hash value, does not silently skip a learner if summary reading fails, and clears old replay diagnostics before each begin attempt. Not run. Static JS syntax check only; no C++ compiler or hardware verification.

## v0.2.22 (2026-11-07) — historical roster projection and snapshot paging model
- Added `RosterLogPrefix` bound to the exact `LogStore` open generation, record count, last sequence and `EventGroupAudit` generation. Any append/reopen/repair invalidates the prefix.
- Added read-only `RosterTransitionAudit` for backwards effective days, lifecycle state violations, same-day activate/deactivate ambiguity and pre-SET actions. It is a reporting guard only; `RosterStore`, event encoding and attendance semantics are unchanged. E27 is OPEN pending review against real roster histories.
- Added `replayRosterPrefix()` to reconstruct a caller-owned scratch roster from the exact prefix, run transition checks, compare final known/active state and reject changes/incomplete replay.
- Added explicit-prefix `RosterNameHistoryReader` constructor; the original constructor's strict changed-snapshot behavior remains.
- Added a pure host-side revision-vector/ticket/page assembler model with monotonic expiry, revision rechecks, exact page sizes and duplicate/missing page/item rejection. No server ticket store, auth, HTTP route, lock, secure ID generation or persistence was added.
- Added 17 C++ and 17 Node test registrations. None were executed. SF2 JS modules/tests received static `node --check` only; this is not functional verification.
- Specialized audit: 360 C++ registrations across 22 suites; 187 SF2 Node registrations; no C++ compile, PlatformIO, ESP32 build or device verification.

## v0.2.19 (2026-11-07) — SF2 snapshot validation and diagnostics
- Added strict proposed snapshot schema validator and adapter to existing pure section calculations.
- Added record-oriented unresolved attendance/coverage preview diagnostics.
- Added real-source close-group provenance requirements: one `EV_CLOSE` plus exactly five contiguous 64-bit `EV_PAYLOAD` records for the 300-ID bitmap; validates `closeCoverageFlag`, event sequence uniqueness, `uint32_t` bounds and bitmap/enrollment agreement.
- Static audit found C25: normal `LogStore::open()` does not run the semantic restore group audit, and `RestoreGroupAudit` itself currently permits short CLOSE groups. The future live snapshot service needs a stricter full-log audit before it can expose trusted absence/coverage.
- Added 89 Node test registrations; intentionally not executed. All SF2 JS modules passed syntax-only `node --check`.
- No C++ source/tests changed. No API, dashboard or XLSX writer added.

## v0.2.17 (2026-11-06) — native test runner correction
- Fixed `test_config_guard` entry point: replaced Arduino-style `void setup()` with the native test executable's `int main()` and `return UNITY_END()`.
- The user's v0.2.16 log failed during linking with `undefined reference to main`; no assertions in that suite executed. This was a test-harness entry-point defect, not a production-code failure.
- Retained all seven `RUN_TEST()` calls and all production sources unchanged.
- Source registration count remains 318 across 19 suites.
- No tests or compiler were run for this correction; focused and full PlatformIO reruns remain pending.

## v0.2.16 (2026-11-06) — config boot health and current-window guard
- Added `updateHealthFromConfigBoot()` to map the one-shot config boot state into `HealthFsm` while preserving RTC/storage/sensor facts. COLD/FAULT map to `H_CONFIG_FAULT`; only a verified `NEEDS_SETUP` maps to `H_UNPROVISIONED`.
- Added `deriveConfigWindowOpen()` for validated current-config and trusted-RTC window derivation. Error paths set the output to open (fail-closed).
- Added 7 source registrations in `test_config_guard`; source total: 318 across 19 suites. Tests were not run.
- Specialized audit found C17: the active timetable's effective-from boundary/history is not persisted, while `ClosureEngine::step()` uses its current timetable over historical days. A naive `unclosedPast` scan would be unsafe and may allow/reconstruct false historical closures. No schedule-history or config-slot change was made because E2 is DELEGATED.
- Specialized audit also found C18: API date validation treats `termEnd` as inclusive, fingerprint scanning treats it as exclusive, and `ClosureEngine` has no `termEnd` input. No boundary policy was guessed or patched.
- `HANDOFF_FINAL.md` is unchanged. E26 was added as OPEN; no existing signed/delegated decision was altered. No compiler, PlatformIO, ESP32 target build, or device verification was run.


## v0.2.67 (2026-11-10) — reservation-ticketed ReqRing completion

- Fixed a stale-handler race in the in-memory request ring: expiry followed by reuse of the same req_id allowed an old handler's finish/abort call to mutate the new reservation.
- Every RQ_NEW result now carries a nonzero 64-bit ticket. finish() and abort() require the matching req_id and ticket; expired/stale/zero-ticket calls are ignored.
- Added a regression that reuses the same req_id and fingerprint, then verifies stale finish and abort cannot alter the current pending reservation.
- Test-first compile failure confirmed the new interface was absent; final focused test_api passed 40/40. Current inventory is 610 registrations across 35 suites; the other 570 were not run.
- ReqRing remains RAM-only. Expiry does not cancel old work or prevent duplicate side effects if the old handler continues. Durable exactly-once semantics remain unresolved (U112). No production caller, ESP32 target, SF2 or hardware verification.


## v0.2.68 (2026-11-10) — success-only ReqRing completion

- Enforced ReqRing's documented rule that only successful operations may be cached. finish() now returns bool and accepts only A_OK with the active matching (req_id, ticket).
- Non-success codes, stale tickets and zero tickets return false without mutating a live reservation. Callers must explicitly abort unsuccessful or uncertain operations before retrying.
- Added a regression proving A_INVALID is rejected, the reservation remains inflight, explicit abort allows a retry, and a later A_OK result is replayable.
- Test-first compile failure confirmed the prior void signature could not express acceptance/rejection; final focused test_api passed 41/41. Inventory is 611 registrations across 35 suites; 570 were not run.
- Static audit found no production ReqRing callers under lib/. ReqRing is still RAM-only; uncertain storage commits, unfinished handlers, entry eviction and durable exactly-once behavior remain unresolved (U112). No ESP32 target, SF2 or hardware verification.

## v0.2.79 (2026-11-11) — SF2 diagonal marks count as absent

- Corrected SF2 monthly counts: `BLANK` and `E` count Present; `X`, upper-left, and lower-right diagonal marks count Absent.
- Reserved `E` for Excused in both AM and PM. An Excused+Confirmed-Absent pair now uses the corresponding diagonal mark.
- Consecutive-absence streaks now include diagonal days.
- Reaffirmed one generated semantic learner row per reporting-month roster member; added a 60-learner no-four-row-cap regression. The actual XLSX writer remains unimplemented.
- Focused SF2 verification passed 83/83 across attendance, section, learner-layout, snapshot-adapter and preview suites. No full suite, XLSX, firmware, target or physical hardware verification.

## v0.2.95 — 2026-11-12
- Added a native MonthSet/LogStore regression for `addFile()` false-after-create: the log faults, blocks retry, and requires a fresh read-only open before append can resume. Test-only; no recovery policy or production behavior changed. C13.3 remains BLOCKING.


## 2026-11-13 — ATT-CLOCK-HEALTH-REVERIFY-OWNER-01 — clock-domain health recovery owner (source only)

- Restored the explicit HealthInputs.clockTrusted field and provisioned-only trust gate that target/main.cpp and clock-confirmation tests already expected but the live HealthFsm header/implementation lacked. Updated the remaining current aggregate initializers to supply the tenth field.
- Added the separate clockReverified argument to HealthFsm::update() and updateHealthFromConfigBoot(). It clears only HB_CLOCK when current clock facts are healthy; the legacy broad reverified signal retains its original semantics.
- Added ClockHealthOwner, which validates exact health/config owner identity through its dispatcher, performs request-bound dispatch itself, and consumes only an invoked, authorized, fully verified CCF_OK result. It preserves independent sensor/storage/config state and revokes trust after observed RTC/epoch faults or a service-latched ambiguous failure.
- Added one test_coreb registration and strengthened the existing authenticated clock-dispatch integration case. Both affected assertions are authored but NOT RUN. The actual source inventory is 884 registrations across 47 suites after a direct recount; earlier 972/970 figures were stale.
- No production HTTP route or boot-shell composition was added. No tests, checker, compiler, build, PlatformIO, flash or hardware operation was run; C3/C6 and C13.3 remain open.


## 2026-11-13 — ATT-CLOCK-HEALTH-OWNER-REVERIFY-BOUNDARY-01 — prevent cross-domain latch release (source only)

- Closed a fail-open path in `ClockHealthOwner::update()`: it now suppresses the caller's broad `HealthInputs::reverified` signal, just as `confirm()` already did. This clock-specific owner may not clear storage/configuration fault latches.
- Extended the existing authenticated clock-owner integration regression: after latching a storage fault, a caller-supplied `reverified=true` must not clear it. The test remains NOT RUN; registration count is unchanged.
- Updated the clock policy contract. No production route or target composition changed. No tests, checker, compiler, build, PlatformIO, flash or hardware operation was run.
