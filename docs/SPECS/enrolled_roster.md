# SPEC: enrolled_roster (lib/core/src/enrolled_roster.h/.cpp)

Status: DELEGATED design. Host tests: test_enroll (7). NOT run on the device yet.

## ID to sensor page
idToPage(id) = id - 1. The AS608 addresses pages 0..capacity-1, so ID 300 maps to page 299. The handoff text "ID = sensor slot, 1..C" is ambiguous; this mapping is the code's rule. UNVERIFIED on hardware beyond page 0 (see UNVERIFIED.md).

## Occupancy (which IDs have a template)
- 300-bit set, valid flag. Starts EMPTY AND INVALID.
- begin() clears and invalidates. addIndexPage(page, 32 bytes, capacity) sets bits: slot = page*256 + byte*8 + bit (LSB first, ASSUMED). Slots >= capacity are ignored. finish() marks valid, only after every needed page was added.
- pagesNeeded(capacity) = ceil(capacity / 256).

## EnrolledRoster : IRoster
expected(id, day) = occupancy.valid AND template present AND RosterStore.expected(id, day). Holds references, so a refresh takes effect at once.
Rule: a student without a template is never expected. A failed or unfinished sensor index read excludes everybody. This prevents false Absent after a restore onto a blank or swapped sensor.

## Reconcile (diagnostic, read-only)
Counts: matched (active with template), orphans (active without template, first id), strays (template with no known student, first id). Known and inactive students are not strays.

## Queue and slot helpers
nextToEnroll(roster, occupancy, after) = next active student without a template, after the given id, no wraparound. firstFreeSlot(roster, occupancy, capacity) = lowest id <= capacity that is neither known nor occupied (IDs are never reused).

## Known gaps
- The adapter must call begin() / addIndexPage() / finish() before every closure tick that needs it. Nothing in the core forces this. A stale valid occupancy keeps old answers.
- ReadIndexTable command, its page size and bit order are UNVERIFIED on the AS608.
- Enrollment state machine (enroll_fsm) is NOT written. Duplicate search and retries are not implemented.
