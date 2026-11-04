# CONTEXT: start here

Snapshot: branch `main` of this repository (2 commits: handoff baseline, then this snapshot). Written 2026-11-04 for continuing development outside this chat (ChatGPT).

## 1. What the device is
An ESP32 attendance kiosk. An AS608 fingerprint sensor (300 templates), a DS3231 RTC, an OLED, and a buzzer. Powered from a 10,000 mAh bank in always-on mode. The admin uses a phone on the device's Wi-Fi access point. Two sessions per school day. Marks are Present, Late, Excused or Clear. Absent is derived and shown only when the session is closed and coverage is trusted. The product rules are in HANDOFF_FINAL.md and are signed off. Every decision is in docs/DECISIONS.md with its status.

## 2. Where it stands
| Part | Code | Host | Device | Status |
|---|---|---|---|---|
| Handoff core A/B (events, codec, log, index, closure, day view, health, clock) | lib/core/src | 20+36+11 | yes | Done, plus the Core B fixes (docs/SPECS/core_fixes.md) |
| config_store | config_store.* | 12 | yes | Done. Engine link missing (C1) |
| month files | fileset.h, month_set.* | 10 | yes | Interface done. LittleFS driver NOT written |
| roster | roster_store.* | 9 | yes | Done |
| backup container | backup_format.* | 9 | yes before template cut | Container done. Crypto NOT wired. Restore orchestrator NOT written (C2) |
| API logic | api_logic.* | 20 | yes | Pure logic done. No HTTP or JSON adapter |
| enrolled roster | enrolled_roster.* | 7 | NO | Helpers done. enroll_fsm NOT written (C3) |
| Diagnostic sketch | sketch/ | — | yes | Bring-up tool only. Has the duplicate-template hazard (C5) |
| Production firmware (drivers, scan path, NVS, OLED, buzzer patterns) | — | — | — | NOT STARTED |
| Bulk enroll | — | — | — | Method chosen by me, not built |
| 10-hour soak and brownout | — | — | — | NOT DONE |
| Web dashboard | — | — | — | NOT STARTED (last in the plan) |

Test totals: 149 in the sandbox, 142 on the device (before the template cut and test_enroll). See docs/TESTS.md.

## 3. Hardware (bring-up build, 3V3 only)
Buzzer GPIO23. I2C SDA 21, SCL 22. AS608 TX to ESP32 RX2 (GPIO16), AS608 RX from ESP32 TX2 (GPIO17). Touch GPIO4. AS608 V+ and VA on 3V3, GND common. No bulk capacitor. Full detail: docs/SPECS/diag_sketch.md and docs/BRINGUP.md.

## 4. What is verified and how
- Device output is ground truth. Your logs are the only proof of hardware behaviour.
- Host tests use a stand-in Unity and a toy cipher. They prove logic only.
- docs/UNVERIFIED.md lists every claim that still needs proof, and the test that settles each.

## 5. Critical issues (full list: docs/CRITICAL.md)
1. The running ClosureEngine never receives a changed schedule (C1). A config change is accepted but not applied until the engine is rebuilt.
2. Restore is only partly built (C2).
3. Enrollment is not built, so fast enrollment is not possible yet (C3).
4. Duplicate threshold and minScore have no data (C4).
5. The diagnostic sketch leaves a duplicate template on the sensor (C5).
6. Security duties are the adapter's to enforce. Nothing enforces them yet (C6).

## 6. Next steps, in order
1. Fix C1: rebuild the engine after a schedule change, or add a setter. Add a test that a changed lateMin closes a day correctly.
2. Write enroll_fsm: queue (roster minus occupancy), duplicate search against the whole library before storing, retry on recoverable sensor errors, optional verify scan, occupancy refresh before each closure. Host tests first.
3. Write the restore orchestrator (C2) with a failure test for every step.
4. Write an adapter design: NVS keys, LittleFS IFileSet, mbedtls ICipher, HTTP endpoint mapping. Table first, code second.
5. Device work, one sitting: empty the sensor, read the index table (U5), check ID 300 (U7), log scores for duplicate threshold (C4), code 23 (U6), power-cut test (C13), 10-hour soak (C8).
6. Production firmware skeleton on the bench, then the scan path and buzzer and OLED patterns.
7. Dashboard last.

## 7. Working rules for the next assistant
- Do not mark anything verified without the user's device output, a header signature, or pinned library docs.
- Keep each reply short. Numbered decisions, "Decisions needed", "Open uncertainties" at the end.
- Propose 2 to 5 steps before expanding scope. Wait for the user.
- Do not regenerate tested core files. Add a test and a patch instead.
- The user prefers few round trips: give exact, complete commands and batched deliverables.
- Tools: ArduinoDroid builds and flashes the sketch. PlatformIO in Termux with proot Debian runs the core tests. Flashing is by USB (Web Serial) per the handoff.
- Ask which decisions are "your judgement" versus approved before changing a DELEGATED item.
- Firmware is USB only (no OTA). The user chose this.
- Install only from the full r2 archive. Never merge single files into an older tree (C14).

## 8. File map
- README.md: layout and quick start
- HANDOFF_FINAL.md: signed-off product and architecture handoff (source of truth for rules)
- lib/core/src: core modules (headers document each contract)
- test/: 10 native suites
- sketch/: bring-up sketch
- docs/CONTEXT.md (this file), CRITICAL.md, DECISIONS.md, TESTS.md, UNVERIFIED.md, BRINGUP.md
- docs/SPECS/: one spec per module
- docs/patches/: core_fixes.patch, fix_b2_tests.py, apply.sh (already applied, do not run), BASELINE_DIFF.txt
