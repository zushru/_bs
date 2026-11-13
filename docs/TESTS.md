## Current source inventory — static recount, 2026-11-13

A direct recount of current C++ test sources found **884 RUN_TEST registrations across 47 suites** after ATT-CLOCK-HEALTH-REVERIFY-OWNER-01. This is an inventory count, not a pass count. The prior 972/970 figures in working-memory snapshots do not match the current source tree and are superseded. Current relevant suite counts: test_coreb 45, test_clock_confirmation 23, test_config_guard 7, test_as608_pipeline 16, test_monthset 24. One new health-latch regression was added; affected assertions are NOT RUN. No tests, checker, compiler, build, PlatformIO, flash or hardware action was run.

## v0.2.95 — MonthSet/LogStore reconciliation after visible empty-file creation

- Added `test_add_file_false_after_create_faults_log_until_reopen` in `test/test_monthset/test_main.cpp`. The fake `addFile()` makes an empty month file visible and then returns false. The test drives the real `MonthSet`/`LogStore` boundary and verifies no payload append, fault latching, retry refusal, and successful later append only after a fresh read-only reopen reconciles the empty file.
- Exact command: `pio test -e native -f test_monthset -f test_littlefs_fileset -f test_littlefs_mount_owner -f test_littlefs_serialized_fileset` — **100/100 passed**, exit 0 (`test_monthset` 24/24; `test_littlefs_fileset` 50/50; `test_littlefs_mount_owner` 23/23; `test_littlefs_serialized_fileset` 3/3). Log: `/tmp/att_v0295_c13_native.log`.
- This is POSIX/fake-backend host behavior. No production source behavior changed. No target build was rerun for this test-only increment; no mounted ESP-IDF VFS, actual descriptor, low-space, parent-directory durability, restart interruption, power-cut or brownout test was performed. Full native N1 and SF2 were not run. C13.3 remains BLOCKING.

## v0.2.94 — C13.3 native month-creation fault coverage

- Adds native-only fault routing for LittleFsFileSet::addFile() open/fsync/close. openFn now accepts mode_t; the ESP_PLATFORM branch remains direct open/fsync/close calls.
- Test-first red reproduced three creation-path failures before implementation. Final focused command: pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset' — 76/76 passed, exit 0. Suite results: fileset 50/50; mount owner 23/23; serialized fileset 3/3. Full log: /tmp/att_v0294_c13_native.log.
- The fsync and close failures occur after successful file creation. addFile() correctly returns false, but the empty month file remains. The regression verifies it stays enumerable and a blind same-key retry is refused. This is an indeterminate side effect, not a successful commit or a recoverable storage transaction.
- Current source inventory: 738 C++ RUN_TEST registrations across 39 suites. Only the three C13 suites (76 cases) ran; the other 662 registrations were not run. SF2 remains 270 registrations across 12 files and was not run. Full native N1 remains deferred.
- ESP32 command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — SUCCESS, exit 0, 1,987.27 s. Pinned Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12 / esptool 5.4.0 / Xtensa 14.2.0+20260121. RAM 67,892 B (20.7%); flash 1,010,340 B (77.1%). ELF SHA-256 18af01b5664e794b22f27e2d3acb383dbbbdfb3c00135a4a08d3198cfb677660; BIN SHA-256 40ce2e4416b2a59c524bf40d97b7d648566b0054d894e9aa7cdbcd88a689c909. ELF retains addFile/readFile/appendFile/validateRecordFiles and mount-owner/backend methods. Full log: /tmp/att_v0294_esp32_link.log.
- Host tests and inert compile/link do not prove target LittleFS mount/runtime, real descriptor lifecycle, low-space behavior, parent-directory durability, restart recovery or physical power-cut behavior. The smoke target was not flashed. C13.3 remains blocking.

## v0.2.93 — C13.3 native read-path fault coverage

- Test-first red: four new registered read-path cases failed before the source patch because readFile() bypassed native open/pread/close hooks and did not exercise injected EINTR. The intermediate run was an expected red reproduction, not a green result.
- Final focused command: pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset' — 73/73 passed, exit 0. Suite results: fileset 47/47; mount owner 23/23; serialized fileset 3/3.
- Current source inventory: 735 C++ RUN_TEST registrations across 39 suites. Only the three C13 suites (73 cases) ran in this increment; the other 662 registrations were not run. SF2 remains 270 registrations across 12 files and was not run. Full native N1 remains deferred.
- ESP32 command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — SUCCESS, exit 0. Pinned Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / esptool 5.4.0 / Xtensa 14.2.0+20260121. ELF retains LittleFsFileSet read/append/validation, mount-owner and ESP32 mount/unmount methods. RAM 67,892 B (20.7%); flash 1,010,340 B (77.1%). ELF SHA-256 be804edcc9fc096f9c27ff3bb27994c9928eb295b5cf402ed982395e85d34405; BIN SHA-256 8162f9e3542cda56220af661d9a1d93a664fde50d53cea9c4e01be767f99e433.
- This is host/POSIX fault-seam evidence plus inert target compile/link. It does not establish target LittleFS mount/runtime, real VFS fault behavior, descriptor close semantics, low-space handling, file/directory durability, restart recovery or physical power-cut behavior. The smoke target was not flashed. C13.3 remains blocking.

## v0.2.92 — C3 application attendance gate

- Test-first red: /tmp/att_c3_gate_red2.log reproduced two failures: config-mismatch owner startup was accepted, and As608OwnerConfigRuntime accepted a different closure engine and allowed a config write.
- Final green command: pio test -e native -f test_as608_pipeline — 16/16 passed, exit 0. Log: /tmp/att_c3_gate_green5.log. Covers gate graph, NEEDS_SETUP denial without constructing runtime storage, mismatch config/object rejection, clock-health change during a pending scan, closure identity and existing match/duplicate/uncertain-commit paths.
- Current source inventory: 731 C++ RUN_TEST registrations across 39 suites. Only test_as608_pipeline (16 cases) ran in this increment; the other 715 registrations were not run. SF2 remains 270 registrations across 12 files and was not run. Full native N1 remains deferred.
- ESP32 command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — SUCCESS, exit 0. Pinned Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / Xtensa 14.2.0+20260121. Final ELF retains gate, owner, config-runtime and ClosureEngine::matchesConfig methods. PlatformIO: 67,892 B RAM (20.7%), 1,010,340 B flash (77.1%). ELF SHA-256 b6a2007f9adaf6713032202d7b4e9cf91643a2a83a6fa6432284c08db3f67515.
- This is an inert compile/link smoke app. It does not construct the new runtime objects, call methods, mount LittleFS, access NVS/I2C/UART, start AP or perform attendance. Never flash it. No physical, target VFS, RTC, power-cut or production-root test was run.

## v0.2.91 — require validated config boot for AP/PIN NVS stores

- Test-first compile-time red: `pio test -e native -f 'test_nvs_ap_credential' -f 'test_nvs_config'` failed the new construction assertions because the AP store was default-constructible and the PIN store accepted only a namespace. Log: `/tmp/att_c1_nvs_gate_red.log` (compile-time expected red; no Unity cases ran).
- Green focused command: `pio test -e native -f 'test_nvs_ap_credential' -f 'test_nvs_config'` — **65/65 passed**, exit 0. `test_nvs_ap_credential` 17/17; `test_nvs_config` 48/48. Includes new COLD/FAULT no-`nvs_open()` regressions and all existing READY/NEEDS_SETUP, PIN counter and AP credential tests. Log: `/tmp/att_c1_nvs_gate_green3.log`.
- Target command: `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link`. Pinned target `main.cpp.o` compiled; bootloader and partition images generated; Wire, SPI and Adafruit BusIO libraries compiled/archived. The process was terminated at 7m37s during Adafruit GFX compilation to conserve the phone, before the changed HAL translation units and final link. No `firmware.elf` or `firmware.bin` exists. Log: `/tmp/att_c1_nvs_gate_esp32.log`. This is incomplete target evidence, not a pass.
- Source inventory: **728 C++ `RUN_TEST` registrations across 39 suites**. Only the two owning NVS suites (65 cases) were run; the other 663 registrations were not run. Full N1, SF2, hardware-probe build, final ELF inspection, flash, NVS corruption, hardware/runtime and power-cut tests were not run.

## v0.2.90 — LittleFS owner-to-log host integration

- Added `test_mounted_owner_to_monthset_logstore_reopen` to `test_littlefs_mount_owner`. It begins a `LittleFsStorageOwner`, obtains its serialized `IFileSet`, opens an empty `LogStore` over `MonthSet`, appends one `EV_SCAN`, then creates a fresh backend/owner and verifies structural/semantic preflight plus record replay from the same POSIX directory.
- Exact command: `pio test -e native -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'` — **69/69 passed**, exit 0. Suite results: `test_littlefs_fileset` 43/43; `test_littlefs_mount_owner` 23/23; `test_littlefs_serialized_fileset` 3/3. The new owner case also passed independently in its first focused run (23/23, exit 0).
- Source inventory: **726 C++ `RUN_TEST` registrations across 39 suites**. These are the complete three focused C13 suites only; the other 657 registrations were not run. Full native N1 and SF2 Node suite were not run.
- All new integration behavior is POSIX-host behavior using `FakeMountBackend`; it does not mount LittleFS or exercise ESP-IDF VFS. No ESP32 target build/ELF, mounted target runtime, low-space test, power-cut/brownout test, flash, or physical test was run. C13.3 remains blocked.

## v0.2.89 — scan metadata and directory fault coverage

- Test-first red identified that `scan_()` bypassed the existing native hooks; four scan directory/metadata error cases failed before the routing patch. Two further red regressions showed that transient root and `/log` stat errors could be rechecked and mistaken for a valid empty directory. PlatformIO emitted a signal after the red Unity output; treat these as assertion-level red reproductions, not clean red process exits.
- Green owning suite: `pio test -e native -f 'test_littlefs_fileset'` — **43/43 passed** in 17.210 s. This includes the six new scan-fault tests and the earlier append/validator read, close, stat/open/fstat and directory fault tests. The output contained a complete Unity/PlatformIO PASSED summary.
- Dependent suite: `pio test -e native -f 'test_littlefs_mount_owner'` — **22/22 passed**, exit 0, 21.021 s. This covers the owner’s fail-closed exposure contract after the scan change.
- Static source inventory: **725 C++ `RUN_TEST` registrations across 39 suites**; `test_littlefs_fileset` has **43**. SF2 inventory remains 270 Node registrations across 12 files; not run. Full native N1 was not run.
- Target command: `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link`. The modified `littlefs_file_set.cpp` compiled and `liblittlefs_file_set.a` archived under the pinned Xtensa toolchain. The build proceeded to unrelated `hal_esp32` recompilation and was manually stopped to conserve the phone; no final summary, `firmware.elf` or `firmware.bin` exists. This is target translation-unit/library evidence only, not a complete target build.
- No hardware-probe build, flash, mount/unmount, mounted LittleFS VFS fault injection, NVS test, power-cut/brownout test, physical test or SF2 suite was run. See `RELEASE_v0.2.89.md` for commands, logs and scope.

## v0.2.88 — build-only validator fault-coverage increment

- Added seven pending native regression registrations to `test_littlefs_fileset`: `stat` error for missing `/log` (must not become empty), month-file `stat` error, month-file `open` error, descriptor `fstat` error, `opendir` error, `readdir` error and `closedir` error. The preserving tests verify the original record bytes after failure. The injected close/closedir callbacks close the actual host resource before reporting `EIO`.
- **Execution status: NOT RUN.** The user deferred native tests until RDC is available. No red/green test result is claimed. The earlier v0.2.86 `pread` and v0.2.87 descriptor-close regressions are also still pending.
- Build-only evidence: 57/57 production library TUs compiled with host `g++ -std=gnu++11 -Wall -Wextra`; changed storage TU compiled with `-Werror`; `-DESP_PLATFORM` conditional branch compiled using host headers/compiler with `-Werror` (not target evidence); owning test TU syntax-compiled with temporary Unity macros (no assertions executed); 32-object core/LittleFS host closure linked. All commands and limits are in `RELEASE_v0.2.88.md`.
- No PlatformIO native suite, SF2 test, ESP32 target build, ELF inspection, flash, mounted VFS runtime or physical test was run. `scan_()` remains outside the new injection seam.
- Source registration inventory is 719 C++ `RUN_TEST` registrations across 39 suites; `test_littlefs_fileset` has 37. This is inventory only, not a pass count.

## v0.2.87 — startup validator close-fault injection (pending)

- Added one registered case to `test_littlefs_fileset`: `test_record_validation_fails_closed_on_close_error_without_mutation`. It supplies the existing native-only `closeFn` through `LittleFsFileSetTestOps`; `validateRecordFiles()` must fail when closing the read-only descriptor reports `EIO`, while the stored month-file size and exact record bytes remain unchanged.
- The callback intentionally calls POSIX `close()` successfully before returning `-1/EIO`. This verifies error propagation and data preservation, but does not model an actual target descriptor that remains open after close failure. ESP32 production code still calls the direct libc/VFS `close()` path.
- **No native test was run.** Both this test and the v0.2.86 `preadFn` validator regression are pending. Owning command when RDC/PlatformIO is available: `pio test -e native -f 'test_littlefs_fileset'`.
- Host build only: all 57 `lib/**/*.cpp` production translation units compiled with `g++ -std=gnu++11 -Wall -Wextra -c` using the four project library include directories and all `test/*/stubs` include directories (57/57, exit 0). The modified `littlefs_file_set.cpp` also compiled with `-Werror` (exit 0). The core + LittleFS storage closure linked 32 object files against a dummy `main()` (exit 0); this checks host link closure only and is not a product executable.
- `test_littlefs_fileset` now has 30 active `RUN_TEST` registrations; project inventory is 712 registrations across 39 suites. SF2 remains 270 Node registrations across 12 files. No native suite, SF2 test, PlatformIO build, Xtensa/ESP32 build, ELF inspection, mounted VFS operation, flash, or physical test was run.

## v0.2.86 — startup validator read-fault injection (pending)

- Added one registered case to `test_littlefs_fileset`: `test_record_validation_fails_closed_on_pread_error_without_mutation`. It uses the existing native-only `preadFn` hook to return `EIO` from the `IStorage::read()` path consumed by `LogStore::open()` during `validateRecordFiles()`.
- Expected contract: one injected `EINTR` is retried; a later injected `EIO` makes `validateRecordFiles()` return false; the original month-file size and all record bytes remain unchanged. The test source is present but **has not been executed**. No pass count is available.
- Run first when RDC/PlatformIO is available: `pio test -e native -f 'test_littlefs_fileset'`. Add dependent suites only if this owning suite reveals a shared behavior issue.
- Source inventory reconciled from the archive: **710 active RUN_TEST registrations across 39 suites before this change**, despite v0.2.85 docs claiming 704; this increment adds one, for **711 registrations across 39 suites**. SF2 remains 270 registrations across 12 files. No native suite, SF2 suite, target build, ELF inspection, VFS runtime, or physical test was run for v0.2.86.

