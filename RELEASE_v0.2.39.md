# Release v0.2.39 — 2026-11-08

Status: **direct ESP-IDF SoftAP lifecycle adapter and RF-ready RNG ownership added**. Development snapshot only; not production firmware and not hardware-verified. Replace the complete `att/` tree; never merge individual files into an older tree.

## Changes

1. Added `Esp32WifiAp` in `lib/hal_esp32/src/esp32_wifi_ap.*` as the intended sole owner of the ESP-IDF SoftAP lifecycle and `Esp32SecureRandom` RF readiness notifications.
2. The adapter accepts caller-supplied SSID/passphrase only. It validates SSID length 1–32 bytes with no embedded NUL, passphrase length 8–63 printable ASCII bytes, and an explicit channel 1–13. It broadcasts the SSID and fixes the signed two-client limit.
3. It configures `WIFI_AUTH_WPA2_PSK` and `WIFI_CIPHER_TYPE_CCMP` in a complete `wifi_config_t` and calls `esp_wifi_set_config()` before `esp_wifi_start()`. Wi-Fi storage is RAM-only; application config remains authoritative. The RNG gate opens only after `WIFI_EVENT_AP_START` is received.
4. A source audit found the Arduino-ESP32 3.3.12 `WiFi.softAP()` path starts AP mode and waits for AP_START before `AP.create()` applies the supplied Wi-Fi config. This adapter therefore does not call `WiFi.mode()` or `WiFi.softAP()`. It uses checked lower-level ESP-IDF netif creation, attach and default-handler registration rather than `esp_netif_create_default_wifi_ap()`, whose v5.5.5 header documents assert/abort behavior on setup errors.
5. Added 19 native registrations in `test_esp32_wifi_ap`, including netif allocation/attach/default-handler error paths and config-before-start ordering. Current source inventory: **450 registrations across 26 native suites**. v0.2.38 and v0.2.39 add 37 registrations beyond the v0.2.36 baseline (18 and 19 respectively); all 37 remain unrun.
6. E28 remains OPEN for the first-provision WPA2 AP credential source/rotation. E29 remains OPEN for production SSID naming. The adapter deliberately invents neither credential nor SSID.

## Verification status

- User-reported v0.2.36 native baseline: **413/413 passed across 23 suites**. That applies only to the unchanged v0.2.36 C++ source/test tree. v0.2.38/v0.2.39 add 37 registrations beyond it; these have not been executed.
- SF2 Node suite: **257/257 passed locally in v0.2.37**; the SF2 tree is unchanged here.
- Source/API review: checked the direct sequence and declarations against ESP-IDF v5.5.5 reference docs/header/source. This is not a target compile.
- Manifest and archive checksum are verified; the archive was independently extracted and every manifest entry checked.
- No v0.2.39 PlatformIO test, Arduino-ESP32 target build, AP radio test, phone association test or hardware verification was run.

## Boundaries and remaining blockers

- **E28 OPEN:** no first-provision WPA2 credential source/rotation. **E29 OPEN:** no production SSID source/naming format. The AP must not be opened or given a diagnostic default. `Config::wifiPass` is unavailable in `CFG_BOOT_NEEDS_SETUP`.
- The documented source package mapping is Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, but the exact framework installed in the user's ArduinoDroid environment is not confirmed. No target compile or packet capture proves first-beacon security.
- `Esp32WifiAp` has no production caller. It assumes sole ownership of the Wi-Fi driver/netif and must run on the same serialized task as `Esp32SecureRandom::fill()`; the random adapter's readiness flag is not thread-safe.
- The default AP netif is created through checked lower-level calls. Its `esp_netif_config_t` uses the pinned AP base/stack descriptors assigned explicitly (avoiding a C designated-initializer macro in C++), then calls `esp_netif_new`, `esp_netif_attach_wifi_ap` and `esp_wifi_set_default_wifi_ap_handlers`. This avoids the convenience constructor's documented abort behavior. Target cleanup behavior for failures inside attach/default-handler setup still needs framework verification.
- C6 remains blocking: no production boot composition, credential verifier/writer, PIN KDF policy, E12 counter-domain selection, HTTP/JSON adapter, protected routes or complete fault/session integration.
- No NVS power-cut guarantee, LittleFS driver/atomicity proof, restore swap/recovery, official XLSX output, physical sensor verification, soak or brownout test is claimed.
- No signed/delegated product policy, existing persisted config format, sensor mapping or backup scope changed. E12 remains DELEGATED; no KDF iteration count was selected.

## Required verification

From a clean full v0.2.39 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native 2>&1 | tee native-test.log
```

Then build against the exact installed Arduino-ESP32/ESP-IDF package. Confirm `esp_wifi_default.h` declarations, direct API availability, AP_START ordering, actual WPA2-PSK/CCMP from the first beacon (packet capture), broadcast SSID and max two associated clients. Do not implement production first-boot AP startup until E28 and E29 are resolved.
