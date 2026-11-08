# ESP32 Wi-Fi AP adapter

Status: v0.2.42 corrected the AP suite's test-local `esp_fill_random` link dependency, and the user reports that the complete v0.2.42 PlatformIO native run passed all 29 suites, including the 20-case AP suite. The prior 20/20 AP and 36/36 credential bootstrap results under a sanitizer-enabled Unity-compatible stand-in remain supplemental local evidence. v0.2.43 adds an isolated target compile/link smoke environment pinned to Espressif32 platform 8.0.0 and selecting the Arduino framework. That release supports Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5; exact resolved framework/toolchain package revisions remain to be captured from the build output. The target build has not yet run. v0.2.45 adds an AP DHCP-assignment observer and first-provision coordinator interface. No AP radio test, phone association test or production boot/HTTP caller exists.

## Purpose

`Esp32WifiAp` is the intended single owner of the production SoftAP lifecycle and the RF-ready notification consumed by `Esp32SecureRandom`. It uses ESP-IDF directly so the complete AP configuration is installed before the Wi-Fi driver is started. The Arduino `WiFi.softAP()` wrapper is deliberately not used.

## Why direct ESP-IDF is required

The recorded framework package `4.30312.261024` maps to Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 (U38). In the exact Arduino-ESP32 3.3.12 source, `WiFiAPClass::softAP()` calls `AP.begin()` and then `AP.create()`. `AP.begin()` enables AP mode and waits for the AP-start event; `AP.create()` calls `begin()` and only afterward builds the `wifi_config_t` and calls `esp_wifi_set_config()`. The radio therefore starts before the caller-supplied SSID/passphrase/auth/cipher configuration is installed. A prior driver-stored configuration may influence that interval; the adapter cannot claim a continuous WPA2-only startup guarantee through this wrapper.

The v0.2.39 adapter instead follows the Espressif IDF sequence:

1. Initialize `esp_netif` and the default event loop (accepting `ESP_ERR_INVALID_STATE` only as “already initialized”).
2. Create the default AP netif with a zero-initialized `esp_netif_config_t`, assigning `base = ESP_NETIF_BASE_DEFAULT_WIFI_AP`, `driver = nullptr` and `stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP` explicitly. Then call checked `esp_netif_new()`, `esp_netif_attach_wifi_ap()` and `esp_wifi_set_default_wifi_ap_handlers()`. The `ESP_NETIF_DEFAULT_WIFI_AP()` designated-initializer macro is intentionally avoided for C++ language-mode compatibility. The convenience `esp_netif_create_default_wifi_ap()` wrapper is also avoided because the pinned v5.5.5 declaration says it may abort on allocation/attach/handler errors.
3. Initialize the Wi-Fi driver and set storage to RAM.
4. Set mode to `WIFI_MODE_AP` while the driver is not yet started.
5. Fill the complete `wifi_config_t`: caller SSID/passphrase, `WIFI_AUTH_WPA2_PSK`, `WIFI_CIPHER_TYPE_CCMP`, broadcast SSID, channel, FTM disabled and two-client cap.
6. Call `esp_wifi_set_config(WIFI_IF_AP, ...)` before `esp_wifi_start()`.
7. Register for `WIFI_EVENT_AP_START`, start the driver, and wait up to 5 seconds for that event. Only then does the adapter open the `Esp32SecureRandom` RF gate.

This sequence removes the Arduino wrapper's config-after-start ordering from this adapter. It remains [UNVERIFIED: compile against the exact installed target headers, then verify over-the-air that no pre-configuration beacon is emitted and WPA2-PSK/CCMP is active from the first beacon].

## Contract

