# DECISIONS

Status legend
- SIGNED: you approved it explicitly in chat.
- SIGNED-HANDOFF: stated in HANDOFF_FINAL.md, signed off in earlier work.
- DELEGATED: you said "your judgement" (or equivalent). I chose it. You have NOT reviewed the detail.
- MEASURED: a fact from your device logs.
- OPEN: not decided.
- DEFECT: known problem, not fixed.
- SUPERSEDED: replaced. Do not use.
- CHANGED: differs from an earlier version (see note).

Source column: "Ledger" = your signed-off decisions file (memory copy), "Handoff" = HANDOFF_FINAL.md, or the chat date and gist.

## Product rules (from HANDOFF_FINAL.md)
| ID | Decision | Status | Source |
|---|---|---|---|
| P1 | Max 45 students; morning and afternoon sessions, Mon-Fri; one school year | SIGNED-HANDOFF | Ledger |
| P2 | Scan window opens 30 min before a session, ends at session end | CHANGED | Chat 2026-11-10 |
| P3 | Late window is 30 min after session start | SIGNED-HANDOFF | Ledger |
| P4 | Marks: Present, Late, Excused, Clear. Absent derived, never stored | SIGNED-HANDOFF | Ledger |
| P5 | Absent only when session closed, coverage yes, and student expected; else Unverified | SIGNED-HANDOFF | Ledger |
| P6 | Present and Late frozen at scan time; setting changes forward-only | SIGNED-HANDOFF | Ledger |
| P7 | No-class applies to one session or the whole day; marks kept | SIGNED-HANDOFF | Ledger |
| P8 | Roster: deactivate only, never delete; name up to 32 bytes UTF-8 | SIGNED-HANDOFF | Ledger |
| P9 | Student ID = sensor slot, never reused; AP always on; OLED shows ID and initials only | SIGNED-HANDOFF, see E24 | Ledger |
| P10 | Timezone PHT, UTC+8 (480 min) | SIGNED-HANDOFF | Ledger |
| P11 | Firmware: USB only, no OTA | SIGNED (you answered "USB" to the OTA question; matches the handoff) | Ledger, chat |
| P12 | OLED dims when idle instead of turning off | SIGNED-HANDOFF | Ledger |
| P13 | Backups encrypted with a separate passphrase; restore staged and validated; year rollover needs a verified backup and typed confirmation | SIGNED-HANDOFF | Ledger |
| P14 | Admin PIN 6 digits with lockout; session token idle 15 min, max 2 | SIGNED-HANDOFF (numbers for the lockout are DELEGATED, E12) | Ledger |
| P15 | Setup code on the OLED required for first run, restore, PIN reset, and year rollover | SIGNED-HANDOFF | Ledger |

## Operating choices (chat)
| ID | Decision | Status | Source |
|---|---|---|---|
| O1 | Power: always-on. No deep sleep, no AP on demand. Admin can power off anytime | SIGNED | Chat 2026-10-31, option 1 |
| O2 | Session defaults 08:00-12:30 and 13:30-16:30; configurable; two sessions only | SIGNED | Chat, "use the current defaults" |
| O3 | Timetable change rule: refuse Params or Timetable changes while a window is open or a past session is unclosed | SIGNED (option 1). Core setter added; config-adapter integration pending (C1) | Chat 2026-11-01, implementation 2026-11-04 |
| O4 | No marks on future days. Present/Late only for today's open window or earlier. Excused only for today or earlier | SIGNED ("keep"). The handoff is silent (C9). Your "keep" was matched to this rule, the one you were shown | Chat 2026-11-02, 2026-11-04 |
| O5 | GET /status is public, limited to basic non-sensitive status fields; administrative operations remain authenticated | SIGNED (user chose Option A on 2026-11-04) | Chat 2026-11-04 |
| O6 | Future-day no-class allowed (holidays) | DELEGATED | Chat |
| O7 | Deactivated students can still be corrected | DELEGATED | Chat |
| O8 | Template backup CUT. Backup = config + roster + log | SIGNED ("let's cut template back-up") | Chat 2026-11-03 |
| O9 | Bulk enrollment uses the approved queue workflow rather than self-service or one-at-a-time phone commands | SIGNED (user approved queue recommendation on 2026-11-04) | Chat 2026-11-04 |
| O10 | Fast enrollment queue = active roster IDs minus valid sensor occupancy | SIGNED (user approved on 2026-11-04) | Chat 2026-11-04 |
| O11 | Post-enrollment verification scan can be toggled ON or OFF by the administrator | SIGNED (user chose toggle ON/OFF on 2026-11-04) | Chat 2026-11-04 |
| O12 | Diagnostic sketch is a separate tool, not production | DELEGATED | Chat |

