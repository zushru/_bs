# ESP32 Fingerprint Attendance: FINAL HANDOFF
Snapshot 2026-10-31 (end of chat 1). This file supersedes every earlier handoff note. If anything conflicts, this file wins, then the code.

## 0. Resume protocol (new chat)
1. User uploads `att-handoff.tar.gz`. Extract: `tar xzf att-handoff.tar.gz`. Sandbox files do NOT persist across chats.
2. Read this file end to end. Then read project memory (areas/esp32-attendance, areas/esp32-attendance-ledger). Memory is user-stated only; this file has the critique.
3. First user action: run `pio test -e native` in `att/`. Expected: Core A 20/20, Core B 36/36, Core B2 11/11 (B2 and the clip fix have NEVER run on device).
4. Evidence tags: [V] verified on user's hardware or PlatformIO; [T] sandbox only (host g++ + Unity shim); [U] user-stated; [C] Claude assessment, not approved; [X] unverified assumption.

## 1. Working agreement with this user
- Solo developer. Android phone is the main device (Claude app, Termux + proot Debian PlatformIO). Laptop roughly weekly, so hardware test windows are rare. Batch every hardware question into one sitting.
- Concise. Mobile screen. Sections under about 40 lines. Tables for trade-offs. Numbered decisions with a recommendation.
- Be critical. Name the flaw in every substantive reply. No flattery. Socratic when useful.
- Code: chunks of at most ~30 lines with exact paths, or a tarball for anything bigger. Pasting big blocks is unreliable.
- Deliver every file with present_files. A file not presented cannot be opened on mobile (this was missed once).
- "Your judgement" means: make the call and state the reason in one line.
- Fail-closed for attendance, clock, and storage. Convenience never overrides it. The user accepted this rule.
- Admin convenience is the stated priority for UI and flow, but never at the cost of record integrity.

