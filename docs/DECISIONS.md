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
| P2 | Scan window opens 15 min before a session, ends at session end | SIGNED-HANDOFF | Ledger |
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
| O3 | Timetable change rule: refuse Params or Timetable changes while a window is open or a past session is unclosed | SIGNED (option 1). DEFECT: not applied to the engine (C1) | Chat 2026-11-01 |
| O4 | No marks on future days. Present/Late only for today's open window or earlier. Excused only for today or earlier | SIGNED ("keep"). The handoff is silent (C9). Your "keep" was matched to this rule, the one you were shown | Chat 2026-11-02, 2026-11-04 |
| O5 | GET /status is public | DELEGATED. Still NOT confirmed by you (U20). Needs a yes or no | Chat 2026-11-02 |
| O6 | Future-day no-class allowed (holidays) | DELEGATED | Chat |
| O7 | Deactivated students can still be corrected | DELEGATED | Chat |
| O8 | Template backup CUT. Backup = config + roster + log | SIGNED ("let's cut template back-up") | Chat 2026-11-03 |
| O9 | Bulk enrollment: I choose the method | DELEGATED. Not built (C3). Options were A queue (recommended), B self-service (rejected), C per-student from phone | Chat 2026-11-03 |
| O10 | Fast enroll uses a queue of roster minus occupancy | DELEGATED. Proposed, not built | Chat |
| O11 | Verify scan after each enroll: default on or off | OPEN. Not answered | Chat |
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
| E21 | Adapter responsibilities (NVS, LittleFS, HTTP, mbedtls) are not built | OPEN. Next work | Chat |

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