## v0.2.85 — unique ESP32 LittleFS mount backend ownership

- Erratum (recounted in v0.2.86): the uploaded v0.2.85 source archive contained **710 active RUN_TEST registrations across 39 suites**, not 704. This is a source inventory correction only; it does not change the historical test execution results.

- Test-first red: `pio test -e native -f 'test_littlefs_mount_owner'` failed compilation on four new static assertions: backend copy construction, copy assignment, move construction and move assignment were all permitted before the patch. Log: `/tmp/att-v0.2.85-mount-ownership-red.log`. No Unity runtime cases ran in this red phase.
- Green owning suite: `pio test -e native -f 'test_littlefs_mount_owner'` — **22/22 runtime cases passed**, exit 0; all four compile-time assertions also passed. Log: `/tmp/att-v0.2.85-mount-ownership-green.log`.
- The three focused C13.3 storage suites (`test_littlefs_mount_owner`, `test_littlefs_serialized_fileset`, `test_littlefs_fileset`) passed **53/53** before the ownership patch. Log: `/tmp/att-v0.2.85-c13.3-native.log`. The mount-owner suite was rerun after the patch; the other two suite implementations were unchanged.
- Pinned ESP32 command: `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link`. `target/esp32_adapter_link/main.cpp` compiled; the full `hal_esp32` library compiled and archived, including `esp32_littlefs_mount.cpp`. The run was manually stopped during Arduino framework compilation at `StreamString.cpp`; no final summary or `firmware.elf` exists. Treat as **INCOMPLETE / NOT VERIFIED**, not a pass. Log: `/tmp/att-v0.2.85-c13.3-smoke-resume.log`.
- Inventory remains **704 C++ registrations across 39 suites** and **270 SF2 Node registrations across 12 files**. Compile-time assertions do not add runtime registrations. Full native N1, SF2, final ELF inspection, hardware-probe build, flash and physical VFS/power-cut behavior were not run.

## v0.2.84 — shared I2C owner and RTC status probe

- Owning/dependent command: `pio test -e native -f 'test_i2c_bus_owner' -f 'test_ds3231' -f 'test_esp32_oled'` — **56/56 passed**, exit 0. Suite counts: owner 4, DS3231 36, OLED 16. This is focused verification, not full N1.
- The owner suite covers one-time `Wire.begin()` forwarding, invalid pins/clock, terminal begin failure, readiness-gated lock acquisition and shared lock identity.
- Direct Xtensa C++11 compile of `i2c_bus_lock.cpp` and `esp32_i2c_bus_owner.cpp` against pinned Arduino-ESP32 3.3.12 / ESP-IDF headers passed; both relocatable Xtensa objects were produced, exit 0. Translation-unit compile only, not full link.
- Pinned smoke build attempted twice with `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link`; remote process sessions ended without final summary and no ELF was produced. Not a pass. Logs: `/tmp/att-v0.2.84-smoke.log`, `/tmp/att-v0.2.84-smoke-resume.log`.
- Hardware-probe build, full native, SF2, ELF inspection, flash and physical RTC/I2C behavior were not run.
- Current inventory: 704 C++ registrations across 39 suites (700/38 before this 4-test suite); 270 SF2 registrations across 12 files. Full native N1 and SF2 remain unverified for this increment.

## v0.2.82 — LittleFS read-only startup owner and cross-month roster validation

- Test-first regression: pio test -e native -f 'test_littlefs_mount_owner' initially showed that startup accepted a duplicate student deactivation split across November and December files. The failing Unity assertion is retained in the run log; PlatformIO stopped with SIGHUP after the assertion output, so this is recorded as a red reproduction, not a normal green exit.
- Final owning suite: pio test -e native -f 'test_littlefs_mount_owner' — 22/22 passed. It covers invalid and valid cross-month lifecycle histories, malformed UTF-8 and post-NUL padding, explicit UTC offset bounds, and existing owner fail-closed/idempotence behavior. Log: /tmp/test_roster_fileset_green2.log.
- Fileset suite: pio test -e native -f 'test_littlefs_fileset' — 28/28 passed, exit 0. This is the owning suite for read-only POSIX file validation and the existing append fault cases. Log: /tmp/test_littlefs_fileset_green.log.
- Event framing suite: pio test -e native -f 'test_event_group_audit' — 20/20 passed in the dependent run. Log: /tmp/test_roster_dependents.log.
- Roster prefix/transition suite: pio test -e native -f 'test_roster_snapshot' — 26/26 passed in the dependent run. Log: /tmp/test_roster_dependents.log.
- Serialized fileset suite: pio test -e native -f 'test_littlefs_serialized_fileset' — 3/3 passed, exit 0. Log: /tmp/test_littlefs_serialized_fileset_green.log.
- LogStore sequence-base suite: pio test -e native -f 'test_monthset' — 23/23 passed, exit 0. Log: /tmp/test_logstore_sequence_base.log.
- The first combined dependent invocation errored at compile time because three test call sites had not yet been updated to the new required UTC-offset argument. Those calls were corrected; the isolated fileset rerun then passed 28/28. Do not count the errored combined invocation as a pass.
- Current source inventory: 667 C++ RUN_TEST registrations across 37 suites and 270 SF2 Node registrations across 12 test files (the latter was already present in the v0.2.81 baseline). Unique selected native cases across the six final focused runs: 122. The runs were separate, not one combined invocation. The other 545 C++ registrations were not run. Full native N1 and SF2 Node tests were not run; the historical 257/257 SF2 pass does not cover 13 later registrations.
- ESP32 command: pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link — SUCCESS, exit 0, 2,963.63 s; RAM 67,684/327,680 B (20.7%); app flash 984,168/1,310,720 B (75.1%). Exact ELF verifier passed 12 NVS wrapper routes, 6 LittleFS owner/backend/serialization symbols and 7 NVS/config/AP/PIN symbols, exit 0. Logs: /tmp/esp32_v082_build.log and /tmp/esp32_v082_elf_verify.log. The target does not call begin() or mount/read/write LittleFS. Never flash this target. Subsequent native testing cleaned the shared ELF output before hashes were captured.
- No ESP32 LittleFS runtime, VFS fault injection, directory/file durability, power-cut/brownout, hardware, flash or destructive recovery test was performed.

## v0.2.77 — read-only fail-closed log startup and observer rollback

- Test-first regressions were added for a partial record tail, a complete CRC-invalid final record, and an I/O failure during the second replay pass after one record had already reached an observer. The damaged-tail tests failed against the old implementation because `LogStore::open()` returned success after truncating the suffix. The observer regression reproduced one partially delivered record remaining after open failed.
- The final grouped command was `pio test -e native -f 'test_monthset' -f 'test_coreb' -f 'test_coreb2' -f 'test_event_group_audit' -f 'test_restore_replay'` — **116/116 passed**, exit 0, 1:34.985. Full log: `/tmp/att_v0277_final_native_selected.log`.
- Suite results: `test_monthset` 21/21; `test_coreb` 44/44; `test_coreb2` 17/17; `test_event_group_audit` 19/19; `test_restore_replay` 15/15.
- Selected verification: **116/116 registrations across five suites**. Other 519 C++ registrations were not run at this increment; SF2's 257 Node tests were not run. Full native N1 is deferred. This is not a full regression result.
- Red regression logs: `/tmp/att_v0277_red_test_monthset.log` (old tail-truncation behavior); `/tmp/att_v0277_red2_test_monthset.log` (partial observer state after replay read failure). An observer fake initially counted resets without clearing its record state; the fake was corrected to follow the observer reset contract before final green.
- ESP32 compile/link command: `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2` — **SUCCESS**, exit 0, 1,203.99 s; RAM 67,356/327,680 B (20.6%), application flash 935,792/1,310,720 B (71.4%). Log: `/tmp/att_v0277_esp32_build2.log`.
- Exact final ELF inspector: `./tools/verify_v0.2.77.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` — **PASS**, exit 0; 12 NVS wrapper routes, 7 NVS/config/AP/PIN symbols and 9 LittleFsFileSet symbols. Log: `/tmp/att_v0277_elf_verify.log`. ELF SHA-256 `e0b0db6ae8b5ca17bf93c64c9511507315a1ca008cda0b570d5581c0e6eb264f`; BIN SHA-256 `8bebd67e22697627e3946e8a25110f162ac2a4c04cf01014e87534ed9cf0ae9a`.
- No target flash, mounted LittleFS runtime, physical power-cut/brownout, sensor, OLED, RTC, AP, powerbank or physical-device test is implied. The inert adapter-link target is never to be flashed.

## v0.2.76 — low-level AP/PIN NVS adapter startup guards

- Test-first regressions were added for the AP credential store and PIN-attempt store. Before implementation, `test_nvs_ap_credential` failed because an unsafe snapshot reached NVS and returned NOT_FOUND instead of the required read error; `test_nvs_config` failed because the PIN adapter attempted access. PlatformIO emitted SIGHUP after each Unity failure summary; these are red reproductions, not normal completed test exits.
- `pio test -e native -f 'test_nvs_ap_credential'` — **16/16 passed** after implementation. Log: `/tmp/att_v0276_green_test_nvs_ap_credential.log`.
- `pio test -e native -f 'test_nvs_config'` — **47/47 passed** after implementation. Log: `/tmp/att_v0276_green_test_nvs_config.log`.
- Selected total: **63/63** across two distinct owning suites. New regressions prove unsafe startup prevents every public AP/PIN adapter operation from opening NVS; they also verify the AP output buffer is cleared and PIN output is unchanged.
- Current source inventory: **632 C++ registrations across 35 suites**; SF2 remains 257 Node registrations across 12 files. No other native suite was selected; N1 full native remains deferred.
- `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2` — first full target build succeeded in 1,076.82 s, but the initial ELF verifier correctly exposed that the smoke anchor did not keep `initializeFailureCount()` live. After fixing the link probe in `target/esp32_adapter_link/main.cpp`, the incremental rebuild succeeded in 100.83 s. Final RAM 67,356/327,680 B (20.6%); application flash 935,792/1,310,720 B (71.4%).
- Final ELF verifier `./tools/verify_v0.2.76.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` passed 12/12 wrapper routes, 7/7 NVS backend/guard/AP/PIN symbol checks and 9/9 LittleFsFileSet symbol checks, exit 0. Final ELF SHA-256 `3e5989be6ef00ed708159e7a17bd70618f7c8b2955a6e597bc5465301bf359ec`; BIN SHA-256 `20138030906d1f619be41736c5303b091c0ab8462906d56be52ecd17a6f252c7`. Logs: `/tmp/att_v0276_esp32_build.log`, `/tmp/att_v0276_esp32_build2.log`, `/tmp/att_v0276_elf_verify.log` (initial fail) and `/tmp/att_v0276_elf_verify2.log` (final pass).
- No target flash, runtime NVS, hardware probe, format/erase, SF2 test or physical-device test was performed.
- Critical limitation: these guards check only the Arduino pre-setup startup snapshot. They do not prove `ConfigBootCoordinator::start()` has run or reached `READY` / `NEEDS_SETUP`; the production root must still enforce that sequence. C34 remains blocking because the erase wrapper is not a complete firewall and no production root exists.

## v0.2.75 — fail-closed NVS config boot gate

- Test-first change in Esp32NvsConfigBackend::begin(): use the Arduino pre-setup startup snapshot, refuse unsafe initialization and do not call nvs_flash_init() again. The config boot coordinator latches CFG_BOOT_FAULT before this backend boundary.
- The corrected red run reproduced four intended failures in test_nvs_config. The earlier red attempt also showed one fixture issue: the legacy PIN-store pre-init case inherited the new initialized-driver default; it was corrected to set the fake driver uninitialized explicitly. The red PlatformIO runner emitted SIGILL after its failure summary and is not counted as a normal completion.
- Final owning command: pio test -e native -f 'test_nvs_config' — 46/46 passed, exit 0.
- Directly dependent policy command: pio test -e native -f 'test_nvs_startup_guard_policy' — 9/9 passed, exit 0. Selected total 55/55 across two distinct suites.
- Target command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — SUCCESS, 2,086.96 s. RAM 67,356/327,680 B (20.6%); application flash 935,040/1,310,720 B (71.3%).
- Exact ELF SHA-256: 1e959f325be97ee57c13eb852d40bdc40666cbbaceace8933f56a83d9c0ac57e. Final-ELF inspection: 12/12 NVS wrapper routes; Esp32NvsConfigBackend::begin() and Esp32NvsStartupGuard::status() retained; all 9 LittleFsFileSet methods retained.
- At the v0.2.75 snapshot, inventory was 630 C++ RUN_TEST registrations across 35 suites; SF2 was 257 Node registrations across 12 files. The other 575 C++ registrations were not run in that increment. No full native, SF2, hardware-probe, flash, runtime NVS/LittleFS, power-cut or physical-device test was performed. N1 remains deferred.

## v0.2.74 — C34 NVS erase-surface audit (documentation-only)

- Read the pinned Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 headers, package sdkconfig and exact prior v0.2.72 inert adapter-link ELF.
- Exact prior ELF: `./tools/verify_v0.2.74.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` — passed 12/12 NVS wrapper routing and 9/9 LittleFsFileSet symbol retention, exit 0. ELF SHA-256: `bfbb50a538582d53d101a80ad0e01b2dcefc7e7d4f1e84f61bfe012af0b8154b`. This is a re-inspection of the prior image, not a v0.2.74 target build.
- Static audit: exact whole-DATA_NVS partition-range erase is blocked; chunked ranges, DATA_NVS_KEYS and direct lower-level flash erase APIs are not generally covered. NVS bulk erase may deinitialize handles before wrapper rejection. Current ELF has no observed direct app call to `esp_flash_erase_region()`; only the partition erase implementation calls it. The public API remains a future bypass surface.
- Package sdkconfig confirms `CONFIG_VFS_SUPPORT_DIR=y` and LittleFS parameters; the pinned header exposes `ESP_LITTLEFS_ENABLE_FTRUNCATE`. This is not runtime ftruncate or filesystem durability evidence.
- No PIO test, SF2 test, target rebuild, hardware probe, flash or physical-device test was run. Source, test registrations, API and persistent formats are unchanged.

## v0.2.73 — C13.2 recovery protocol proposal (documentation-only)

