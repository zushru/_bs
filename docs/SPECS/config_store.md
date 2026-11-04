# SPEC: config_store (lib/core/src/config_store.h/.cpp)

Status: DELEGATED design (you said "your judgement"). Host tests: test_config (12). Device: included in your 77-case run and every later full run (the suite has not changed since).

## Config fields
| Field | Type | Default | Rule |
|---|---|---|---|
| params.utcOffsetMin | int16 | 480 (PHT) | -720..840 |
| params.lateMin | uint16 | 30 | <= 240 and < length of every enabled slot |
| params.scanLeadMin | uint16 | 15 | <= 60 |
| tt | Timetable (5 days x 2 Slot{start,end,enabled}) | defaultTimetable() = 08:00-12:30, 13:30-16:30 | validateTimetable(tt, scanLeadMin) |
| termStart, termEnd | int32 day numbers (dayNumber, time_rules.h) | -1 (unset) | termStart >= 0, termEnd > termStart |
| autoSync | uint8 | 1 | 0 or 1 |
| soundMode | uint8 | 0 (Full) | 0..2 (Full, Quiet, Silent) |
| minScore | uint16 | 0 | placeholder: 0 = sensor security level decides. OPEN until bench data |
| pinSet, pinHash[32], pinSalt[8] | | 0 | pinSet == 1 and hash not all zero. Hashing is the adapter's job |
| wifiPass[64] | char | empty | 8..63 printable ASCII, NUL-terminated, zero-padded after the NUL |

defaultConfig() is a wizard starting point. It is NOT valid and is never a fallback.

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
Schedule fields = utcOffsetMin, lateMin, scanLeadMin, termStart, termEnd, every timetable slot. Same schedule -> OK. Different schedule -> REFUSED_WINDOW_OPEN if a window is open, REFUSED_UNCLOSED if a past session is unclosed, else OK.

## Known gaps
- C1 (CRITICAL.md): the ClosureEngine does not receive a changed schedule. This spec's gate is necessary but not sufficient.
- seq wraps at 2^32 with no handling (about 4 billion writes; not a practical risk, noted only).
- The two booleans in canApply are supplied by the caller. A wrong value silently bypasses the guard.