## Hardware
| ID | Decision | Status | Source |
|---|---|---|---|
| H1 | AS608 on 3V3 only, no 5 V | SIGNED | Chat, "3v3, no 5v, it might hurt the pins" |
| H2 | No bulk capacitor. Dropped | SIGNED | Chat |
| H3 | Buzzer on GPIO23 (was 25) | SIGNED | Chat 2026-11-02 |
| H4 | GPIO25 pull-up not needed. Boot chirp accepted | SIGNED | Chat |
| H5 | Bring-up pass line: at least 9 of 10 clean scans, no BROWNOUT | DELEGATED, accepted by you | Chat |
| H6 | Template round-trip verified by byte-exact re-download, not by the module's reply | DELEGATED | Chat |
| H7 | UART reads never call handleClient. RX buffer 2048 | MEASURED (fix confirmed) | Chat 2026-11-03 |
| H8 | Diagnostic sketch leaves a duplicate template on page 2 | DEFECT (C5) | Read from code |
| H9 | Scan result code 23 counts as a failure in the pass line | DELEGATED. Meaning UNVERIFIED (U6) | Chat |
| H10 | Packet size 128 bytes for template transfer | MEASURED on one sensor | Device log |
| H11 | Template size 768 bytes | MEASURED on one sensor | Device log |