- Added docs/SPECS/littlefs_recovery_protocol.md as a proposal only; it is NOT APPROVED FOR IMPLEMENTATION.
- The proposal records the current LogStore::open() EOF-suffix truncation ambiguity, the non-journaled MonthSet::repair() copy/move/truncate sequence, host-fake limitations, required recovery invariants, transaction concepts, restart states, fault matrix, target/physical verification requirements, and D1–D11 decisions requiring explicit approval.
- Updated docs/SPECS/month_set.md without changing its DELEGATED status or enabling destructive methods.
- No C++/Node source, test registration, API, persistent format, firmware target, or runtime behavior changed.
- No PIO native suite, SF2 test, target build, ELF inspection, flash, or physical-device test was run. This is a documentation/design increment; the v0.2.72 test/build evidence remains historical and unchanged.

## v0.2.72 — host append fault injection

- Current inventory: **628 C++ RUN_TEST registrations across 35 native suites**; SF2 remains 257 Node registrations across 12 test files. Seven C++ registrations were added in test_littlefs_fileset; no suite directory was added.
- New native-only per-instance seam injects appendFile write, fsync, verification pread and close outcomes. ESP_PLATFORM uses the original direct syscalls; the test header explicitly rejects ESP_PLATFORM inclusion.
- Test-first red phase: pio test -e native -f 'test_littlefs_fileset' reported seven new assertion failures and 20 prior passes, then the PlatformIO wrapper printed SIGBUS with an inconsistent summary count. The direct binary reproduced the assertions without SIGBUS: **27 registrations, 7 failures, 20 passes**, exit 7. Log: /tmp/att_v0272_red_direct.log. PIO wrapper log: /tmp/att_v0272_red_test_littlefs.log.
- Final owning command: pio test -e native -f 'test_littlefs_fileset' — **27/27 passed**, exit 0; exact log: /tmp/att_v0272_green_test_littlefs.log.
- Cases cover successful short-write looping, write EINTR retry, 7-byte prefix then EIO, fsync failure after full write, verification pread failure after full write, read-back mismatch, and close error after the POSIX descriptor is actually closed.
- The failure cases assert the important contract: false does not prove that nothing was written. The complete payload may remain visible after sync/read-back/close failure; a partial prefix remains after the hard write error. The synthetic close test does not model a real ESP-IDF descriptor-close failure or descriptor leak.
- No MonthSet/LogStore shared contract or implementation changed, so test_monthset was not rerun. The owning suite already contains MonthSet + LogStore rollover/reopen and torn-tail fail-closed checks.
- Affected inert ESP32 adapter-link rebuild: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — passed, exit 0, 1,769.31 s. Pinned toolchain: Espressif32 8.0.0 / ESP32 Dev Module, Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121. RAM 67,356/327,680 B (20.6%); flash 934,976/1,310,720 B (71.3%). Exact ELF inspection passed 12/12 NVS wrapper-routing checks and retained all nine LittleFsFileSet methods. This is compile/link/static evidence only. No mounted LittleFS runtime, target VFS fault injection, destructive recovery, power cut, full native or SF2 suite, hardware probe, flash or physical-device test has been performed.

## v0.2.71 — LittleFsFileSet ESP32 compile/link compatibility

- Current inventory remains 621 C++ RUN_TEST registrations across 35 native suites; no test registration was added.
- Owning host suite: pio test -e native -f 'test_littlefs_fileset' — **20/20 passed**, exit 0 after the target-compatibility patch. The host cases retain symlink-root and symlink-month rejection through POSIX lstat().
- Initial ESP32 target attempt failed at compile time because pinned newlib does not declare lstat(). The first attempt ended exit 1 after 1,220.24 seconds. The adapter now uses stat() only under ESP_PLATFORM; POSIX host behavior remains lstat().
- Final exact target command: pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2 — **SUCCESS**, exit 0, 1,522.09 seconds. Platform: Espressif32 8.0.0; board esp32dev; Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5; Xtensa 14.2.0+20260121. RAM 67,356/327,680 bytes (20.6%); application flash 934,976/1,310,720 bytes (71.3%).
- tools/verify_v0.2.71.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf" — **12/12 NVS wrapper-routing checks plus 9/9 LittleFsFileSet symbol-retention checks passed**, exit 0.
- The smoke target only constructs/retains the adapter and its public methods. It performs no mount, open, read, write, truncate or quarantine operation. No LittleFS runtime, physical device, power-cut, brownout, hardware-probe, flash, SF2 or full native test was performed.
- The other 601 C++ registrations were not run. The v0.2.69 43/43 roster result remains historical and was not rerun. C13 remains blocking.

## v0.2.70 — ESP32 smoke-link and final-ELF audit footprint

- Current inventory: 621 C++ `RUN_TEST` registrations across 35 native suites; no registrations were added or changed.
- First `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2` attempt failed during `main.cpp` compilation because the smoke target retained old mutable/private AS608 member-function anchors. This was a harness compile failure, not a production API regression.
- After correcting the smoke target to current public const accessors and removing private-method anchors, the same command completed successfully, exit 0.
- Target: `esp32dev`, Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, Xtensa 14.2.0. PlatformIO size: RAM 67,308/327,680 bytes (20.5%); app flash 927,120/1,310,720 bytes (70.7%).
- `xtensa-esp-elf-nm -S -C --size-sort .pio/build/esp32-adapter-link/firmware.elf` measured `setup()::rosterAuditSmoke` as `0xE2C` = 3,628 bytes BSS. This is the complete object size, not the incremental cost of the newly added field.
- `tools/inspect_probe_wrappers.sh "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` — 12/12 final-ELF route checks passed, exit 0. The script's mode was corrected from 0640 to 0755, then direct execution passed.
- No native or SF2 suite was run in this increment. The v0.2.69 roster suites remain historical evidence (43/43), not a v0.2.70 rerun. The other 578 registrations were not run. No hardware-probe build, flash or physical test.

## v0.2.69 — separated roster effective-day and lifecycle-day tracking

- Current inventory: 621 C++ RUN_TEST registrations across 35 native suites.
- Test-first red run: the four new replay regressions were added before implementation. `pio test -e native -f 'test_roster_snapshot'` produced two runtime assertion failures (both valid rename/lifecycle same-day cases returned `RPR_TRANSITION_INVALID`), while the backwards-date and E27 controls passed. Exit 1; no compile failure was involved.
- After implementation, `pio test -e native -f 'test_roster_snapshot' -f 'test_month_roster'` passed **38/38** before the additional direct audit-code tests were added.
- Final expanded snapshot audit: `pio test -e native -f 'test_roster_snapshot'` — **26/26 passed**, exit 0. The dependent month-projection suite passed **17/17**, exit 0, in the preceding combined run; no month-test source changed afterwards.
- Final release verifier reruns both suites and checks the complete manifest and registration inventory.
- The other 578 registrations were not run. No full native/SF2 run, ESP32 target, ELF, flash or hardware verification.
- U61 remains open: the prefix/report projection path uses the semantic audit, but operational `RosterStore::historyComplete()` and all live writers/consumers do not. E27's same-day lifecycle restriction remains reporting-only and unchanged.

## v0.2.68 — success-only ReqRing completion

- Current inventory: 611 C++ RUN_TEST registrations across 35 native suites.
- Added a regression proving `finish()` rejects `A_INVALID`, leaves the current reservation pending, and that an explicit abort permits a retry whose `A_OK` result is then replayable.
- Exact test-first command before implementation: `pio test -e native -f 'test_api'` — compile failed as expected because `finish()` returned void; 0 tests executed, exit 1.
- Final command after implementation: `pio test -e native -f 'test_api'` — **41/41 passed**, exit 0.
- Static call-site audit: `ReqRing` has no production caller in `lib/`; all current uses are in `test/test_api`.
- The other 570 registrations were not run. No SF2, ESP32 target, ELF, flash or hardware verification.
- `finish()` returns false for failure codes, stale tickets, and zero tickets. A failed result leaves a current reservation pending; caller must explicitly abort. This does not solve uncertain storage commit or durable exactly-once semantics (U112).

## v0.2.67 — reservation-ticketed ReqRing completion

- Current inventory: 610 C++ RUN_TEST registrations across 35 native suites.
- Added a regression proving that after a pending reservation expires and the same req_id plus fingerprint is reused, the old handler's ticket cannot finish or abort the new reservation.
- Exact test-first command before implementation: `pio test -e native -f 'test_api'` — compile failed as expected because the ticket field and ticketed methods did not exist; 0 tests executed, exit 1.
- Final command after implementation: `pio test -e native -f 'test_api'` — **40/40 passed**, exit 0.
- The other 570 registrations were not run. No SF2, ESP32 target, ELF, flash or hardware verification.
- Important limitation: expiry does not cancel the old handler or reverse side effects. The future caller must stop/serialize/reconcile expired work before retrying. ReqRing remains RAM-only and is not durable exactly-once protection; U112 remains unresolved.

## v0.2.66 — checked API input/output boundaries

- Current inventory: 609 C++ RUN_TEST registrations across 35 native suites.
- Added coverage for reqFingerprint() null/nonzero input rejection, unchanged output on failure, valid zero-length input and distinct valid digests.
- Added coverage for null Event* destinations to buildMark(), buildNoClass(), rosterSet(), rosterDeactivate() and rosterActivate(); all return A_INVALID and set count to zero.
- Added direct RosterStore coverage for RE_BAD_OUTPUT from planSet(), planDeactivate() and planActivate(); the enum value was appended so existing values are preserved. Null nameOf() destinations safely return zero without changing roster state.
- Exact focused commands:
  - pio test -e native -f 'test_api' — 39/39 passed, exit 0.
  - pio test -e native -f 'test_roster' — 15/15 passed, exit 0.
  - pio test -e native -f 'test_roster_history' -f 'test_month_roster' — 23/23 passed, exit 0.
- Exact final release command: ./tools/verify_v0.2.66.sh targeted — **77/77 passed**, exit 0.
- Total selected verification: **77/77 across 4 suites**. The other 532 registrations were not run. No full native/SF2 suite, ESP32 target, ELF, flash or hardware test.
- Test-first note: the new tests were written before implementation. The first compile attempts failed because the new checked fingerprint signature and RE_BAD_OUTPUT enum had not yet been added; the final interfaces and implementations then compiled and passed the focused suites. Those initial compile errors are not being represented as runtime red-phase evidence.

## v0.2.65 — null-pointer handling for session tokens

- Current inventory: 607 C++ RUN_TEST registrations across 35 native suites.
- Test-first regression: before the fix, the new last-registered test crashed `test_api` with SIGSEGV at `TokenTable::check(nullptr, nowS)` after all 37 prior assertions passed.
- Added fail-safe behavior for null token checks, null revocation, and `ctEqual()` with null operands and nonzero length. The regression then passed, proving existing sessions remain live and LRU order is unchanged after null operations.
- Exact final command: `pio test -e native -f 'test_api' -f 'test_pin_auth_session'` — **49/49 passed**, exit 0; `test_api` 38/38 and `test_pin_auth_session` 11/11.
- The other 558 registrations were not run. No full native/SF2 suite, ESP32 target, ELF, flash or hardware test.

## v0.2.64 — PIN lockout deadline wrap regression

- Current inventory: 606 C++ RUN_TEST registrations across 35 native suites.
- Test-first regression: before the fix, test_api failed because a fifth-failure lock whose deadline wrapped to zero was interpreted as unlocked. The same case also covers a lock reconstructed from the persisted failure count.
- Exact final commands:
  - pio test -e native -f 'test_api' — 37/37 passed, exit 0.
  - pio test -e native -f 'test_pin_auth_session' — 11/11 passed, exit 0.
- Exact final release command: ./tools/verify_v0.2.64.sh targeted — 48/48 passed across both suites, exit 0; aggregate suite time 40.381 seconds. The other 558 registrations were not run. No full native/SF2 suite, ESP32 target, ELF, flash or hardware test.

## v0.2.63 — final GAP arithmetic and caller-preflight regression

- Current inventory: **605 C++ RUN_TEST registrations across 35 native suites**.
- Final focused set: **246 registrations across 12 affected suites**. Exact final command tools/verify_v0.2.63.sh targeted passed 246/246, exit 0, runtime 3:40.440: test_roster 14, test_roster_history 7, test_api 36, test_enroll 40, test_as608_pipeline 13, test_coreb 44, test_coreb2 17, test_roster_snapshot 17, test_month_roster 16, test_restore_replay 15, test_hardening 15, test_attendance_admission 12.
- Latest final-source dependent batch: pio test -e native -f test_coreb2 -f test_coreb -f test_hardening -f test_restore_replay — **91/91 passed**, exit 0.
- This includes red-to-green tests for 32-bit minute-rounding overflow, a near-UINT32_MAX derived endpoint, and caller-level rejection before any write in appendGaps() and applyClockSet().
- Other **359 registrations were not run**. No full native suite, SF2 Node suite, ESP32 target build, ELF inspection, flash, actual VFS fault injection or power-cut test.

## v0.2.62 — final focused verification for roster integrity and clock commit contract

- Current source inventory: **601 C++ RUN_TEST registrations across 35 suites**.
- **242 registrations across 12 directly affected suites passed** in the current source tree: test_roster (14), test_roster_history (7), test_api (36), test_enroll (40), test_as608_pipeline (13), test_coreb (43), test_coreb2 (14), test_roster_snapshot (17), test_month_roster (16), test_restore_replay (15), test_hardening (15), and test_attendance_admission (12).
- Test execution was in separate targeted batches (not one 242-case invocation). The other **359 registrations were not run**.
- Exact commands and run results are recorded in RELEASE_v0.2.62.md. Key final runs: test_coreb + test_restore_replay 58/58; test_hardening 15/15; test_attendance_admission 12/12; latest test_roster 14/14.
- No full native run, SF2 Node test run, target build, ELF inspection, flash, physical sensor, VFS fault injection or power-cut test was performed.

## v0.2.61 — roster-history integrity WIP completion

- RosterStore historyComplete() includes !col_ so an unfinished RA_SET payload group is untrusted before malformed() increments.
- Fail-closed consumer tests exercise this exact pending-group state in API manual mark/enrollment gates, enrollment FSM begin, attendance scan begin, and ClosureEngine through EnrolledRoster.
- Complete-write-then-false roster retries are reopened/replayed and checked for state-based no-op behavior for SET/rename/deactivate/activate.
- Initial seven-suite batch passed 163/163; roster projection suites passed 33/33; strengthened four-suite batch passed 103/103; adapter closure check passed 14/14. These runs overlap and must not be summed as unique cases.

## v0.2.60 — indeterminate attendance append result and replay suppression

