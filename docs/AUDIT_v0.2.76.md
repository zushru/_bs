# v0.2.76 audit — AP/PIN NVS adapter startup boundary

Date: 2026-11-10  
Baseline: v0.2.75  
Scope: C1/C34 boundary hardening only.

## 1. Source change

The following public NVS operations now check Esp32NvsStartupGuard::status() after validating their non-storage arguments and before calling nvs_open():

- Esp32NvsApCredentialStore::load()
- Esp32NvsApCredentialStore::createIfAbsent()
- Esp32NvsPinAttemptStore::loadFailureCount()
- Esp32NvsPinAttemptStore::initializeFailureCount()
- Esp32NvsPinAttemptStore::persistFailureCount()

When safeForApplication is false, each operation returns its existing failure result and sets lastError. If firstInitResult is a non-ESP_OK error, that code is preserved; otherwise ESP_FAIL is used. AP load clears the output buffer before returning. PIN load does not mutate its output on failure.

The startup-snapshot helper is translation-unit-local in each adapter implementation. It does not create shared mutable state, issue NVS calls, retry initialization, deinitialize NVS, erase flash, change namespaces/keys or alter persisted formats.

## 2. Test-first evidence

Regressions were added before implementation.

Red:
- AP suite: /tmp/att_v0276_red_test_nvs_ap_credential.log. The new unsafe-startup test failed because the old adapter reached NVS and reported NOT_FOUND instead of failing closed. Existing 15 cases passed. PlatformIO emitted SIGHUP after Unity reported the failure.
- PIN suite: /tmp/att_v0276_red_test_nvs_config.log. The new unsafe-startup test failed because the old adapter reached NVS. Existing 46 cases passed. PlatformIO emitted SIGHUP after Unity reported the failure.

The red invocations are failure-reproduction evidence only. Their wrapper exits were abnormal after the assertion summary; neither is described as a normal red process completion.

Green:
- Command: pio test -e native -f 'test_nvs_ap_credential'
- Result: 16/16 passed. Log: /tmp/att_v0276_green_test_nvs_ap_credential.log.
- Command: pio test -e native -f 'test_nvs_config'
- Result: 47/47 passed. Log: /tmp/att_v0276_green_test_nvs_config.log.
- Selected total: 63/63 across two distinct suites.
- Live source inventory: 632 registrations / 35 suites. The other 569 registrations were not run.
- SF2 source unchanged: 257 registrations / 12 test files. No SF2 suite run.

## 3. Target build and ELF inspection

Target: `esp32-adapter-link` using `platformio.esp32-smoke.ini` (inert compile/link smoke application).

Pinned toolchain: Espressif32 8.0.0; ESP32 Dev Module; Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5; esptool 5.4.0; Xtensa toolchain 14.2.0+20260121.

Build command:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

First build: `/tmp/att_v0276_esp32_build.log` completed successfully in **1,076.82 s**, RAM **67,356/327,680 B (20.6%)**, application flash **935,336/1,310,720 B (71.4%)**. The first ELF verification passed 12/12 NVS wrapper routes and the first five symbol checks, but found `Esp32NvsPinAttemptStore::initializeFailureCount()` had been garbage-collected. The verifier exited at that failure, so its later `persistFailureCount()` check was not reached. The function existed in the compiled HAL object, while its link-probe pointer's discarded-value expression did not keep the data section live. This was a real verifier failure, not a spelling-only mismatch.

Corrective probe change: `target/esp32_adapter_link/main.cpp` now copies the volatile member-function pointer into a local and compares it with `nullptr`, forcing a live relocation without calling the method or touching NVS. Second incremental build: `/tmp/att_v0276_esp32_build2.log` completed successfully in **100.83 s**, RAM **67,356/327,680 B (20.6%)**, application flash **935,792/1,310,720 B (71.4%)**.

Final ELF: `.pio/build/esp32-adapter-link/firmware.elf`, SHA-256 `3e5989be6ef00ed708159e7a17bd70618f7c8b2955a6e597bc5465301bf359ec`. Final BIN: `.pio/build/esp32-adapter-link/firmware.bin`, SHA-256 `20138030906d1f619be41736c5303b091c0ab8462906d56be52ecd17a6f252c7`.

Exact final inspection command:

```sh
./tools/verify_v0.2.76.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```

Result: **12/12 NVS wrapper-routing checks passed**, all seven checked NVS symbols retained (`Esp32NvsConfigBackend::begin`, `Esp32NvsStartupGuard::status`, AP `load`/`createIfAbsent`, PIN `loadFailureCount`/`initializeFailureCount`/`persistFailureCount`), and **9/9 LittleFsFileSet methods** retained; exit 0. Log: `/tmp/att_v0276_elf_verify2.log`. The earlier failed first inspection remains in `/tmp/att_v0276_elf_verify.log` and is documented above.

This is compile/link and static ELF evidence only. The target is not production firmware and must never be flashed. No runtime NVS, LittleFS, hardware, startup, or physical behavior is inferred.

## 4. Critical limitations

The new adapters reject a startup snapshot that is unsafe. They do not require or verify that ConfigBootCoordinator::start() has been called, nor that it returned READY or NEEDS_SETUP. A caller can still invoke the adapters when Arduino startup is safe but before validated config boot. This boundary must be enforced by the future production root, including ordering of all application NVS and storage owners.

C34 remains blocking:
- The wrapper blocks only one exact full DATA_NVS partition erase request through esp_partition_erase_range().
- Chunked ranges, DATA_NVS_KEYS, and direct lower-level flash erase APIs are not comprehensively blocked.
- NVS erase APIs can deinitialize handles before the wrapped erase is rejected.
- No production startup owner or approved recovery/migration workflow exists.

C13.2 remains NOT APPROVED FOR IMPLEMENTATION. No NVS runtime, flash erase, format, storage mount, physical hardware or power-cut operation was attempted. HANDOFF_FINAL.md and docs/DECISIONS.md were not changed.
