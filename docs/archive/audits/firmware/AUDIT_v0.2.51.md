# Specialized audit — v0.2.51

## Scope reviewed

- New attendance scan-to-log integration test suite.
- Whether tests exercise the actual shared As608Runtime composition root.
- Duplicate suppression and durable event failure behavior.
- Preservation of delegated E18 mapping and all existing product decisions.
- Targeted verification workflow, executable script modes and release archive hygiene.

## Findings

### 1. Missing integration test coverage — addressed at host-logic level

Before this increment, the sensor client/owner lease and log-backed admission were tested separately, but no dedicated test exercised a completed sensor match through As608AttendancePipeline into a durable EV_SCAN. The new six-case suite now wires As608Runtime, LogAttendanceAdmission, RosterStore, LogStore, and As608AttendancePipeline together. It asserts event type, student ID, score, timestamp, session and status on the saved record.

The suite covers Present and Late calculation, duplicate suppression, unresolved page mapping, storage append failure, and no-finger handling. A failed append faults the log and does not report the mark as saved. The unresolved resolver path does not append a scan.

### 2. Test boundary — no product mapping was invented

IAs608PageResolver remains injected. The fixture maps physical page 7 to ID 4 solely to test the rest of the pipeline. That fixture is not evidence for the production ID/page convention and does not amend E18. The successful event tests are a proof of injected-path logic, not evidence that a physical AS608 template maps to the correct learner.

### 3. Production integration remains incomplete

No production firmware scheduler, boot owner, event pipeline caller, HTTP route or admin UI was added. The new suite validates the composition and event append behavior using stand-in UART and storage. It does not prove actual sensor timing, UART electrical behavior, filesystem durability, RTC correctness, brownout handling or power-loss recovery. C3 remains open/blocking and E18 remains delegated.

### 4. Targeted verification — passed

tools/verify_v0.2.51.sh targeted completed with exit code 0. It ran shell syntax checks, 11 ELF-inspector fixtures and seven native suites (108/108): NVS policy 9, probe credential 4, AS608 adapter 15, AS608 gate 26, new attendance pipeline 6, log admission 12 and enrollment 36. Static source inventory is 539 registrations across 34 suites; 431 registrations were not run.

The first verifier attempt stopped before test execution because the extracted working tree lacked executable mode on tools/inspect_probe_wrappers.sh. The necessary executable bits on both inspector scripts were restored, the fixture set then passed, and the verifier completed successfully. The new verifier script is also executable. The archive must preserve these modes.

No ESP32 target was rebuilt: the increment changes only test code, verification script and documentation. The v0.2.50 compile/link and ELF-routing results remain historical evidence for unchanged target sources, not fresh v0.2.51 build evidence. The full native suite and SF2 tests were not run.

### 5. Product decisions and safety boundary — unchanged

- HANDOFF_FINAL.md unchanged.
- docs/DECISIONS.md unchanged; no SIGNED, DELEGATED or OPEN item was changed.
- No production source or operational attendance behavior changed.
- No device was flashed and no NVS corruption was induced.
- C3, C6 and C34 remain blocking; E18 is unresolved.

## Verification evidence

- Targeted native suites: 108/108 across seven suites.
- Inspector fixtures: 11/11.
- Verifier: TARGETED_V0_2_51=PASS, exit code 0.
- Full 539-case native suite: not run (431 registrations outside the targeted set).
- SF2 Node suite: not rerun; source unchanged.
- ESP32 target builds/ELF routing: not rerun; v0.2.50 results only.
- Physical device: not tested; no flash performed.