## 2. Product scope (user-stated)
- Up to 45 students. Two sessions per day (AM, PM), Monday to Friday, one school year.
- Timezone PHT, UTC+8, fixed offset +480 min for the year. [U]
- Scan window opens 15 min before session start and closes at session end. [U]
- Late = more than 30 min after session start. [U] (The baseline HTML default was 10; the user chose 30.)
- Session times 08:00-12:30 and 13:30-16:30 come from the baseline sample data. [X] The user has NOT confirmed them as the real school timetable.
- Marks: Present, Late, Excused, Clear. Absent is derived, never stored. Absent requires a closed session, coverage = yes, and an expected student. Otherwise the status is Unverified. [U]
- Present/Late frozen at scan time. Setting changes are forward-only. [U]
- No-class per session or whole day. Marks kept under no-class. [U]
- Roster: deactivate only, never delete. Name max 32 bytes UTF-8. [U]
- Student ID = sensor slot, 1..C. Capacity C = 300 (read from the user's AS608). Provisioning refuses if C < 60. [U]
- OLED shows ID + initials only, never full names. [U]
- No admin fingerprint. The access point (AP) is always on. No OTA. No deep sleep. No AP-on-demand. [U]

## 3. Decision ledger (user-accepted)
### Time
- UTC epoch stored; fixed offset per year. [U]
- Clock policy (Option B): ordering by sequence number. Backward sets and jumps over 24 h need explicit confirm. Each set writes CLOCK_SET old and new, plus a GAP over the ambiguous span. [U]
- Provisioning wizard shows RTC time next to phone time, requires explicit Confirm/Sync. [U]
- Auto-sync config ON by default with limits: |delta| in (120 s, 300 s]; at most once per 24 h; never within 5 min before or inside a scan window; never in CLOCK_FAULT or UNPROVISIONED; audited as source=AUTO. [U-implied, judgement]
- Sync is blocked inside scan windows except in CLOCK_FAULT. [U]
- The phone's clock is never used silently for the UTC offset. [U]

### Storage and integrity
- 16-byte records: type(1) flags(1) a(2) epoch(4) seq(4) b(2) crc16(2). CRC-16/CCITT-FALSE over bytes 0-13, little-endian. [U]
- Flags: bit0 session (0 AM, 1 PM), bits1-2 status (0 clear, 1 P, 2 L, 3 E), bit3 coverage, bit4 MORE (group continues), bit5 whole-day. [U]
- Commit groups: one storage append per group, verified by read-back. Boot drops a trailing unfinished group. [U]
- Expected-student list at closure: bitmap in PAYLOAD records, 64 IDs per record, C=300 needs 5 records. [judgement, accepted]
- Config: A/B slots with generation and CRC. Write the older slot, verify, switch. [U]
- Monthly append-only log files. Single writer task (store_task). No filesystem mutex, no RAM scan queue. [U]
- Success is shown only after read-back verification. [U]
- Never auto-format. Initialization is explicit, behind the OLED setup code. [U]
- FS_VERSION bump on format change; mismatch = CONFIG_FAULT. [U]

### Faults (Stage 2 section 5 matrix)
- States: UNPROVISIONED, CLOCK_FAULT, STORAGE_FAULT, CONFIG_FAULT, SENSOR_FAULT. Permissions in health_fsm.cpp. [U]
- Faults latch until the cause is gone AND re-verified. [U]
- SENSOR_FAULT still allows manual corrections. [U]

### API and dashboard
- HTTP JSON at /api/v1 on core WebServer. No HTTPS. [U]
- Polling every 2 s inside a scan window, 10 s otherwise. Paused when the tab is hidden. Stale over 6 s shows "Disconnected" and greys derived statuses. [U]
- Marks are idempotent. req_id only for roster and enroll (16-entry RAM ring). [U]
- Config writes carry expect_gen; mismatch = 409. 423 = blocked by a fault state. 422 = validation. [U]
- Today screen is the landing page. Wizard order: clock, term, timetable, PIN, roster. [U]
- Mark dialogs default to the previous note. [U]
- Export is chunked (4 KB). [U]
- Day view served from RAM for today; past days from the log. [U]
- Sound setting: Full (default), Quiet (no beep for Present/Late), Silent (only the NOT SAVED/fault beep). Sound never changes screen text. Speaker icon on Today cycles modes. [U; Silent keeping the fault beep accepted by implication]

### Security
- Admin PIN: 6 digits, salted hash (PBKDF2-SHA256 via mbedtls [X]). Lockout: 5 failures = 60 s, doubling, 5 min cap, counter in NVS. [U]
- Token: 128-bit, RAM-only, 15 min idle expiry, max 2 sessions. [U]
- Setup code (6 digits, random, on OLED) required for first run, restore, PIN reset, year rollover. [U]
- Wi-Fi WPA2 passphrase known only to admin. If it leaks, the admin token can be sniffed over HTTP. Accepted. [U]
- Max 2 clients. [U]
- Backups: full, encrypted with a passphrase separate from the PIN (AES-GCM [X]). A lost passphrase means the backup is useless. [U]
- BACKUP_ACK: browser posts CRC32 and length; device compares; match = backup recorded. [U]
- Restore is staged, validated, then swapped. Never merged. Writes RESTORE event and a GAP over the lost window. [U]
- Year rollover needs BACKUP_ACK for the current last_seq plus a typed confirmation. Prior year read-only. [U]
- Template backup depends on sensor library support [X]. Fallback: re-enroll 45 students. [U]

### Firmware and build
- USB flashing only (Web Serial via jason2866.github.io/WebSerial_ESPTool, Android Chrome). [U]
- Partition table: nvs 0x9000 size 0x5000; otadata 0xe000 size 0x2000; factory app 0x10000 size 0x180000; spiffs 0x190000 size 0x270000. [V: FS total 2,555,904 B]
- First flash: bootloader 0x1000, partitions 0x8000, boot_app0 0xE000, firmware 0x10000. Erase allowed once. Updates: firmware 0x10000 only. Never erase on update. [U]
- CPU 240 MHz. [U] (Overrode the earlier 80 MHz default.)
- Build envs planned: native (core), sim (fakes on real board, SIM banner, SIM flag in log header), prod (real drivers, no sim code). [U]
- UI embedded in firmware. The baseline HTML is a UX and product-rules reference only. [U]

### Power and feedback
- Portable. 10,000 mAh power bank, no low-current mode. Charge nightly. [U]
- OLED dims when idle, does not turn off. [U]
- Buzzer patterns: Present = 1 short; Late = 2 short; Already marked = 1 long; Bad image = 1 very short; No match = 2 long; Outside window = 1 long; No class = 1 long; Not active = 2 long; NOT SAVED / fault = 3 long; enroll step = 1 short. [U]

## 4. Hardware facts (measured unless marked)
| Item | Fact | Evidence |
|---|---|---|
| Board | ESP32-WROOM-32 DevKit V1, 4 MB flash | [V] |
| Toolchain | PlatformIO 6.2.0, g++ 14.2.0, espressif32 8.0.0, framework 4.30312.261024 | [V] |
| Pinned libs | RTClib 2.1.4, Adafruit BusIO 1.17.4, ArduinoJson 7.4.3 | [V] |
| Not pinned | Adafruit_SSD1306, Adafruit_GFX, Adafruit_Fingerprint | [X] |
| Heap idle (no Wi-Fi) | 328,436 B free, largest block 110,580 B | [V] |
| Heap, SoftAP + WebServer + 1 client | about 211 KB free, minimum about 196.5 KB | [V] |
| Dashboard gz | 34,076 B, served in 65-76 ms, renders on phone | [V] |
| LittleFS 16 B append + flush | min 28-37 ms, avg 54-61 ms, max 144-172 ms | [V] |
| OLED | 0x3C alone | [V] |
| RTC DS3231 + CR2032 | 0x57 and 0x68; lostPower = 0; time consistent with UTC (one sample) | [V] partial |
| AS608 | detected; Storage Capacity 300; Security Level 3; Packet Size 128 B; Baud 57600 | [U] |
| Buzzer | 3V3 only, GPIO25, LOW = ON; audible at 50 ms; chirps once at boot (bug) | [U] |
| Bank run | 24.3 min at 240 MHz, RTC only, no cutoff | [V] partial |
| Android | "continue connecting?" prompt on no-internet AP; page loads | [U] |
| /sdcard | accessible from proot | [U] |
| Not arrived / not known | breadboard, jumper wires (unknown) | [U] |
| Not owned | inline USB power meter | [U] |

Wiring (from the design): OLED SDA 21, SCL 22, VCC 3V3, GND. RTC shares I2C. AS608 TX to GPIO16 (RX2), RX to GPIO17 (TX2), VCC 3V3 or 5V (decision pending, see C3), GND. Touch/wake to GPIO4 (not verified). Buzzer I/O to GPIO25, VCC 3V3.

## 5. Software state
### Files (att-handoff.tar.gz)
- `att/platformio.ini`: env `native` only.
- `att/lib/core/src/`:
  - `event_codec`: record codec, CRC16, flags, payload bitmap
  - `time_rules`: day numbers, weekday, minute-of-day, dayStartEpoch, timetable validation, sessionAt, isLate
  - `status_derive`: derive() precedence
  - `istorage.h`: storage interface (size, read, append, truncate)
  - `log_store`: open and repair, commit groups, fault latch, observers (max 16), attach() replays
  - `log_index`: RAM index of GAPs (cap 512) and CLOSEs (cap 800)
  - `day_view`: applyEvent (shared), buildDayView (full replay), DayCache (incremental, one target)
  - `closure_engine`: GAP writing (clipped to scan windows), closure via index, refuses if index attach failed
  - `health_fsm`: fault states, latching, permissions
  - `clock_policy`: decideClock, applyClockSet, nearScanWindow
- `att/test/test_core/test_main.cpp`: 20 tests [V]
- `att/test/test_coreb/{fakes.h,test_main.cpp}`: 36 tests [V]
- `att/test/test_coreb2/{fakes.h,test_main.cpp}`: 11 tests [T]
- `s0/`: hardware probe (platformio.ini, partitions.csv, src/main.cpp, src/index_gz.h generated baseline array, test/test_smoke)

### Changes in the last session
1. RAM index added. The closure engine no longer replays the whole log.
2. Boot and fault GAPs are now clipped to the span touching scan windows. Overnight power cycles write no GAP. Before this, every daily power-on wrote one, which would overflow the 512-GAP cap in about 2.5 years. Covered by 2 new tests and a mutation check.
3. Index overflow behavior: GAP overflow = coverage No (Unverified, never Absent). CLOSE overflow = closure refused.

### Test commands
- Run all: `cd ~/att && pio test -e native`
- Expected: test_core 20 passed; test_coreb 36 passed; test_coreb2 11 passed.
- Sandbox mutation checks broke index, cache, attach, engine gating, gap overflow, gap clipping, and duplicate-close handling. Each was caught.

### What the sandbox tests do NOT prove
- Flash timing, LittleFS atomicity on power loss, and real storage errors. They use fakes.
- The shared `applyEvent` logic is verified only by the original Core B tests, because the differential test compares two paths that share it.
- Anything on the ESP32 under real Wi-Fi load.

### Not built yet
config_store (A/B slots), api_logic, enroll_fsm, backup_format, the sim env (store task, WebServer routes, fake RTC and sensor, SIM banner), the dashboard rewrite (the baseline has simulated logic and demo data), all prod drivers (RTC, AS608, OLED, buzzer, LittleFS, NVS), the restore path, the quarantine path, and the admin wizard.

## 6. Critical audit (ordered by severity). Every item needs a decision or a test.

### BLOCKERS: must be resolved before any field use
B1. Power-on dependency. A device-off minute inside a scan window makes that whole session Unverified. Fail-closed, so correct, but it means the admin must power the unit on before 07:45 (AM window start) and before 13:15 (PM window start) every school day. A nightly power-bank routine plus morning boot makes this easy to miss. Needs: a power-on procedure, an OLED or dashboard warning when the unit boots inside a window, and a decision on whether the device should stay powered through the day.
B2. Bank runtime is unmeasured. The estimate is 22-32 h at 0.8-1.2 W, and 240 MHz with OLED, sensor, and AP may draw more. Sensor current spikes may also trip the bank. Measure with the full load for 60+ minutes before relying on it. [X]
B3. AS608 power source and brownout are unmeasured. Powering the sensor from the DevKit 3V3 pin shares the LDO with Wi-Fi TX peaks. Sensor 5V on VIN is an alternative. Add bulk capacitance. Decide after a measurement. [X]
B4. Log scope across years. The index caps (512 GAPs, 800 CLOSEs) assume one school year. The design says prior years are read-only, but LogStore is currently a single stream. Each year needs its own log stream (or the index must be reset at rollover). Not designed in code. [C]
B5. Core B2 and the clipping change have not run on the user's device. [T]
B6. No usable product yet. There is no API, no dashboard, no admin wizard, and no scan path (status commit, duplicate rule, OLED text, buzzer). The user cannot run the attendance flow end to end. [C]
B7. Template backup is unverified. If the AS608 cannot download or upload templates, "verified full restore" is false, and re-enrollment of 45 students is the fallback. [X]

### HIGH
H1. Boot chirp on GPIO25. The buzzer sounds before setup() drives the pin HIGH. Proposed fix: 10 kOhm pull-up from GPIO25 to 3V3. Untested. The chirp looks like a false scan to students.
H2. Corrupted log refuses to open. One flipped bit in any record stops scanning until a restore. There is no quarantine path, and the restore path is unbuilt. Consider per-month files (Stage 2 had them) so one bad month does not disable the device. [C]
H3. LittleFS has no truncate call [X]. The plan is copy-to-temp plus rename, which is untested and must latch STORAGE_FAULT on failure.
H4. Torn-write behavior on LittleFS is unverified. Group commits rely on the MORE flag and read-back. A power cut inside a flush is only proven on fakes.
H5. Closure loop cost: up to 400 days x 2 sessions x up to 800 index compares per step. About 640k compares worst case, per call. Measure on device. The index is about 14 KB and must not live on a task stack.
H6. AS608 scoring: minScore = 120 comes from the baseline. The AS608 score scale is unknown. A wrong threshold causes false rejects (students cannot check in) or false accepts. Measure real scores during enrollment and matching. [X]
H7. Sensor replacement and slot mapping. The roster maps IDs to sensor slots. If the sensor is replaced, the capacity check passes but the templates are gone. Stage 2 §6 required probing occupied slots at boot. The library call for that is unverified. [X]
H8. Admin token over plain HTTP plus a shared PSK. Accepted by the user, but the global PIN lockout lets any joined client lock the admin out. Accepted risk; document it for the admin.
H9. RTC charge circuit: the CR2032 sits in a module that may charge a primary cell [X]. Lift the charge path or use a non-charging module. Only 3V3 is used.
H10. Timezone and DST: the Philippines is believed to observe no DST [X]. Verify before the school year starts, since a wrong offset is hard to undo under the fixed-offset rule.

### MEDIUM
M1. Buzzer loudness at 3V3 in a noisy classroom is untested. 50 ms is audible in a quiet room only.
M2. OLED burn-in and the dim mode are unverified [X] (pixel shift is a plan, not code).
M3. Auto-sync can move the clock up to 5 min once per 24 h, audited only. Accepted, but any admin login can change time.
M4. Orphan templates: the "no delete" rule applies to roster rows, not sensor templates. Leavers' templates stay in the sensor. With 300 slots this is manageable, but it needs an admin procedure.
M5. Test file has 3 harmless compiler warnings (missing initializers in test_coreb2). Cosmetic.
M6. The baseline dashboard contains demo data and a "download demo file" flow that is not restorable. Do not carry any of it into firmware.
M7. The Android "continue connecting?" prompt may return after reboot or reconnect. The UI must recover from a lost AP connection without losing the pending action.
M8. Browser polling stops when the screen locks. The stale-data rule must handle this (show "Disconnected", never a cached Absent as live).
M9. Late threshold uses minute resolution. A scan at exactly start + 30 min is Present (the rule is "more than 30"). Confirm this matches the user's intent.

### LOW
L1. Constants duplicated between test code and production code (scanLeadMin = 15, lateMin = 30 in tests). Keep one source when config_store exists.
L2. `ClosureEngine::invalidate()` is a no-op kept for compatibility. Remove it at the next cleanup.
L3. `MAX_TRACK_DAYS = 400`: closures for terms older than about 400 days are silently skipped. Provisioning must reject such a term start.

## 7. Mistakes log (chat 1)
1. Partition table: phy_init and boot_app0 both placed at 0xE000. Corrected.
2. Stage 2 coverage: a 5-min tick cannot see shorter faults. Replaced in Core B with per-fault GAPs. Stage 2 §4 is superseded.
3. S0 "compile verified" was a syntax check with stubs. Weaker than I implied. The user's device run later proved the APIs.
4. Assumed UTC+8 before the user said so. Later confirmed PHT.
5. Accepted the user's "30+ min" bank claim without checking. The log proved 24.3 min.
6. Several early numbers were guesses (heap, flash budgets, scan latency). Replaced by measurements where possible.
7. Replies were too long for the phone. Keep sections under about 40 lines.
8. Some memory lines were tagged [stated] when they were my recommendations. Fixed in the ledger.
9. Core B2 was built and not presented to the user until asked. Fixed.
10. A stale note said the AS608 had not arrived after the user had read it. Superseded in this file.
11. The overnight GAP design (one per power-on) was a real index-growth bug I shipped in Core B. Fixed by clipping.
12. Session times 08:00-12:30 and 13:30-16:30 came from baseline sample data and were treated as real. Not confirmed.

## 8. Unverified library and API register (verify against the pinned version's header)
| Item | Used for | Status |
|---|---|---|
| RTC_DS3231 begin, lostPower, now, adjust | clock | begin, lostPower, now used in S0 [V]; adjust [X] |
| Adafruit_SSD1306 (128,32,&Wire,-1); begin(SSD1306_SWITCHCAPVCC,0x3C); dim() | OLED | [X] |
| Adafruit_Fingerprint: constructor, getParameters().capacity, getImage, image2Tz, createModel, storeModel, fingerFastSearch, deleteModel, template download/upload, LED control, timeouts | sensor | capacity read by the user's sketch [U]; the rest [X] |
| WebServer: send_P 4-arg, body limits, chunked responses | HTTP | send_P used in S0 [V]; rest [X] |
| LittleFS: begin(false), format, totalBytes, usedBytes, open, write, flush | storage | used in S0 [V] except truncate [X] |
| Preferences (NVS) | counters | used in S0 [V] |
| ArduinoJson 7 (JsonDocument, deserializeJson) | input parsing | used in S0 [V] |
| mbedtls PBKDF2, AES-GCM; esp_random quality | security | [X] |
| embed approach (C array now; build-step embed later) | UI | C array works [V]; production method open [X] |

## 9. Constants (current code)
| Name | Value | Where |
|---|---|---|
| SCAN_LEAD | 15 min | Params, tests |
| lateMin | 30 min | user-stated (baseline default 10) |
| TICK_PERIOD_S | 300 s | closure_engine.h |
| CLK_INFO_S | 120 s | clock_policy.h |
| CLK_AUTO_MAX_S | 300 s | clock_policy.h |
| CLK_JUMP_S | 86400 s | clock_policy.h |
| CLK_AUTO_EVERY_S | 86400 s | clock_policy.h |
| MAX_TRACK_DAYS | 400 | closure_engine.h |
| MAX_GAPS | 512 | log_index.h |
| MAX_CLOSES | 800 | log_index.h |
| MAX_OBSERVERS | 16 | log_store.h |
| MAX_GROUP | 16 records | log_store.h |
| MAX_ID | 300 | day_view.h |
| PIN lockout | 5 fails, 60 s, doubling, 300 s cap | Stage 5 (not coded) |
| Token | 15 min idle, 2 sessions | Stage 5 (not coded) |
| minScore | 120 | baseline (AS608 scale unverified) |
| reminderDays | 7 | baseline |
| maxAhead | 30 days | baseline |
| Probe AP | S0-PROBE / probe12345 | probe only, never production |

## 10. Admin operating procedure (draft, needs user review)
- Before 07:45 every school day: power the unit on and confirm the OLED shows READY (not a window-blocked state). A late boot makes that session Unverified. [C]
- Before 13:15: check the PM window is ready (usually the same device state). [C]
- Charge the bank nightly. Confirm the charge level before each school day. [C]
- Weekly: run a backup and record BACKUP_ACK. Copy the backup off the phone during the laptop window. [C]
- Year end: backup, then rollover with the setup code. [U-derived]
- Never leave the Wi-Fi passphrase where students can read it. [C]

## 11. Test plan for the next hardware sitting (one sitting, all results pasted raw)
| # | Test | Pass criterion |
|---|---|---|
| 1 | Pull-up on GPIO25 (10 kOhm to 3V3). Reset 5 times. | No boot chirp on any reset |
| 2 | `/b?ms=50` and `/b?ms=100` | Audible both (50 ms already audible) |
| 3 | OLED + RTC on one bus. I2C scan. | 0x3C, 0x57, 0x68 all present |
| 4 | RTC read after power cycle | Time continues, lostPower stays 0 |
| 5 | AS608 touch pin: hold finger; read GPIO4 | Pin changes state with touch (or document that it does not) |
| 6 | AS608 `getParameters()` with its library | Capacity 300 confirmed through the library |
| 7 | Enroll one finger, then search it 10 times | Scores recorded; pick minScore from data |
| 8 | Template download: one template to the phone or to flash | Works or fails (decides backup scope) |
| 9 | Sensor VCC from 3V3 vs VIN: 10 consecutive scans | No resets, no bad reads |
| 10 | Bank, full load (240 MHz, OLED, sensor, AP), 60 min | Uptime recorded; no cutoff |
| 11 | `pio test -e native` | 20 + 36 + 11 pass |
| 12 | S1 probe: group write latency; Wi-Fi during a long write | Recorded |
| 13 | Two Android clients on the AP | cli=2 works; a third refused |

## 12. Roadmap (order and gates)
Gate 1 (now): user runs test_coreb2 on device. Pass = 11 tests passed.
Gate 2: breadboard arrives. Run section 11 tests 1-5 and 9.
Gate 3: AS608 bench. Tests 6-8. Decide templates-in-backup (B7) and minScore (H6).
Gate 4: power. Tests 10 and B1 procedure. Decide the device-on policy and whether to add a UI warning.
Gate 5: Core C: config_store (A/B slots), api_logic (JSON-free core), enroll_fsm, backup_format. Includes B4 (per-year log streams) and H2 (per-month quarantine). Design these in the chat before coding.
Gate 6: sim env: store task, WebServer routes, fake RTC and sensor, SIM banner. Lets the whole API and UI run on the user's ESP32 before parts integration.
Gate 7: dashboard rewrite against the API. Not a patch of the baseline.
Gate 8: prod drivers and the scan path. Full end-to-end test on hardware.
Gate 9: S1 probe, backup/restore rehearsal, year-rollover rehearsal on a copy.

## 13. Open questions for the user (answer these first)
1. Confirm session times. Are 08:00-12:30 and 13:30-16:30 the real school schedule? Per weekday, or the same every day?
2. Power policy: will the unit stay on through the school day, or be powered on each morning? (Drives B1.)
3. AS608 power: 3V3 or 5V (VIN)? Recommend deciding after test 9.
4. Breadboard and jumper wires: arrived?
5. Probe result for the pull-up fix (test 1).
6. Do you want one device per class, or one device for the school? (Assumed one device, one roster.)
7. Is the Wi-Fi passphrase shared beyond you? (Affects H8.)
8. Who else is an admin? (Affects PIN and passphrase handling.)

## 14. Decisions still open (with my recommendation)
D1. Templates in backup: decide after test 8. Recommend: include if supported; otherwise re-enroll fallback.
D2. minScore: decide after test 7. Recommend: a configurable threshold with a default from measured data, never the baseline 120 unverified.
D3. AS608 power source: decide after test 9.
D4. Per-month log files for quarantine (H2): recommend yes; it is cheap and limits damage.
D5. Per-year log streams (B4): recommend yes, one stream per school year.
D6. Device-on policy (B1): recommend the UI warn when the unit boots inside a window, and the admin procedure in section 10.

## 15. Open uncertainties (all unverified)
- Every [X] item in sections 4, 6, and 8.
- The boot-chirp fix (pull-up) and whether a 10 kOhm value suits the buzzer module.
- Closure loop time and index memory on device.
- Flash write stalls on Wi-Fi, and LittleFS torn-write behavior under power loss.
- Bank runtime at full load and sensor current spikes.
- AS608 score scale and template download support.
- Philippine DST status (believed none).
- Android behavior when the AP disappears and returns.
