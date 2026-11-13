# Release v0.2.48 — shared NVS startup guard

**Date:** 2026-11-09  
**Base:** uploaded `att_v0.2.47_2026-11-08.tar.gz`  
**Status:** source snapshot only; no compile, PlatformIO test, ESP32 target build, ELF inspection or hardware run was performed for this release.

## Changes

- Extracted the NVS init/erase linker wrappers from the diagnostic sketch into `lib/hal_esp32/src/esp32_nvs_startup_guard.*`. This makes the startup guard a shared HAL component rather than probe-only code.
- Added `nvs_startup_guard_policy.h` with host-testable rules: allow application NVS use only when exactly one pre-setup framework NVS init was observed and succeeded and no whole-partition erase was blocked; block only a full DATA_NVS `esp_partition_erase_range` call. Missing or multiple init calls fail closed.
- Linked the shared wrapper into both `esp32-adapter-link` and `esp32-hardware-probe` through their linker flags. Both target mains retain non-invoked references to the standard NVS erase APIs so final-ELF inspection can verify their call routes; the probe queries the shared guard status before I2C/OLED/AP work.
- Added nine policy tests and moved the four erase-predicate registrations out of `test_hardware_probe`; the probe suite now contains four credential-encoding tests.
- Added `tools/verify_v0.2.48.sh` to run the full native suite, both ESP32 target builds, and final-ELF call-routing inspection without uploading or flashing. The inspector checks both target ELFs.

## Test inventory and evidence

- **533 C++ test registrations across 33 suites** (static source inventory).
- The user-reported **520/520 PlatformIO pass applies to v0.2.46 only**. The 13 current probe/policy registrations are not run.
- No test, compile or target-build result is claimed for v0.2.48. Run `./tools/verify_v0.2.48.sh` from the complete extracted tree and preserve its logs.
- SF2 source is unchanged; its latest historical evidence remains 257/257 Node tests and 25 syntax checks.

## Critical limitations

1. **C34 remains blocking.** The wrapper must be proven in both final ELFs by actual call-target inspection, then the diagnostic probe must report `SAFE_NVS_INIT=PASS`. Do not intentionally corrupt NVS to exercise the failure branch.
2. **This is not a general erase firewall.** Pinned ESP-IDF 5.5.5 source routes the standard `nvs_flash_erase()`, named-partition erase and partition-pointer erase APIs through `esp_partition_erase_range`, and the updated ELF inspector checks those call chains. The guard still permits smaller ranges, so chunked partition erasure and alternate/direct flash erase paths are not blocked. The standard NVS erase APIs may deinitialize open handles before their lower-level erase is rejected; they are not a recovery workflow.
3. **Not yet a production boot solution.** There is no production firmware target/entrypoint. Any future production target must explicitly link the wrappers, check `safeForApplication` before any application NVS handle/service starts, and define an explicit reviewed recovery/migration path.
4. **The probe is diagnostic only.** It is not attendance firmware. Never flash `esp32-adapter-link`; do not flash the probe until both target builds and wrapper routing inspection pass and are reviewed.
5. No hardware behavior is verified: NVS retention, I2C protocol operation, OLED pixels, AP first beacon, DHCP event, AS608 behavior, power stability and soak testing remain unverified.

## Install

Install only by replacing the full project tree with the complete v0.2.48 archive. Do not merge this patch into an older tree.