- The caller supplies SSID and passphrase. The adapter has no default credentials and does not reuse diagnostic sketch credentials.
- SSID length is explicit and limited to 1–32 bytes. Embedded NUL is rejected; no silent truncation.
- Passphrase length is explicit and limited to 8–63 printable ASCII bytes, matching `Config::wifiPass` validation. Empty/open APs are refused.
- Channel is explicit and limited to 1–13. No regulatory country code is set by this adapter. The production composition must choose and set the Philippines country configuration before startup; the permitted channel plan and actual framework behavior remain unverified.
- SSID is broadcast (`ssid_hidden=0`); the simultaneous client limit is fixed to the signed handoff value of two.
- `wifi_config_t` is populated with WPA2-PSK and CCMP before `esp_wifi_start()`. Wi-Fi driver storage is set to RAM; the application config/NVS remains the source of truth.
- The caller must not initialize Wi-Fi first or call Arduino `WiFi.mode()` / `WiFi.softAP()` independently. `esp_netif_defaults.h` provides the AP base/stack descriptors; `esp_wifi_default.h` provides the attach/default-handler/destroy declarations. This class is the sole Wi-Fi driver owner. Calls, AP lifecycle and RNG use must stay on one serialized application task.
- The AP_START callback only signals a FreeRTOS EventGroup. It does not write `Esp32SecureRandom::rfStarted_`; the serialized caller opens the RNG gate after the event wait succeeds.
- Any initialization/config/start/timeout failure latches a fault. Cleanup stops the driver when `esp_wifi_start()` reported success, unregisters the custom AP_START handler, deinitializes Wi-Fi, checks the return from `esp_wifi_clear_default_wifi_driver_and_handlers()`, and only then calls `esp_netif_destroy()`. The void `esp_netif_destroy_default_wifi()` wrapper is not used because it ignores the clear result and destroys the netif regardless. If stop or unregister fails, still-referenced resources are retained rather than freed unsafely. The AP_START callback argument is the independently allocated EventGroup, not `this`, so a failed unregister cannot leave a dangling owner pointer; the group is intentionally leaked until reboot on that path. Reboot is the only recovery. [UNVERIFIED: if `esp_wifi_start()` returns an error after partially changing driver state, current `wifiStarted_` tracking assumes no stop is required; target API/device behavior must confirm this before relying on failure cleanup].
- `stop()` clears the RNG gate before `esp_wifi_stop()`. It is for controlled shutdown only; the signed product rule is AP always-on. Do not expose stop/restart to HTTP or users.
- v0.2.45 registers `IP_EVENT_AP_STAIPASSIGNED` before `esp_wifi_start()`. The callback compares `ip_event_ap_staipassigned_t::esp_netif` with this AP's netif and sets a single non-PII event bit only for a matching lease. `hasAssignedClientIp()` reads that bit for the current AP run; a new AP start receives a fresh EventGroup. No station MAC/IP is copied or persisted. The bit is latched and is not cleared on station disconnect: it proves a DHCP assignment happened during this AP run, not that a station is still connected, not who it is, and not that setup authorization succeeded.
- The passphrase is held in a bounded `wifi_config_t` temporary and volatile-cleared immediately after `esp_wifi_set_config()`. The Wi-Fi driver must retain its own copy to operate the AP.

## First-provision credential source and remaining gates

E28 is now SIGNED and specified in [`ap_credential_bootstrap.md`](ap_credential_bootstrap.md). v0.2.41 adds a core boot-state resolver, a create-only NVS string store (`attwifi/ap_pass_v1`), and a one-shot pre-radio RNG adapter. `CFG_BOOT_NEEDS_SETUP` creates or reuses a 128-bit secret and returns `disclose=true` on every setup-required boot. `CFG_BOOT_READY` uses validated `Config::wifiPass`, backfills the separate NVS record only when absent, and fails closed if an existing value differs. The implementation and its failure paths are host-tested under a temporary Unity-compatible stand-in only.

v0.2.44 adds `Esp32Oled`; v0.2.45 adds `ApCredentialSetupCoordinator`. It asks the display to transfer a `disclose=true` secret before starting the AP, waits for AP readiness and a matching DHCP assignment, and requires an authorization-gate object supplied to the current `provision()` call. It re-reads the durable secret and replaces `Config::wifiPass` with that exact passphrase before provisioning. This prevents the previously possible AP/config mismatch. The coordinator is not a production boot or HTTP caller. The Adafruit display API cannot prove that pixels were physically visible. The future HTTP route must create a fresh authorization gate bound to the current request/session; the gate's provenance is not proven by the core. Exact `bootloader_support` linkage and true-entropy behavior on the installed target are [UNVERIFIED: exact ESP32 build and first-boot hardware test]. The production first-provision coordinator now supplies the fixed SSID `att` and channel 1; callers cannot override either. E30 remains DELEGATED to no reset/rotation API and fail-closed behavior; a physically supervised USB recovery procedure is not yet implemented.

