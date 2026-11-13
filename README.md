# att: ESP32 fingerprint attendance, v0.2.96 (2026-11-12; documentation-only workflow alignment)

**Development snapshot, not a production firmware release.** Not hardware-verified. Replace the entire project tree when installing an archive; never merge individual files. Read docs/WORKING_MEMORY.md first, then HANDOFF_FINAL.md, docs/DECISIONS.md, docs/CONTEXT.md, docs/CRITICAL.md and docs/DEVELOPMENT_ROADMAP.md.

- `DEVELOPMENT_WORKFLOW.md` — canonical deadline-first execution, blocker, batching, verification and reporting protocol.
- `PROJECT_INSTRUCTIONS_ADDENDUM.md` — paste-ready text for the ChatGPT Project Instructions field.

## Layout

- lib/core/src: hardware-independent attendance, event, config, roster, audit, authentication, clock and restore logic.
- lib/as608/src: raw AS608 command adapter, occupancy refresh, shared sensor arbiter, enrollment session/FSM, attendance scan client and event pipeline.
- lib/hal_esp32/src: Arduino Stream UART bridge, owned-map AS608 runtime, startup coordinator, attendance-only owner, config-apply adapter and ESP32 NVS, Wi-Fi, RNG, OLED, DS3231 and shared I2C-bus-owner adapters. These are reusable components, not a production firmware root.
- lib/littlefs_file_set/src: POSIX/VFS-backed, non-destructive LittleFS IFileSet adapter. Host POSIX uses lstat() for no-follow path checks; the ESP-IDF LittleFS build uses stat() because the pinned newlib has no lstat() declaration. A separate LittleFsStorageOwner / Esp32LittleFsMountBackend owns the no-format mount and performs read-only file/record/roster preflight before exposing a serialized IFileSet. v0.2.82 adds global sequence continuity and cross-month roster lifecycle/name validation. Truncate/quarantine remain disabled; the owner is not wired into a production firmware root.
- target/esp32_adapter_link/: inert compile/link smoke application. Never flash it.
- target/esp32_hardware_probe/: diagnostic-only I2C/OLED/SoftAP probe. Not attendance firmware.
- sketch/: legacy diagnostic bring-up sketch.
- test/: 39 native suites, 738 C++ test registrations in this snapshot.
- web/dashboard/: canonical self-contained local dashboard source; the current target embeds it and serves a read-only status view from GET /api/status. Attendance, roster, timetable, administration and reports remain unavailable until their real owners and APIs are integrated.
- web/sf2/: separate browser-side SF2 reference models and tests; not an authoritative device API or production XLSX exporter.
- docs/: source-of-truth decisions, critical issues, unverified items, test evidence, module specifications, audits (including AUDIT_v0.2.74.md through AUDIT_v0.2.95.md), approved roadmap (DEVELOPMENT_ROADMAP.md) and read-first working memory (WORKING_MEMORY.md).

## Verification

From the complete extracted project root:

- sha256sum -c MANIFEST.sha256
- Latest code bundle: att_v0.2.95_2026-11-12-dev.tar.gz (complete source snapshot; not a production firmware release). The v0.2.96 workflow alignment is documentation-only and does not change code.
- Focused native commands and exact evidence: RELEASE_v0.2.95.md and docs/TESTS.md.
- Critical scope and unresolved constraints: docs/AUDIT_v0.2.95.md and docs/CRITICAL.md.

Read `docs/WORKING_MEMORY.md` first and follow `DEVELOPMENT_WORKFLOW.md`. This remains a development snapshot, not production firmware; the ESP32 adapter-link image is inert and must never be flashed.

v0.2.95 adds a MonthSet/LogStore integration regression for addFile() returning false after an empty month entry becomes visible: LogStore faults, blocks retry, and requires a fresh read-only open before another append. This is host-only logic evidence; the v0.2.94 inert ESP32 compile/link result was not rerun. C13.3 remains blocking: mounted target VFS, low-space, directory-entry durability, restart/interruption and physical power-cut proof remain absent. No destructive recovery behavior was enabled. Never flash the inert target.

