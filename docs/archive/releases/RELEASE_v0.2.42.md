# Release v0.2.42 — 2026-11-08

Status: **native AP suite link-stub correction**. Development snapshot only; not production firmware and not hardware-verified. Install only by replacing the complete project tree with this archive. Do not merge individual files into an older tree.

## Root cause

The user's v0.2.41 PlatformIO run collected 29 suites. The user reports all suites passed except `test_esp32_wifi_ap`. That suite compiled its test object and the `hal_esp32` library, then failed at final link:

```text
undefined reference to `esp_fill_random'
```

`Esp32WifiAp` depends on `Esp32SecureRandom`, so the AP suite pulls in `esp32_secure_random.cpp`. The header stub declared `esp_fill_random`, but only the separate `test_esp32_secure_random` executable defined its host fake. PlatformIO builds each suite as a separate executable; a fake defined in another suite cannot satisfy this linker symbol. The 20 AP assertions did not run in the failed attempt. This was a test harness omission, not evidence of an ESP32 radio failure.

## Changes

1. Added a deterministic `esp_fill_random` fake in `test/test_esp32_wifi_ap/test_main.cpp`. It is test-local and has no effect on firmware.
2. Extended the AP/RNG integration test: `Esp32SecureRandom::fill()` must refuse before AP start, succeed after AP_START opens the RF gate and produce the deterministic test bytes, then refuse after `Esp32WifiAp::stop()` closes the gate.
3. No production source, public API, decision or test registration count changed. The inventory remains 487 registrations across 29 suites.
4. Updated test evidence, critical issues, context and README to record the user's v0.2.41 output and this correction.

## Verification

- Local host stand-in build with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined`: **20 tests, 267 assertions, 0 failures**.
- Leak detection was disabled in this focused run because the existing stop-failure test intentionally retains the event-group callback context until reboot; it asserts that the still-referenced context is not prematurely destroyed.
- SF2 Node suite: **257/257 passed**.
- JavaScript syntax: **25/25 `.mjs` files passed** `node --check`.
- The user's report says the other 28 suites in the v0.2.41 PlatformIO run passed. The corrected AP suite and full v0.2.42 native suite still require PlatformIO rerun.
- No PlatformIO run was available in the packaging environment; no ESP32 target build, entropy measurement, OLED test, AP radio capture, or device test was performed.

## Required next verification

From a clean full v0.2.42 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_wifi_ap -vvv 2>&1 | tee wifi-ap-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
```

Do not treat the local stand-in run as a substitute for PlatformIO/Unity or for the later ESP32 target build.
