# Audit v0.2.85 — LittleFS mount backend ownership

Date: 2026-11-11  
Scope: `Esp32LittleFsMountBackend` copy/move semantics and C13.3 build evidence.

## Finding addressed

The ESP-IDF VFS backend stores whether it owns a mounted registration (`mounted_`) and the associated partition label (`partitionLabel_`). Before this increment, the class had a user-declared default constructor but no deleted copy/move operations, so its ownership state could be duplicated by compiler-generated special members.

A copied object could therefore carry `mounted_ == true` and the same partition label as the original. Calling `unmountOwned()` on either object could unregister the shared VFS registration while the other object still considered itself mounted. Moving was also prohibited to keep owner identity stable and avoid a future implicit transfer that would need a reviewed transfer contract.

The header now deletes copy construction, copy assignment, move construction and move assignment. `test_littlefs_mount_owner` has compile-time assertions for all four properties. The red run showed all four assertions failing before the patch. The green run passed all 22 runtime cases and compiled all four assertions.

## Adversarial review

- The defect was in ownership identity, not merely accidental state copying. A copied `mounted_` flag is dangerous because unregister acts on a global VFS registration keyed by partition label.
- Deleting only copy construction would leave copy assignment as a bypass; both are deleted.
- Deleting copy alone does not reliably express the intended prohibition on moving. Both move operations are explicitly deleted and compile-time checked.
- This does not prove that only one backend instance can be created in the entire firmware. The composition root must still instantiate one owner and pass references; a global singleton constraint is not added here.
- It does not prevent code outside the owner from calling ESP-IDF VFS registration/unregistration APIs directly.
- The backend remains an explicit owner, not an automatic RAII unmount guard. Destruction/lifetime ordering and failed-unmount recovery remain separate design concerns and are not silently changed.

## Verification boundary

The three storage suites passed 53/53 before the patch; after the test extension and backend patch, the mount-owner suite passed 22/22. This is host test evidence only.

The pinned target compiler compiled `target/esp32_adapter_link/main.cpp` and the complete `hal_esp32` library, including `esp32_littlefs_mount.cpp`; `libhal_esp32.a` was archived. This confirms target translation-unit compatibility for the HAL library. The overall PlatformIO build was stopped while compiling Arduino framework `StreamString.cpp`. No final summary and no `firmware.elf` were produced. Do not treat this as a full target build or link pass.

No mount/unmount operation was executed against a real ESP-IDF VFS. No format, truncate, quarantine, repair, directory-sync, short-write, fsync/close fault, power-cut, brownout or physical-device test was performed.

## Remaining blockers

1. Complete the pinned `esp32-adapter-link` final build and inspect the resulting ELF. The target remains inert and must not be flashed.
2. Validate the actual ESP-IDF LittleFS VFS mount, open/read/append/sync/close and unmount behavior with controlled fault injection.
3. Prove the commit/durability primitive and power-loss outcome before enabling recovery or claiming durable append behavior.
4. Keep D1–D11 and the month-set recovery protocol unchanged until separately reviewed. No implementation should infer a repair transaction from the current host fakes.
5. Wire one storage owner into a production composition root only after the approved roadmap prerequisites, including NVS/config startup ordering and auth/API boundaries, are satisfied.

## Disposition

The backend copy/move ownership defect is closed at source and native-test level. C13.3 is **not complete**. The target HAL archive compiled, but final link, mounted VFS runtime, durability and physical interruption behavior remain unverified.
