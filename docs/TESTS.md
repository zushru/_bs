# v0.2.48 verification status

- Source inventory: **533 C++ `RUN_TEST` registrations across 33 suites**.
- Full PlatformIO native batch in connected Termux/PRoot: **533 test cases: 533 succeeded in 00:19:09.008**. This is a host-side PlatformIO result, not a board test.
- The focused `test_nvs_startup_guard` suite passed **9/9** and is included in the aggregate 533-case result. The probe credential helper suite passed 4/4.
- `tools/test_inspect_probe_wrappers.sh` passed its positive Xtensa branch-target fixture and negative missing-delegate fixture.
- SF2 Node suite: **257/257 passed** in the local staging environment. All **25** `.mjs` modules passed `node --check`.
- Before the final `ARDUINO_ARCH_ESP32` preprocessor guard was added to the wrapper translation unit, the smoke target compiled/linked successfully (47,028 B RAM, 14.4%; 912,812 B flash, 69.6%) and its ELF inspector passed. The diagnostic probe also compiled/linked successfully (46,756 B RAM, 14.3%; 928,156 B flash, 70.8%).
- A final incremental PRoot target rebuild after the native-only preprocessor guard correction stalled before producing build output and was stopped. The exact final source tree's target rebuild, smoke ELF inspection and probe ELF inspection must be rerun in Codespaces using `./tools/codespace-build-esp32.sh`.
- No firmware was flashed. No hardware behavior is verified.


# Test inventory and evidence

## v0.2.47 — diagnostic hardware probe and NVS erase guard (not run)

- Added `test_hardware_probe` with eight registrations: four for exact 128-bit to 32-character lowercase-hex credential encoding, invalid input/output handling and full output-buffer clearing; four for the NVS erase guard's exact scope (block only a full DATA_NVS partition erase, permit sector erase, permit full non-NVS erase, reject invalid ranges).
- Static inventory: **528 C++ `RUN_TEST` registrations across 32 suites** (previous snapshot 520 across 31). The new eight registrations have **not** been run; no compiler or PlatformIO execution was performed for v0.2.47 in this environment.
- Added a separate `platformio.esp32-probe.ini` target. It is not built yet. The inert v0.2.46 adapter smoke result does not verify it. The NVS wrapper's final ELF routing must be checked, not inferred from a successful compile.
- The new probe must be built only after reading C34. Run `./tools/inspect_probe_wrappers.sh` after linking; it must prove `initArduino()` calls both wrappers and each wrapper calls the real IDF function. Do not flash based only on a successful build or symbol presence. It aborts before I2C/OLED/AP if NVS init fails. Do not induce NVS corruption to test the failure branch on the only device.

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
| test_enroll | 36 | Enrolled-roster helpers plus FSM lifecycle, pending-command, cancel-release, in-flight store cancellation, and skip-race cases |
| test_as608 | 15 | Raw command packets, ACK validation, capacity, search, 32-byte index pages, and fail-closed occupancy refresh |
| test_as608_gate | 26 | Attendance/enrollment arbitration, Stream owner checks, enrollment session, attendance setup and scan leases, runtime composition, no-finger/no-match release, and poisoned-link fail-closed behavior |
| test_esp32_oled | 13 | 128x32 SSD1306 adapter: shared TwoWire bus, fixed 0x3C address/100 kHz settings, exact E28 credential formatting, non-disclosure/invalid-secret clearing and framebuffer clearing. All 13 were included in the user-reported v0.2.46 520/520 PlatformIO pass; physical panel verification remains pending |
| test_esp32_secure_random | 5 | RF-start/stop lifecycle gate around `esp_fill_random()`; the user reports this suite passed in the full v0.2.42 PlatformIO run |
| test_ap_credential_setup | 18 | NEEDS_SETUP E28 disclosure → AP start ordering, SSID/channel validation, DHCP + per-request auth gates, `Config::wifiPass` consistency, failure latch and retry-only blank-frame cleanup; all 18 were included in the user-reported v0.2.46 520/520 PlatformIO pass |
| test_esp32_wifi_ap | 22 | Direct ESP-IDF config-before-start ordering, WPA2/CCMP config, AP_START-gated RNG, netif allocation/attach/default-handler failures, driver/start timeout cleanup, stop/input validation, and AP-netif-only DHCP lease event plus callback lifetime; all 22 were included in the user-reported v0.2.46 520/520 PlatformIO pass |
| test_hardware_probe | 8 | Probe-only lowercase-hex credential encoding/output clearing and NVS whole-partition erase guard predicate; new in v0.2.47 and not yet run |
| test_attendance_admission | 12 | Log-backed no-class/closed-session admission, duplicate suppression, config replay, and fail-closed paths |
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
| **CURRENT INVENTORY** | **528** | 528 source `RUN_TEST` registrations across 32 suites in v0.2.47. The user reports all 520 previous v0.2.46 cases passed in PlatformIO; the eight new probe-helper registrations remain unrun. The earlier v0.2.32 SIGHUP and 390-versus-389 output remain historical anomalies, not current failures. |



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
- LittleFS IFileSet driver, power loss, atomic rename.
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
