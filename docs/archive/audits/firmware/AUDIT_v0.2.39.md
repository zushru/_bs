# Specialized audit — v0.2.39

Date: 2026-11-08  
Scope: direct ESP-IDF SoftAP adapter, RF-ready RNG ownership, callback lifetime, tests, release documentation and archive integrity.

## Executive result

v0.2.39 adds a hardware-specific SoftAP lifecycle adapter as a new composition component. It is not wired into production boot or an HTTP server. The adapter accepts caller-supplied credentials, installs WPA2-PSK/CCMP configuration before starting the Wi-Fi driver, and only notifies `Esp32SecureRandom` after the AP-start event.

A source review found and corrected two design hazards before packaging:

1. The Arduino-ESP32 3.3.12 `WiFi.softAP()` path calls AP start before applying the new caller-supplied `wifi_config_t`. The production adapter therefore uses the direct ESP-IDF sequence rather than that wrapper.
2. An unregister failure previously would have retained an event handler whose argument was `this`, allowing a later event to dereference a destroyed owner. The callback now receives only the separately allocated EventGroup. If unregister fails, cleanup deliberately retains that group and dependent resources until reboot. A new regression checks the callback after the owner leaves scope.

The AP netif config is built by explicit assignment of its three fields (`base`, `driver`, `stack`) rather than the `ESP_NETIF_DEFAULT_WIFI_AP()` designated-initializer macro, reducing C++ language-mode dependence.

## Implementation inventory

### `Esp32WifiAp`

- New files: `lib/hal_esp32/src/esp32_wifi_ap.h` and `.cpp`.
- Caller supplies SSID, passphrase and channel; no hard-coded production secret or SSID is invented.
- SSID: 1–32 bytes; embedded NUL rejected.
- Passphrase: 8–63 printable ASCII bytes, matching `Config::wifiPass` validation; this is only shape validation, not a strength check (U79).
- Channel input is 1–13; no regulatory country code is configured (U93).
- SSID broadcast; maximum simultaneous clients fixed to two.
- Wi-Fi driver storage is RAM-only; config/NVS remains authoritative.
- WPA2-PSK and CCMP are set in `wifi_config_t` before `esp_wifi_start()`.
- RNG readiness opens only after `WIFI_EVENT_AP_START`; the gate closes before stop/cleanup.
- The AP_START callback receives an EventGroup handle, not the owner pointer. If unregister fails, the EventGroup and resources needed by the still-registered callback are retained rather than freed.
- Calls and `Esp32SecureRandom::fill()` must remain on the same serialized task. The random adapter's readiness flag is a plain `bool`, not a cross-task synchronization primitive.

### Test additions

`test/test_esp32_wifi_ap` contains 19 `RUN_TEST` registrations with 19 matching function definitions. The suite checks:

- Default AP netif `base`, null `driver`, and AP `stack` descriptors.
- Netif creation/attach/default-handler call ordering and failure cleanup.
- AP mode/configuration-before-driver-start ordering.
- WPA2-PSK/CCMP, broadcast SSID and two-client cap.
- RNG gate remains closed during config and driver start, and opens after AP_START.
- Start/config/netif/event-loop failures, AP_START timeout and stop failure.
- Input length, embedded NUL, passphrase shape and channel bounds.
- Callback context remains valid after owner destruction when handler unregister fails.

These are authored source tests only. No compile or test execution was performed for v0.2.39.

## Source/API review

The direct sequence and declarations were checked against the pinned ESP-IDF v5.5.5 source/header/docs and the Arduino-ESP32 3.3.12 source tree:

- Arduino `WiFiAPClass::softAP()` / `APClass::begin()` / `APClass::create()` ordering:  
  https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WiFi/src/WiFiAP.cpp  
  https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WiFi/src/AP.cpp
- ESP-IDF v5.5.5 Wi-Fi API:  
  https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/network/esp_wifi.html
