# SPEC: roster_store (lib/core/src/roster_store.h/.cpp)

Status: DELEGATED design. Host tests: test_roster (9). Device: run once.

## Record format (event-sourced from the log)
- Create or rename: EV_ROSTER(a = id, b = RA_SET=1, FL_MORE) then four EV_PAYLOAD records. Payloads 1-3 carry FL_MORE, payload 4 does not. Together they hold the name (32 bytes, zero-padded, 8 bytes per payload, little-endian).
- Deactivate: EV_ROSTER(a = id, b = RA_DEACTIVATE=2), no FL_MORE.
- Activate: EV_ROSTER(a = id, b = RA_ACTIVATE=3), no FL_MORE.
- IDs 1..300 (MAX_ID). Day = dayNumber(epoch, utcOffset).

## Name rules (validName)
1..32 bytes, valid UTF-8 (no overlong, no surrogates, max U+10FFFF), no control characters, no leading or trailing space. Replay applies the same rules and rejects a non-zero byte after the NUL.

## Replay strictness (fail-closed)
A malformed group or record increments malformed() and changes nothing. Rejections: wrong FL_MORE pattern, a group cut short by another record, invalid name, bad ID, bad day, unknown action, DEACTIVATE/ACTIVATE on an unknown ID. A stray EV_PAYLOAD outside a group is ignored.

## Expectation intervals
- A SET that creates a student opens an interval at that day. ACTIVATE opens one. DEACTIVATE closes the open one at its day.
- expected(id, day) is true for day in [from, to). A student is not expected on their deactivation day.
- Renaming does not move the start day.
- Interval table capped at 512. On overflow, overflow() = true and expected() returns false for everyone. Planning refuses before overflow (RE_FULL).

## Identity
known(id) = ever created. IDs are never reused. Deactivated students stay known and can still be corrected.

## Plan functions
planSet / planDeactivate / planActivate return events only. The store task appends them. RE_NOOP = no change needed (same name, already inactive or already active).

## Known gaps
- The roster is not stored separately. Restore relies on replaying the log. Cost of replaying a large log at boot: UNMEASURED.
