## v0.2.48 (2026-11-08) — shared startup guard and Codespaces migration

- Promoted the NVS startup guard out of the diagnostic probe into `Esp32NvsStartupGuard`, shared by the inert adapter smoke target and diagnostic probe. The native policy is tested independently; the ESP-IDF wrapper source is gated to `ARDUINO_ARCH_ESP32` so native builds do not include target-only headers.
- Added nine `test_nvs_startup_guard` registrations for fail-closed startup, first-result latching, whole-DATA_NVS erase blocking, and normal sector/non-NVS erase allowance. Four duplicate probe-only predicate tests were removed from `test_hardware_probe`; that suite now covers credential encoding only.
- Fixed Xtensa inspector false negatives using fresh disassembly at internal branch targets. Added positive and negative regression fixtures.
- Added Debian 12 Codespaces devcontainer with PlatformIO Core 6.2.0 and Node 22, plus repeatable host-test, ESP32 compile/link and environment diagnostic scripts. No upload command is included. `docs/CODESPACES.md` and `CODESPACES_IMPORT.md` document setup from Android and the branch-preserving import from the older GitHub main branch.
- No handoff rule, signed/delegated product decision, SF2 source, attendance core or hardware pin assignment changed. C34 remains a production startup blocker until the guard is wired into the actual firmware boot owner and runtime-tested.
- Verification: full PlatformIO native run passed **533/533 in 00:19:09.008**; SF2 Node run passed **257/257**; all 25 `.mjs` files passed syntax checks; inspector positive/negative fixtures passed. Both shared-guard ESP32 targets compiled/linked before the final native-only preprocessor guard was added. The final PRoot rebuild stalled before output; final-tree target rebuild and both ELF inspections are deferred to Codespaces. No hardware test or flash was performed.

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
