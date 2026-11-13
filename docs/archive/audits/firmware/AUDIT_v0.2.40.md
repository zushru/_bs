# Audit — v0.2.40 checked SoftAP teardown

Date: 2026-11-08

## Scope

This is a continuation snapshot from v0.2.39. The only C++ implementation change is the teardown correction in `lib/hal_esp32/src/esp32_wifi_ap.cpp`; the AP test fake/stub and assertions were adjusted to match the checked lower-level operation. Product policy, core logic, event formats, config schema, hardware pins, SF2 code and signed/delegated decisions were not changed.

## Defect corrected

The v0.2.39 cleanup path used `esp_netif_destroy_default_wifi()`, a `void` convenience wrapper. The pinned ESP-IDF v5.5.5 header declares `esp_wifi_clear_default_wifi_driver_and_handlers(void*)` as returning `esp_err_t`, and separately declares `esp_netif_destroy_default_wifi(void*)` as returning `void`. In pinned `wifi_default.c`, the destroy wrapper ignores the clear result before destroying the netif. References: `https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_wifi/include/esp_wifi_default.h` and `https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_wifi/src/wifi_default.c`. That made the adapter's error handling unverifiable: if detach failed, the wrapper could still destroy a possibly attached netif. The v0.2.40 cleanup now calls the error-returning clear function directly and invokes `esp_netif_destroy()` only after `ESP_OK`. On a failed clear, the AP owner retains the netif and latches its cleanup fault.

## Local test execution

Two suites were independently compiled and executed in this environment using `g++ -std=c++11 -Wall -Wextra -Werror`, project test sources, and a temporary Unity-compatible stand-in harness outside the project tree:

| Suite | Registrations | Result |
|---|---:|---:|
| `test_esp32_wifi_ap` | 20 | 20 passed, 0 failed |
| `test_esp32_secure_random` | 5 | 5 passed, 0 failed |

The AP test compile included an external test stub for `esp_fill_random`; the RNG suite defines its own fake. The temporary harness is not Unity and is not the project's PlatformIO environment. These are supplemental host checks only. **No PlatformIO test, full native regression, Arduino-ESP32 target build, radio test, or hardware test was run here.**

## Test inventory

- 451 C++ `RUN_TEST` registrations across 26 suites (source recount: registration count matches test function definitions).
- User-supplied v0.2.36 PlatformIO evidence: 413/413 passed across 23 suites; this applies only to that earlier C++ tree.
- 38 registrations were added after v0.2.36 (18 in v0.2.38, 20 in v0.2.39). Twenty-five of these have supplemental stand-in-harness execution (20 AP + 5 RNG); 13 token-session/API registrations have not been executed here. All 38 still require PlatformIO verification.
- SF2 Node: rerun against the v0.2.40 working tree and passed 257/257; all 25 `.mjs` modules passed `node --check`. No SF2 source changed in v0.2.38–v0.2.40.

## Critical unresolved items

1. **No exact target build.** The recorded intended Arduino-ESP32/ESP-IDF version does not prove the user's ArduinoDroid installation matches. Direct ESP-IDF header/API compatibility remains [UNVERIFIED: compile against the exact installed toolchain].
2. **First AP credential (E28) OPEN.** No valid WPA2 passphrase exists before first provisioning. Do not start an open AP, use the setup code, or use diagnostic credentials.
3. **Production SSID (E29) OPEN.** No approved source/format.
4. **Partial `esp_wifi_start()` failure (U92) OPEN.** `wifiStarted_` is set only after `ESP_OK`; target behavior after a non-OK result that partially changes state is unverified.
5. **Regulatory country/channel (U93) OPEN.** Adapter accepts channel 1–13 but does not set the Philippines country configuration.
6. **Single-task contract.** AP lifecycle, `Esp32SecureRandom` notifications/fill, and future TokenTable mutations must be serialized. `rfStarted_` is a plain `bool`; no cross-task synchronization is provided.
7. **C6 incomplete.** No production composition, PIN credential verifier/writer, KDF policy, E12 counter-domain selection, HTTP/JSON adapter or protected-route wiring.
8. **Power-loss/restore/hardware qualification remain absent.** No LittleFS driver or atomicity proof, restore swap/recovery journal, 10-hour soak, brownout test or device verification.

## Required user verification

From a clean extraction of the complete v0.2.40 archive, run the commands in `RELEASE_v0.2.40.md`. In particular, run focused `test_esp32_wifi_ap`, `test_esp32_secure_random`, `test_pin_auth_session`, `test_api`, then the complete native suite under PlatformIO. Preserve full output and exit status. After native tests pass, build against the exact installed Arduino-ESP32 package and verify the AP's first beacon is WPA2-PSK/CCMP, SSID is broadcast, the two-client cap is enforced, AP_START gates RNG availability, and shutdown/failure cleanup works.