- Added `ATT_PIPE_COMMIT_UNCERTAIN` for a false `LogStore::append()` result. `ATT_PIPE_NOT_SAVED` remains reserved for failures before a log append is attempted. The attendance pipeline no longer claims a failed append definitely was not saved.
- Added fake storage that writes the full EV_SCAN record but returns false once. The test confirms the live LogStore faults and retains its pre-append count, a fresh LogStore reopens and replays the complete record, LogAttendanceAdmission rebuilds the student/session scan state, and a repeated physical scan returns `ATT_PIPE_ALREADY_RECORDED` without appending another event.
- Existing definite pre-write failure fixture now expects `ATT_PIPE_COMMIT_UNCERTAIN` too, because the IStorage boolean contract cannot prove that false means zero bytes were written.
- Exact command: `pio test -e native -f 'test_as608_pipeline' -f 'test_attendance_admission'`. Result: **24/24 passed**, exit 0 (`test_attendance_admission` 12/12; `test_as608_pipeline` 12/12; 61.349 s aggregate suite time).
- Test-first evidence: after adding the new result and changing the expected status, the pipeline suite failed both append-failure assertions against the old implementation (expected result 13, observed 11); after mapping append false to `ATT_PIPE_COMMIT_UNCERTAIN`, the 12 pipeline tests passed. The directly related 12-case admission suite passed as well.
- Scope limitation: this verifies attendance first-scan-wins only when a complete record is recovered. It does not solve request idempotency for clock changes, roster writes, GAP records or future APIs. A partial append may still make reopen fail because the current filesystem adapter cannot safely truncate (C13). No production UI/HTTP caller exists (C3).
- Inventory: **585 C++ registrations across 35 suites**; **561 were not run** in this increment.

## v0.2.59 — GAP coverage-index coalescing and retry regression

- Changed `LogIndex` to maintain a union of overlapping or adjacent half-open GAP spans rather than allocating one slot per `EV_GAP` record. This preserves `gapOverlaps()` semantics while preventing repeated complete-write retries and adjacent `appendGaps()` chunks from exhausting the 512-span cache.
- Invalid zero-length or epoch-overflowing GAP spans now set `gapOverflow_` and are not cached. A representable span ending exactly at `UINT32_MAX` remains valid.
- Added a complete-write-then-false fake-storage test: reopen accepts the persisted GAP, 512 repeated retries retain one index span, and a subsequent closure remains covered when the GAP is outside its scan window. Added overlap/adjacency transitive-merge, malformed-boundary and distinct-span overflow regressions.
- Changed generic overflow fixtures to use genuinely disjoint spans; 513 distinct spans still set overflow and force Unverified coverage.
- Exact command: `pio test -e native -f 'test_coreb2' -f 'test_restore_replay'`. Result: **29/29 passed**, exit 0 (40.395 s aggregate suite time).
- Test-first evidence: before the implementation, the new retry regression failed because `gapCount()` reached 512 after retries. After union coalescing, all 29 targeted tests passed.
- Original GAP event records remain in the append-only log. This does not make `appendGaps()` exactly-once: duplicate retries still consume log storage, and production request idempotency remains U112. Host fakes only; no LittleFS/VFS, ESP32, power-cut or hardware test.
- Inventory: **584 C++ registrations across 35 suites**; **555 were not run** in this increment.

## v0.2.58 — append failure and indeterminate commit semantics

- Added four test_monthset registrations using a fault-injecting FakeFiles backend:
  - a partial byte write returns false, latches LogStore, does not notify observers, and blocks in-session retries;
  - a complete record written before a simulated later failure returns false but is visible after a fresh LogStore::open() (indeterminate commit);
  - a partial two-record commit group containing only its first FL_MORE record is not replayed as a committed prefix when required truncation is unavailable;
  - a partial byte tail remains unchanged and reopen fails closed when the backend refuses truncation.
- Exact command: pio test -e native -f 'test_monthset'. Result: **18/18 passed**, exit 0 (19.207 s).
- Test-first runs initially failed to compile because the fake backend lacked the new failure controls; after adding those controls, the final behavior run passed. No production implementation behavior changed.
- These tests simulate return/write order in a host fake. They are not actual LittleFS short-write/fsync/close fault injection, power-cut simulation, or evidence that the device can recover.

## v0.2.57 LittleFS adapter and MonthSet

- `test_littlefs_fileset`: 20 registrations. Covers missing `/log` as a valid empty set, canonical/numeric month ordering, invalid keys and entries, symlink rejection, safe first-file creation, strict range reads, 16-byte append alignment, refusal of partial pre-existing records, `fsync()`/size/read-back path, month rollover/reopen through `MonthSet` + `LogStore`, and fail-closed torn-tail behavior when truncate is disabled. Uses a temporary host POSIX filesystem.
- `test_monthset`: 14 registrations, rerun as the directly dependent core suite.
- Exact command: `pio test -e native -f 'test_littlefs_fileset' -f 'test_monthset'`. Result: **34/34 passed**, exit 0 (47.513 s aggregate suite time).
- Not evidence: ESP32 compilation, actual LittleFS mount/VFS execution, directory-entry durability, error injection on target, power-cut/brownout recovery, or production integration.

# Test inventory and evidence

## v0.2.56 — mandatory checked storage metadata

- `IStorage::sizeChecked()` is now pure virtual; every storage implementation must distinguish a valid empty stream from a failed metadata query. The base `size()` convenience helper returns `SIZE_MAX` on failure, while boot/replay code uses the checked form.
- `IFileSet::countChecked()`, `monthKeyChecked()` and `fileSizeChecked()` are pure virtual. The unsafe value-only metadata contract has been removed; both native file-set fakes implement the checked forms.
- `MonthSet::sizeChecked()`, `read()`, `append()`, `truncate()`, `planAt()` and `repair()` now use checked metadata and reject metadata errors/overflow. `MonthSet::append()` also verifies that a newly created month file is visible with the requested key, zero length, and exactly one additional entry before writing.
- `LogStore::open()` fails closed on a failed size query and refuses a record count that cannot be represented by its next sequence. `RestoreReplayAudit` distinguishes a failed stage-size query (`RESTORE_REPLAY_STAGE_SIZE_FAILED`) from a real size mismatch.
- Test-first evidence: before the fix, the four new `test_monthset` metadata-failure regressions all failed (10 existing cases passed; process exited 1 after Unity assertion failures). After the fix and mandatory interface conversion, the exact command below passed **264/264 across 15 suites**, exit 0, 4m22.776s:

  `pio test -e native -f 'test_monthset' -f 'test_restore_replay' -f 'test_enroll' -f 'test_api' -f 'test_roster' -f 'test_roster_snapshot' -f 'test_event_group_audit' -f 'test_roster_history' -f 'test_coreb' -f 'test_backup' -f 'test_coreb2' -f 'test_hardening' -f 'test_attendance_admission' -f 'test_month_roster' -f 'test_as608_pipeline'`
- Inventory: **557 C++ registrations across 34 suites**; **293 were not run** in this increment. This is host test/compile evidence only. No LittleFS driver, ESP32 build, ELF inspection, device, or power-cut test was performed.

## v0.2.55 — owner-bound configuration application

- Added `As608OwnerConfigRuntime`, an `IConfigApplyRuntime` adapter for the preferred `As608AttendanceOwner`. It gates config application on startup READY and owner/pipeline idle state, then updates owner pipeline/admission state before calling `ClosureEngine::setSchedule()`.
- New integration test drives the real `ConfigApplyCoordinator` with an in-memory A/B config backend, a real test `ClosureEngine`, the attendance owner and synthetic UART responses.
- It verifies: config apply is BUSY with zero backend writes before startup READY; a live UTC-offset change is BUSY with zero writes because `RosterStore` retains its construction-time offset; config apply remains BUSY with zero writes during a scan; after a no-finger terminal result the scan releases, the next timetable config is durably written/read back and generation 8 is applied; both attendance scan admission and closure scheduling use the new disabled timetable; reapplying the same Config with generation 8 returns UNCHANGED without another write.
- Exact final command: `pio test -e native -f 'test_as608_pipeline'` — exit 0; **11/11 passed**, 39.382 seconds (remote shell runtime 47.96 seconds).
- The new test was test-first: initial compile failed because the adapter header did not exist. A later run exposed a stale expected generation in the unchanged-config fixture; the helper was corrected to send generation 8. A subsequent regression exposed that the pipeline accepted a live UTC-offset change despite the immutable roster offset. The pipeline preflight now rejects that change before storage writes. The final run passed.
- Current static inventory: **552 C++ registrations across 34 suites**; **541 were not run** in this increment. The v0.2.54 `test_as608_gate` result 34/34 is historical and was not rerun.
- No full native suite, other C++ suite, SF2 Node suite, final-source ESP32 compile/link, final ELF inspection, flash, NVS operation, power-cut test or physical hardware test was performed. UART and config storage in the new test are synthetic.

## v0.2.54 — owned occupancy and attendance-only owner

- The preferred `As608Runtime(Stream&, RosterStore&, password)` constructor owns the occupancy map. Public `occupancy()` returns `const Occupancy&`; coordinator-only private access performs startup refresh. Public `enrollment()` returns a const session reference, and public `attendance()` returns a const client view, so callers cannot acquire/sync/abort the runtime-managed enrollment session or start/step raw runtime attendance. The pipeline now binds directly to Runtime through a private friend accessor instead of extracting that client. The injected-map constructor remains available for low-level tests/adapters.
- `As608AttendanceOwner` composes one owned-map runtime, startup coordinator and checked attendance pipeline. Its public API exposes startup begin/step, READY/fault/capacity status, read-only occupancy, checked scan begin/step, and config-after-commit forwarding. It exposes no raw runtime, AS608 client, UART link, enrollment FSM/session or transport.
- Test-first API assertions showed v0.2.53 exposed mutable occupancy/enrollment references; after those were fixed, the added attendance const-view assertion failed against the intermediate tree until raw attendance access was hidden. The first owner test failed because the owner header did not exist. After implementation, the new owner tests verified pre-READY scan rejection, complete synthetic startup, durable match-to-EV_SCAN append, and authentication failure remaining terminal/fail-closed.
- Exact final commands and results:
  - `./tools/verify_v0.2.54.sh targeted` — exit 0; `TARGETED_V0_2_54=PASS`; total 590.06 seconds.
  - Inside verifier, `pio test -e native -f 'test_as608_gate'` — **34/34 passed**, 247.18 seconds.
  - Inside verifier, `pio test -e native -f 'test_as608_pipeline'` — **10/10 passed**, 257.67 seconds.
  - Total: **44/44** focused tests.
- Current inventory: **551 C++ registrations across 34 suites**; **507 were not run** in this increment.
- No full native suite, SF2 Node suite, final-source ESP32 smoke build, final ELF inspection, flash or hardware test was performed. The v0.2.54 target compile/link is [UNVERIFIED: run the inert ESP32 smoke target against the final source].

## v0.2.52 — AS608 startup coordinator

- Added `test_startup_*` cases in `test_as608_gate`: complete occupancy refresh before READY (including invalidation of a stale-valid map), partial index failure, enrollment/attendance capacity disagreement, poisoned transport retaining the reservation, and enrollment-side authentication rejection releasing a drained setup lease.
- Final focused results: `pio test -e native -f 'test_as608_gate'` **31/31 passed**; `pio test -e native -f 'test_as608_pipeline'` **8/8 passed**. Total **39/39**.
- Test-first proof: the gate suite initially failed to compile because the one-argument coordinator API and runtime occupancy accessor were absent. The pipeline suite also initially failed to compile because the occupancy dependency had not yet been added to its constructor. After both narrow implementations, the final gate and pipeline suites passed.
- Pipeline regression additionally verifies an invalid map blocks scan start and that invalidation after scan start blocks the `EV_SCAN` append; the existing six cases remain passing.
- Static inventory: **546 C++ registrations across 34 suites**. The other **507** were not rerun.
- The inert ESP32 smoke target now anchors the startup constructor via a static non-serviced object, `begin()`, `step()`, `As608Runtime::occupancy()` and `invalidateReady()`. The target build is documented in RELEASE_v0.2.52.md; this does not flash or run the coordinator.
- No physical sensor, Job 11, E18 page mapping, full native suite or SF2 suite was tested.

## v0.2.51 — AS608 attendance pipeline integration coverage

- Added test_as608_pipeline with six PlatformIO registrations. Tests run through the actual As608Runtime composition root and cover Present/Late event commit, duplicate suppression, unresolved injected page mapping, log append failure and no-finger handling.
- Targeted verifier passed: NVS policy 9/9; probe credential 4/4; AS608 adapter 15/15; AS608 gate 26/26; attendance pipeline 6/6; log admission 12/12; enrollment 36/36. **108/108 across seven suites.**
- ELF-inspector fixtures: 11/11. ./tools/verify_v0.2.51.sh targeted exited 0 and printed TARGETED_V0_2_51=PASS.
- Static inventory: **539 C++ registrations across 34 suites**. The other 431 registrations were not rerun.
- No ESP32 target rebuild, SF2 test, flash or hardware test was performed. v0.2.50 target/ELF results remain historical evidence only.
- E18 remains delegated: the physical-page mapping in the new tests is a fixture, not a product rule. C3 production scheduler/pipeline integration remains incomplete.

## v0.2.50 — targeted verification and AS608 target composition
- Fixed native compilation of `esp32_nvs_startup_guard.cpp` by compiling the ESP-IDF implementation only when `ARDUINO_ARCH_ESP32` is defined; the pure policy remains host-testable.
- Targeted results: NVS policy 9/9; probe credential 4/4; AS608 gate 26/26; AS608 adapter 15/15; enrollment 36/36. **90/90 passed.** The remaining 443 registrations were not rerun.
- Inert `esp32-adapter-link` build: SUCCESS; RAM 63,652 B (19.4%); flash 926,248 B (70.7%); 1,144.83 s.
- Diagnostic `esp32-hardware-probe` build: SUCCESS; RAM 46,764 B (14.3%); flash 928,880 B (70.9%); 1,023.89 s.
- Both exact final ELFs independently reported `NVS_WRAPPER_ROUTING=PASS`. This proves only the inspected linked call routes.
- ELF inspector fixture harness passed 11 scenarios: three accepted paths and eight rejected/missing-route cases, including an internal Xtensa branch target.
- The two PlatformIO configs share `.pio/build`; the workflow now inspects each ELF immediately after its build. Neither image was flashed. Physical behavior remains unverified.


## v0.2.50 — targeted tests and both ESP32 target builds

