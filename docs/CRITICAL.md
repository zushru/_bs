# CRITICAL ISSUES (read before building anything)

Ranked by risk to attendance data or security. "Blocking" = the feature must not ship until fixed.

| # | Issue | Evidence | Impact | Needed | Blocking |
|---|---|---|---|---|---|
| C1 | A config schedule change never reaches the running ClosureEngine | closure_engine.h stores `Params p_` and `Timetable t_` by value; no setter exists | Gate accepts a schedule change when no window is open, but the engine keeps the old schedule until rebuilt. Closures could use stale times | Rebuild the engine after every schedule change, or add a setter. Write a test that changes lateMin then closes a day | Yes, for config |
| C2 | Restore is incomplete | RestoreValidator stages and checks only. No orchestrator, no atomic swap, no LogIndex or closure replay of the staged log | A backup can pass every check and still leave a bad log or index | Orchestrator: stage, replay through index and roster, atomic swap, reboot, ack. Failure tests for each step | Yes, for restore |
| C3 | Enrollment is not built | No enroll_fsm. Only enrolled_roster helpers and gateEnroll | Cannot enroll 45 students quickly. No duplicate check. No retry on sensor errors | Write enroll_fsm: queue, duplicate search, retries, optional verify scan, occupancy refresh | Yes, for setup |
| C4 | Duplicate-finger threshold and minScore have no data | Scores are never logged. minScore is a 0 placeholder | Duplicates possible. Match strictness is the sensor default only | Bench: enroll two people, log scores, pick the threshold | Yes, for enroll |
| C5 | Diagnostic sketch leaves a duplicate template | Job 9 stores into page 2 and never deletes. No delete command exists | The sensor holds the test finger under ID 2 too. Dangerous if that sensor is reused | Empty the sensor before any reuse. Add a delete job to the diagnostic | No (diagnostic only), but do it |
| C6 | Security duties are the caller's, not enforced | api_logic header comments only. Setup-code lockout, PIN change needs the old PIN, persist-before-verify | Brute force of the 6-digit setup code over Wi-Fi if the adapter misses it. A stolen token could take over the admin PIN | Adapter tests that exercise each duty. Decide GET /status exposure | Yes, for the API |
| C7 | Test evidence is incomplete | 149 host tests with a stand-in Unity and a toy cipher. Device last ran 142 (before the template cut and test_enroll) | Confidence is overstated if read as "done" | Run `pio test -e native` on the device. Record the number in TESTS.md | Yes, before merge |
| C8 | No soak, no brownout data | 10-hour soak not run. No BROWNOUT boot observed in the logs so far | Power policy (always-on) rests on estimates. Bank runtime unknown | Run the soak. Pull the bank and log boot lines | Yes, for power |
| C9 | Spec gaps and a handoff error | Handoff has no rule for marks on future days (the earlier "conflict" was wrong: it is silent). Handoff says "ID = sensor slot 1..C"; code uses page = ID-1 | Future-day behaviour undefined. Off-by-one at ID 300 if the handoff is followed literally | You decide the future-day rule. Correct the handoff text | Yes, for marks |
| C10 | OTA conflict | RESOLVED: you chose USB. The handoff says USB only, so OTA is out of scope | None | Closed | No |
| C11 | Decisions are reconstructed | Memory was unavailable for much of the chat. DECISIONS.md was written from the conversation, not from a verified store | A decision may be missing or misattributed | Review DECISIONS.md. Mark what is wrong | Yes, before merge |
| C12 | KDF iteration count and cipher speed unknown | No count chosen. mbedtls on the ESP32 unmeasured | Restore may take minutes, or the work factor may be too weak | Choose a count after timing it on the device | Yes, for backup |
| C13 | LittleFS behaviour unverified | Restore and quarantine assume atomic rename, copy and truncate across power loss | Silent loss or corruption after a power cut during restore or repair | Power-cut test on the device, with a log comparison | Yes, for restore |

| C14 | Install state drift (cause of your last test error) | Your error matches the handoff log_store.h (makeGapEvents with 5 arguments, no appendGaps). Your working tree still held the baseline Core B file, so the Core B fix was never applied there. Earlier bundles needed a separate apply.sh step, and partial installs were easy to make | test_hardening fails to compile, and any other suite that depends on Core B fixes runs against the wrong code | Replace the whole att/ tree from r2 (one archive, no apply step). Do not merge pieces | Yes, until your tree matches r2 |

## Corrections to earlier chat claims
- "Handoff future-day rule conflicts with no-future-marks": wrong. The handoff is silent. Recorded as a gap in C9.
- "Restore replay through the closure engine is written": false. Only the observer hook and the roster semantic check exist (C2).
- "Template backup is viable": measured for one sensor only. It is now cut, so the claim no longer matters.
