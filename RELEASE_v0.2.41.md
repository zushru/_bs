# Release v0.2.41 — 2026-11-08

Status: **first-provision WPA2 AP credential bootstrap modules added**. Development snapshot only; not production firmware and not hardware-verified. Install only by replacing the complete project tree with this archive. Never merge individual files into an older tree.

## Changes

1. Added `ApCredentialBootstrap` in the core. On `CFG_BOOT_NEEDS_SETUP`, it generates 16 bytes (128 bits), encodes them as a 32-character lower-case-hex passphrase, persists it create-only and validates a fresh read-back. It requests OLED disclosure on every boot that remains in NEEDS_SETUP, so resets before setup completion reuse and re-display the same passphrase.
2. On `CFG_BOOT_READY`, the validated `Config::wifiPass` is authoritative. The manager copies it to the separate credential record only when that record is absent (upgrade/restore path), and fails closed on an existing mismatch or NVS fault. READY mode never requests disclosure.
3. Added `Esp32NvsApCredentialStore` using namespace `attwifi`, key `ap_pass_v1`, NVS string storage, `nvs_commit()` and error/malformed-value handling. It never overwrites an existing record.
4. Added `Esp32BootstrapRandom`, which temporarily enables the ESP-IDF SAR-ADC entropy source, fills using `esp_fill_random()`, and disables that source before Wi-Fi/ADC use. This is separate from the RF-gated token RNG.
5. Added three test suites: 18 core bootstrap registrations, 15 NVS-store registrations and 3 RNG-adapter registrations. Added a specification at `docs/SPECS/ap_credential_bootstrap.md`.
6. E28 is now SIGNED for the bootstrap source/disclosure design. E29 SSID naming and E30 passphrase reset/rotation/recovery remain OPEN. Production boot composition, OLED driver/disclosure UI, AP caller, and recovery path are not implemented.

## Verification evidence

- The 18 core tests passed: 83 assertions, 0 failures.
- The 15 NVS tests passed: 111 assertions, 0 failures.
- The 3 entropy adapter tests passed: 30 assertions, 0 failures.
- All 36 registrations were compiled/executed with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer` and a temporary Unity-compatible stand-in harness. This is not PlatformIO/Unity evidence.
- Current inventory: 487 C++ `RUN_TEST` registrations across 29 suites. The user-reported v0.2.36 PlatformIO baseline remains 413/413 across 23 suites; 74 post-baseline registrations have not been run through PlatformIO. 61 of those 74 have supplemental stand-in execution; 13 token-session/API registrations remain unexecuted here.
- The unchanged SF2 Node suite was rerun and passed 257/257; all 25 `.mjs` modules passed `node --check`.
- No PlatformIO full-suite run, ESP32 target build, OLED check, entropy measurement, AP radio test or device test was run for v0.2.41.

## Remaining blockers and uncertainties

- E29 OPEN: production SSID source/naming format. Do not invent it.
- E30 OPEN: AP passphrase rotation/reset/recovery. There is no overwrite/delete/reset API.
- No production composition calls `ApCredentialBootstrap`, displays its `disclose` result on the OLED, passes the secret into first config provisioning or calls `Esp32WifiAp`. The caller must show the secret before transitioning to READY.
- Exact `bootloader_random_enable()` header/component linkage and actual ESP32 pre-radio entropy behavior are unverified. Build against the exact installed Arduino-ESP32/ESP-IDF package.
- NVS commit/read-back does not prove power-cut durability. Test creation, interrupted writes, reboot reuse, READY migration and restore mismatch on the target.
- C6 remains blocking: PIN verifier/writer, KDF policy, E12 counter domain, HTTP/JSON adapter, route enforcement and production boot composition remain absent.
- U92/U93 (AP partial-start cleanup and regulatory-country/channel behavior) remain open.

## Required verification

From a clean full v0.2.41 extraction:

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

Then build the exact ESP32 target/framework, verify pre-radio RNG linkage and first credential generation, inspect OLED disclosure sequencing, and capture the SoftAP's first beacon to verify WPA2-PSK/CCMP, two-client cap, AP_START ordering and shutdown. Do not claim production readiness until those checks and the unresolved SSID/recovery decisions are completed.