- Targeted PlatformIO native results: test_nvs_startup_guard_policy 9/9, test_hardware_probe 4/4, test_as608 15/15, test_as608_gate 26/26, test_enroll 36/36. Total: 90/90.
- Static inventory remains 533 registrations across 33 suites. The other 443 registrations were not run in this increment; do not infer that they passed.
- ELF-inspector fixtures passed 11/11. These synthetic fixtures test inspector matching/failure behavior, including internal Xtensa branch-target handling.
- ESP32 adapter smoke target: SUCCESS; RAM 63,652 B (19.4%), flash 926,248 B (70.7%); 1,144.83 seconds.
- ESP32 hardware-probe target: SUCCESS; RAM 46,764 B (14.3%), flash 928,880 B (70.9%); 1,023.89 seconds.
- Final-ELF inspection passed for both images: Arduino initArduino() reaches both wrappers; each wrapper delegates to the real IDF function; default, named-partition and partition-pointer erase APIs reach the wrapped partition-erase boundary.
- Toolchain: Espressif32 8.0.0, Arduino-ESP32 3.3.12 / IDF 5.5.5, esptool 5.4.0, Xtensa 14.2.0+20260121, SSD1306 2.5.17, GFX 1.12.6, BusIO 1.17.4.
- No device was flashed. No hardware behavior or production firmware behavior is proven.


## v0.2.48 — shared NVS startup guard (initially unrun; targeted suites passed in v0.2.50)

- Moved the whole-DATA_NVS erase policy from the diagnostic sketch into `lib/hal_esp32/src/nvs_startup_guard_policy.h`; moved the GNU ld wrappers and startup status snapshot into `esp32_nvs_startup_guard.*`. Both ESP32 target configurations now link the shared guard.
- Added `test_nvs_startup_guard_policy` with nine registrations: exact full-partition block; ordinary sector erases; non-NVS/partial/unknown-size allowances; and fail-closed startup status, including missing/multiple pre-setup init calls. `test_hardware_probe` retains four credential-encoding registrations; its four old policy registrations were removed.
- Static inventory: **533 C++ `RUN_TEST` registrations across 33 suites**. The user-reported 520/520 PlatformIO result covers v0.2.46 only. The 13 current probe/policy registrations have not been run. No v0.2.48 compiler, PlatformIO, ESP32 target or device execution was performed.
- Both smoke and probe targets now carry the linker-wrap flags. `tools/verify_v0.2.48.sh` batches native tests, both target builds and inspection of both final ELFs. Its success would prove host test/link routing only, not NVS runtime safety or device behavior.
- Scope remains narrow: the guard blocks a whole DATA_NVS `esp_partition_erase_range` call only. Pinned ESP-IDF 5.5.5 source routes the standard NVS bulk-erase APIs through this function, and the final-ELF inspector checks those call chains. Smaller ranges remain allowed, so chunked full-partition erasure and alternate/direct flash erase paths are not blocked. The standard NVS erase APIs may deinitialize handles before the lower-level erase is rejected. This is not a generic erase firewall or a production startup solution. Do not induce NVS corruption on the only device.

## v0.2.47 — diagnostic hardware probe and NVS erase guard (not run)

- Added `test_hardware_probe` with eight registrations: four for exact 128-bit to 32-character lowercase-hex credential encoding, invalid input/output handling and full output-buffer clearing; four for the NVS erase guard's exact scope (block only a full DATA_NVS partition erase, permit sector erase, permit full non-NVS erase, reject invalid ranges).
- Static inventory: **528 C++ `RUN_TEST` registrations across 32 suites** (previous snapshot 520 across 31). The new eight registrations have **not** been run; no compiler or PlatformIO execution was performed for v0.2.47 in this environment.
- Added a separate `platformio.esp32-probe.ini` target. It is not built yet. The inert v0.2.46 adapter smoke result does not verify it. The NVS wrapper's final ELF routing must be checked, not inferred from a successful compile.
- Both targets must be built only after reading C34. Run `./tools/inspect_probe_wrappers.sh` after linking; it must prove `initArduino()` calls both wrappers, the wrappers delegate to the real IDF functions, and the standard NVS bulk-erase APIs route through the partition-erase wrapper. Do not flash based only on a successful build or symbol presence. The probe aborts before I2C/OLED/AP if NVS init fails. Do not induce NVS corruption to test the failure branch on the only device.

## v0.2.46 — fixed first-provision AP identity

- The user supplied the complete PlatformIO result: **520 test cases: 520 succeeded in 00:08:06.877**. This covers all 520 v0.2.46 native registrations across 31 suites.
- The user supplied the complete `esp32-adapter-link` result: **SUCCESS in 548.96 seconds** using Espressif32 8.0.0, Arduino framework/ESP-IDF libraries 3.3.12, esptool 5.4.0, Xtensa 14.2.0+20260121, SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. Reported RAM 47,012 bytes (14.3%); flash 912,216 bytes (69.6%).
- The target is an inert compile/link smoke image. These results prove native tests and adapter compile/link, not flashing, boot, radio behavior, OLED visibility, DHCP, NVS durability or production firmware.

---


## v0.2.45 — first-provision coordinator and DHCP assignment gate

- Added `test_ap_credential_setup` with **18** registrations covering NEEDS_SETUP-only entry, argument validation before secret disclosure, OLED-transfer-before-AP ordering, display/AP failure handling, retries when a post-disclosure blank-frame request fails, AP readiness and DHCP gating, request-scoped authorization, exact AP credential persistence into `Config::wifiPass`, correctable invalid config and ambiguous storage failure.
- The new core suite passed locally under ASan/UBSan with a temporary Unity-compatible stand-in: **18 tests, 145 assertions, 0 failures**. This is not PlatformIO Unity evidence.
- Extended `test_esp32_wifi_ap` from 20 to **22** registrations for `IP_EVENT_AP_STAIPASSIGNED`: ignore leases on other netifs, accept only this AP netif, clear the assignment latch on a new/closed AP run, and retain callback context safely through unregister failures. The updated AP suite passed locally under ASan/UBSan stand-in: **22 tests, 315 assertions, 0 failures**. This is not PlatformIO Unity evidence.
- Re-ran the unchanged OLED stub suite: **13 tests, 67 assertions, 0 failures** under ASan/UBSan. This does not test a physical panel or Adafruit I2C acknowledgement.
- Current source inventory: **520 C++ registrations across 31 suites**. The user reports the previous v0.2.44 tree passed **500/500** PlatformIO native cases; the 20 additional v0.2.45 registrations (18 coordinator, 2 AP DHCP) have only stand-in execution so far. Full v0.2.45 PlatformIO and target smoke builds remain pending.
- The user also reports the v0.2.44 expanded ESP32 smoke build passed with PlatformIO Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12, Xtensa toolchain 14.2.0+20260121, Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. It used 24,028 bytes RAM (7.3%) and 350,004 bytes flash (26.7%). This result predates the v0.2.45 coordinator/DHCP callback changes.
- **Subsequent verification:** the user reports that v0.2.46 passed all 520/520 PlatformIO native cases and that the expanded inert adapter smoke target compiled/linked successfully. This includes the v0.2.45 coordinator/AP additions and v0.2.46 OLED/start-signature change; it does not include the v0.2.47 probe suite or target.
- SF2 source is unchanged. Reran `npm test`: **257/257 passed**. Ran `node --check` across all **25** `.mjs` files; all passed.

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_ap_credential_setup -vvv 2>&1 | tee ap-credential-setup.log
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap.log
pio test -e native 2>&1 | tee native-test.log
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-target-smoke.log
(cd web/sf2 && npm test)
```

---

## v0.2.44 OLED adapter


- Added `test_esp32_oled` with **13** registrations for constructor side effects, 128x32/0x3C configuration, shared-bus ownership, begin failure/retry/idempotence, disclosure gating, exact 32-character two-row rendering, READY/failure clearing, invalid credential rejection, caller-buffer preservation and framebuffer clearing.
- Local `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined` run using a temporary Unity-compatible stand-in: **13/13 tests, 67 assertions passed**. This validates adapter logic against the display stub only; it is not PlatformIO/Unity or actual Adafruit-library verification.
- At the v0.2.44 release point the inventory was **500 C++ registrations across 30 suites**. The user subsequently reported all 500/500 passed in PlatformIO; v0.2.45 adds a new suite and extends the AP suite.
- SF2 suite rerun in v0.2.44: **257/257 passed**; all 25 `.mjs` modules pass `node --check`.
- The user subsequently reports the expanded v0.2.44 target smoke build passed with Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, esptool 5.4.0, Xtensa toolchain 14.2.0+20260121, SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4. The v0.2.45 target rebuild is pending.

## v0.2.43 — ESP32 adapter target smoke project

- Added an isolated `platformio.esp32-smoke.ini` target project. It pins PlatformIO Espressif32 `8.0.0`, board `esp32dev`, and the Arduino framework. The v8.0.0 release supports Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5; the platform manifest constrains its package versions, and exact resolved package revisions must be recorded from target build output. It does not alter the default native `platformio.ini` configuration.
- Added `target/esp32_adapter_link/main.cpp` to keep direct references to the Wi-Fi AP, RF RNG, bootstrap RNG, config boot coordinator, core AP credential bootstrap coordinator, NVS AP credential, NVS config and NVS PIN-attempt adapter methods. Its global constructors and setup perform no AP start, entropy request, NVS initialization or storage write. It exists only to force target compilation/linkage of those adapters.
- Host-side `g++ -std=gnu++11 -Wall -Wextra -Werror -fsyntax-only` passed for the smoke translation unit against the project's existing ESP-IDF test stubs. This checks C++ declaration/member-pointer syntax only; it is not an ESP32 compile or link.
- The user reports that all 29 PlatformIO native suites in v0.2.42 passed. The complete inventory is 487 registrations across 29 suites. The user also reports the full v0.2.42 SF2 run passed; this release independently reran `npm test` and confirmed **257/257**.
- The user subsequently reported that the v0.2.43 target smoke build passed; that result predates the OLED adapter and Adafruit library dependencies. The expanded v0.2.44 target build remains the next verification step. Do not flash the smoke image as production firmware.

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

---

## v0.2.42 — AP suite host-link shim correction

- User-provided v0.2.41 PlatformIO output collected all 29 native suites. The user reports every suite except `test_esp32_wifi_ap` passed. The AP test object compiled, but final linking failed with `undefined reference to esp_fill_random`; therefore its 20 test cases did not execute in that run. This was a host-test dependency gap, not evidence of an ESP32 runtime failure. The user subsequently reports the full v0.2.42 native run passed all 29 suites, including the corrected AP suite.
- Root cause: each PlatformIO suite builds a separate executable. `test_esp32_wifi_ap` links `esp32_secure_random.cpp` through `Esp32WifiAp`, but only `test_esp32_secure_random` defined the `esp_fill_random` fake. The header declaration was present; the AP executable lacked the definition.
- v0.2.42 adds a deterministic `esp_fill_random` implementation inside `test/test_esp32_wifi_ap/test_main.cpp` only. No production source, library, API or test registration count changed.
- The existing AP/RNG integration test now verifies that `Esp32SecureRandom::fill()` refuses before AP start, succeeds only after AP_START opens the RF gate, writes the deterministic test bytes, then refuses after `Esp32WifiAp::stop()` closes the gate. AP suite inventory remains 20 registrations.
- Focused local check: the AP suite compiled with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined` and ran under a temporary Unity-compatible stand-in: **20 tests, 267 assertions, 0 failures**. Leak detection was disabled because the existing stop-failure case intentionally retains the callback event group until reboot. This is not PlatformIO/Unity evidence.
- The user subsequently reported that all 29 v0.2.42 PlatformIO native suites passed. No ESP32 target build or device verification is established.
- Source inventory remains **487 C++ registrations across 29 suites**; no registrations were added. SF2 remains **257/257 passed**; all 25 `.mjs` files pass `node --check` in the v0.2.42 tree.

The native verification commands used for v0.2.42 were:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

---

## v0.2.41 — first-provision AP credential bootstrap

- Added 18 core `test_ap_credential` registrations for boot-state behavior, 128-bit random generation/hex encoding, create-if-absent/read-back, repeated disclosure while NEEDS_SETUP, READY migration/match, mismatches, race behavior, and fail-closed store/RNG paths.
- Added 15 `test_nvs_ap_credential` registrations for NVS namespace/key use, missing versus I/O error, create-only behavior, wrong type/malformed values, 32- and 63-character passphrases, open/get/set/commit failures, and output clearing.
- Added 3 `test_esp32_bootstrap_random` registrations for SAR entropy enable/fill/disable ordering and invalid arguments.
- All 36 new registrations compiled and executed with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer` and a temporary Unity-compatible stand-in: **36 tests, 224 assertions, 0 failures**. This is not PlatformIO/Unity evidence and does not prove ESP-IDF header/link compatibility or device entropy behavior.
- At the v0.2.41 release point, the inventory was **487 C++ registrations across 29 suites**. The user subsequently reports that the full v0.2.42 PlatformIO native run passed all 29 suites. The earlier supplemental stand-in coverage totals are retained as historical evidence only.
- Reran unchanged SF2 Node suite: **257/257 passed**; all 25 `.mjs` modules passed `node --check`.
- No PlatformIO native run, ESP32 target build or hardware test was run for v0.2.41.

Required focused and full native verification from a clean v0.2.41 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_ap_credential -vvv 2>&1 | tee ap-credential-test.log
pio test -e native -f test_nvs_ap_credential -vvv 2>&1 | tee nvs-ap-credential-test.log
pio test -e native -f test_esp32_bootstrap_random -vvv 2>&1 | tee bootstrap-rng-test.log
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

---


## v0.2.40 — checked SoftAP netif teardown

- Corrected `Esp32WifiAp::cleanup_()` to call the error-returning `esp_wifi_clear_default_wifi_driver_and_handlers()` directly and call `esp_netif_destroy()` only after that succeeds. The convenience `esp_netif_destroy_default_wifi()` returns `void` and destroys the netif even when its internal clear operation reports failure.
- Strengthened the existing AP suite assertions for `wifi_deinit` → checked netif clear → netif destroy ordering, and confirmed no destroy after a failed clear. No registration was added. The AP test stub no longer declares the unused void-wrapper API.
- The AP suite contains 20 registrations. At the v0.2.40 release point the inventory was **451 C++ registrations across 26 suites**; 38 registrations had been added after the v0.2.36 baseline. Current v0.2.41 inventory is 487 across 29 suites.
- Supplemental local check: the AP suite compiled with `g++ -std=c++11 -Wall -Wextra -Werror` and executed **20/20** using a temporary Unity-compatible stand-in harness and RNG stub. The RF-gated RNG suite independently compiled/executed **5/5** with the same harness. This is not the project's PlatformIO/Unity environment. No PlatformIO full-suite run, exact ESP32 target build or device test was run here.
- 25/38 post-v0.2.36 registrations have this supplemental stand-in execution; the remaining 13 token-session/API registrations were not executed in this environment. All 38 still require a PlatformIO run. The unchanged SF2 Node suite was rerun from this v0.2.40 tree: **257/257 passed**, and all 25 `.mjs` modules passed `node --check`.

## v0.2.39 — direct ESP-IDF SoftAP lifecycle adapter (no execution at that release point)

- Added 20 C++ registrations in `test_esp32_wifi_ap` for broadcast SSID/two-client configuration, RNG readiness ordering, startup and cleanup failure, input bounds, no truncation, passphrase character policy, channel validation, stop ordering/failure, repeated start and invalid-argument retry behavior.
- The adapter configures `WIFI_AUTH_WPA2_PSK` and `WIFI_CIPHER_TYPE_CCMP` directly in the IDF `wifi_config_t`. Native stubs mirror the ESP-IDF declarations; they do not prove target header compatibility or radio behavior.
- v0.2.38 adds 18 other C++ registrations (11 token-session, 5 secure-random and 2 TokenTable preflight). Total post-v0.2.36 additions: **38 registrations**. Current inventory: **451 C++ registrations across 26 native suites**; all 38 additions lacked PlatformIO verification; 25 had supplemental stand-in-harness execution and 13 remained unexecuted at that release point. User-reported v0.2.36 baseline remains 413/413 across 23 suites.
- `Esp32WifiAp` is not production-wired. The exact Arduino-ESP32 version, AP auth mode/cipher, radio start/stop behavior, station limit and cross-task lifecycle remain unverified. E28 first-provision AP credential source is OPEN.
- At the v0.2.39 release point, no compiler/PlatformIO/target/device run had been made. The later v0.2.40 stand-in run is recorded in the section above. Registration counts are not passing-test counts.

Required focused and full native verification from a clean v0.2.40 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native 2>&1 | tee native-test.log
```

