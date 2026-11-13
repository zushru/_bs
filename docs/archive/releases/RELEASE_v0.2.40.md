# Release v0.2.40 — 2026-11-08

Status: **checked SoftAP netif teardown correction**. Development snapshot only; not production firmware and not hardware-verified. Install only by replacing the complete project tree with this archive. Never merge individual files into an older tree.

## Changes

1. `Esp32WifiAp::cleanup_()` now calls the error-returning `esp_wifi_clear_default_wifi_driver_and_handlers()` directly.
2. `esp_netif_destroy()` is called only if the clear/detach operation returns `ESP_OK`. On a non-OK result the netif remains allocated and the AP owner latches a fault rather than destroying an object with an indeterminate detach result.
3. Removed the AP test stub for the `void` `esp_netif_destroy_default_wifi()` wrapper. The fake now implements the same checked lower-level clear and destroy calls as production.
4. Strengthened two existing AP test cases to assert teardown order and that a failed clear never triggers netif destruction. No test registration was added.
5. Corrected the release inventory: `test_esp32_wifi_ap` has 20 registrations, not 19. The project has 451 C++ `RUN_TEST` registrations across 26 suites. The 38 registrations added after the v0.2.36 baseline (18 in v0.2.38 and 20 in v0.2.39) have not been run under PlatformIO. The AP and RNG suites passed supplemental stand-in runs (20/20 and 5/5); the remaining 13 token-session/API registrations were not executed here.

## Verification evidence

- The user supplied a v0.2.36 native run reporting 413/413 passed across 23 suites. That result applies only to the unchanged v0.2.36 C++ tree.
- SF2 Node tests were rerun against the v0.2.40 working tree: **257/257 passed**; all 25 `.mjs` modules passed `node --check`. The SF2 source itself is unchanged since v0.2.37.
- Source review used the pinned ESP-IDF v5.5.5 implementation. Its `esp_wifi_clear_default_wifi_driver_and_handlers()` returns `esp_err_t`; `esp_netif_destroy_default_wifi()` ignores that return and calls `esp_netif_destroy()` regardless.
- AP 20/20 and RNG 5/5 compiled/executed with a temporary Unity-compatible stand-in harness and `g++ -std=c++11 -Wall -Wextra -Werror`. This is not PlatformIO/Unity evidence. SF2 Node tests passed 257/257 and all 25 `.mjs` files passed `node --check`. No PlatformIO full-suite run, ESP32 target build or device test was run for v0.2.40.

## Remaining blockers and uncertainties

- E28 OPEN: no first-provision WPA2 AP credential source/rotation path. `Config::wifiPass` is not available in `CFG_BOOT_NEEDS_SETUP`. No open AP, diagnostic password, or setup PIN fallback is permitted.
- E29 OPEN: production SSID source/naming format is undecided.
- The adapter has no production caller and assumes sole ownership of the Wi-Fi driver/AP netif. It and `Esp32SecureRandom::fill()` must run on one serialized task.
- U92: cleanup after a non-OK `esp_wifi_start()` return is not proven against partial driver-start states.
- U93: channel/country configuration remains unresolved.
- C6 remains blocking: no production boot composition, PIN credential verifier/writer, chosen KDF policy, E12 counter-domain resolution, HTTP/JSON adapter, or protected route enforcement.
- E12 remains DELEGATED. No PIN KDF iteration count was selected.
- No ESP32 target compile or hardware verification is claimed. The actual ArduinoDroid-installed framework package is not independently confirmed.

## Required verification

From a clean full v0.2.40 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native 2>&1 | tee native-test.log
```

Then compile the production ESP32 adapter against the exact installed framework, followed by device verification of the AP's first beacon, WPA2-PSK/CCMP, broadcast SSID, two-client limit, AP_START ordering, RNG lifecycle, and shutdown. Do not integrate first-provision AP startup until E28 and E29 are resolved.
