# Release v0.2.74 — C34 NVS erase surface audit

Date: 2026-11-10  
Baseline: v0.2.73  
Status: documentation-only development snapshot; no implementation or runtime behavior changed.

## Scope

Audited the actual pinned ESP32 NVS/flash erase surface and corrected one C13 LittleFS package-configuration statement.

New report: `docs/AUDIT_v0.2.74.md`.

## Main findings

1. The shared NVS startup guard blocks only an exact full-partition `esp_partition_erase_range()` request for a DATA_NVS partition. Smaller ranges are deliberately allowed.
2. The pinned `esp_flash.h` also declares raw `esp_flash_erase_region()` and `esp_flash_erase_chip()`. A direct raw-region call would bypass the partition wrapper. The current inert smoke ELF contains `esp_flash_erase_region()`, but its only observed call site is the partition-range implementation; no direct application caller was found in that image. `esp_flash_erase_chip()` was not retained in this ELF.
3. The NVS keys partition uses a distinct DATA_NVS_KEYS subtype and is not covered by the guard.
4. Pinned NVS API documentation and exact ELF disassembly show that NVS erase APIs can deinitialize handles before the wrapped erase attempt. A denied runtime erase can therefore still disrupt the in-memory NVS service.
5. The existing 12-call-route ELF verifier remains valid for its narrow scope; it does not establish a complete flash erase firewall.
6. The selected ESP32 libs package sdkconfig sets `CONFIG_VFS_SUPPORT_DIR=y`; the pinned LittleFS header consequently defines `ESP_LITTLEFS_ENABLE_FTRUNCATE`. This is configuration/interface evidence only. The smoke image did not link/mount LittleFS or test ftruncate runtime behavior.

## Verification

Exact existing ELF:
- Path: `/root/att/.pio/build/esp32-adapter-link/firmware.elf`
- SHA-256: `bfbb50a538582d53d101a80ad0e01b2dcefc7e7d4f1e84f61bfe012af0b8154b`
- Size: 11,193,008 bytes; v0.2.72 inert adapter-link image, not flashed.

Command:
```sh
./tools/verify_v0.2.73.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```
Result: 12/12 NVS wrapper route checks and 9/9 LittleFsFileSet symbol-retention checks passed, exit 0.

Additional source/ELF inspection:
- pinned ESP32 libs package metadata and sdkconfig;
- esp_partition.h, esp_flash.h, nvs_flash.h and esp_littlefs.h;
- Arduino LittleFS.cpp/LittleFS.h;
- nm symbol inventory and Xtensa objdump of NVS erase, partition erase and raw flash erase functions.

No native suite, SF2 test, target rebuild, hardware probe, flash, runtime NVS, LittleFS mount/VFS operation, or physical-device test was performed. This increment changes documentation only; the existing 628/35 native and 257/12 SF2 inventories are unchanged.

## Disposition

C34 remains blocking. Do not describe the guard as a complete firewall and do not broaden it without a design that preserves legitimate NVS page reclamation and filesystem erases.

C13 remains blocking. The C13.2 recovery proposal is still NOT APPROVED FOR IMPLEMENTATION. ftruncate configuration support does not establish runtime or power-loss safety.