Production first-provision SSID is fixed by E29 to `att`; channel 1 is delegated by E31; the coordinator does not accept caller overrides. E30 is DELEGATED to no remote rotation/reset or automatic erase. Malformed or mismatched storage fails closed; a physically supervised USB service recovery procedure is not implemented.

## External references reviewed

- Arduino-ESP32 3.3.12 `WiFiAP.cpp`: `WiFiAPClass::softAP()` calls `AP.begin()` before `AP.create()`: https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WiFi/src/WiFiAP.cpp
- Arduino-ESP32 3.3.12 `AP.cpp`: `APClass::begin()` enables AP and waits for AP_START; `APClass::create()` calls `begin()` before `esp_wifi_set_config()`: https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WiFi/src/AP.cpp
- ESP-IDF 5.5.5 Wi-Fi API: `esp_wifi_init`, `esp_wifi_set_mode`, `esp_wifi_set_config`, `esp_wifi_start`, `esp_wifi_stop`, storage modes and event semantics: https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/api-reference/network/esp_wifi.html
- ESP-IDF 5.5.5 SoftAP example: configure `wifi_config_t`, call `esp_wifi_set_mode`, `esp_wifi_set_config`, then `esp_wifi_start`: https://github.com/espressif/esp-idf/blob/v5.5.5/examples/wifi/getting_started/softAP/main/softap_example_main.c

## Test coverage updated in v0.2.45

`test_esp32_wifi_ap` has 22 registrations covering AP-netif base/driver/stack fields, config-before-start ordering, WPA2/CCMP fields, RF readiness after AP_START only, config/init/start failure, AP_START timeout and stop cleanup, accepted already-initialized netif/event loop, invalid SSID, maximum SSID/passphrase lengths, printable passphrase policy, channel bounds, stop failure, repeated start and invalid-input retry, netif allocation failure, attach failure and default-handler registration failure, and callback-context survival after failed unregister and owner destruction, AP-netif-only DHCP assignment, unrelated-netif rejection, and DHCP event cleanup. The user reports that the 20 AP registrations in v0.2.44 passed in the full v0.2.44 PlatformIO run (within 500/500). At v0.2.45, the updated 22-case suite had only local g++ ASan/UBSan stand-in evidence (22/22, 315 assertions). Subsequent user-supplied v0.2.46 evidence reports all 520/520 PlatformIO native cases passed and the expanded target smoke build linked, covering this AP suite and coordinator at native/compile-link level. Neither host tests nor target compilation can verify RF transmission or first-beacon radio security; those require target/device observation.


## v0.2.41 AP credential bootstrap tests

`test_ap_credential` has 18 registrations for cold/fault boot rejection, generation/encoding, secret reuse and repeated disclosure while NEEDS_SETUP, entropy and store failures, malformed existing records, READY matching and migration, mismatch/failure handling, create-race behavior and null output. `test_nvs_ap_credential` has 15 registrations for missing keys, 32/63-character round-trip, create-only semantics, read/write open failures, get/set/commit faults, wrong NVS type, malformed strings and buffer clearing. `test_esp32_bootstrap_random` has 3 registrations for enable/fill/disable order and argument validation. All 36 were compiled/executed using g++ with a temporary Unity-compatible stand-in and ASan/UBSan; this is supplemental host evidence only. The user reports that all 29 native PlatformIO suites passed in v0.2.42, including the three credential bootstrap suites. The target build and entropy measurement remain unverified.


## Subsequent verification status (v0.2.46/v0.2.47)

The user reports that v0.2.46 passed all 520 PlatformIO native registrations, including the 22 AP tests, and that the expanded inert ESP32 adapter smoke target compiled/linked. This does not prove AP start, first-beacon security, country/channel acceptance, a real DHCP assignment or station disconnect behavior. v0.2.47 adds an independent hardware-probe target; it has not yet been built. E31 remains DELEGATED (channel 1), and U93 remains open.
