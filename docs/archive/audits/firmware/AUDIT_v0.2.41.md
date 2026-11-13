# Specialized audit — v0.2.41

Date: 2026-11-08  
Baseline: uploaded `att_v0.2.40_2026-11-08.tar.gz`  
Scope: first-provision WPA2 SoftAP credential bootstrap only.

## Result

**Module-level implementation added; production feature remains blocked.** The increment adds a core bootstrap coordinator, an ESP32 NVS adapter and a pre-radio RNG adapter. It does not add a production firmware entry point, OLED driver, setup orchestrator, or production AP caller. It is not safe to describe the device as having a completed first-provision Wi-Fi flow.

## Implementation

- `ApCredentialBootstrap` generates 16 random bytes and encodes them as a 32-character lower-case hexadecimal WPA2 passphrase while `CFG_BOOT_NEEDS_SETUP` is active.
- The generated passphrase is stored create-only at NVS namespace `attwifi`, key `ap_pass_v1`, and read back before the coordinator returns it. A subsequent setup-required boot reuses and requests disclosure of the same stored value.
- `disclose=true` is a caller request, not evidence that an OLED rendered the secret. Production must display it before committing the transition to `CFG_BOOT_READY`.
- In `CFG_BOOT_READY`, validated `Config::wifiPass` is authoritative. The NVS mirror is backfilled only if absent. A mismatch, malformed value or storage fault fails closed; no automatic overwrite or rotation exists.
- `Esp32NvsApCredentialStore` rejects invalid candidates and does not replace an existing valid or malformed key. Its methods are synchronous and must be called by one serialized boot/application task. `createIfAbsent()` is not a cross-task atomic primitive; callers must not race it.
- `Esp32BootstrapRandom` temporarily enables the ESP-IDF SAR-ADC entropy source, calls `esp_fill_random()`, and disables the source before any Wi-Fi, Bluetooth, ADC or I2S activity. The IDF functions return `void`, so the adapter cannot detect an internal entropy hardware fault.

## Critical review findings

1. **No production wiring.** Nothing in this increment invokes the new coordinator during boot, guarantees OLED disclosure, blocks `READY` until display, or passes the selected secret to the single `Esp32WifiAp` owner. The sequence is specified but not implemented.
2. **Target linkage is unverified.** The pinned ESP-IDF 5.5.5 API documents `bootloader_random_enable()` / `bootloader_random_disable()` and places the header in `bootloader_support`. The project has no ESP32 target build here; Arduino-ESP32/PlatformIO linkage of that component remains unproven.
3. **Entropy call is an early-boot critical section.** It must not overlap RF, ADC or I2S initialization/use. Do not move this call later or call it concurrently. The existing RF-gated `Esp32SecureRandom` remains a separate post-AP token RNG.
4. **NVS power-cut behavior is not device-tested.** The store commits and reads back before the caller may start the AP. ESP-IDF documents that a power loss can lose the new key if it occurs while that key is being written. Because the AP must not start before a successful read-back, an uncommitted first credential should not yet have been disclosed or used. Target power-cut/reboot tests are still required.
5. **Backup interaction is asymmetric by design.** `Config::wifiPass` remains in config/backup; `attwifi/ap_pass_v1` is not included. A target with a missing mirror can migrate from restored config. If a target already has a different mirror, boot fails closed. No restore orchestrator reconciles that conflict yet.
6. **No recovery path.** There is no reset, erase, rotation or forgotten-password recovery method. Adding one requires E30 approval and tests before implementation.
7. **SSID remains undefined.** The AP adapter still requires a caller-supplied SSID. E29 remains OPEN; do not invent a production SSID or expose a device identifier.
8. **NVS confidentiality is not claimed.** No NVS encryption is added. The signed R11 threat model excludes physical flash access; this change does not expand that threat model.
9. **Secret hygiene is caller-dependent.** The core clears its own temporary buffers on the implemented paths. The eventual firmware caller must clear its output, avoid serial/HTTP/status logging, and minimize any longer-lived copies.
10. **Credential copy must stay synchronized.** During initial setup, the exact same passphrase must be committed to `Config::wifiPass` before the boot state becomes READY. A caller that commits READY first violates the coordinator's contract.