## Engineering (data layer, mostly delegated)
| ID | Decision | Status | Source |
|---|---|---|---|
| E1 | Core B fixes: long-outage GAP chunking, clock-set group split, attach rollback, duplicate attach refused | DELEGATED. MEASURED on device through the 122 and 142 runs | Chat, ChatGPT review findings |
| E2 | config_store layout, CRC-16, A/B slots, slot rules | DELEGATED | Chat |
| E3 | minScore placeholder 0 | DELEGATED. OPEN until bench data (C4) | Chat |
| E4 | lateMin must be shorter than every enabled slot | DELEGATED | Chat |
| E5 | Month-file layout, per-year streams, quarantine copies aside rather than discarding | DELEGATED | Chat |
| E6 | Roster is event-sourced; SET is a commit group; interval table cap 512, overflow fails closed | DELEGATED | Chat |
| E7 | Replay of malformed roster records is rejected and counted, never applied | DELEGATED | Chat (ChatGPT review) |
| E8 | Backup container: AEAD chunks, header bound as AAD, KDF range 1000 to 5,000,000 | DELEGATED. Count not chosen (C12) | Chat |
| E9 | Backup acknowledgement needs a finished stream; rollover needs ack, typed confirmation and setup code | DELEGATED | Chat (ChatGPT review) |
| E10 | Inner backup format version 2, no templates | DELEGATED, follows O8 | Chat |
| E11 | Restore needs typed confirmation unless the device is in a recovery state | DELEGATED | Chat |
| E12 | Lockout: 5 failures = 60 s, doubling, cap 300 s, counter persisted before verify | DELEGATED | Chat |
| E13 | Tokens: 16 random bytes, RAM only, two sessions, least-recently-used eviction | DELEGATED (length and eviction) | Chat |
| E14 | Request ring of 16 with fingerprint; enroll never overwrites a taken slot | DELEGATED | Chat |
| E15 | API endpoint list and status codes (401 before 423; 409 for stale generation) | DELEGATED | Chat |
| E16 | Sessions and lockouts use monotonic seconds, never the RTC | DELEGATED | Chat |
| E17 | Schedule changes refused while the clock is in fault | DELEGATED | Chat |
| E18 | Student ID to sensor page: page = ID - 1 | DELEGATED. CONFLICT with P9 text (C9) | Chat |
| E19 | Student without a template is never expected; an unfinished index read excludes everybody | DELEGATED | Chat |
| E20 | Restore on a healthy device requires a typed confirmation | DELEGATED | Chat |
| E21 | Adapter responsibilities (NVS, LittleFS, HTTP, mbedtls) are not all built | OPEN. v0.2.13 adds an NVS config-slot adapter; v0.2.34 adds an NVS PIN-attempt store and boot owner. LittleFS, HTTP and mbedTLS credential/cipher adapters remain absent; target build remains unverified. NVS encryption for physical-flash confidentiality is out of scope under signed R11 | Chat |
| E22 | Enrollment implementation details: ascending ID queue; verification toggle is selected per batch and is not persisted in Config; one sensor operation per step; three consecutive recoverable errors pause for admin retry; any non-OK store reply invalidates occupancy and requires full index refresh; never blindly retry a store; wait for finger release before next student and before EN_DONE; verification failure does not auto-delete a template | DELEGATED. Implemented in v0.2.2; not yet reviewed | Engineering choice 2026-11-04 |
| E23 | AS608 is exclusively owned for the duration of an enrollment batch; attendance scan commands are paused while the FSM has control, while web/RTC/display work continues; cancellation requires a finger-release fence before scans resume | SIGNED (user approved Option A on 2026-11-04). Shared owner-aware gate/session lease implemented in v0.2.7; sensor-bus composition root and attendance scan lease added in v0.2.9; firmware adoption pending | Chat 2026-11-04, Option A |
| E25 | Attendance owns the sensor for the complete authentication/capacity setup or capture-convert-search scan; a poisoned transport keeps the attendance lease held until a full power-cycle/reconstruction | SIGNED. User approved on 2026-11-05: attendance owns the sensor for complete setup/scan; a poisoned transport keeps ownership until full power-cycle/reconstruction | Engineering choice 2026-11-05 |
| E26 | `termEnd` day-number boundary semantics and consistent enforcement across API, fingerprint scan, closure, no-class and day view | DELEGATED. PISHDA selected an inclusive final local calendar day on 2026-11-13 under the user's explicit authority to decide engineering choices. Contract: `0 <= termStart <= termEnd <= 31446`; a one-day term is valid. The upper bound is stricter than the 16-bit event day field: uint32 Unix timestamps can represent every second of local day 31446 (2106-02-05) at all supported offsets, but not a later full local day. API/manual mutations, AS608 scan begin/commit, closure/tick/outage clipping and config/setup validation now use this bound. `apiDayInInclusiveTerm()` is the shared guard required by any future day-view/read route before building a view. Source and regression changes are authored; current tests are NOT RUN. C18 remains blocking until tests run and a production day-view route uses the helper. This is delegated engineering policy, not SIGNED product authority. C17 schedule-effective history remains separately blocking. | User delegated engineering judgment 2026-11-13 |
| E27 | Day-granular roster transition semantics when multiple lifecycle changes occur on the same local day | DELEGATED. Assistant judgment accepted by user on 2026-11-07: keep same-local-day ACTIVATE/DEACTIVATE fail-closed for authoritative monthly reporting. The guard is a reporting-projection restriction only; it does not reject or rewrite committed operational roster events. Rationale: ACTIVATE then DEACTIVATE can collapse to `[D,D)` and omit a learner who may have attended part of that day; DEACTIVATE then ACTIVATE can make the learner appear enrolled for the whole day despite an intraday gap. Event epoch/sequence order exists, but RosterStore reduces boundaries to local days. Do not relax until a separate time-granular projection defines AM/PM session eligibility and RTC rollback semantics. Same-day display-name revisions remain source-sequence ordered and are diagnostics only, not legal identity. | User delegated judgment 2026-11-07; decision selected by assistant |
| E28 | First-provision WPA2 AP bootstrap credential source and disclosure | SIGNED. During `CFG_BOOT_NEEDS_SETUP`, create a 128-bit secret from the ESP-IDF SAR-ADC entropy source, encode it as 32 lower-case hexadecimal characters, persist create-only in NVS `attwifi/ap_pass_v1`, and read it back before it is used. Return a disclosure request on every boot that remains `NEEDS_SETUP`, so a power loss before setup completion does not lose the display opportunity. The production setup caller must display it on the OLED before provisioning transitions to `READY`. v0.2.44 adds a standalone 128x32 OLED adapter; v0.2.45 adds a reusable coordinator that requests the display transfer before AP startup, then requires AP readiness, an AP-netif DHCP-assignment event and a per-call setup-auth gate before provisioning. It replaces `Config::wifiPass` with the exact bootstrap credential so the committed config matches the AP. It is not a production boot/HTTP caller, and the void display API cannot confirm physical visibility; the DHCP latch is not current-client identity or authentication. In `CFG_BOOT_READY`, validated `Config::wifiPass` is authoritative: backfill the separate NVS record only if missing; fail closed if an existing value differs. Never use open Wi-Fi, a diagnostic default or the setup code. Rotation/reset/recovery is separate and governed by delegated E30. | User explicitly instructed implementation of this proposed design on 2026-11-08 |
| E29 | Production SoftAP SSID source and naming format | SIGNED. Use the fixed generic SSID `att`; do not append a MAC, school, owner, or device identifier. The setup coordinator owns this SSID; callers cannot override it. | User approved 2026-11-08 |
| E30 | AP passphrase rotation, reset and recovery | DELEGATED. Conservative v0.2.46 policy: no remote rotation, no reset endpoint, no touch/boot gesture, and no automatic NVS erase/overwrite. The first-provision 128-bit credential remains stable and create-only. If the credential is forgotten, mismatched, or corrupt, fail closed and require a separately designed, physically supervised USB service procedure; that service procedure is NOT implemented. Do not clear attendance/config data as a side effect of AP recovery. Revisit rotation only with a transaction spanning the AP credential record and authoritative Config slot, including rollback/power-cut tests. | Assistant judgment explicitly requested by user 2026-11-08; DELEGATED, not user-reviewed |
| E31 | Production first-provision AP channel | DELEGATED. Use channel 1 as the conservative fixed default; do not allow the production first-provision caller to override it. Actual regulatory-country configuration and channel acceptance remain unverified under U93 and must be confirmed on the target. | Assistant engineering judgment 2026-11-08; not user-reviewed |

