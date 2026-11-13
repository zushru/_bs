# Audit v0.2.77 — log startup preservation

Date: 2026-11-10  
Root: `/root/att`  
Baseline: v0.2.76 archive  
Project type: development snapshot; not production firmware.

## 1. Baseline and identity

- Verified root: `/root/att`.
- Version marker before edits: v0.2.76.
- Source tree initially matched the extracted v0.2.76 archive after excluding generated `.pio`.
- v0.2.76 archive SHA-256: `78a79a84c8ea3156b6c97119a100b37bae1fcfbf1255b8ff504cb4fa3a9a9ff0`.
- This checkout has no Git metadata; no clean Git status is claimed.
- Protected files remained unchanged:
  - `HANDOFF_FINAL.md`: `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`
  - `docs/DECISIONS.md`: `d3a543db4cb60e99a22572d499a2b5d91b6bda934a91cbd57cf30975e62e0a44`

## 2. Source change review

### `lib/core/src/log_store.cpp`

- Removed startup truncation of invalid suffixes.
- Requires a complete structurally valid sequence before observer replay is published.
- Rejects partial record bytes, invalid CRC/record decode, sequence errors, interior corruption and unfinished trailing `FL_MORE` groups.
- Does not replay a valid-looking prefix when any anomaly is detected.
- A second-pass read/decode/sequence failure after partial observer delivery clears internal replay counters and resets observers before returning failure.
- No event record, sequence, CRC, append, or on-disk format changed.

### Tests

- `test/test_monthset/test_main.cpp`: partial tail preservation; complete CRC-invalid EOF preservation; second-pass replay read failure/observer rollback; truncation-call expectations updated; test observer reset semantics corrected.
- `test/test_coreb/test_main.cpp`: torn partial record, torn group, invalid EOF record and clock group expectations changed to preservation/fail-closed.
- `test/test_coreb2/test_main.cpp`: tail-loss replay fixture now cuts at a complete commit-group boundary instead of manufacturing an unfinished group.
- `test/test_event_group_audit/test_main.cpp`: faulted logs are rejected before audit observer attachment; stale-open behavior expects preserved tails.
- `test/test_restore_replay/test_main.cpp`: unchanged; directly reverified as dependent consumer.

## 3. Delegated C13.2 policy review

On 2026-11-10 the user delegated D1–D11 engineering choices to assistant judgment. The chosen policies are documented in `docs/SPECS/littlefs_recovery_protocol.md` as **DELEGATED engineering decisions**, not user-signed product rules.

Core constraints:
- No automatic suffix deletion, incomplete commit-group discard, or formatting.
- Preserve original evidence; no automatic expiry or capacity-reclaim deletion.
- Do not select a LittleFS commit primitive or persistent journal schema until the pinned target’s durability semantics are experimentally demonstrated.
- Unknown/insufficient free space aborts before mutation.
- Destructive recovery requires a future authenticated administrator and physical service workflow.
- `EV_REPAIR` remains unused.
- Signed O8 ordinary backup scope remains config + roster + attendance log.
- `MonthSet::repair()` remains non-transactional and disabled for production use.

No signed decision, delegated MonthSet decision, or protected handoff file was modified.

## 4. Native evidence

Final command:

```sh
pio test -e native -f 'test_monthset' -f 'test_coreb' -f 'test_coreb2' -f 'test_event_group_audit' -f 'test_restore_replay'
```

Result: **116/116 passed**, exit 0, 1:34.985.  
Log: `/tmp/att_v0277_final_native_selected.log`.

Suite counts: monthset 21, coreb 44, coreb2 17, event_group_audit 19, restore_replay 15.

Red regressions:
- `/tmp/att_v0277_red_test_monthset.log`: old open accepted/repaired partial and CRC-invalid tails.
- `/tmp/att_v0277_red2_test_monthset.log`: second-pass read failure left one observer record visible.
- Earlier owner/dependent corrections and runs are preserved in `/tmp/att_v0277_green3_test_monthset.log`, `/tmp/att_v0277_dependent_logs.log`, `/tmp/att_v0277_green_test_event_group_audit.log`, and `/tmp/att_v0277_green_test_restore_replay.log`.

Inventory: 635 C++ registrations across 35 suites, 257 SF2 Node tests across 12 files. 519 C++ registrations not selected; SF2 not run; N1 full native deferred. This is not a full regression pass.

## 5. ESP32 target evidence

Command:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

Result: **SUCCESS**, exit 0, 1,203.99 seconds.  
Log: `/tmp/att_v0277_esp32_build2.log`.

Target: ESP32 Dev Module, Espressif32 8.0.0, Arduino-ESP32/ESP-IDF libraries 3.3.12, Xtensa 14.2.0+20260121.

RAM 67,356/327,680 bytes (20.6%); app flash 935,792/1,310,720 bytes (71.4%).

Captured final hashes:
- ELF: `e0b0db6ae8b5ca17bf93c64c9511507315a1ca008cda0b570d5581c0e6eb264f`
- BIN: `8bebd67e22697627e3946e8a25110f162ac2a4c04cf01014e87534ed9cf0ae9a`

Exact ELF verifier:

```sh
./tools/verify_v0.2.77.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"
```

Result: PASS, exit 0. Log: `/tmp/att_v0277_elf_verify.log`.
- 12/12 wrapper-routing checks.
- Seven config/startup/AP/PIN NVS symbols.
- Nine LittleFsFileSet methods.

This is compile/link and static ELF evidence only. The target was not flashed. It did not mount LittleFS, operate VFS, initialize the production attendance workflow, or prove physical behavior.

The native test invocation subsequently removed the adapter-link build directory from `.pio/build`. The final image hashes and successful ELF verifier output were captured before cleanup. The build log and inspection log are retained.

## 6. Remaining blockers

- C3: no production firmware root/scheduler, production clock caller, HTTP routes, admin UI, or enrollment service.
- C6: no integrated production HTTP/session/auth boundary or selected PIN verifier/KDF writer.
- C13: no production LittleFS mount owner, mounted VFS fault injection, proven durable commit point, recovery journal, restart-safe repair, or physical power-cut evidence. An anomalous log remains read-only and may require service.
- C2: no transactional restore swap/journal/reboot recovery.
- C34: the NVS erase wrapper remains narrow; no production composition root enforces config boot ordering.
- E18/U5: physical sensor page/index semantics remain unresolved.
- Hardware behavior: sensor, OLED, RTC, AP/DHCP, powerbank, brownout, soak and end-to-end attendance remain unverified.

No target flash, NVS/partition erase, filesystem format, template deletion, destructive recovery, or physical-device test was performed.
