# Release v0.2.77 — read-only, fail-closed log startup

Date: 2026-11-10  
Baseline: v0.2.76  
Baseline archive SHA-256: `78a79a84c8ea3156b6c97119a100b37bae1fcfbf1255b8ff504cb4fa3a9a9ff0`  
Status: development snapshot; not production firmware; not hardware-verified.

## Scope

C13.2 safe startup slice. The user delegated the D1–D11 recovery-policy engineering choices to assistant judgment on 2026-11-10. Those policies are recorded as delegated engineering decisions in `docs/SPECS/littlefs_recovery_protocol.md`; they are not signed product rules and do not change `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, O8, the event format, or the delegated status of the MonthSet design.

## Behavior change

`LogStore::open()` is now read-only with respect to storage:

- It never truncates or discards an EOF suffix.
- Partial record bytes, CRC-invalid complete records, sequence errors, interior corruption and unfinished `FL_MORE` groups cause open failure.
- The exact original bytes are preserved; a valid-looking prefix is not replayed to observers.
- The log remains faulted after an anomalous open.
- If a read/decode/sequence failure occurs during the second replay pass after some observers received records, internal replay counters are cleared and all observers receive `onReset()` again before failure returns.
- A clean full scan and successful replay are still required before the log becomes usable.

This deliberately prioritizes attendance-data preservation over automatic availability. A torn append can leave the device unable to resume attendance until a separately designed and validated service-recovery workflow is performed. v0.2.77 does not provide that workflow.

## Recovery-policy boundary

The delegated D1–D11 policies prohibit automatic suffix deletion, incomplete-group discard, automatic evidence expiry, guessed commit points, guessed journal schemas, in-place low-space fallback, remote destructive recovery, unvalidated `EV_REPAIR`, silent expansion of ordinary backup scope, and automatic formatting.

No destructive operation is enabled. `LittleFsFileSet::truncateFile()`, `quarantineCopy()`, and `quarantineFrom()` remain unavailable. `MonthSet::repair()` is still a non-transactional copy/move/truncate sequence and must not be called by production.

## Native verification

Final grouped command:

```sh
pio test -e native -f 'test_monthset' -f 'test_coreb' -f 'test_coreb2' -f 'test_event_group_audit' -f 'test_restore_replay'
```

<text>Host native suite results:</text>

| Suite | Passed |
|---|---:|
| `test_monthset` | 21/21 |
| `test_coreb` | 44/44 |
| `test_coreb2` | 17/17 |
| `test_event_group_audit` | 19/19 |
| `test_restore_replay` | 15/15 |
| **Selected total** | **116/116** |

Exit status: **0**  
Duration: **1:34.985**  
Full log: `/tmp/att_v0277_final_native_selected.log`

The red regressions reproduced the old tail-truncation behavior for partial and complete CRC-invalid EOF records. A separate red regression reproduced an observer retaining one record after a second-pass read failure. The final test fake models the observer reset contract by clearing its accumulated state on `onReset()`.

Current inventory: 635 C++ registrations across 35 suites; 519 registrations were not run. Full native N1 and the 257-test SF2 suite were not run. SF2 source was unchanged.

## ESP32 compile/link and static ELF inspection

Exact build command:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

Result: **SUCCESS**, exit status **0**  
Build duration: **1,203.99 seconds**  
Target: ESP32 Dev Module (`esp32dev`)  
Pinned platform/framework: Espressif32 8.0.0; Arduino-ESP32 / ESP-IDF libraries 3.3.12; Xtensa toolchain 14.2.0+20260121.

- RAM: **67,356 / 327,680 bytes (20.6%)**
- Application flash: **935,792 / 1,310,720 bytes (71.4%)**

Full build log: `/tmp/att_v0277_esp32_build2.log`

Captured final image hashes:

- ELF SHA-256: `e0b0db6ae8b5ca17bf93c64c9511507315a1ca008cda0b570d5581c0e6eb264f`
- BIN SHA-256: `8bebd67e22697627e3946e8a25110f162ac2a4c04cf01014e87534ed9cf0ae9a`

Exact final ELF inspection:

```sh
./tools/verify_v0.2.77.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```

Result: **PASS**, exit status **0**. Evidence:
- 12/12 NVS wrapper-routing checks.
- 7/7 required config/startup/AP/PIN NVS symbols retained.
- 9/9 LittleFsFileSet methods retained.

Full inspection log: `/tmp/att_v0277_elf_verify.log`

This is compile/link and static ELF evidence only. The inert adapter-link image is not production firmware and was not flashed. The target did not mount or operate LittleFS, initialize the production attendance workflow, test sensor/display/RTC/AP behavior, or exercise power loss.

The native test invocation later removed the adapter-link environment directory from `.pio/build`; the exact ELF and BIN hashes and the successful inspection log were captured before that cleanup. The build is reproducible from the command above.

## Explicitly not verified

- Full native N1 or SF2 regression.
- Production firmware composition root, serialized scheduler, clock caller, HTTP/session/auth boundary, admin UI, or enrollment service.
- ConfigBootCoordinator readiness ordering in a real production root; C34 remains blocking.
- Production LittleFS mount owner, mounted-target VFS behavior, directory-entry durability, close/sync durability, recovery journal, crash recovery, or physical power-cut/brownout behavior.
- AS608 physical page/index semantics (E18/U5), real sensor scans, OLED pixels, RTC operation, AP beacon/DHCP, powerbank runtime, soak or physical attendance end-to-end behavior.
- Restore transaction/storage swap and reboot recovery (C2).

No target flash, NVS/partition erase, filesystem format, template deletion, destructive storage recovery, or physical-device operation was performed.

## Installation

Install only by replacing the complete project tree with the v0.2.77 archive. Do not merge individual files into an older checkout. Never flash `esp32-adapter-link`; it is an inert compile/link target, not attendance firmware.