---

## v0.2.38 — PIN token bridge and ESP32 RNG adapter (tests not run)

- Added eleven new C++ registrations in `test_pin_auth_session` for token delivery lifecycle, no-active-session-before-response, abandoned response, LRU preservation on abandonment, explicit abort, one-shot behavior, RNG failure, all-zero/duplicate rejection and post-delivery LRU reporting.
- Added five new C++ registrations in `test_esp32_secure_random` for refusing before RF start, invalid buffers, exact buffer fill after the RF-start notification, refusal after RF stop and idempotent notifications. The ESP-IDF `esp_random.h` header is represented by a host stub in this suite.
- Added two `test_api` registrations for `TokenTable::canIssue()` being read-only and rejecting null, all-zero and duplicate tokens.
- Current inventory: **431 C++ `RUN_TEST` registrations across 25 suites** and **257 SF2 Node registrations**. The user-provided 413/413 native result applies to the v0.2.36 baseline only; the 18 new C++ cases have not been run. The SF2 suite is unchanged from v0.2.37 and its 257/257 local pass remains the latest evidence.
- The token action now preflights without table mutation and inserts only after response-delivery confirmation. The route must serialize TokenTable changes between preflight and confirmation; this scheduler constraint has no production implementation or test yet.
- No C++ compiler, PlatformIO, ESP32 target build or hardware test was run for v0.2.38. Test registrations are not passing tests.

Required focused and full native verification from a clean v0.2.38 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native 2>&1 | tee native-test.log
```

---

## v0.2.37 — SF2 regression repair and verification

- The user supplied the v0.2.36 PlatformIO summary: **23/23 suites passed; 413/413 native cases succeeded** in `00:08:56.629`. `test_nvs_config` passed, so the v0.2.36 NVS read-error fixture correction is host-test verified in the user's environment. This does not prove target ESP32 NVS behavior or brownout durability.
- The same user run supplied the SF2 Node summary: **238 passed, 19 failed out of 257**. I reproduced all 19 failures by running `npm test` against the v0.2.36 archive in this environment.
- Seven categories of tests had invalid or outdated fixtures/expectations: source-sequence order in the month-roster name stream; incomplete blockers read only from `issues`; month-roster order inconsistent with male-then-female/source-order planning; the event-sequence fixture reused an in-range CLOSE sequence; a reporting-roster test corrupted an ignored input rather than the projected output; a D1/D2 fixture retained unrelated section-profile IDs; and a coverage-gap fixture contradicted its own CLOSE coverage flags. Those tests were corrected without relaxing the relevant validation rules.
- Two preview-model implementation defects were corrected: `buildSnapshotPreview()` mislabeled a structurally valid `PREVIEW_READY` calculation as `BLOCKED`, and generic diagnostics appeared before the same findings with learner/date/session context. The preview now reports `PREVIEW_READY` only when schema validation is `VALID` and calculation state is `PREVIEW_READY`, prioritizes contextual diagnostics, and continues to return `finalExportReady: false`.
- Event provenance now distinguishes a structurally valid source event whose status disagrees with the projected result (`EVENT_STATUS_MISMATCH`) from malformed/wrong-type provenance (`INVALID_EVENT_PROVENANCE`). The mismatch remains blocking; only the diagnostic classification changed. This was a third implementation correction, limited to precise issue classification.
- After those changes, the complete SF2 Node suite passed locally: **257/257, 0 failed, 0 skipped**. `node --check` also passed for all 25 SF2 `.mjs` modules. This run was executed in this environment, not on the user's Android/Termux setup.
- No C++ source, native C++ test, C++ registration, signed policy, delegated decision, hardware setting, production API or persisted data format changed in v0.2.37. The user's v0.2.36 413/413 native result therefore applies to the unchanged C++ tree, but a clean full-archive rerun remains recommended.
- Host tests prove only the tested reference-model logic. No browser UI integration, ESP32 target build, production route, NVS power-loss guarantee, official workbook compatibility or device behavior is established.

Required verification from a clean full v0.2.37 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test 2>&1 | tee sf2-node-test.log)
```

---

## v0.2.36 — NVS runtime read-error fixture correction

- The user supplied the v0.2.35 focused `test_nvs_config` output: **44 tests, 1 failure, 0 ignored**. The one failure reported `Expected 4 Was 5` in `test_pin_auth_runtime_does_not_initialize_when_nvs_read_has_io_error`; the suite then received `SIGHUP` and was marked `ERRORED`.
- The actual test expected `EPAR_COUNTER_READ_FAILED` (4), but the runtime returned `EPAR_COUNTER_INITIALIZE_FAILED` (5). The fake `attsec` namespace was absent, so `nvs_open(NVS_READONLY)` returned `ESP_ERR_NVS_NOT_FOUND` before the injected `getResult = ESP_FAIL` could reach `nvs_get_u16()`.
- v0.2.36 keeps the namespace present and the counter key absent, injects the error at `nvs_get_u16()`, and asserts one open, one get, zero set and zero commit calls. The case is renamed to make the failing boundary explicit.
- No production implementation or registration count changed in v0.2.36. The later user-provided full v0.2.36 PlatformIO summary reports all 413 native cases passed, including this 44-case suite. The earlier SIGHUP followed the v0.2.35 assertion failure and does not recur in the supplied v0.2.36 full summary.

Required commands from a clean full v0.2.36 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_nvs_config -vvv 2>&1 | tee nvs-focused.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

## Earlier test-fix chronology — v0.2.35 and v0.2.34


- The user's pasted compiler diagnostics show `test_nvs_config/test_main.cpp` failing at two `static_assert` lines: the test referenced `Esp32NvsPinAttemptStore` and `Esp32PinAuthRuntime` without the enclosing `att::` namespace. The `using namespace att;` declaration occurs after those assertions and therefore cannot apply to them.
- v0.2.35 qualifies both types as `att::Esp32NvsPinAttemptStore` and `att::Esp32PinAuthRuntime`. This fixes the identified name lookup defect without changing production code or test semantics. The user then ran the focused suite and exposed a separate fixture defect, addressed in v0.2.36.
- The pasted output does not contain the complete summary or process exit status. It shows no evidence that the 24 NVS test functions ran in that attempt because the translation unit failed to compile.
- The separately uploaded `native-test.log` is inconsistent with the pasted output: it reports `test_nvs_config` as passed but contains an older `test_month_roster` assertion failure (`Expected 1 Was 2`) followed by `SIGHUP`, and the historical 390-case summary. Do not attribute that file to the pasted current run.
- v0.2.35 has 413 source `RUN_TEST` registrations across 23 suites and 257 SF2 Node registrations. The user-provided v0.2.35 focused run executed 44 cases: 43 passed and one fixture case failed; registrations remain inventory, not all-pass evidence.

Required commands from a clean full v0.2.35 extraction:

```sh
sha256sum -c MANIFEST.sha256
pio test -e native -f test_nvs_config -vvv
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

---

## v0.2.34 NVS PIN-attempt persistence and boot owner (v0.2.35 compile correction; v0.2.36 fixture correction)

- Added `Esp32NvsPinAttemptStore`: typed `uint16_t` `pin_fails` counter in a caller-selected NVS namespace; refuses the reserved `attcfg` namespace; missing or wrong-type state fails closed; initialization is explicit and non-overwriting; updates require `nvs_commit()`, close the write handle, reopen read-only and compare the stored value.
- Added `Esp32PinAuthRuntime`, which owns the store and coordinator. It starts only after config boot reaches `READY` or `NEEDS_SETUP`. On `NEEDS_SETUP`, it initializes only after a read returns `ESP_ERR_NVS_NOT_FOUND`, and preserves an existing value after an interrupted first-time setup. On `READY`, missing state causes boot failure; it is never silently recreated. `COLD`/`FAULT` config states are rejected.
- Added 24 C++ registrations in `test_nvs_config` (16 store + 8 boot-owner cases). Current inventory: **413 C++ `RUN_TEST` registrations across 23 suites** and **257 SF2 Node registrations**. Registrations are not pass counts.
- The native NVS fake models `ESP_ERR_NVS_TYPE_MISMATCH` as `0x1103`, in line with the ESP-IDF 5.5 documentation. The user-provided v0.2.34 output exposed a compile-time namespace defect in two test assertions; v0.2.35 corrected it. The user-provided v0.2.35 focused PlatformIO run then executed 44 cases with one fixture failure. v0.2.36 fixes that fixture, and the user later supplied a full 413/413 native pass across 23 suites. No ESP32 target build or device power-loss test has been run.
- No production main/scheduler constructs `Esp32PinAuthRuntime`; routes, PBKDF2 credential verifier/writer, secure RNG, token/session action and fault handling remain absent. `E12` counter-domain selection remains DELEGATED. Fresh-handle read-back is an API verification, not power-cut durability proof.

Required commands from a clean full v0.2.35 extraction:

```sh
sha256sum -c MANIFEST.sha256
pio test -e native -f test_nvs_config -vvv
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

---


## v0.2.32 user-supplied PlatformIO run and v0.2.33 correction

The user supplied the complete v0.2.32 native test log on 2026-11-07.

- 23 native suites were attempted. Twenty-two suites were marked `PASSED`.
- `test_event_group_audit` passed all 19 assertions, confirming that the v0.2.32 direct-header fix removed the v0.2.31 missing-declaration compile blocker.
- `test_month_roster` printed 15 passing and one failing assertion. The failure was `Expected 1 Was 2` at the enrolled-on-final-day count. The fixture deactivated a student on day 131 while the month was `[100, 131)`. Because the range is half-open, day 130 is the last included day and an interval ending on 131 still covers it. Source logic matched the documented contract; the fixture was wrong.
- The month suite received `SIGHUP` after all 16 test results were printed, and PlatformIO marked it `ERRORED`. The provided log does not establish the signal's cause.
- PlatformIO printed `390 test cases: 1 failed, 388 succeeded`; source enumeration shows 389 `RUN_TEST` calls across 23 suites. Do not state 390 as the source registration count. Reconcile the discrepancy from the next run.
- v0.2.33 changes only the C++ test fixture and adds one equivalent Node boundary regression. No production implementation changed. The user later reported all v0.2.33 tests passed; the exact terminal output/counts were not supplied, so retain this only as user-reported evidence.

Required commands from a clean v0.2.33 extraction:

```sh
pio test -e native -f test_month_roster -vvv
pio test -e native 2>&1 | tee native-test.log
cd web/sf2
npm test
```

Record assertion results, suite status and process exit separately. A suite marked `ERRORED` is not a clean pass even if its assertion lines show all `PASSED`.

---

## Historical test accounting — v0.2.36

Current source counts (not execution results):
- C++: **413** `RUN_TEST` registrations across 23 suites. This is 389 from v0.2.33 plus 24 NVS store/boot-owner registrations.
- SF2 Node: **257** authored registrations; no Node tests were added in v0.2.34.
- The user reports all v0.2.33 tests passed, but exact terminal output/counts were not supplied. This remains user-reported evidence, not an independently inspected log.
- The v0.2.32 log's 390-versus-389 accounting discrepancy and `SIGHUP` after the month-roster results remain unexplained. Do not silently close either item.
- The v0.2.34 pasted run did not compile the NVS test translation unit. v0.2.35 corrected the namespace references; the user-provided v0.2.35 focused run then executed 44 cases with one fixture failure. v0.2.36 corrects the fixture and requires rerun. No v0.2.36 compiler, full native run, Node test, ESP32 build or device test has been performed here.
- Host tests do not prove NVS behavior on target, power interruption, real authentication routes or device operation.

## User-submitted native run — v0.2.31 archive (2026-11-07)
- Archive SHA-256 matched `3bfa9c022b87bd9273ea2097941062dacca17ae6179727aaa583cac42b1adee7`.
- PlatformIO attempted all 23 native suites. All 23 failed in the build stage on the same missing declarations in `lib/core/src/event_group_audit.cpp`: `MAX_ID`, `RA_SET`, `RA_DEACTIVATE`, and `RA_ACTIVATE`.
- Summary: 23 suites attempted, 0 succeeded. No functional assertions executed. This is not a 0/389 assertion result; the test binaries did not build.
- v0.2.32 adds the direct defining-header includes. No post-fix compiler or test result exists yet.

Historical test details follow.

---

## Historical suite inventory and release records