v0.2.92 adds the mandatory ApplicationAttendanceGate to the preferred As608AttendanceOwner. The gate binds validated config boot, the READY no-format LittleFS owner and exact file-set/MonthSet/LogStore chain, roster history, log-backed admission, closure index/graph and live HealthFsm. Sensor-only H_SENSOR_FAULT is permitted only for bounded read-only sensor startup; attendance and commit require H_OK. Owner startup rejects mismatched object/config wiring, and As608OwnerConfigRuntime rejects a different closure engine before config persistence. The focused pipeline suite passed 16/16. The inert ESP32 smoke target compiled/linked successfully and final ELF symbol inspection retained the gate, owner, closure-config-check and config-runtime methods; RAM 67,892 B (20.7%), flash 1,010,340 B (77.1%). This is compile/link evidence only. No production root/caller, RTC caller, physical attendance, mounted-target VFS or power-cut behavior is established. C3 remains blocking; never flash the inert target.

v0.2.90 adds a host integration regression for the complete reusable storage path: `LittleFsStorageOwner` → serialized `IFileSet` → `MonthSet` → `LogStore` append, followed by a fresh-owner reopen and record verification. The three focused C13 suites passed 69/69 (fileset 43, mount owner 23, serialized fileset 3), exit 0. This uses a POSIX temporary directory and fake mount backend; it is not mounted ESP-IDF LittleFS or power-loss evidence. C13.3 remains blocked by target VFS runtime/fault validation, a production application root, proven persistence semantics, and physical interruption tests.

v0.2.89 extends the existing native-only storage fault seam through `scan_()` and `ensureLogDirectory_()`. Root and `/log` metadata are checked once; only `/log` returning `ENOENT` is treated as a valid empty directory. Transient metadata errors, non-directory paths, `opendir`, `readdir` (including an error after a real entry), and `closedir` failures now fail closed without publishing partial counts. Six new regressions bring `test_littlefs_fileset` to 43 registrations; the full source inventory is 725 C++ registrations across 39 suites. Focused results: `test_littlefs_fileset` 43/43 and `test_littlefs_mount_owner` 22/22. The modified storage library compiled and archived under the pinned Xtensa/ESP-IDF toolchain. The complete adapter-link target was stopped during unrelated HAL recompilation; no final ELF or firmware link is claimed. No mounted VFS runtime, flash, or physical hardware test was performed.

v0.2.87 routed validator descriptor closes through native-only `closeFn`; its regression now passes as part of the v0.2.89 `test_littlefs_fileset` 43/43 result. The synthetic close callback closes the host descriptor before reporting `EIO`; the target branch still uses direct ESP32 `close()`.

v0.2.61 closes the roster-history WIP: roster history is trusted only when the interval table has not overflowed, replay rejected no roster records, and no RA_SET name-payload group is pending. Attendance, enrollment, manual-mark, enrollment planning and closure snapshot paths fail closed on incomplete roster history. Roster write reconciliation after reopen/replay is state-based, not durable request idempotency.

v0.2.62 replaces clock-apply's ambiguous bool with ClockApplyResult and enforces CA_APPLY, valid source and ordered GAP bounds. It distinguishes rejection, an already-faulted log, successful commit and uncertain append result.

v0.2.63 hardens GAP duration conversion against uint32 rounding overflow and prevents both appendGaps() and applyClockSet() from writing any records for a range whose rounded whole-minute endpoint cannot be represented. The event format is unchanged.

v0.2.64 fixes a PIN lockout edge case where a valid uint32 seconds deadline could wrap to zero and be mistaken for the unlocked sentinel. Lock activity is now tracked independently from the deadline value; persisted failure counts and the 60/120/240/300-second schedule are unchanged.

v0.2.65 hardens public token comparison/session operations against null pointers: token checks reject null without mutating session state, null revocation is a no-op, and ctEqual() rejects null operands for nonzero lengths while preserving zero-length equality.

v0.2.66 hardens pointer/length and caller-output boundaries: request fingerprinting now has an explicit boolean failure channel for null/nonzero input (no colliding sentinel hash); API event builders reject null output buffers with count zero; RosterStore planning returns a distinct appended RE_BAD_OUTPUT; and nameOf() safely rejects a null destination.

v0.2.67 gives each new ReqRing reservation a nonzero 64-bit ticket. `finish()` and `abort()` must present that ticket, preventing a late handler from mutating a newer reservation after pending-entry expiry and request-ID reuse. This does not cancel timed-out work or provide durable exactly-once behavior.