## SF2-SHS reporting decisions (reporting layer only; no attendance-core change)
| ID | Decision | Status | Source |
|---|---|---|---|
| R1 | Support both Regular SHS and Strengthened SHS reporting profiles; one device is configured for one curriculum profile at a time because the device supports one section only | SIGNED | Chat 2026-11-07 |
| R2 | One section per device; ask for the section during initial setup | SIGNED | Chat 2026-11-07 |
| R3 | Generate learner rows automatically from every enrollment overlapping any part of the reporting month, including learners who transfer out before export | SIGNED | Chat 2026-11-07 |
| R4 | Use the HTML builder's AM/PM diagonal convention as a local reporting adaptation (upper-left = AM only, lower-right = PM only, blank = full day, E = excused, x = both sessions absent) | SIGNED as desired local convention; school/DepEd acceptance remains OPEN | Chat 2026-11-07 |
| R5 | Excused (`E`) counts as Present in monthly totals | SIGNED | Chat 2026-11-07 |
| R6 | Preserve internal Late marks; do not discard or rewrite them for SF2 | SIGNED | Chat 2026-11-07 |
| R7 | Treat Late as attended for local quadrant selection, while leaving monthly TARDY calculation deferred | PROVISIONAL implementation rule; teacher confirmation pending | Engineering bridge from existing Late status and user's “not really do anything yet” instruction |
| R8 | Exact daily cell behaviour for one-session no-class days | OPEN | Teacher confirmation pending |
| R9 | Meaning of the SF2 `Semester` field under current school-year term structure | OPEN | Teacher/school confirmation pending |
| R10 | Whether the customized AM/PM legend is accepted for the school's submitted SF2 | OPEN | Teacher/school/LIS confirmation pending |
| R11 | Security threat model for SF2 identity review and device API: protect only against unauthorized Wi-Fi clients; physical access/flash modification and malicious authenticated administrators are out of scope | SIGNED | User statement 2026-11-07; existing WPA2/PIN/token/no-HTTPS handoff retained |
| R12 | One authorized administrator may both enter and confirm the same learner identity; self-review is permitted and must not be represented as independent verification | SIGNED | User statement 2026-11-07: “allowed for both” |
| R13 | Identity-review audit attributes the action only to the authorized `ADMIN` role and labels the workflow `SELF_REVIEW`; it stores no operator-supplied name, ID or label and makes no individual-human identity claim | DELEGATED | User delegated D4-3 to assistant judgment on 2026-11-07; selected the least misleading attribution model |
| R14 | Identity review uses four broad source categories (`SCHOOL_RECORD`, `GOVERNMENT_DOCUMENT`, `GUARDIAN_SUBMISSION`, `OTHER_SCHOOL_ACCEPTED_SOURCE`); an administrator configures the school's accepted set once. No uploads, document numbers, unrestricted references or free-text evidence are collected. Categories are declarations, not proof of authenticity or DepEd/LIS acceptance. | DELEGATED | Assistant judgment under user's 2026-11-07 instruction to prioritize convenience and foolproof operation |
| R15 | Review order is the committed metadata order tuple (store lineage, transaction revision, change index). RTC may be shown only as an explicitly untrusted diagnostic observation; it never establishes order, effective date or export authority. | DELEGATED | Assistant judgment; DS3231 is adjustable and no trusted calendar-time source exists |
| R16 | Review is bound to the exact immutable learner-identity revision and records the source-policy revision. Any identity correction invalidates review for the new revision; disabling a source category used by an existing review also invalidates it. Historical revisions/snapshots are preserved. Affected prior report periods require an explicit re-generation decision, never silent mutation. | DELEGATED | Assistant judgment; selected revision-scoped invalidation to preserve auditability without rewriting history |
| R17 | Keep O8 unchanged. Do not persist or expose reporting PII until the user explicitly amends O8 or approves a separate companion backup/recovery workflow that preserves metadata and attendance together. Authoritative SF2 export remains disabled in the interim. | DELEGATED | Assistant judgment; does not amend the SIGNED O8 backup scope |