### Suites (PlatformIO environment `native`)
| Suite | Source registrations in current tree | Notes |
|---|---:|---|
| test_core | 20 | Core logic |
| test_coreb | 39 | Includes three C1 regression cases |
| test_coreb2 | 11 | Core B regression suite |
| test_config | 12 | Config store |
| test_monthset | 10 | Month-set interface |
| test_roster | 9 | Roster store |
| test_backup | 9 | Backup container only; template backup was cut |
| test_hardening | 15 | Core hardening |
| test_api | 35 | Existing API logic and PIN-attempt coordinator registrations, plus read-only TokenTable preflight cases; the user reports the full v0.2.42 PlatformIO run passed this suite |
| test_enroll | 36 | Enrolled-roster helpers plus FSM lifecycle, pending-command, cancel-release, in-flight store cancellation, and skip-race cases; v0.2.51 targeted run passed 36/36 |
| test_as608 | 15 | Raw command packets, ACK validation, capacity, search, 32-byte index pages, and fail-closed occupancy refresh; v0.2.51 targeted run passed 15/15 |
| test_as608_pipeline | 10 | Synthetic coordinator startup and runtime-owned attendance owner; owner rejects scans before READY, succeeds through startup to durable match-to-EV_SCAN commit, remains closed after password/authentication fault; existing Present/Late, duplicate, unresolved mapping, failed append, no-finger and invalid-occupancy regressions remain passing; v0.2.54 targeted run passed 10/10 |
| test_as608_gate | 34 | Attendance/enrollment arbitration, Stream owner checks, runtime occupancy/enrollment/attendance read-only API compile-time assertions, preferred owned-map constructor, coordinator-only attendance setup, pipeline-only raw scan servicing, startup full-index gate, capacity mismatch, auth rejection, no-finger/no-match release and poisoned-link fail-closed behavior; v0.2.54 targeted run passed 34/34 |
| test_esp32_oled | 13 | 128x32 SSD1306 adapter: shared TwoWire bus, fixed 0x3C address/100 kHz settings, exact E28 credential formatting, non-disclosure/invalid-secret clearing and framebuffer clearing. All 13 were included in the user-reported v0.2.46 520/520 PlatformIO pass; physical panel verification remains pending |
| test_esp32_secure_random | 5 | RF-start/stop lifecycle gate around `esp_fill_random()`; the user reports this suite passed in the full v0.2.42 PlatformIO run |
| test_ap_credential_setup | 18 | NEEDS_SETUP E28 disclosure → AP start ordering, SSID/channel validation, DHCP + per-request auth gates, `Config::wifiPass` consistency, failure latch and retry-only blank-frame cleanup; all 18 were included in the user-reported v0.2.46 520/520 PlatformIO pass |
| test_esp32_wifi_ap | 22 | Direct ESP-IDF config-before-start ordering, WPA2/CCMP config, AP_START-gated RNG, netif allocation/attach/default-handler failures, driver/start timeout cleanup, stop/input validation, and AP-netif-only DHCP lease event plus callback lifetime; all 22 were included in the user-reported v0.2.46 520/520 PlatformIO pass |
| test_hardware_probe | 4 | Probe-only lowercase-hex credential encoding/output clearing; v0.2.51 targeted run passed 4/4 |
| test_nvs_startup_guard_policy | 9 | Shared NVS whole-partition erase policy and fail-closed startup decision; v0.2.51 targeted run passed 9/9 |
| test_attendance_admission | 12 | Log-backed no-class/closed-session admission, duplicate suppression, config replay, and fail-closed paths; v0.2.51 targeted run passed 12/12 |
| test_config_apply | 16 | A/B inactive-slot write, generation gate, read-back verification, runtime apply ordering and fail-closed faults |
| test_nvs_config | 44 | NVS config-slot tests plus 16 PIN-attempt-store and 8 PIN-auth boot-owner cases; the user reports this suite passed in the full v0.2.42 PlatformIO run (the earlier v0.2.36 log also marked it PASSED) |
| test_pin_auth_session | 11 | PIN-success token staging, post-response insertion, failed/abandoned response handling, LRU preservation and eviction; the user reports the full v0.2.42 PlatformIO run passed this suite |
| test_config_provision | 11 | Authorized first-provision gate, independent durable-marker check, blank-slot recheck, slot-A generation-1 write, read-back verification and fail-closed behavior |
| test_restore_replay | 14 | Authenticated-stage replay through fresh roster/index observers; group shape, stage mutation, CLOSE bitmap, GAP continuity and range checks |
| test_config_boot | 16 | One-shot backend begin/load, unprovisioned gating, first-provision reload and fail-closed boot state |
| test_config_guard | 7 | Config boot-to-health mapping and fail-closed current scan-window derivation |
| test_event_group_audit | 19 | Current event-group framing, exact 300-ID CLOSE bitmap, sequence continuity, SCAN/MARK/no-class fields and fail-closed unknown event types; the user reports this suite passed in the full v0.2.42 PlatformIO run (19/19 was also in the v0.2.32 log) |
| test_roster_history | 6 | Half-open roster interval access, sorted IDs, historical RA_SET name stream and snapshot revision guard; the user reports this suite passed in the full v0.2.42 PlatformIO run (6/6 was also in the v0.2.32 log) |
| test_roster_snapshot | 17 | Exact log-prefix capture, day-granular roster transition semantics, scratch-roster replay, explicit prefix name reader and stale-prefix rejection; the user reports this suite passed in the full v0.2.42 PlatformIO run (17/17 was also in the v0.2.32 log) |
| test_month_roster | 16 | Exact-prefix month-overlap projection, interval preservation and E27 guards; v0.2.32 exposed an off-by-one fixture defect, corrected in v0.2.33; the user reports this suite passed in the full v0.2.42 PlatformIO run |
| **CURRENT INVENTORY** | **551** | 551 source RUN_TEST registrations across 34 suites in v0.2.54. The latest targeted run covers 44 cases across two AS608 suites; 507 were not run. The v0.2.52 target smoke build/ELF routing evidence predates the v0.2.54 owned-map and attendance-owner changes and is not final-source evidence; no device was flashed or tested. |



## v0.2.25 SF2 cross-layer join and source-prefix contract (tests intentionally not run)
- Added 4 `source_prefix.test.mjs` registrations for canonical unsigned uint64 decimal text, full-range maximum, invalid numeric/padded/overflow values, and uint32 counter boundaries.
- Added 10 `reporting_snapshot_join.test.mjs` registrations for D1/D2 exact-prefix joins, learner-ID and enrollment interval alignment, legal-name/movement mapping, display-name exclusion, malformed D2 diagnostic arrays, rejection of caller-forged verification state, and integration with the snapshot schema.
- Added 1 snapshot-adapter registration and 2 snapshot-coordinator registrations; the join tests include explicit rejection of non-overlapping and overlapping/unsorted intervals. The adapter now returns `PREVIEW_READY` (never authoritative `READY`) even when a synthetic snapshot is structurally valid; a caller-supplied `SOURCE_CHECKED` claim does not change this.
- Total SF2 Node registrations: **256**, all authored but unrun. No functional Node tests were executed.
- C++ registrations remain **376 across 23 suites**; no C++ source/test registrations were changed. The 318/318 PlatformIO pass remains user-reported for v0.2.17 only.

## v0.2.24 SF2 reporting-identity alignment (tests intentionally not run)
- Added 34 Node registrations across `metadata_history_model.test.mjs` (37 total), `reporting_identity_projection.test.mjs` (21 total), and `snapshot_schema.test.mjs` (70 total). They cover explicit legal-name components separated from operational display labels, exact metadata-revision reconstruction, required legal-identity coverage only on enrolled dates, identity gaps and mid-month identity changes, movement categories/date meanings, same-day movement collisions, interval-boundary checks, duplicate LRN rejection across different permanent device IDs, malformed projection input, and the fact that `SOURCE_CHECKED` is only an unaudited claim.
- D1 operational display-name changes/missing/backdated labels are diagnostics only; they do not provide legal report identity and do not alone block the legal identity projection.
- The identity output always remains `UNVERIFIED` until reviewer attribution and immutable evidence exist. The caller-supplied `SOURCE_CHECKED` value is retained only as `sourceStatusClaim` and triggers an incomplete warning.
- E27 is DELEGATED: same-local-day ACTIVATE/DEACTIVATE remains fail-closed for authoritative monthly reporting only; live operational roster events are not rejected or rewritten.
- C++ registrations remain 376 across 23 suites; no C++ source or test registrations changed in v0.2.24. SF2 Node registrations at v0.2.24 were 239 (34 added in that release); all 239 were unrun at that release. The v0.2.25 section below records the subsequent 17 additions.

## v0.2.23 SF2 month-overlap roster projection (tests intentionally not run)
- Added 16 C++ registrations in `test_month_roster` and 18 Node registrations in `month_roster_projection.test.mjs`.
- C++ `MonthRosterProjection` replays the exact audited prefix, validates every interval, selects unique learners with at least one interval intersecting the month, preserves all overlapping intervals, reports enrollment on the final month day separately, and detects source-prefix or scratch-roster changes.
- Browser `projectMonthRoster()` validates exact prefix identity, known IDs, half-open intervals, complete name stream/source ordering, operational display-name byte length, and missing/conflicting name revisions. It retains every interval and every applicable name revision; distinct name changes during the month block export rather than selecting a name silently.
- E27 is DELEGATED to retain fail-closed same-day ACTIVATE/DEACTIVATE for authoritative report projection only. Live roster events are not rejected or rewritten.
- C++ source registrations are 376 across 23 suites. SF2 Node registrations were 205 at v0.2.23; current count is 239. The 16 C++ and 18 Node cases in this increment are authored but unrun. Only JS syntax checks were performed; no C++ compiler, PlatformIO, ESP32 build or device verification was performed.

## v0.2.22 SF2 historical roster and snapshot coordinator model (tests intentionally not run)
- Added 18 C++ registrations in `test_roster_snapshot`: prefix capture, clean-audit requirements, scratch replay, backwards roster dates, redundant activation/deactivation, same-day lifecycle rejection, historical name revision ordering, explicit prefix name streaming, stale-prefix invalidation, stale `OpenInfo` rejection and scratch/log UTC-offset mismatch.
- Added `RosterLogPrefix`, `captureRosterLogPrefix()`, `rosterLogPrefixCurrent()`, `RosterTransitionAudit` and `replayRosterPrefix()`. The scratch replay reconstructs a caller-owned `RosterStore` from exactly one current, audited log prefix and fails if the prefix changes. It never mutates the live roster.
- Added an explicit-prefix constructor to `RosterNameHistoryReader`; the original constructor remains strict and still aborts on any log change. Prefix reads are not allowed to continue after appends/reopen.
- Added 17 Node registrations in `snapshot_coordinator_model.test.mjs`: source vector checks, stale generation, roster-prefix mismatch, incomplete preview-only state, ticket/page validation, out-of-order page assembly, missing/duplicate pages/items, monotonic expiry and empty snapshot.
- C++ registrations were **360 across 22 suites** at v0.2.22; the v0.2.22 cases remain unrun. SF2 Node registrations were **187**, all intentionally unrun. JavaScript static syntax checks passed; no Node functional tests, PlatformIO, compiler, ESP32 build or device verification was performed.
- E27 is DELEGATED: same-day activation/deactivation remains a reporting-only fail-closed guard because existing roster intervals are day-granular. The live roster parser and committed events are unchanged.
- The host-side snapshot coordinator is only a reference model; it provides no HTTP route, authentication, concurrency lock, secure ticket ID, persistence or source-store immutability.

## v0.2.21 SF2 metadata-history reference model (tests intentionally not run)
- Added 26 registrations in `web/sf2/test/metadata_history_model.test.mjs`. They cover schema/revision validity, immutable record IDs, valid-time intervals, learner identity/LRN/sex fields, single-section constraints, duplicate active LRNs, correction/revocation semantics, revision-pinned lookups, movement queries, duplicate expected IDs and invalid movement-query IDs.
- Total SF2 Node registrations: **170** (55 in v0.2.18, 89 in v0.2.19, and 26 in v0.2.21; all 170 intentionally unrun).
- `metadata_history_model.mjs` is a host/browser reference validator only. It does not persist data, guarantee physical commit, authenticate callers or provide an ESP32 API.
- Static syntax-only check: `node --check` completed successfully for all `web/sf2/*.mjs` and `web/sf2/test/*.test.mjs`. No `node --test`/`npm test` was executed.
- C++ registrations remain 343 across 21 suites, including 25 v0.2.20 cases that remain unrun. The 318/318 native test result is user-reported for v0.2.17 only.

## v0.2.20 source additions (tests intentionally not run)
- Added 19 `test_event_group_audit` registrations and 6 `test_roster_history` registrations. They cover current event-group shape checks, exact five-word CLOSE bitmap handling, selected SCAN/MARK/no-class field validation, unsupported event rejection, live append readiness, stale `OpenInfo` rejection after a later clean or tail-repairing `LogStore::open()`, roster interval access and streaming historical names.
- `OpenInfo` now includes an in-memory `generation` token. `LogStore::openGeneration()` identifies the current open invocation; `EventGroupAudit::readyFor()` requires an exact generation match. The log record format and persisted bytes are unchanged.
- `readyFor()` compares current live `LogStore` record count and last sequence with the observer, while using `OpenInfo` only for the current open/repair outcome. This is necessary because the open-time count becomes stale after a valid append.
- These are authored test registrations only. No compiler or test runner was executed in v0.2.20; the 318/318 result remains user-reported for v0.2.17.

## v0.2.17 runner correction and user-reported verification
- The user reported that all v0.2.16 suites passed except `test_config_guard`, whose link failed with `undefined reference to main`. The emitted failure was at the link stage, before any of the seven assertions ran.
- Root cause: `test/test_config_guard/test_main.cpp` defined `void setup()` (Arduino sketch convention) while the PlatformIO `native` environment and all other native suites use an `int main()` entry point. The native linker therefore had no entry point.
- v0.2.17 changes only this runner function to `int main()`, retains all seven `RUN_TEST()` calls, and returns `UNITY_END()`. No test assertions or production code changed.
- Static convention audit: all 19 `test_main.cpp` files now define `main()`; none relies on `setup()`.
- The user subsequently reported: **all 318 native tests passed** on v0.2.17. This is user-reported PlatformIO host-test evidence, not ESP32 or hardware verification.

## v0.2.19 SF2 snapshot contract (tests intentionally not run)
- Added 67 cases in `web/sf2/test/snapshot_schema.test.mjs`, 12 in `snapshot_adapter.test.mjs`, and 10 in `preview_model.test.mjs` (89 in this increment; 144 SF2 JavaScript registrations total).
- Contract validation ties source revisions, the full-month calendar, month-overlap roster, enrollment intervals, movement details, attendance event references and coverage evidence together.
- Closed coverage rows must cite one `EV_CLOSE` and exactly five contiguous `EV_PAYLOAD` sequences for the 300-ID bitmap. `coverageComplete` must match the `EV_CLOSE` coverage flag; close-group sequences cannot overlap each other or any returned attendance event; `Event.seq` is bounded to uint32.
- The snapshot adapter feeds the existing pure section calculator; the preview model adds learner/date/session diagnostics. Final XLSX export remains disabled.
- All 89 cases were authored but not executed per the user's skip-tests instruction. A syntax-only `node --check` pass over `.mjs` files is not a test result.
- The C++ PlatformIO suite remains at 318 registrations, unchanged from v0.2.17; the user-reported 318/318 result applies to v0.2.17 only.

