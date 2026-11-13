# Specialized audit — v0.2.43

Date: 2026-11-08  
Baseline: uploaded `att_v0.2.42_2026-11-08.tar.gz`  
Scope: isolated ESP32 adapter compile/link smoke project and status/documentation reconciliation

## Result

**No production source or existing test source was changed.** This increment adds an isolated PlatformIO target project intended to expose ESP32 framework/API compile and link errors before production boot composition is started. It is not production firmware and must not be flashed as the attendance application.

## Exact source comparison

Compared the working tree byte-for-byte against a fresh extraction of the uploaded v0.2.42 archive:

- Added: 5 files.
  - `docs/AUDIT_v0.2.43.md`
  - `platformio.esp32-smoke.ini`
  - `target/esp32_adapter_link/main.cpp`
  - `target/esp32_adapter_link/README.md`
  - `RELEASE_v0.2.43.md`
- Modified: 8 files.
  - `README.md`, `RELEASE.md`
  - `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`, `docs/SPECS/esp32_wifi_ap.md`
- Removed: 0 files.
- Modified files under `lib/`, `test/`, `sketch/` or `web/sf2/`: **0**.
- C++ test registrations added: **0**.

The source inventory remains **487 `RUN_TEST` registrations across 29 native suites**.

## Target smoke design review

`platformio.esp32-smoke.ini` is separate from the default native `platformio.ini`. It selects PlatformIO Espressif32 `8.0.0`, board `esp32dev`, and the Arduino framework. PlatformIO's v8.0.0 release documents Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5. The pinned platform manifest requires PlatformIO Core `^6` and constrains the framework package versions; the exact resolved framework/toolchain package revisions still need to be captured in the user's target build log.

`target/esp32_adapter_link/main.cpp` retains direct member-function references to the following adapter surfaces so their translation units and required symbols must be pulled into the target link:

- `Esp32WifiAp` start/stop
- `Esp32SecureRandom` fill
- `Esp32BootstrapRandom` fill
- `ApCredentialBootstrap::prepare()`
- `Esp32NvsApCredentialStore` load/create-if-absent
- `Esp32NvsConfigBackend` begin/load/slot read/slot write/provision marker read
- `ConfigBootCoordinator::start()` and `provision()`
- `Esp32NvsPinAttemptStore` initialize/load/persist
- `Esp32PinAuthRuntime` start

The smoke program's global constructors initialize ordinary C++ object state only. `setup()` reads volatile function pointers and makes no adapter calls. The program does not start Wi-Fi, start an entropy source, initialize NVS, read or write credentials, accept network requests, or flash itself. It is a build/link probe only.

## Verification evidence

| Check | Result | Evidence boundary |
|---|---:|---|
| v0.2.42 full native PlatformIO run | User reports all 29 suites passed | Not rerun here; no production/test source changed in v0.2.43 |
| SF2 Node suite | 257/257 passed | Executed in this v0.2.43 tree |
| JavaScript syntax | 25/25 passed | `node --check` on every `.mjs` file |
| Smoke translation-unit C++ syntax | Passed | Host `g++` with project test stubs; not target compilation |
| Smoke INI parse / target assertions | Passed | Python `configparser`; verifies config values only |
| ESP32 target compile/link | **Not run** | PlatformIO CLI is not installed in this packaging environment |
| ESP32 device/radio/NVS/entropy tests | **Not run** | No hardware access |

The user's v0.2.42 PlatformIO result is recorded as user-reported evidence, not an independently repeated test in this environment. Registration count is not a pass count; the user did separately report the full 29-suite run passed.

## External toolchain basis

- PlatformIO Espressif32 v8.0.0 release: https://github.com/platformio/platform-espressif32/releases/tag/v8.0.0
- v8.0.0 manifest: https://github.com/platformio/platform-espressif32/blob/v8.0.0/platform.json

The release page states that v8.0.0 supports Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5. The manifest specifies PlatformIO Core `^6`, toolchain package versions and constrained framework package versions. A successful target build is still required to prove this source actually compiles and links against the resolved packages.

## Remaining gates

1. Run the target smoke command and provide the full build output with resolved platform/framework/toolchain versions.
2. E29 remains OPEN: production SoftAP SSID source and naming format.
3. E30 remains OPEN: AP credential rotation, reset and recovery.
4. C6 remains blocking: no production boot composition, OLED credential disclosure, PIN verifier/KDF implementation, HTTP/JSON routes or protected-route enforcement.
5. A successful target smoke build will not establish entropy quality, NVS power-cut durability, first-beacon WPA2/CCMP, AP lifecycle behavior, client cap or physical operation. Those require later target/device checks.

## Archive hygiene

The release archive is generated from the full v0.2.42-derived project tree. It must exclude `.git`, `.pio`, `node_modules`, Python caches and local build artifacts. `MANIFEST.sha256` covers every archived file except itself; the final release procedure verifies the manifest, extracts the archive into a clean directory and verifies the extracted files again.
