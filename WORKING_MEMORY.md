# ATT working memory — read first

**Project:** ESP32 fingerprint attendance system (ATT)  
**Baseline:** v0.2.95 development snapshot  
**Source baseline date:** 2026-11-12  
**Context/workflow update:** 2026-11-13  
**Release state:** development baseline; production-candidate objective; current target is still a read-only boot shell and not release-ready  
**Rule:** This file is a compact state pointer, not product authority. See `HANDOFF_FINAL.md` and `docs/DECISIONS.md`.

## 1. What this repository contains

ATT has reusable firmware components, native C++ tests and a separate browser-side SF2 reporting reference model. A first production-oriented **read-only boot shell** now exists under `target/esp32_attendance`, but it does **not** contain the integrated attendance runtime, authenticated administration/API, complete dashboard, or production XLSX exporter. Components that compile and test independently are not thereby production-integrated.

- `lib/core/src/`: hardware-independent attendance/event, roster, configuration, time/clock, closure, health, authentication, log and restore logic.
- `lib/as608/src/`: AS608 protocol-facing client, enrollment/attendance pipeline, sensor arbiter and config bridge.
- `lib/hal_esp32/src/`: ESP32/NVS/Wi-Fi/OLED/RTC/I2C/random adapters and reusable startup/ownership gates.
- `lib/littlefs_file_set/src/`: file-set adapter, no-format mount owner and serialized access wrapper. Production owner/root adoption and target durability are not established.
- `target/esp32_adapter_link/`: inert compile/link smoke application. **Never flash.**
- `target/esp32_hardware_probe/`: diagnostic-only hardware probe. Not attendance firmware.
- `target/esp32_attendance/`: production-oriented boot shell with signed partition table, read-only status HTTP endpoints, READY-only DS3231 UTC/calendar/OSF diagnostics, full read-only `MonthSet`/`LogStore::open()` replay and HealthFsm diagnostics; attendance and all privileged functions are disabled.
- `test/`: 47 native C++ suites (884 source registrations; not a pass count).
- `web/sf2/`: browser-side pure/reference models and Node tests, not a production API/dashboard/exporter.

## 2. Baseline identity and integrity

