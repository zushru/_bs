# Critical audit — v0.2.74 NVS erase surface and pinned LittleFS configuration

Date: 2026-11-10  
Baseline: v0.2.73  
Change type: documentation-only static audit  
No production rule, erase policy, recovery policy or persisted format changed.

## 1. Scope and baseline

This audit examines C34 (NVS startup/erase guard) and corrects one C13.2 package-configuration uncertainty.

Baseline archive:
- `att_v0.2.73_2026-11-10.tar.gz`
- SHA-256: `2453b1541484e2412b003ef30058184fc83070d47072caa0f73427a546ed8673`

Protected files remain unchanged:
- `HANDOFF_FINAL.md`: `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`
- `docs/DECISIONS.md`: `d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44`

## 2. Pinned environment inspected

Installed package metadata:
- PlatformIO platform: Espressif32 8.0.0.
- Arduino-ESP32 / ESP32 libs package: 3.3.12.
- ESP-IDF: v5.5.5, commit `b774170ff46`.
- Arduino metadata: `master e7c1e8cd4`.
- `joltwallet__littlefs`: 1.22.3.
- Xtensa toolchain: 14.2.0+20260121 (recorded by the v0.2.72 build).

The pinned target was not rebuilt in this documentation-only increment. The exact prior inert adapter-link ELF was re-inspected:
- Path: `/root/att/.pio/build/esp32-adapter-link/firmware.elf`
- SHA-256: `bfbb50a538582d53d101a80ad0e01b2dcefc7e7d4f1e84f61bfe012af0b8154b`
- Size: 11,193,008 bytes.
- It is the v0.2.72 final smoke ELF; it is not a production firmware image and was not flashed.

## 3. What the NVS guard actually protects

The policy in `nvs_startup_guard_policy.h` blocks exactly this condition:

- partition type = `ESP_PARTITION_TYPE_DATA`;
- subtype = `ESP_PARTITION_SUBTYPE_DATA_NVS`;
- offset = zero;
- erase size = the complete nonzero partition size.

The test suite deliberately confirms that sector-sized and partial ranges are allowed. This is not an accidental untested edge; it is the policy implemented.

The final ELF contains the wrapper and its route checks pass:
- `initArduino()` calls `__wrap_nvs_flash_init()`.
- `initArduino()` calls `__wrap_esp_partition_erase_range()`.
- The wrappers delegate to the real IDF functions when the policy permits.
- `nvs_flash_erase()` delegates to named-partition erase.
- `nvs_flash_erase_partition()` and `nvs_flash_erase_partition_ptr()` reach the wrapped partition-erase boundary.

Exact command:
```sh
./tools/verify_v0.2.73.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```

Result: 12/12 NVS wrapper-routing checks and 9/9 LittleFsFileSet method-retention checks passed, exit 0. This is static ELF evidence only.

## 4. Additional erase paths and important limits

### 4.1 Chunked erase remains possible

Because the wrapper checks each call independently for the exact whole-partition range, an aligned sequence of smaller `esp_partition_erase_range()` requests can cover the same partition without satisfying the block predicate on any one call.

This audit does not propose blocking every DATA_NVS subrange. NVS itself needs page-level erase operations as part of normal flash storage maintenance. A broad block at the partition API could therefore break ordinary NVS writes/compaction. The allowed partial-range policy must not be described as protecting against an intentionally chunked full erase.

### 4.2 Public raw flash erase API

The pinned `esp_flash.h` declares:
- `esp_flash_erase_region(esp_flash_t *chip, uint32_t start, uint32_t len)`;
- `esp_flash_erase_chip(esp_flash_t *chip)`.

`esp_flash_erase_region()` operates on an absolute flash address/length, not an `esp_partition_t`. A direct future call to it would not pass through the `--wrap=esp_partition_erase_range` hook. The current smoke ELF retains `esp_flash_erase_region`; the only observed call instruction to it in that ELF is inside `esp_partition_erase_range()`. No direct application call site was observed in this specific inert image.

`esp_flash_erase_chip()` is declared by the pinned header but is not retained in this specific ELF. Its availability in the SDK is another reason this narrow linker wrapper is not a general flash firewall.

The exact `esp_partition_erase_range()` disassembly delegates to `esp_flash_erase_region()`. The wrapper is above that layer; it cannot intercept code that chooses the lower-level API directly.

### 4.3 NVS keys partition is a separate subtype

The pinned partition header defines separate subtypes:
- `ESP_PARTITION_SUBTYPE_DATA_NVS` = 0x02;
- `ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS` = 0x04.

The current guard matches only `DATA_NVS`. It does not protect a full `DATA_NVS_KEYS` erase. NVS encryption/key provisioning is not part of the current signed product scope; if that scope changes, the key partition needs an explicit threat/recovery design. Do not silently extend the current guard claim to it.