These decisions apply only to the reporting adapter. They do not amend P4-P7 or any signed event/coverage/absence rule in `HANDOFF_FINAL.md`. Full policy notes and the teacher questionnaire are in `docs/sf2/`.

## Process and deliverable
| ID | Decision | Status | Source |
|---|---|---|---|
| X1 | "Main" means a full tree on branch main, with the handoff baseline as commit 1 | SIGNED ("1. main") | Chat |
| X2 | Development continues in ChatGPT | SIGNED | Chat |
| X3 | Documents must make clear what you approved and what I chose | SIGNED ("just make it clear") | Chat |
| X4 | Include the handoff file for comparison | DELEGATED. Done (HANDOFF_FINAL.md at root, identical to the uploaded copy) | Chat |
| X5 | Be efficient: fewer round trips, batched deliverables | SIGNED | Chat |
| X6 | Host tests use a stand-in Unity and a toy cipher; device is the ground truth | DELEGATED | Chat |
| X7 | Patch and apply.sh kept in docs/patches for provenance, not for re-running | DELEGATED | Chat |

## Superseded (do not use)
| ID | Was | Replaced by |
|---|---|---|
| S1 | Diagnostic sketch v1 (buzzer 25, no template fix) | sketch/sketch_compact_minimal_fixed.ino |
| S2 | Older closure_engine from your repo | Handoff version plus the Core B patch |
| S3 | Template backup (512-byte template assumption) | O8, 768 bytes measured (H11) |
| S4 | Deep sleep | O1 |
| S5 | "65 vs 67 tests" puzzle | Resolved: the handoff Core B test file had 11 tests, and your device run confirmed it after the restore |
| S6 | Earlier claim: handoff future-day rule conflicts with O4 | Corrected: the handoff is silent (C9) |