## v0.2.18 SF2 reporting model (tests intentionally not run)
- Added 31 Node built-in test registrations under `web/sf2/test/attendance_model.test.mjs`, 11 under `learner_layout.test.mjs` and 15 under `section_model.test.mjs` (55 total). None was executed, per the user's instruction to defer the consolidated verification pass.
- `web/sf2/attendance_model.mjs` resolves the agreed/provisional AM/PM convention from already-normalized session results and selects the report-month roster using stable IDs and `[startInclusive,endExclusive)` intervals; it never writes to source attendance records.
- `web/sf2/learner_layout.mjs` builds a semantic male-then-female learner-row plan and paginates it using an explicit capacity supplied by a future verified template profile; it has no default capacity and never truncates rows.
- `web/sf2/section_model.mjs` verifies complete roster/calendar/snapshot flags, matches each learner's attendance dates to half-open enrollment history, blocks missing/extra results, and derives sex-separated and combined daily learner totals.
- Confirmed mapping: E counts as Present in monthly totals. Late is retained and observed, provisionally treated as attended for quadrant selection; TARDY total remains deferred and is not written as zero.
- Missing/CLEAR/uncommitted results and insufficient absence evidence block calculation. One-session no-class dates remain unresolved.
- The C++ PlatformIO suite remains 318 registrations and has not been rerun in v0.2.18; no existing C++ source/test file changed.
- No JS tests, npm install, compiler, PlatformIO run, ESP32 target build, browser integration, XLSX write, or device test was executed.

## v0.2.16 development status
- Added 7 source registrations in `test_config_guard`; current source total is 318 across 19 suites.
- `updateHealthFromConfigBoot()` and `deriveConfigWindowOpen()` are source additions only. They do not constitute production firmware boot wiring.
- Static audit discovered C17: the current timetable has no persisted effective-from boundary/history, so a correct `unclosedPast` derivation cannot be built from today’s timetable alone. No such helper was added.
- Static audit also discovered C18: API accepts `termEnd` inclusively, fingerprint scan rejects that date, and closure processing has no term-end bound. No semantics were guessed and no closure-core patch was made.
- No tests, compiler, PlatformIO build, ESP32 target build, or device test was run during the v0.2.16 development pass. The user later ran the suite and reported the `test_config_guard` link error documented above.

## v0.2.15 development status
- User-reported verification for the v0.2.14 baseline: **295/295 native tests passed** on 2026-11-06. This is a user report; the exact terminal tail was not pasted.
- Added 16 source registrations in `test_config_boot`; v0.2.15 source total was 311 across 18 suites. The 16 v0.2.15 cases have not been run, per the instruction to defer further test execution.
- `ConfigBootCoordinator` enforces backend begin -> A/B load -> READY or NEEDS_SETUP; first provisioning is followed by a second canonical backend load before exposing the config. Storage or verification ambiguity latches BOOT_FAULT.
- No compiler, PlatformIO test, ESP32 target build, or device test was run for v0.2.15.

## v0.2.14 user-reported verification
- On 2026-11-06, the user reported **295/295 native tests passed** after installing v0.2.14. This confirms the native suite in the user's environment; it does not verify ESP32 build, NVS durability under power loss, restore swap/recovery, or sensor behavior.
- The specialized v0.2.14 audit corrected the NVS test stub's numeric value for `ESP_ERR_NVS_NOT_INITIALIZED` and added six compile-time constant assertions. The user-reported 295/295 run includes those assertions.
- `RestoreReplayAudit` checks staged record identity by CRC32, log integrity/count/sequence, roster reconstruction, index overflow, and known event-group shapes. It is not a storage-swap or crash-recovery test.

## v0.2.13 development status
- **No tests or compiler were run**, per the user's instruction to defer the consolidated verification batch.
- Added twenty cases in `test_nvs_config` and eleven in `test_config_provision`; source registrations were 281 across 16 suites at v0.2.13. The user-reported v0.2.14 295/295 run subsequently covered these and the earlier admission/config-apply cases.
- The new cases are source registrations only. They do not establish correct execution under PlatformIO Unity.
- v0.2.13 adds an ESP-IDF NVS adapter, boot slot loader, hardware-independent first-provision coordinator and native fake-API regression suites, but no exact ESP32 build or hardware validation was performed. The latest two first-provision regressions cover surviving-marker rejection and marker-read failure; the v0.2.14 user report confirms the complete 295-case baseline passed.
- The user-reported v0.2.14 295/295 run covered the eleven admission cases and the additional log-fault preflight case. Full application-loop integration tests are still required.

## v0.2.8 user-reported verification
- On 2026-11-05, the user reported **214/214 native tests passed** after installing v0.2.8.
- This is user-reported PlatformIO native evidence, not ESP32 or sensor verification.

## v0.2.9 local verification in this session
- Added eight registration cases for the attendance client and shared runtime composition. Source registrations are now 222 across 12 suites.
- All 12 suites passed locally: 222/222 using a stand-in Unity header and `g++ -std=c++11 -Wall -Wextra`. The focused 26-case gate suite also passed `-Werror`.
- This environment did not have PlatformIO installed. The v0.2.9 PlatformIO run is pending. The user's 214/214 report applies to v0.2.8 only.
- A pre-existing aggregate-initializer warning in `test_coreb2` remains; it did not fail the suite.

## User-provided run (v0.2.0)
The user supplied this PlatformIO `native` summary after installing v0.2.0:

`149 test cases: 149 succeeded in 00:02:28.567`

This is a host-native logic test run. It does not verify the ESP32 production firmware, AS608 behavior, electrical stability, or power-loss behavior. Do not label it as a hardware test.

## v0.2.1 user report
The user stated that all tests passed after installing v0.2.1. The exact terminal summary was not pasted, so this is recorded as a user report rather than a quoted test log.

## v0.2.2 user-reported result
- On 2026-11-04, the user reported all 174 native checks passed.
- The exact terminal summary was not pasted, so this remains a user report.
- It is a host-native logic test result, not ESP32 hardware verification.

## v0.2.4 user-reported verification
- Added 7 enrollment lifecycle/pending/cancel/skip-race cases and 12 AS608 adapter cases; source registrations rose from 174 to 193.
- On 2026-11-05, the user reported: **193 passed**. The exact terminal tail was not pasted, so this remains a user report.

## v0.2.6 user-reported verification
- On 2026-11-05, the user reported **196/196 native checks passed** after installing v0.2.6.
- This is user-reported PlatformIO native evidence; it is not ESP32 or sensor verification.

## v0.2.7 user-reported PlatformIO result and v0.2.8 fix (historical)
- The user reported all tests passed except `native test_as608_gate ERRORED 00:00:13.808` on v0.2.7.
- The detailed output was not supplied. `ERRORED` alone did not distinguish compile failure, process termination, timeout or runner issue.
- Source review found that `test/test_as608_gate/test_main.cpp` called `memcpy()` without directly including `<string.h>`. An isolated C++11 compile reproduced the missing declaration with a minimal Unity stub.
- v0.2.8 added the explicit include. The user subsequently reported 214/214 native tests passed on v0.2.8. This later result is user-reported PlatformIO evidence; it does not identify the exact cause of the earlier `ERRORED` result.

## v0.2.7 host verification in this session
- Added 18 cases in `test_as608_gate`, including a real Stream-link guard proving enrollment cannot transmit without a reservation; source registrations rose from 196 to 214.
- All 12 suites were compiled and executed with `g++ -std=c++11 -Wall -Wextra` using a local stand-in Unity header. All assertions passed. The gate suite also compiled and exercised `StreamAs608Link` against a minimal Arduino `Stream` stub, including owner rejection, ACK completion, timeout, and partial-write paths. This is not the PlatformIO Unity runner and does not prove compatibility with the user's exact ESP32 Arduino core.
- The user later reported 214/214 on v0.2.8. v0.2.9 extends this to 222 registrations; its PlatformIO run is pending.
- The physical sensor, actual UART behavior and exact ESP32 core remain unverified. v0.2.9 adds the host-tested sensor-bus composition root, but the production attendance scheduler/event pipeline is still not implemented.
- To reproduce the current snapshot from the extracted project root: `pio test -e native 2>&1 | tail -14`.
- Expected successful v0.2.9 summary: `222 test cases: 222 succeeded`.

## Host sandbox method for the original 149-case snapshot
- Unity replaced by a 60-line stand-in (not the real library). Equivalent assertions only.
- Compiled with `g++ -std=c++17 -Wall -Wextra`, one suite at a time, with lib/core/src/*.cpp.
- A clean-room check started from the handoff baseline, applied patches, and ran all suites.
- Mutation testing: api_logic (20 of 21 mutants caught, the survivor is equivalent), enrolled_roster (8 of 8 caught).
- Syntax check of sketch/sketch_compact_minimal_fixed.ino against hand-written API stubs: passes. Stub signatures are guesses; the device build is the real check.

## Not tested by these suites
- Real mbedtls AES-GCM and PBKDF2 (ICipher is a toy in tests).
- Actual ESP32 LittleFS target/VFS behavior, power loss, atomic rename, and recoverable truncate/quarantine; v0.2.57 only host-tests the non-destructive POSIX/VFS subset.
- Target AS608 index-table compatibility and known-page mapping, ID 300 page on device, code 23 handling, sensor wipe, and the Arduino Stream bridge.
- Restore end to end and production config-adapter integration for schedule changes. v0.2.12 adds the hardware-independent coordinator; v0.2.13 adds NVS storage, boot slot loading and first-provision logic but not production boot/health wiring.
- Real AS608 enrollment, full-library duplicate search, verification scans, target index behavior, and E23 device behavior. The sensor-bus composition root is host-tested but not yet instantiated by product firmware; no attendance event/log pipeline is wired. The occupancy reader is exercised with mocked 0x1F ACKs, not the physical sensor.
- UART TX latency through `Stream::write()`, late-reply behavior after timeout on the real sensor, and loop-time impact under Wi-Fi load.
- Production firmware, 10-hour soak, and brownout behavior.

## v0.2.13 source-only suites
- `test_nvs_config`: 20 authored cases covering initialization failure, missing namespace/key, A/B load selection, malformed blob size, read error buffer preservation, direct durable-marker reads and corruption/error handling, commit ordering, indeterminate commit persistence, marker migration, invalid slot rejection, and coordinator-to-NVS reload.
- `test_config_provision`: 11 authored cases covering mandatory authorization gate, independent durable marker check, stale/unprovisioned state recheck, config validation, slot-A generation-1 write, read-back verification, ambiguous-write latching and no retry.
- They were authored at v0.2.13; the later user-reported v0.2.14 PlatformIO run covers them.

## v0.2.14 restore replay suite
- `test_restore_replay`: 14 cases covering valid roster/index reconstruction, validator construction-order and storage identity gates, malformed roster data/actions, stage read failure/corruption/mutation, orphan payloads, CLOSE bitmap count mismatch, GAP epoch overflow/discontinuity, and one-shot behavior. The user-reported 295/295 v0.2.14 PlatformIO run covers this suite.

## C17 audit note
- `ClosureEngine::setSchedule()` replaces the live timetable, but the log/config do not persist the effective-from boundary of each schedule. `ClosureEngine::step()` iterates historical days with the currently loaded timetable. After a schedule change/reboot, this can append a `CLOSE` for a session that was not scheduled on that historical day. This is a source-level finding; no runtime reproduction was performed.
- E2 (config slot layout) is DELEGATED. Do not add a schema field or schedule-history events without the user approving the persistence design.


## v0.2.20 C++ additions (tests intentionally not run)
- `EventGroupAudit` is shared with `RestoreReplayAudit`. The 19 new registrations exercise current writer group framing, six-record CLOSE groups (one close plus five 64-bit bitmap payloads), bitmap popcount/padding, sequence continuity, orphan/truncated groups, current SCAN/MARK/no-class fields, and fail-closed handling for event types without a current schema validator.
- `RosterStore` adds read-only interval and stable ID enumeration. `RosterNameHistoryReader` streams historical display-name revisions with effective local day and source sequence; it checks that `LogStore` count/last-sequence and audit generation have not changed through the read. Six tests were authored for these behaviors.
- 25 new cases are source registrations only. No C++ compiler, PlatformIO test, or ESP32 target build was run. They are not passing-test evidence.
- Scope limitation: `EventGroupAudit` validates the current event-group structure and selected current attendance fields; it is not a cryptographic verifier or a complete domain validator for every future event type. The observer must be long-lived, the exact current `OpenInfo` (generation matched to the current `LogStore::openGeneration()`) must be supplied, and the snapshot caller must serialize against writes and account for the 16-observer limit.


## v0.2.31 — PIN-attempt coordinator (test status at release time)
- Added `lib/core/src/pin_auth.h/.cpp`, a hardware-independent coordinator around the existing `PinLockout` primitive.
- Added 13 `test_api` registrations covering boot read failure, persist-before-verify ordering, durable counter reset before action, bad-secret persistence, lockout/reboot restore, initial/reset persistence failures, verifier errors, invalid input, indeterminate actions, current-PIN-gated change, weak new-PIN rejection, wrong-old-PIN refusal and indeterminate PIN-write latching.
- At the v0.2.31 release, test registrations totalled **389 C++ across 23 suites**; the 71 additions after v0.2.17 had not yet run. The subsequent v0.2.32 user log executed the complete C++ inventory: all 13 new PIN coordinator cases passed as part of `test_api`.
- `pin_auth.cpp` passed `g++ -std=c++11 -Wall -Wextra -Werror -fsyntax-only`. `test_api/test_main.cpp` passed a syntax-only compile using an inert temporary Unity macro stub. This did not execute assertions. No PlatformIO test or ESP32 build was run.
- At v0.2.31, SF2 Node registrations were **256**, unrun at that historical release. v0.2.33 adds one Node boundary registration. The current v0.2.37 total is 257, with all 257 passing locally as recorded at the top of this file.

## v0.2.30

No source code or test registrations changed in v0.2.30. The 376 C++ and 256 SF2 Node registration counts were unchanged. No functional tests or static source checks were run for that documentation-only release.

## v0.2.95 focused C13 evidence
- Added `test_add_file_false_after_create_faults_log_until_reopen` in `test_monthset`. It confirms no payload append occurs after `addFile()` returns false, retry is blocked by the LogStore fault latch, and a clean new `open()` reconciles the empty file before a later append.
- Exact command: `pio test -e native -f 'test_monthset' -f 'test_littlefs_fileset' -f 'test_littlefs_mount_owner' -f 'test_littlefs_serialized_fileset'`. See `/tmp/att_v0295_c13_native.log` and `docs/AUDIT_v0.2.95.md`.
- Native-only. No target VFS or physical durability proof.