### 4.4 A blocked NVS bulk erase may still deinitialize handles

The pinned NVS API header says that `nvs_flash_erase()`, `nvs_flash_erase_partition()`, and `nvs_flash_erase_partition_ptr()` deinitialize an initialized NVS partition before attempting the erase.

The exact final ELF disassembly of `nvs_flash_erase_partition_ptr()` shows a `close_handles_and_deinit()` path before it calls `__wrap_esp_partition_erase_range()`. Thus, if this API is invoked while NVS is active and the whole-partition erase is blocked, the flash erase is denied but the in-memory NVS service may already be deinitialized. The guard does not make that runtime operation harmless.

The Arduino `initArduino()` fallback path occurs before sketch `setup()`; it is a separate startup case. The existing guard's status snapshot is intended to make the application fail closed after a blocked startup erase attempt. There is no production application root yet to enforce a runtime response to later calls.

## 5. Disposition for C34

**Keep the current claim narrow. Do not call this a complete erase firewall.**

The current code is useful for one known dangerous Arduino startup fallback: an exact whole-default-`DATA_NVS` erase request made through the partition API is blocked before `setup()`. The final ELF proves that the current framework and standard NVS bulk-erase APIs use the wrappers in the inspected smoke image.

It does not prove:
- protection against chunked erase requests;
- protection against direct `esp_flash_erase_region()` or chip-level erase calls;
- protection of `DATA_NVS_KEYS`;
- protection from arbitrary direct flash-driver/ROM routes;
- safe runtime behavior after a blocked NVS API deinitializes handles;
- actual NVS behavior, power-loss behavior or physical-device behavior;
- production adoption, because there is no production firmware root.

A complete firewall would need to distinguish legitimate NVS page reclamation and LittleFS/other storage writes from destructive bulk erase. Simply blocking all NVS partition ranges at the lower-level API would likely break ordinary storage operation. Do not implement a broader hook without a reviewed call-path and ownership model.

## 6. C13 configuration correction

The installed `framework-arduinoespressif32-esp32-libs` 3.3.12 package's `sdkconfig` records:
- `CONFIG_VFS_SUPPORT_DIR=y`;
- `CONFIG_LITTLEFS_PAGE_SIZE=256`;
- `CONFIG_LITTLEFS_MAX_PARTITIONS=3`;
- `CONFIG_LITTLEFS_READ_SIZE=128`;
- `CONFIG_LITTLEFS_WRITE_SIZE=128`;
- `CONFIG_LITTLEFS_BLOCK_CYCLES=512`;
- `CONFIG_LITTLEFS_FLUSH_FILE_EVERY_WRITE` is not set.

The pinned `esp_littlefs.h` defines `ESP_LITTLEFS_ENABLE_FTRUNCATE` when `CONFIG_VFS_SUPPORT_DIR` is enabled. Therefore the selected package configuration advertises the LittleFS ftruncate VFS capability.

This does **not** mean the v0.2.72 smoke ELF verified the LittleFS component: the inert target did not mount LittleFS, perform VFS operations, or link a production storage owner. The current `LittleFsFileSet::truncateFile()` still returns false. The actual `ftruncate()` implementation, error propagation, crash consistency and power-loss behavior remain unverified.

The pinned Arduino `LittleFS.begin()` wrapper sets `conf.format_if_mount_failed=false`, but if its caller passes `formatOnFail=true`, it explicitly calls `format()` after `ESP_FAIL` and retries registration. The production mount owner must use a non-formatting policy; mount failure is never permission to erase attendance data.

## 7. Verification and omissions

Commands/evidence:
- `./tools/verify_v0.2.73.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` — 12/12 NVS wrapper checks and 9/9 LittleFsFileSet symbol checks; exit 0.
- `nm -C --defined-only firmware.elf` — confirmed wrapper, partition-range, raw erase-region and NVS erase symbols; raw chip erase was not retained.
- Xtensa `objdump -dC` — confirmed NVS pointer erase calls deinitialize before the wrapped erase boundary; partition-range erase calls raw region erase; the only observed raw-region call site in this ELF is the partition-range implementation.
- Pinned package headers and sdkconfig read directly; no upstream-version inference used.

No source implementation changed. No native suite, SF2 test, target rebuild, hardware probe, flash, runtime NVS, LittleFS mount, filesystem operation, brownout or physical-device test was run in v0.2.74.

## 8. Next gate

C34 remains blocking. The production root must link the guard before any NVS/flash-using component runs and must fail closed on startup status. Before widening protection, audit all application call sites to low-level flash APIs and determine how to preserve legitimate NVS page erase and LittleFS writes. Do not deliberately corrupt/erase NVS on the only device.

C13 remains independently blocking. The C13.2 recovery proposal remains **NOT APPROVED FOR IMPLEMENTATION**; the package ftruncate setting does not authorize destructive recovery.
