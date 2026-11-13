# Specialized audit — v0.2.48

Date: 2026-11-09  
Base: uploaded `att_v0.2.47_2026-11-08.tar.gz`.

## Scope

- Added `lib/hal_esp32/src/esp32_nvs_startup_guard.h/.cpp` and the pure `nvs_startup_guard_policy.h`.
- Added nine host-testable policy registrations in `test/test_nvs_startup_guard_policy`.
- Removed four NVS-policy registrations from `test_hardware_probe`; that suite retains the four probe-credential tests.
- Moved wrapper definitions out of `target/esp32_hardware_probe/main.cpp`; the probe now reads the shared guard status.
- Added the shared wrapper linker flags to both `platformio.esp32-smoke.ini` and `platformio.esp32-probe.ini`; the smoke target retains a direct reference to the shared status API.
- Added non-invoked function-address anchors for `nvs_flash_erase()`, `nvs_flash_erase_partition()` and `nvs_flash_erase_partition_ptr()` in both target mains so linker garbage collection cannot remove the APIs before route inspection. Neither target calls these functions.
- Updated the final-ELF inspector to check both target images, Arduino startup routing, wrapper delegation and all three standard NVS erase call chains. Added `tools/verify_v0.2.48.sh` to batch native tests, both target builds and inspection.
- Updated current release, critical, test and unverified documentation. No signed product rule, DELEGATED decision, core business logic, AS608 behavior, SF2 source or existing tested core file was changed.

## Why the change is needed

Arduino-ESP32 3.3.12 calls `initArduino()` from `app_main()` before creating the Arduino loop task; the latter invokes the sketch's `setup()` only after `initArduino()` returns. In `initArduino()`, `nvs_flash_init()` is followed by a whole-default-DATA_NVS erase attempt when it returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`. A guard placed only in `setup()` is too late. Pinned sources: `cores/esp32/esp32-hal-misc.c` and `cores/esp32/main.cpp` at tag 3.3.12.

The shared implementation captures the first `nvs_flash_init()` result, counts all pre-setup calls, and fails closed unless exactly one call occurred and succeeded. This also detects a future global constructor that initializes NVS before `initArduino()`. It denies only `esp_partition_erase_range(partition, 0, partition->size)` when the partition metadata identifies DATA_NVS. It returns `ESP_ERR_NOT_ALLOWED` for that precise request. Normal smaller NVS erases are delegated to the real IDF function. The diagnostic probe checks `safeForApplication` before touching I2C, OLED or AP.

## Pinned API source references

- Arduino-ESP32 3.3.12 startup ordering: <https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.12/cores/esp32/main.cpp>
- Arduino-ESP32 3.3.12 NVS startup fallback: <https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.12/cores/esp32/esp32-hal-misc.c>
- ESP-IDF 5.5.5 NVS erase API implementation: <https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/nvs_flash/src/nvs_api.cpp>
- ESP-IDF 5.5.5 NVS erase API declarations: <https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/nvs_flash/include/nvs_flash.h>
- ESP-IDF 5.5.5 `ESP_ERR_NOT_ALLOWED`: <https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_common/include/esp_err.h>
- ESP-IDF 5.5.5 DATA_NVS partition type/subtype: <https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_partition/include/esp_partition.h>

## Findings and residual risks

1. **The guard is intentionally not a complete erase firewall.** Pinned ESP-IDF 5.5.5 source shows `nvs_flash_erase()`, `nvs_flash_erase_partition()` and `nvs_flash_erase_partition_ptr()` all delegate the full erase to `esp_partition_erase_range`; the final-ELF inspector now checks those call chains. However, smaller range calls are allowed, so application code could erase the partition sector-by-sector, and alternate/direct flash erase APIs may bypass the wrapper. The standard NVS erase functions deinitialize existing handles before calling the wrapped erase API; a rejected erase may therefore leave NVS handles closed. Do not represent this as universal NVS protection or use these APIs as a recovery workflow.
2. **Link-time routing is the critical unverified boundary.** Adding `--wrap` flags and compiling a wrapper source file do not prove that the framework's actual `initArduino()` call sites were redirected. `tools/inspect_probe_wrappers.sh` checks both wrappers, both call sites and real-function delegation in both final ELFs. Until it passes, the NVS guard is only source-reviewed.
3. **The shared HAL does not create a production boot owner.** No production `src/` entrypoint exists. A future production target must link the wrappers, call/check the guard before creating any application NVS handle or storage service, and fail closed on a bad startup result. The current diagnostic sketch and inert smoke image are not production firmware.
4. **The first init result is retained and the call count is checked.** `safeForApplication` requires exactly one pre-setup call and that call must have succeeded; zero or multiple calls fail closed. The wrapper status is startup evidence; it does not continuously audit later NVS operations.
5. **Plain global state assumes serialized startup.** The framework calls these wrappers before the Arduino loop task is created. No thread-safety contract is claimed for arbitrary later concurrent calls.
6. **No destructive fault injection.** Do not deliberately corrupt or downgrade the only device's NVS to exercise the blocked branch. The policy is covered by pure host tests; the wrapper path requires final-ELF review, and the clean device boot should report `SAFE_NVS_INIT=PASS`.
7. **No compile/test/link/device execution is claimed for v0.2.48.** The archive was source-reviewed only. The 13 current probe/policy tests are unrun.

## Test inventory

- Static source inventory: **533 C++ `RUN_TEST` registrations across 33 suites**.
- Prior evidence: user-reported v0.2.46 **520/520** PlatformIO native pass and expanded inert smoke target link. That result predates this change.
- Current unverified registrations: four probe credential tests plus nine NVS policy tests.
- SF2 source is unchanged; latest historical evidence remains 257/257 Node tests and 25 syntax checks.

## Required verification batch

From the full extracted v0.2.48 project root:

```sh
sha256sum -c MANIFEST.sha256
./tools/verify_v0.2.48.sh
```

The script runs only host tests and target builds; it does not upload or flash. Preserve `/tmp/att-v0.2.48-verification/` output. Inspect both final ELF routes before any probe flash. The inert `esp32-adapter-link` image must never be flashed.

## Decision impact

No decisions changed. E29 remains SIGNED (`att`). E30 and E31 remain DELEGATED (no remote credential reset/rotation; channel 1). E12 remains DELEGATED. O8 remains SIGNED and unchanged. C34 remains an open production blocker.