- ESP-IDF v5.5.5 default netif header and implementation:  
  https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_wifi/include/esp_wifi_default.h  
  https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_wifi/src/wifi_default.c
- ESP-IDF v5.5.5 netif default descriptors:  
  https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_netif/include/esp_netif_defaults.h
- ESP-IDF v5.5.5 SoftAP example (set mode, set config, then start):  
  https://github.com/espressif/esp-idf/blob/v5.5.5/examples/wifi/getting_started/softAP/main/softap_example_main.c

The convenience `esp_netif_create_default_wifi_ap()` function uses `assert()` and `ESP_ERROR_CHECK()` around netif creation/attachment/default-handler setup in the reviewed source. The adapter uses checked lower-level operations instead. The actual ArduinoDroid-installed framework remains unconfirmed; source review is not a target compile.

## Test accounting

- Current source inventory: **450 C++ `RUN_TEST` registrations across 26 native suite directories**.
- v0.2.36 native result: **413/413 passed**, reported by the user; that evidence applies only to the v0.2.36 C++ tree.
- v0.2.38 additions: 18 registrations (token-session, RNG and TokenTable preflight).
- v0.2.39 additions: 19 registrations (SoftAP lifecycle).
- **37 registrations added after the v0.2.36 baseline remain unrun.**
- SF2 remains unchanged since v0.2.37, where 257/257 Node tests passed locally. This release adds no SF2 changes.

Registration counts are not pass counts. No native test, Arduino-ESP32 target build, device test, packet capture or phone association test was run for v0.2.39.

## Blocking issues and open uncertainties

1. **E28 — first provisioning:** no approved WPA2 bootstrap credential source, disclosure, rotation or recovery process. `Config::wifiPass` is not available in `CFG_BOOT_NEEDS_SETUP`. The adapter has no fallback and must not start an open AP.
2. **E29 — SSID:** production SSID source/format is undecided.
3. **U79 — passphrase strength:** current validation checks only shape, not entropy/guess resistance or per-device uniqueness.
4. **U89 — target compatibility/first beacon:** actual installed Arduino-ESP32 version is unconfirmed. Build the exact target and capture the first beacon to verify WPA2-PSK/CCMP is active from first transmission.
5. **U92 — failed start state:** `wifiStarted_` is set only after `esp_wifi_start()` returns `ESP_OK`. A non-OK result skips `esp_wifi_stop()`; the target's partial-state semantics must be confirmed.
6. **U93 — regulatory channel plan:** input validation allows 1–13, but no Philippines country code/regulatory configuration is applied. Set and verify the allowed target channel plan in the future production composition.
7. **C6 remains blocking:** there is no production boot caller, PIN credential verifier/writer or KDF policy, HTTP/JSON adapter, protected route wiring, or production fault/session enforcement. E12 remains DELEGATED.
8. **Token scheduling:** `TokenSessionSuccessAction` requires serialization across preflight, response write and session insertion. No production HTTP scheduler proves this contract yet.
9. **RNG task ownership:** `Esp32SecureRandom::rfStarted_` is a plain boolean; use only on the same serialized task as AP lifecycle, or add target-appropriate synchronization.
10. **AP start/cleanup:** netif attach/default-handler cleanup and `esp_wifi_start()` failure paths still require exact target behavior verification. An event-group retention path deliberately requires reboot recovery.

## Unchanged policies and scope

No signed product rule, delegated decision, attendance semantics, sensor page mapping, backup scope, storage format or tested core module was changed. E12 remains DELEGATED. No PIN KDF iteration count was selected. E28 and E29 remain OPEN. The AP adapter is a component only, not a production login/network service.

## Installation and verification commands

Replace the complete old tree with the full v0.2.39 archive; never merge individual files. From the extracted project root:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native 2>&1 | tee native-test.log
```

The native test environment is a host fake; it cannot verify the real Wi-Fi driver, RF entropy source, channel regulation, first beacon, phone association, AP client cap or framework ABI. Those require the exact target package and hardware.
