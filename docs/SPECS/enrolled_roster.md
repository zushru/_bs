# SPEC: enrolled_roster (lib/core/src/enrolled_roster.h/.cpp)

Status: DELEGATED occupancy/helper design. The core decoder is LSB-first under the current ID/page mapping; this matches the Adafruit CircuitPython reference implementation. The target module still needs a known-page hardware comparison. The user reported 196/196 native checks passed for v0.2.6 and 214/214 PlatformIO native tests passed for v0.2.8. v0.2.9 adds the attendance client and shared sensor-bus runtime; all 222 registrations passed in this session under a local stand-in Unity runner, not PlatformIO Unity.

## ID to sensor page
idToPage(id) = id - 1. The AS608 addresses pages 0..capacity-1, so ID 300 maps to page 299. The handoff text "ID = sensor slot, 1..C" is ambiguous; this mapping is the code's rule. UNVERIFIED on hardware beyond page 0 (see UNVERIFIED.md).

## Occupancy (which IDs have a template)
- 300-bit set, valid flag. Starts EMPTY AND INVALID.
- begin() clears and invalidates. addIndexPage(page, 32 bytes, capacity) sets bits: slot = page*256 + byte*8 + bit (LSB first). Slots >= capacity are ignored. The bit interpretation matches Adafruit CircuitPython Fingerprint `read_templates()` (source reviewed 2026-11-05); the upstream `main` branch is not pinned and the user's exact sensor remains unverified. finish() marks valid only after every needed page was added.
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
- `As608EnrollSensor::readIndexPage()` and `As608OccupancyReader` now implement a host-testable, asynchronous full refresh. They must not be used to enable production enrollment until Job 11 confirms the actual module's `0x1F` response and bitmap against known physical pages.
- The hardware-independent queue FSM is in `enroll_fsm.*` (see `docs/SPECS/enroll_fsm.md`). The global ownership scheduler, HTTP/UI, ESP32 build, and device validation are still missing. Duplicate-search coverage and sensor result mapping remain unverified.