The original uploaded `att.zip` had 207 files. All 207 were present in the matching v0.2.95 tar snapshot with identical SHA-256 contents. The tar contained 452 files and its original 451-entry `MANIFEST.sha256` passed verification before repackaging. The completed package has a newly generated manifest covering all packaged files except the manifest itself. `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are preserved from the tar snapshot and are not edited by the package reorganization.

No Git metadata is present in the supplied snapshot. Do not assume a branch, commit history or clean working tree.

## 3. Current test inventory and evidence

| Evidence | Current fact | Scope / limitation |
|---|---:|---|
| Native C++ registrations | 884 across 47 suites | Recounted directly from current test source files using RUN_TEST registrations; source inventory, not a pass count. Current focused counts: test_coreb 45, test_clock_confirmation 23, test_config 15, test_config_guard 7, test_as608_pipeline 16, test_nvs_config 48, test_nvs_startup_guard_policy 9, test_monthset 24. Older 972/970 working-memory figures did not match the present source tree and are superseded by this recount. The newly added clock-latch regression is NOT RUN. |
| AS608 resolver-focused native | 18/18 pipeline + 40/40 enrollment | `LinearAs608PageResolver` is used in successful pipeline fixtures; native fakes only, U5 hardware semantics remain open. |
| v0.2.95 focused native run | 100/100 passed | Recorded in `RELEASE_v0.2.95.md`: `test_monthset`, `test_littlefs_fileset`, `test_littlefs_mount_owner`, `test_littlefs_serialized_fileset`. It was not rerun during this packaging pass. |
| v0.2.95 baseline registrations outside its 100-case release run | 642 (historical baseline: 742 total) | Current source inventory is 972; neither the old 100-case run nor the focused 193-case E26 run is full N1. |
| SF2 Node registrations | 270 across 12 test files | Independently rerun during packaging: `npm test` passed 270/270 on Node v22.16.0. |
| SF2 syntax checks | All `.mjs` source, fixtures and test modules | Independently passed with `node --check` on Node v22.16.0. |
| C13 host fallback | 79/79 across four suites, rerun 2026-11-12 | Passed with g++ 14.2.0 normally and under ASan/UBSan. Includes `test_littlefs_vfs_probe`; uses the limited local Unity-compatibility shim and POSIX filesystem, not official PlatformIO Unity or target LittleFS. Logs in `docs/evidence/host_c13_vfs*`. |
| Host production-TU compile | 58/58 library `.cpp` files; rerun 2026-11-12 | Passed with g++ 14.2.0, GNU++11, `-Wall -Wextra`; object compilation only. |
| ESP32 adapter-link | v0.2.94 recorded successful compile/link | Not rerun in this increment; compile/link only; **never flash**. |
| Hardware-probe C13.3 target | Diagnostic-only POSIX/VFS probe built and linked 2026-11-12 | PIO target build succeeded; RAM 47,164 B (14.4%), flash 1,002,680 B (76.5% of 1,310,720 B). Not flashed or run. Exact partition guard skips I/O unless `spiffs` matches signed handoff. Uncertain descriptor-close state prevents VFS unregister. See `target/esp32_hardware_probe/README.md` and target build log. |
| Physical hardware | No complete production-system proof | No end-to-end attendance, mounted-target LittleFS durability, power-cut/brownout or production-root verification is established. |

PlatformIO Core 6.2.0 is available in the remote development environment. On 2026-11-12, seven affected native suites passed 193/193 with the inclusive-term-boundary changes. The diagnostic-only `esp32-hardware-probe` target compiled and linked successfully with Espressif32 8.0.0 / Arduino-ESP32 3.3.12 / Xtensa 14.2.0 (`pio run -c platformio.esp32-probe.ini -e esp32-hardware-probe -j 2`, exit 0, 1,130.50 s); RAM 47,164 B and flash 1,002,680 B. It was not flashed or run. The C13 POSIX fallback passed 79/79 normally and under ASan/UBSan; the host compile helper compiled 58/58 production library translation units. Do not treat the diagnostic target as production firmware.

The recorded 2026-11-12 device diagnostic reports spiffs at 0x290000 / 0x160000, conflicting with the signed 0x190000 / 0x270000. The original full diagnostic/raw partition table is not present in the repository; the rest of the map and physical flash size cannot be reconstructed. The attendance guard rejects the pair. See docs/evidence/partition_reconciliation_2026-11-13.md. Do not flash the diagnostic probe partition binary or alter signed geometry.

## 4. Active blockers — do not lose these

The latest read-only sha256sum -c MANIFEST.sha256 audit on 2026-11-13 exited 1 and reported 77 failed entries. MANIFEST.sha256 remains stale and is not a valid integrity attestation for the current working tree. Do not regenerate it merely to clear the audit.

1. **C3 — production composition:** `target/esp32_attendance/main.cpp` composes config/NVS boot, first-boot AP credential bootstrap, the persistent PIN-attempt runtime, READY-only no-format/read-only LittleFS validation, full read-only `MonthSet`/`LogStore::open()` replay, WPA2 SoftAP and read-only web status. The DS3231 UTC candidate is compared against the successful replayed baseline and supplied to HealthFsm. New `HealthInputs::clockTrusted` remains false, so READY stays in H_CLOCK_FAULT pending an explicit clock-confirmation/ClockPolicy path. The reusable `ClockConfirmationService` now binds HealthFsm, ConfigBootCoordinator and `ILittleFsStorageOwner`; it requires CFG_BOOT_READY and LITTLEFS_OWNER_READY, obtains timetable/Params from the validated boot Config, and requires MonthSet to use the exact serialized IFileSet returned by the owner. It separately requires LogStore to use that exact MonthSet and derives append YYYYMM from requested UTC plus configured offset. `LittleFsStorageOwner` implements the owner capability. The service still has no production caller and does not perform request authentication. A new HAL-level `AdminClockConfirmation` dispatcher binds the existing `AdminSessionBoundary` current-request token check for `EP_CLOCK` to the exact service and uses the service's own HealthFsm state; its two integration registrations are authored but unrun. The dispatcher is not a production HTTP route or composition root. It adds no mutex: the dispatcher, session mutations and every config/log/storage writer must run on the same serialized application task; multi-task use requires one shared lock adopted by every writer. `test_clock_confirmation` now has two unrun POSIX-host integration registrations that construct the concrete owner and exercise its real serialized file set through the service, including foreign-MonthSet rejection; the mount backend alone is faked. This does not establish target LittleFS durability. Clock faults are not imposed while NEEDS_SETUP remains unprovisioned. The boot shell still lacks AS608 owner, writable log/roster path, scan/commit pipeline, closure, attendance health gate, authenticated API/admin and scheduler. READY startup now additionally attaches EventGroupAudit, replays the operational RosterStore, and attaches the exact-config LogAttendanceAdmission observer; this is read-only projection validation, not attendance enablement.
2. **C13.3 — target storage proof:** the production boot shell now source-integrates `LittleFsStorageOwner` only on `CFG_BOOT_READY`, with the exact signed `spiffs` geometry, no-format mount, read-only layout validation and full `MonthSet`/`LogStore::open()` replay; `NEEDS_SETUP` skips attendance storage. The source path contains no file mutation. This has not been rebuilt or target-run. Mounted pinned ESP-IDF LittleFS VFS behavior, descriptor/low-space faults, file and directory durability, interruption/restart, proven commit point and physical power-cut evidence are still missing. The diagnostic-only probe has a successful Xtensa compile/link but was not flashed or run; the recorded device SPIFFS pair still differs from the signed handoff, and the original full partition-table capture is unavailable; the guard rejects the pair.
3. **C6 — production security/API:** The boot shell starts separate `attsec`/`attsetup` runtimes before AP/HTTP. READY composes the E35 2,048-iteration PIN config runtime, `AdminSessionBoundary` and READY-only `AdminSetupCodeBoundary`. NEEDS_SETUP composes session-free `FirstRunSetupCodeBoundary` and `Esp32MbedTlsInitialPinPreparer` using exact `configBoot` and AP-owned RF-gated RNG. It validates initial PIN syntax/weak patterns and non-PIN Config/AP credential before code use; after code match, durable counter reset and H_UNPROVISIONED it derives the salted 2,048-round PBKDF2 hash in RAM, then delegates the one durable first write to ConfigBootCoordinator. Eleven new tests authored NOT RUN. No HTTP setup/login/session/PIN/restore/rollover route or post-provision READY graph transition exists. E36 supervised migration, target KDF latency, RAM/BSS and physical visibility remain open. Code-only PIN reset/restore and E12 no permanent ceiling remain accepted risks.
4. **C2 — restore:** semantic replay audit exists, but a transactional storage swap, journal and reboot recovery are absent.
5. **C1/C34 — startup and NVS:** reusable config/startup gates exist; production owner ordering is not wired. The NVS erase guard is intentionally narrow, not a universal flash-erase firewall.
6. **U61/U112:** `RosterStore` replay rejects duplicate same-state ACTIVATE/DEACTIVATE records and strictly earlier-local-day events; E34 planners reject backdated changed SET/rename, DEACTIVATE and ACTIVATE per learner. Same-day operations remain E27-compatible; monthly projection still rejects ambiguous same-day lifecycle history. PISHDA added `RosterEventWriter`, which binds an exact replayed observer/log generation, plans before append, checks count/sequence/name/lifecycle postconditions and latches uncertain writes; six focused registrations are authored but NOT RUN. Raw `LogStore::append()` remains public and can bypass the helper, no production roster route calls it, and serialized target storage behavior remains unverified. U61 stays open until every production mutation uses this writer and route-to-append-to-replay behavior is evidenced. Request tickets are RAM-only and do not provide durable exactly-once execution or handler cancellation.
7. **E18/U5:** E18 is user-signed and implemented by `LinearAs608PageResolver`: page 0 → ID 1, page 299 → ID 300; page >= 300 rejects. `test_as608_pipeline` fixtures use this resolver for scan-to-event flows. The AS608 client search is separately bounded by reported sensor capacity. U5 remains OPEN: actual index-bitmap bit order and physical page behavior need device verification; do not treat host fixtures as hardware evidence.
8. **Hardware:** no full physical acceptance, soak, brownout or power-cut evidence. The diagnostic sketch has a duplicate-template risk; reconcile sensor state before any real attendance use.
9. **SF2:** reference-model only; teacher-dependent rules, approved workbook fidelity, trusted source/reviewer evidence, PII storage/backup and production export/API remain unresolved.

See `docs/CRITICAL.md` and `docs/UNVERIFIED.md` for the concise register; follow their links for detail.

## 5. Product authority and a known discrepancy

- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are protected authority records. Never edit them casually.
- The handoff records a 15-minute scan lead; decision P2 in `docs/DECISIONS.md` was explicitly changed on 2026-11-10 to 30 minutes, and `config_store.cpp` carries the 30-minute default. Treat the later P2 change as current; preserve the original handoff statement as historical provenance.
- E32 is now signed: the DS3231 stores UTC and production RTC adapter calls use offset 0; PHT conversion remains in the application layer. Other defaults and school-specific reporting rules must be checked against the decision ledger and teacher questions. Do not infer them from demo fixtures.

## 6. Mandatory AI bootstrap and task reading

Every agent starts with one batch read, in this exact order:

1. AGENTS.md
2. WORKING_MEMORY.md
3. index.md
4. AI_COORDINATION.md

The user's one-line codename instruction is in AI_START.md. The agent must
infer its role, active assignment and ownership from the context files; do not
ask the user to repeat project history or architecture already recorded there.

After bootstrap, use index.md to locate only the applicable authority,
specification, critical/unverified records, source, callers, tests and evidence.
Read HANDOFF_FINAL.md and relevant docs/DECISIONS.md entries before changing
product behavior. Read the historical archive only for a specific regression
or provenance question.

## 7. Production-candidate directive

The user explicitly replaced the graduation/demo objective with a production-candidate objective on 2026-11-13. The full intended attendance system is in scope; demo deadlines no longer govern prioritization. Do not weaken signed behavior or present a partial target as complete. Keep privileged and mutating operations disabled until their composition, storage, authorization and failure paths are safe.

Priority is to close blockers that prevent a safe end-to-end workflow: target storage durability/partition geometry, serialized application ownership and clock/health readiness, authenticated administration/API, roster writer integration, enrollment ownership, attendance scan/commit/closure, operator UI, then backup/restore and approved SF2 reporting. These workstreams may proceed in parallel only on disjoint paths.

The user reports controlled testing is not feasible and directed research into preventing regressions without executing tests (2026-11-13). Apply `docs/NO_TEST_ASSURANCE.md` to every increment: source-level contracts, whole-call-graph review, range/resource/persistence analysis and PISHDA adversarial self-review. SHRU is entirely optional and user-invoked; PISHDA does not depend on it or automatically inspect its inbox. PISHDA reviews SHRU material only when the user explicitly asks. Never claim a no-break guarantee or label self-review as independent verification. Dynamic tests/builds/PIO/flash/hardware remain NOT RUN unless later authorized. This does not waive production release gates. The current target remains a read-only shell.

## 8. Active development and review

PISHDA is the sole implementation owner and sole writer across the repository.
All source, test, documentation, specification, coordination, staging and
integration work proceeds through PISHDA.

SHRU is a 100% optional, user-invoked read-only critic. It may inspect the
repository and write critique reports only to comms/inbox/SHRU/. It cannot
edit source/tests/docs, write staging files, run tests/builds/PIO, or perform
hardware actions. PISHDA does not assign, invoke, poll, wait for or routinely
check SHRU or its inbox. PISHDA reads a report only when the user explicitly
asks it to review or incorporate that report. SHRU is not part of PISHDA's
execution path, and no report is required.

All prior SHRU implementation write assignments are revoked:
ATT-P15-MONOTONIC-CROSS-OP-REG-01 and ATT-U61-ROSTER-WRITER-01 no longer
authorize changes to their former paths. PISHDA owns all follow-on work.
Completed SHRU changes already reviewed and accepted remain in the repository.

ATT-NO-TEST-ASSURANCE-REVIEW-01 is no longer a standing SHRU request; no
response is expected or required. The user may independently prompt SHRU for
that or any other read-only critique. QYUPAL and SEI are retired; their past
work and coordination entries are historical only.

PISHDA independently source-inspected and accepted SHRU's completed
ATT-U61-BACKDATED-ROSTER-01 and ATT-P15-SETUP-CODE-EXPIRY-SERVICE-01
contributions before this role change. Their authored tests remain NOT RUN.
This statement records historical acceptance; it does not grant new SHRU
write permissions.

U61 now has a source-only planner-before-append writer and six unrun regressions, but remains open because no production route calls it and raw LogStore appends can still bypass it. P15 expiry is serviced around synchronous HTTP dispatch. The target composes READY-only PIN/session and NEEDS_SETUP first-run boundary owners, but no HTTP route consumes them. The strict 768-byte JSON request decoder and fixed-buffer raw-body handler candidate remain source-only, with 17 NOT RUN regressions. A hash-pinned PlatformIO pre-build patch is now authored to make POST/PUT/PATCH/DELETE requests raw-handler-only, reject duplicate Content-Length/Transfer-Encoding, and cap raw bodies at 768 bytes with a 5,000 ms whole-body deadline. The patch restores the installed framework source on normal process exit and retains a hash-checked recovery copy after abnormal termination. Because the framework package is shared, concurrent PlatformIO builds are prohibited while this patch is active; recover a retained backup before unrelated builds after forced termination. It has NOT RUN; the framework package remains unmodified, main.cpp remains GET-only, and the setup handler remains unregistered. The one-shot post-provision reboot gate remains source-composed but unreachable; READY owners are created only by a fresh boot. Remaining C6 blockers include route composition, E36 USB migration, and target/runtime evidence. Clock confirmation, production scheduler, enrollment façade, durable closure tick source and LittleFS target durability/partition mismatch remain open. A source-only ClosureEngine::step() improvement now handles at most one local day per call, caches the next due session-end boundary, and resumes through a high-water cursor; three bounded-closure regressions plus one sorted-index regression are authored NOT RUN. CLOSE lookup is now O(log n) over the unchanged 800-entry array, with O(n) insertion and no additional index RAM. C17 schedule-history persistence and a production closure caller remain unresolved.

Current native source inventory is 884 registrations across 47 suites; this is not a pass count. Static recount of current test source gives `test_coreb` 45, `test_clock_confirmation` 23, `test_config` 15, `test_config_guard` 7, `test_as608_pipeline` 16, `test_nvs_config` 48, `test_nvs_startup_guard_policy` 9, `test_coreb2` 17 and `test_monthset` 24. Earlier 972/970 totals and their associated focus counts did not match the current source tree; this was a source-inventory discrepancy, not test results. The 17 request-decoder cases, six raw-body-buffer cases, one first-run restart/blank-clear regression, 11 initial-PIN preflight/KDF regressions, three bounded-closure one CLOSE-index binary-search regression, one LogStore batched-read regression, one null-append guard regression, two indexed-read integrity regressions, two storage read/recovery consistency regressions and earlier unrun increments remain NOT RUN. Tests, source checker, compiler, builds, PlatformIO, flash and hardware remain NOT RUN. The reconstructed pre-increment source tree totals 925 (the prior 924-entry ledger undercounted by one). Latest pre-increment read-only manifest audit reported 85 checksum mismatches; it was not regenerated and is not an integrity attestation.

## 9. ATT-STORAGE-READ-RECOVERY-CONSISTENCY-01 — 2026-11-13

PISHDA completed a source-only audit of LogStore::open(), at(), observer
attachment/replay/reset, and every direct production indexed-read consumer.
The inspected production readers (buildDayView, RosterNameHistoryReader,
replayRosterPrefix, and clock-pair verification) stop on failed reads; the
four LogObserver implementations reset derived state without storage work.
No production defect was established, so no speculative implementation change
was made.

Two focused test_monthset regressions now cover same-instance reopen after
an indexed-read fault and rollback/recovery after candidate observer replay
fails partway through. Both are registered but NOT RUN. Inventory is 964
registrations across 47 suites; test_monthset has 30. No test, checker,
compiler, build, PlatformIO, flash or hardware command was run. This audit
does not establish LittleFS durability, concurrency safety or release status.

Next priority remains authoritative resolution of the signed partition-layout
discrepancy before any writable target storage path is considered.

## 10. ATT-PARTITION-DISCREPANCY-AUDIT-01 — 2026-11-13

The signed handoff, attendance partition CSV, attendance PlatformIO config and
runtime guard agree on spiffs 0x190000 / 0x270000. The recorded 2026-11-12
device diagnostic reports 0x290000 / 0x160000. The complete original diagnostic
and raw on-device partition-table dump are not in the repository, so the
remaining entries, physical flash capacity and cause are not established.
The discrepancy is confirmed in the recorded values but not authoritatively
reconciled. See docs/evidence/partition_reconciliation_2026-11-13.md.

The diagnostic probe environment has no explicit board_build.partitions
selection; its generated partitions.bin is not established as the signed
attendance table. Do not flash that binary or a full bundle containing it.
The probe also performs ephemeral filesystem writes if its signed-layout
guard passes, so a matching map alone does not authorize running it.

No partition CSV, signed handoff, runtime constant or firmware behavior was
changed. No build, test, flash, erase, mount or filesystem operation was run.
Next required evidence: read-only extraction of the complete 0x1000-byte
partition table from 0x8000 plus flash identification, hashes and offline CSV
conversion. C13.3 remains open.

## 11. ATT-P15-RAW-BODY-BUFFER-01 — 2026-11-13

Extracted the first-run HTTP handler fixed request-body assembly into the
hardware-independent FirstRunSetupBodyBuffer in lib/core. It enforces a
nonzero declared length up to 768 bytes, cumulative chunk accounting, bounded
copying, exact-length completion and wipe-on-error/abort/destruction. The HAL
handler now delegates its RAW_START/WRITE/END/ABORTED transitions to that
helper. No route was constructed or registered; main.cpp remains GET-only.

Added six native registrations to test_first_run_setup_request, bringing that
suite to 23 and the repository inventory to 970 across 47 suites. The tests
cover invalid state/length, split chunks, incomplete exposure, cumulative
total mismatches, null/empty chunks, exact maximum length, overflow and
abort/reuse. They are registered but NOT RUN. No tests, checker, compiler,
build, PlatformIO, flash or hardware action was run. The pinned Arduino-ESP32
3.3.12 package and Parsing.cpp pre-image hash matched the framework patch
script; no framework package file or backup was changed.

The handler and raw-body parser remain source-only and unregistered. Before
enabling the route, the framework patch, actual body callback, error responses,
one-time setup flow, and fresh-boot transition still require authorized
verification. The partition mismatch and target LittleFS durability blocker
remain open.

## 12. Standalone AS608 diagnostic increment — 2026-11-13

PISHDA added `sketch/as608_readonly_web/att_as608_readonly_web.ino` and its
README as a separate diagnostic, not production firmware. SoftAP is `att`
with a diagnostic-only fixed password; the web page at `192.168.4.1` shows
status, RAM log, and downloadable results. The only sensor commands are
password verification, parameter read, occupancy-index read, image capture,
image conversion and template search. It has no enrollment, template store,
delete, upload or download commands. The index reader reports raw bytes and
both bit-order candidates without choosing one. The parser faults closed on
partial TX, timeout, malformed/oversize/checksum-invalid ACK or extra bytes;
recovery requires power-cycling the ESP32 and sensor.

The diagnostic assumes the documented UART2 pins/57600 baud and sensor
password zero; verify the sensor's actual password before using it. It does not
prescribe sensor supply voltage. Results are RAM-only until downloaded. The
fixed AP password is strictly for this isolated diagnostic and is not safe for
production use. Source is staged/read-back verified, but the sketch is NOT
compiled, flashed or hardware-tested. Do not count this as AS608 physical
verification or production attendance progress.


## 13. ATT-P15-CONTENT-TYPE-DUPLICATE-GUARD-01 — 2026-11-13

Static audit of the pinned Arduino-ESP32 3.3.12 WebServer parser found that
the raw-only mutation patch rejected duplicate Content-Length and any
Transfer-Encoding but did not reject duplicate Content-Type. WebServer's
header(name) returns the first matching field while parser classification
processes each occurrence, leaving ambiguous content-type semantics possible.

Updated tools/apply_pinned_webserver_patch.py to count Content-Type headers
and reject more than one before invoking the raw callback. The patch remains
hash-pinned. A pure Python AST-based reconstruction of the pre-build
replacement tuple verifies every source anchor exactly once and computes the
expected after-image SHA-256 without editing the installed framework package.
The new expected hash is recorded in the hook and the increment stage.

No target route was registered. No tests, checker, compiler, build, PlatformIO,
flash or hardware action was run. The framework source remains at its pinned
original hash with no recovery backup. This is source-level parser hardening,
not runtime evidence.



## 14. ATT-P15-RAW-ERROR-RESPONSE-BOUNDARY-01 — 2026-11-13

Static audit found that Esp32FirstRunHttpHandler sent some responses from
RAW_START/RAW_WRITE callbacks, before WebServer::handleClient() returns from
_parseRequest() and clears response headers for the new request. Prior
response-header state could therefore contaminate an early error reply.
The handler also called client.stop() during parsing, bypassing the ordinary
request-handler response path.

Refactored parse-time failures to record a pending status/error and wipe the
body only. The normal RequestHandler::handle() phase now writes that response
after WebServer's per-request response-header reset. Successful provisioning
and ordinary decoder/auth errors already occur in that phase. The framework
patch now emits bounded 400/408/413 errors through its parser-level
sendErrorResponse() helper for invalid negative lengths, oversize bodies and
connected timeout/zero-progress failures. Disconnects remain response-free.

This is source-only. No tests, checker, compiler, build, PlatformIO, flash or
hardware action was run. The framework patch remains unapplied, and the
production target remains GET-only. Parser/socket response delivery is not
runtime-verified.

## 15. ATT-NVS-CONFIG-LIVE-GUARD-01 — 2026-11-13

Source audit of the pinned Arduino-ESP32 3.3.12 startup confirms that
initArduino() calls nvs_flash_init() before setup(); on NO_FREE_PAGES or
NEW_VERSION_FOUND it attempts one full DATA_NVS esp_partition_erase_range()
and retries initialization only if that erase succeeds. The production
attendance target links the shared wrappers for both functions. The shared
erase wrapper intentionally blocks only that exact full-partition range; it
is not a general flash erase firewall.

A stale-readiness gap existed in Esp32NvsConfigBackend: begin() captured the
safe startup snapshot once, then load/readSlot/readProvisionMarker/writeSlot
trusted only the cached ready_ flag. If the global guard later observed
another init call or a blocked full-partition erase attempt, AP/PIN stores
rechecked and failed closed, but the config backend could continue accessing
NVS. The backend now revalidates the live guard in begin(), ready() and before
each public operation; an unsafe snapshot permanently drops its ready_ latch
for this instance and reports the original startup error where available.

Added one pure-policy regression plus a concrete adapter-level regression
in test_nvs_config. The adapter regression starts a real Esp32NvsConfigBackend
against the existing fake NVS API, trips the mutable startup guard after
begin(), and requires ready(), readSlot(), writeSlot(), readProvisionMarker(),
load() and a repeated begin() to fail without any additional NVS open/set/
commit. Both cases are registered but NOT RUN. Native source inventory is 972
registrations across 47 suites; test_nvs_startup_guard_policy has 10 and
test_nvs_config has 51. No test, checker, compiler, build, PlatformIO, flash
or hardware operation was run.
The NVS partition-erasure hook remains narrow, raw/direct flash erase routes
remain outside it, and C34 remains blocking.


## 16. ATT-CLOCK-HEALTH-OWNERSHIP-CONTRACT-01 — 2026-11-13

Audited ClockConfirmationService and HealthFsm ownership. The service holds a
const HealthFsm reference and returns CCF_OK/clockTrusted=true only after the
audit pair, RTC write/readback, monotonic elapsed-time checks and OSF-clear
readback succeed. It does not mutate HealthFsm, clear HB_CLOCK or grant
attendance permissions. The serialized caller must separately update health
from this verified result while preserving independent sensor/storage/config
faults. This is intentional separation, not a service-side state transition.

Added explicit API/spec comments and strengthened existing successful clock
confirmation regressions: H_CLOCK_FAULT remains latched after a verified
transaction, and H_SENSOR_FAULT remains present after explicit clock recovery.
No production caller exists; target updateRuntimeHealth() still supplies
clockTrusted=false. C3/C6 remain blocking. Modified assertions are authored
but NOT RUN; native source inventory remains 972 registrations across 47
suites. test_clock_confirmation remains 23 registrations. No test, checker,
compiler, build, PlatformIO, flash or hardware action was run.



## 17. ATT-CLOCK-HEALTH-OWNER-REVERIFY-BOUNDARY-01 — 2026-11-13

Static review found `ClockHealthOwner::update()` forwarded the caller's broad
`HealthInputs::reverified` bit. That allowed this clock-only owner to clear a
latched storage fault without a storage-domain re-verification. It now forces
that bit false on every refresh; only its own verified CCF_OK path can release
HB_CLOCK. The existing `test_clock_confirmation` dispatcher integration case
was extended to latch HB_STORAGE and prove a caller-supplied broad reverify
claim does not clear it. The assertion is authored but NOT RUN; test inventory
remains 884 registrations across 47 suites. No production route/composition
changed. No tests, checker, compiler, build, PlatformIO, flash or hardware
operation was run.

Next task remains authoritative partition-layout reconciliation; the complete
read-only 0x1000-byte device partition-table extraction and flash identity
evidence are still unavailable. Do not alter signed geometry or enable writes.


## 18. ATT-READONLY-VFS-MOUNT-01 — 2026-11-13

Static source/contract audit found a mismatch in the ESP32 LittleFS mount: `tools/check_att_boot_shell.py` already required `config.read_only = true`, but `Esp32LittleFsMountBackend::mountWithoutFormatting()` set it false. The backend now requests `read_only=true`; `format_if_mount_failed` and `grow_on_mount` remain false. The target README, recovery spec, blocker summary and roadmap now distinguish this boot-shell-only access restriction from a future writable production storage owner.

This narrows accidental POSIX/VFS write exposure if the exact pinned component enforces the documented field. The exact target component runtime and write rejection have NOT been verified in this session. C13.3 remains blocking for mounted-target errors, durability, metadata persistence, low-space, restart/interruption and physical power-cut/brownout evidence. The partition mismatch remains unresolved.

No tests, checker, compiler, build, PlatformIO, flash or hardware operation was run. The existing static checker assertion was inspected but not executed. MANIFEST.sha256 already had 13 unrelated/stale mismatches before this increment; it was not regenerated.