v0.2.68 enforces the ReqRing success-only cache contract: `finish()` returns true only when it commits `A_OK`; failure codes and stale/zero tickets are rejected, leaving a live reservation pending for explicit `abort()`.

v0.2.69 corrects `RosterTransitionAudit` chronology: it now tracks the latest effective day separately from the last lifecycle boundary. A rename no longer advances the lifecycle boundary, so a transition may share the rename's day only when it is still strictly later than the preceding lifecycle boundary. Backdated events and same-day lifecycle transitions remain fail-closed.

v0.2.71 added LittleFsFileSet to the inert ESP32 smoke composition and retained all nine public adapter methods through final linking. The target exposed that the pinned ESP-IDF/newlib headers do not declare lstat(). The adapter uses stat() only on the ESP LittleFS backend (which has no symlink entry type), while POSIX host builds continue using lstat() and retain symlink-rejection checks. No storage recovery behavior was enabled. The v0.2.71 target result was compile/link and static-symbol evidence only; it did not mount or operate on LittleFS.

v0.2.77 makes LogStore::open() a read-only validation/replay gate: partial tails, invalid complete records, sequence errors, interior corruption and incomplete FL_MORE groups are preserved byte-for-byte and cause fail-closed startup without replaying a valid-looking prefix. If a second-pass read fails after observer replay begins, all observers are reset again before failure returns. Focused host evidence is recorded in RELEASE_v0.2.77.md. No automatic recovery, quarantine, formatting or journal was introduced. The target build/ELF result and exact checks are recorded in the release note; never flash the inert adapter target.

v0.2.76 makes the AP-credential and PIN-attempt NVS adapters independently check the pre-setup Arduino startup snapshot before every public storage operation. Unsafe startup blocks all NVS opens/writes; the stores preserve the original init error where possible and otherwise return ESP_FAIL. Focused native suites passed 63/63 across test_nvs_ap_credential (16) and test_nvs_config (47). This does not prove ConfigBootCoordinator was started or reached READY/NEEDS_SETUP; the future production root must still enforce that sequence. The pinned inert ESP32 adapter-link target was rebuilt and its exact ELF inspected; the first verifier run caught a garbage-collected non-virtual PIN initializer, so the link-only smoke anchor was corrected and an incremental rebuild passed the final 12 wrapper-route, 7 NVS-symbol and 9 LittleFsFileSet-symbol checks. This is compile/link/static evidence only, not runtime or hardware proof. Full native/SF2 suites, flash, runtime NVS/LittleFS and physical testing were not performed. C34 remains blocking because the erase guard is narrow and the production root does not exist.

v0.2.75 adds a fail-closed boundary to Esp32NvsConfigBackend::begin(): the backend now trusts only the pre-setup Arduino NVS startup snapshot and no longer calls nvs_flash_init() a second time. ConfigBootCoordinator latches fault before invoking the backend. Focused native suites passed 55/55 across test_nvs_config and test_nvs_startup_guard_policy. The inert ESP32 adapter-link build succeeded; the exact ELF passed 12 NVS wrapper-routing checks, retained the changed config backend and guard status symbols, and retained all nine LittleFsFileSet methods. This proves compilation/linking/static symbol routing only. No full native/SF2 suite, flash, runtime NVS/LittleFS, or physical test was performed. The AP/PIN low-level NVS stores still relied on their owning coordinators being invoked only after successful config boot; no production root existed.

v0.2.74 is documentation-only. It adds docs/AUDIT_v0.2.74.md, a static audit of the pinned NVS erase APIs and LittleFS configuration. The guard blocks only an exact whole-DATA_NVS partition erase through esp_partition_erase_range(); it is not a complete flash erase firewall. The audit also confirms package-level CONFIG_VFS_SUPPORT_DIR=y and ESP_LITTLEFS_ENABLE_FTRUNCATE, but no LittleFS runtime/ftruncate behavior has been built, mounted or tested. It inspected the prior v0.2.72 smoke ELF; no target rebuild or test suite was run in v0.2.74.

