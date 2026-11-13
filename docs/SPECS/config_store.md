# SPEC: config_store (lib/core/src/config_store.h/.cpp)

Status: DELEGATED design (you said "your judgement"). Host tests: test_config (15 registrations; current E26 additions are NOT RUN). Historical device-run claims below are not current verification.

## Config fields
| Field | Type | Default | Rule |
|---|---|---|---|
| params.utcOffsetMin | int16 | 480 (PHT) | -720..840 |
| params.lateMin | uint16 | 30 | <= 240 and < length of every enabled slot |
| params.scanLeadMin | uint16 | 30 | <= 60; remains configurable under the existing delegated config design |
| tt | Timetable (5 days x 2 Slot{start,end,enabled}) | defaultTimetable() = 08:00-12:30, 13:30-16:30 | validateTimetable(tt, scanLeadMin) |
| termStart, termEnd | int32 day numbers (dayNumber, time_rules.h) | -1 (unset) | `0 <= termStart <= termEnd <= 31446`; both endpoints inclusive; one-day term valid |
| autoSync | uint8 | 1 | 0 or 1 |
| soundMode | uint8 | 0 (Full) | 0..2 (Full, Quiet, Silent) |
| minScore | uint16 | 0 | placeholder: 0 = sensor security level decides. OPEN until bench data |
| pinSet, pinHash[32], pinSalt[8] | | 0 | pinSet == 1 and hash not all zero. Hashing is the adapter's job |
| wifiPass[64] | char | empty | 8..63 printable ASCII, NUL-terminated, zero-padded after the NUL |

defaultConfig() is a wizard starting point. It is NOT valid and is never a fallback.

Stored schema-v1 values equal to 15 are normalized to 30 during decode as the former product default. Other explicitly configured values are preserved. Schema v1 has no bit distinguishing a user-selected 15 from the old default; therefore this migration treats every stored 15 as the old default. The normalized configuration is still validated against the 30-minute schedule window, so custom timetables with a start-before-30 or a too-short inter-session gap fail closed and require schedule review.

## Slot format (192 bytes, erased flash = 0xFF)
- Header 11 bytes: "ATTC", schemaVer=1, payloadLen u16 = 173, seq u32 (little-endian).
- Payload 173 bytes: params 0..5, timetable 6..55 (10 slots x 5 bytes), termStart 56, termEnd 60, autoSync 64, soundMode 65, minScore 66..67, pinSet 68, pinHash 69..100, pinSalt 101..108, wifiPass 109..172.
- CRC-16/CCITT-FALSE over bytes 0..183, stored at 184..185. Bytes 186..191 are 0xFF.

## Decode order
blank (all 0xFF) -> CD_BLANK; magic -> CD_BAD_MAGIC; version -> CD_BAD_VER; length -> CD_BAD_RANGE; CRC -> CD_BAD_CRC; field ranges and validateConfig -> CD_BAD_RANGE. Only CD_OK fills the output.

## Slot selection: pickSlot(a, b)
| Slots | Result |
|---|---|
| both blank | CB_UNPROVISIONED (not a fault) |
| both valid, same seq, same content | CB_OK, slot A |
| both valid, same seq, different content | CB_FAULT |
| both valid, different seq | higher seq wins |
| one valid, the other blank or bad | the valid one |
| neither valid, at least one not blank | CB_FAULT, output untouched |

## Write procedure (adapter must follow)
Write to the non-active slot with seq+1, read it back, compare with configEqual, and only then report success. A torn write leaves the old slot active.

## Schedule-change gate: canApply(cur, next, windowOpen, unclosedPast)
Schedule fields = utcOffsetMin, lateMin, scanLeadMin, termStart, termEnd, every timetable slot. Same schedule -> OK. Different schedule -> REFUSED_WINDOW_OPEN if a window is open, REFUSED_UNCLOSED if a past session is unclosed, else OK. `ClosureEngine` consumes `utcOffsetMin` and `scanLeadMin` from Params; it does not use `lateMin`, which belongs to scan classification.

## Known gaps
- C1 core support: `ClosureEngine::setSchedule(params, timetable, termStartDay, termEndDay)` updates the existing engine without replacing its `LogIndex` or clearing runtime state. The caller must invoke it only after validation, `canApply()`, and successful durable config commit, and must serialize the call with engine operations. The v0.2.13 ESP32 NVS slot adapter and first-provision path now exist as standalone source, but no production boot caller or HealthFsm fault wiring exists; end-to-end schedule application remains pending.
- seq wraps at 2^32 with no handling (about 4 billion writes; not a practical risk, noted only).
- The two booleans in canApply are supplied by the caller. A wrong value silently bypasses the guard. v0.2.16 adds a current-window helper, but C17 blocks safe historical `unclosedPast` derivation until schedule-effective history is persisted (E2 is DELEGATED). E26 now selects an inclusive `termEnd` and the source bounds closure to `0 <= termStart <= termEnd <= 31446`; regressions are authored but NOT RUN, and future day-view routes must call `apiDayInInclusiveTerm()`. Keep production schedule changes disabled until C17/E2 and current verification are resolved.
