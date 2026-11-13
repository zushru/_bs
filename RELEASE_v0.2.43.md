# Release v0.2.43 — 2026-11-08

Status: **isolated ESP32 adapter compile/link smoke project**. This is a development snapshot, not production firmware and not hardware-verified. Install only by replacing the complete project tree with this archive; do not merge individual files into an older tree.

## Why this increment

The v0.2.42 native gate is complete: the user reports all 487 C++ registrations across 29 PlatformIO suites passed. The remaining immediate C6 uncertainty is that the ESP32-only adapters have not been compiled and linked against the package set selected by the pinned Espressif32 platform release. The OPEN E29/E30 decisions still block production AP composition, but they do not block a no-side-effect compile/link check.

## Changes

1. Added `platformio.esp32-smoke.ini`, separate from the default native `platformio.ini`. It pins `espressif32@8.0.0`, board `esp32dev`, and framework `arduino`. The PlatformIO 8.0.0 release supports Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5; its manifest requires PlatformIO Core `^6` and constrains the framework package versions. Exact resolved package revisions must be captured from the user's target build output. References: https://github.com/platformio/platform-espressif32/releases/tag/v8.0.0 and https://github.com/platformio/platform-espressif32/blob/v8.0.0/platform.json.
2. Added `target/esp32_adapter_link/main.cpp`. It retains method references for `Esp32WifiAp`, `Esp32SecureRandom`, `Esp32BootstrapRandom`, `ApCredentialBootstrap::prepare()`, `Esp32NvsApCredentialStore`, `Esp32NvsConfigBackend`, `ConfigBootCoordinator::start/provision()`, `Esp32NvsPinAttemptStore` and `Esp32PinAuthRuntime`. The target linker must resolve the implementations and their ESP-IDF dependencies.
3. The smoke program is intentionally inert: global construction only initializes C++ object state; `setup()` reads volatile function references. It does not start Wi-Fi, enable an entropy source, initialize NVS, or write storage. It is not a production image and must not be flashed as the attendance firmware.
4. Added a target smoke README and updated the context, critical issue list, test evidence, unverified register and quick-start instructions. No production module, signed decision, or existing test registration changed.

## Verification

- Host syntax check of the smoke translation unit: **passed** with `g++ -std=gnu++11 -Wall -Wextra -Werror -fsyntax-only` using existing ESP-IDF host stubs. This validates C++ signatures and member-function-reference syntax only.
- SF2 Node suite: **257/257 passed** in this release tree.
- JavaScript syntax: all **25/25** `.mjs` files passed `node --check`.
- User-reported v0.2.42 PlatformIO native suite: **487 registrations across 29 suites passed**. This is recorded as user-reported evidence, not independently rerun in this packaging environment.
- No ESP32 target compile/link, PlatformIO native rerun, firmware flash or device test was available in the packaging environment.

## Required next verification

From a clean v0.2.43 extraction, run:

```sh
sha256sum -c MANIFEST.sha256
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

Capture the complete output including resolved platform/framework/toolchain versions. A successful build confirms compilation/linkage only. It does not prove SAR-ADC entropy behavior, NVS durability, AP first-beacon security, radio event ordering, station limits, OLED disclosure or any physical behavior. Do not flash the smoke image as production firmware.

## Decisions still open

- **E29:** production SoftAP SSID source and naming format.
- **E30:** AP credential rotation, reset and recovery.

These remain user decisions. Do not invent a MAC-derived/hard-coded SSID or automatic credential overwrite/delete path.
