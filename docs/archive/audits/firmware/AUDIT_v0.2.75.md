# Critical audit — v0.2.75 NVS config boot gate

Date: 2026-11-10  
Baseline: v0.2.74  
Change type: narrow HAL startup-boundary hardening with native regressions and inert target link verification.

## 1. Question and scope

C34 established that Arduino-ESP32 calls nvs_flash_init() in initArduino() before sketch setup(), and may attempt a full default-NVS erase after ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND. The existing shared wrapper blocks only the exact full DATA_NVS partition erase request through esp_partition_erase_range().

The remaining question addressed here is whether the config-storage owner may proceed after Arduino's guarded startup path failed, and whether the config backend creates a second NVS initialization attempt.

## 2. Implementation inspected

Changed:

- Esp32NvsConfigBackend::begin() reads Esp32NvsStartupGuard::status().
- It returns false and keeps ready_ == false unless safeForApplication is true.
- For an unsafe snapshot it preserves firstInitResult as lastError_; when that field is ESP_OK despite an unsafe snapshot, it returns ESP_FAIL.
- For a safe snapshot it sets lastError_ = ESP_OK and ready_ = true.
- It no longer calls nvs_flash_init().

The guard's policy still requires exactly one successful initialization, and no blocked whole-partition erase attempt. The config coordinator latches its own fault before calling the backend. The AP setup coordinator requires config boot to be in NEEDS_SETUP; the PIN runtime requires config boot to be READY or NEEDS_SETUP. These are source-level ownership contracts. There is no production root proving that all application startup follows this sequence.

The AP-credential and PIN-attempt stores themselves remain direct NVS adapters. They do not independently call the startup guard. Their owning production flow must not call them before config boot has succeeded. This is a remaining composition requirement, not proof of a global NVS firewall.

## 3. Test-first evidence

The owning suite was run before implementation. The corrected red run reproduced four intended failures:
- No-free-pages startup was accepted.
- New-version startup was accepted.
- An ambiguous repeated init snapshot was accepted.
- The backend invoked a second init in the successful snapshot case.

The initial red attempt also failed the pre-existing PIN-store “before init” test because its fake driver was unintentionally initialized by the new default fixture. That case was corrected to explicitly set the fake NVS state uninitialized before rerunning the red suite.

The red PlatformIO wrapper emitted SIGILL after the Unity failure summary. The green owning run completed normally:
- pio test -e native -f 'test_nvs_config': 46/46, exit 0.
- pio test -e native -f 'test_nvs_startup_guard_policy': 9/9, exit 0.

The second suite is directly relevant because it owns the pure startup safety predicate consumed by the backend. No other suite was selected.

## 4. Pinned ESP32 compile/link and ELF inspection

Command:

    pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2

PlatformIO reports SUCCESS after 2,086.96 seconds. RAM is 67,356/327,680 bytes (20.6%); application flash is 935,040/1,310,720 bytes (71.3%).

Final ELF:
- SHA-256: 1e959f325be97ee57c13eb852d40bdc40666cbbaceace8933f56a83d9c0ac57e
- Size: 11,192,840 bytes.

Final BIN:
- SHA-256: dfc8c198d2dbd90aa1be6281cf99d6f4179ffbd14fd7072e8232ef95c968420d
- Size: 944,112 bytes.

The ELF inspection reports:
- 12/12 NVS wrapper routing checks passed.
- Esp32NvsConfigBackend::begin() retained.
- Esp32NvsStartupGuard::status() retained.
- 9/9 LittleFsFileSet public methods retained.

This proves the modified code compiles and its relevant symbols are present in the linked inert target. It does not execute the startup path.

## 5. Limits and remaining blockers

- The startup guard remains a narrow whole-partition erase blocker, not a complete flash erase firewall. Chunked erases, DATA_NVS_KEYS, and direct lower-level erase paths remain outside its guarantee.
- A blocked NVS erase API can deinitialize handles before the erase wrapper denies the operation.
- The config backend now fails closed on its startup snapshot, but the full production boot/root is absent. The future root must start ConfigBootCoordinator before any AP/PIN NVS access or other application storage owner.
- The low-level AP credential and PIN attempt stores do not independently enforce startup status.
- No NVS runtime, mount/VFS operation, fault injection against real flash, corruption/migration, erase, format, flash, sensor, physical, brownout or power-cut test was performed.
- The inert adapter-link target was not flashed.
- Full native N1 and SF2 S0/S1 remain deferred as planned.
- The C13.2 storage recovery proposal remains NOT APPROVED FOR IMPLEMENTATION.

No signed or delegated product decision, persisted format, erase policy or recovery policy changed.