v0.2.73 originally documented the C13.2 LittleFS recovery risks: the then-current LogStore::open() could truncate an invalid EOF suffix, and MonthSet::repair() was a non-journaled copy/move/truncate sequence. In v0.2.77, LogStore::open() is read-only and fail-closed; the historical EOF-truncation behavior is removed. The D1–D11 safety policies are now delegated engineering choices recorded in docs/SPECS/littlefs_recovery_protocol.md. The separate destructive recovery transaction, LittleFS commit primitive and journal schema remain unapproved and unimplemented.

v0.2.72 adds a native-only, per-instance append syscall injection seam for LittleFsFileSet. Seven new tests inject short writes, EINTR, partial-prefix write failure, fsync failure, verification read failure, read-back mismatch and close error. The suite command pio test -e native -f 'test_littlefs_fileset' passed **27/27**, exit 0. The tests demonstrate that an append returning false may nevertheless leave partial or complete bytes visible. The synthetic close-fault callback closes the POSIX descriptor before returning EIO, so it does not prove target descriptor behavior. ESP_PLATFORM retains direct syscalls; no production API or storage format changed.

The v0.2.72 inert ESP32 adapter-link rebuild succeeded in 1,769.31 seconds against Espressif32 8.0.0, Arduino-ESP32 3.3.12 / ESP-IDF libs 3.3.12, esptool 5.4.0 and Xtensa 14.2.0+20260121. RAM: 67,356/327,680 bytes (20.6%); application flash: 934,976/1,310,720 bytes (71.3%). The exact ELF passed 12/12 NVS-wrapper route checks and retained all nine LittleFsFileSet public methods. This proves compile/link and static-symbol routing only; it does not prove mounted LittleFS runtime, fault handling, recovery or power-loss durability. No full native or SF2 suite, hardware-probe build, flash or physical-device test has been performed in this snapshot.

The v0.2.69 focused native result remains historical evidence: 43/43 across test_roster_snapshot (26) and test_month_roster (17). Those suites were not rerun. The other 601 of 628 C++ registrations were not run in the v0.2.72 increment.

## Critical limits

C3 remains blocking: no production main/scheduler, HTTP routes, admin UI, production enrollment service or clock caller exists. A future clock caller must handle every ClockApplyResult and stop/reopen after uncertainty. Roster history completeness is parser/projection integrity, not proof of chronological lifecycle semantics (U61).

C13 remains blocking. v0.2.77 makes LogStore::open() inspect-only and fail-closed for anomalous tails; this preserves bytes but does not recover the device automatically after an interrupted append. MonthSet::repair() still has no durable journal or restart protocol, and LittleFsFileSet destructive methods remain disabled. The reusable LittleFS mount owner is not wired into a production root; mounted-target VFS runtime/fault injection, a proven commit primitive, descriptor-close validation on ESP-IDF, and power-loss durability evidence are still missing. Destructive recovery methods remain disabled. Partial writes may still prevent reopen. Clock/roster/GAP retries do not provide general exactly-once semantics.

E18 ID-to-page behavior remains delegated; U5 physical index-page semantics remain unverified. C34's NVS guard is not a complete flash-erase firewall. Only the inert `esp32-adapter-link` smoke target was compiled/linked and its exact ELF inspected in this increment. The `esp32-hardware-probe` was not built or run. No physical sensor, OLED, RTC, EEPROM, AP beacon, DHCP, power-loss, brownout, power-bank runtime or soak behavior was verified.

The current SF2 source has 270 Node test registrations across 12 files. The last recorded 257/257 pass and 25-module syntax check are historical and do not cover the 13 later registrations; no SF2 source changed in v0.2.82. Authoritative SF2 export remains blocked by reporting metadata, snapshot/API and semantic roster-history gaps.


v0.2.84 adds a shared readiness-gated I2C bus owner for the OLED, DS3231 and future EEPROM clients. The hardware probe reads the RTC status register only; it does not set time or clear OSF. Focused native suites passed 56/56. The pinned ESP32 smoke build did not complete or produce an ELF in this session; no target or physical hardware verification is claimed.


v0.2.85 prevents copying or moving `Esp32LittleFsMountBackend`, because its mounted flag and partition label represent one VFS registration. The owning native suite passes 22/22. The pinned target compiled the complete HAL library, but the overall smoke build did not reach final link and produced no ELF. Mounted VFS behavior, recovery and power-loss behavior remain unverified.