## Test evidence

### New suites — local stand-in execution

Compiled with `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer` against a temporary Unity-compatible stand-in, not the PlatformIO Unity framework:

| Suite | Registrations | Assertions | Result |
|---|---:|---:|---|
| `test_ap_credential` | 18 | 83 | 18 passed, 0 failed |
| `test_nvs_ap_credential` | 15 | 111 | 15 passed, 0 failed |
| `test_esp32_bootstrap_random` | 3 | 30 | 3 passed, 0 failed |
| **Total** | **36** | **224** | **36 passed, 0 failed** |

These tests validate host-side control flow, fake NVS behavior and call ordering only. They do not establish actual NVS persistence, target API linkage, entropy quality, OLED behavior or Wi-Fi radio operation.

### Existing project verification

- Source inventory: **487 `RUN_TEST` registrations across 29 native test directories**.
- User-provided v0.2.36 PlatformIO baseline: **413/413** across 23 suites. That evidence applies only to v0.2.36.
- v0.2.38/v0.2.39/v0.2.41 added 74 registrations after that baseline. 61 have supplemental stand-in execution (25 AP/RF-RNG from v0.2.40 and 36 new bootstrap registrations); 13 token-session/API registrations remain unexecuted in this environment. None of the 74 has a confirmed PlatformIO run.
- SF2 Node test suite rerun: **257/257 passed**.
- JavaScript syntax: **25/25 `.mjs` files passed `node --check`**.
- `pio` and `platformio` are not installed in this environment. No PlatformIO native test run, ESP32 target build, or hardware test was performed for v0.2.41.

## Source delta against uploaded v0.2.40

Before adding this audit and regenerating the manifest, the comparison showed 11 modified existing files and 12 new files, no deletions. The packaged v0.2.41 tree additionally contains this audit and a regenerated `MANIFEST.sha256`.

### New source and tests

- `lib/core/src/ap_credential_bootstrap.h/.cpp`
- `lib/hal_esp32/src/nvs_ap_credential_store.h/.cpp`
- `lib/hal_esp32/src/esp32_bootstrap_random.h/.cpp`
- `test/test_ap_credential/test_main.cpp`
- `test/test_nvs_ap_credential/test_main.cpp`
- `test/test_esp32_bootstrap_random/test_main.cpp`
- `test/test_esp32_secure_random/stubs/bootloader_random.h`
- `docs/SPECS/ap_credential_bootstrap.md`
- `RELEASE_v0.2.41.md`
- `docs/AUDIT_v0.2.41.md`

### Modified existing files

- `README.md`, `RELEASE.md`
- `docs/ADAPTER_DESIGN.md`, `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/DECISIONS.md`, `docs/SPECS/esp32_wifi_ap.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`
- `test/test_nvs_config/stubs/nvs.h`
- `MANIFEST.sha256` is regenerated for the release tree.

No pre-existing tested core implementation was changed.

## Decisions and uncertainties

- E28: **SIGNED** — random bootstrap source, 128-bit generated passphrase, NVS path and repeated setup-mode disclosure request.
- E29: **OPEN** — production SSID source and format.
- E30: **OPEN** — reset, rotation and recovery behavior.
- U94: target linkage and entropy source behavior.
- U95: NVS power interruption and restored-config/mirror conflict behavior.
- U96: production OLED disclosure sequencing.
- U97: E30 reset/recovery semantics.

## Required next verification

From the clean archive, verify the manifest and run the three focused suites, then the existing AP/RNG/session/API suites and full native suite. Build the exact ESP32 target before wiring it into the production boot path. Do not enable first-boot AP startup until E29 is resolved and the OLED disclosure ordering is enforced.
