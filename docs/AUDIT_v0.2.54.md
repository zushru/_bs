# Specialized audit — v0.2.54

## Audit scope

- Ownership and mutability of the occupancy map.
- Whether ordinary callers can access runtime enrollment-session mutation.
- Whether attendance can be used without passing through startup and the checked event pipeline when using the new owner.
- Owner dependency lifetime and config/closure ordering.
- Remaining paths to instantiate duplicate runtimes/owners/transports.
- Test coverage, target evidence and delegated decision integrity.

## Findings

### 1. Preferred runtime now owns occupancy

As608Runtime(Stream&, RosterStore&, password) constructs an internal Occupancy and wires the enrollment sensor's reader and FSM to that exact object. Public occupancy() returns const Occupancy&. Public attendance() returns const As608AttendanceClient&, and enrollment() returns const As608EnrollmentSession&. Only As608StartupCoordinator, via friendship/private mutable accessors, can reset/fill the map, operate the boot session and service attendance setup. As608AttendancePipeline receives private attendance-client access through a Runtime-bound constructor and is the only runtime friend allowed to start/service scans.

Compile-time static_asserts established the API defects in sequence: the initial gate compile against v0.2.53 failed because occupancy() and enrollment() returned mutable references; after those were changed, the new attendance() const-view assertion failed against the intermediate tree because that accessor still returned a mutable client. All three are const views in final source. A synthetic full-startup test verifies the internally owned map receives index-page bits and becomes valid before attendance setup can complete.

The injected-map constructor remains public for low-level tests/adapters. It still stores the caller's Occupancy reference; code using that constructor can mutate the map externally. This is not the preferred production API and is documented as such.

### 2. Runtime enrollment handle is now read-only

As608Runtime::enrollment() returns const As608EnrollmentSession&. Its public acquire(), sync() and abortPreparation() methods are non-const, so ordinary runtime callers cannot invoke them through this accessor. Read-only available() and held() remain observable. Startup owns the private session operations via friendship.

This is an API capability reduction, not a global access-control guarantee: a separately constructed standalone four-argument As608EnrollmentSession is still possible in unrelated code. A production root must not construct or expose an independent session for the same physical sensor.

### 3. Attendance owner encapsulates the normal attendance path

As608AttendanceOwner owns:
- one preferred owned-map As608Runtime;
- one As608StartupCoordinator bound to that runtime;
- one As608AttendancePipeline wired to runtime occupancy and attendance client.

It exposes startup begin/step, READY/fault/capacity status, read-only occupancy, checked scan begin/step, scan-active status, transport-fault status and config-after-commit forwarding. It does not expose raw Runtime, AttendanceClient, EnrollmentSession, StreamAs608Link, Enrollment FSM, raw enrollment sensor or occupancy reader.

Its beginScan() requires startup READY. Its stepScan() returns ATT_PIPE_REJECTED unless startup is READY. Once ready, the scan is serviced only by the event pipeline, including its occupancy checks, injected page resolver, roster/admission checks, duplicate suppression and durable EV_SCAN append.

Test evidence:
- The successful owner integration test uses fake UART and memory storage. It rejects a scan before startup, drives the synthetic authentication/capacity/index/attendance-setup sequence, then maps a synthetic physical page to student ID 4 through the injected test resolver and commits one EV_SCAN.
- The owner fault test supplies a non-OK verify-password ACK. It verifies AS608_STARTUP_FAULT, startupFaulted=true, startupReady=false, invalid occupancy, rejected scan and inability to restart the failed owner.

The injected page resolver in the successful test is synthetic. This is not evidence for E18 or physical sensor page semantics.

### 4. Dependency lifetime and serialized-loop contract

As608AttendanceOwner stores references to LogStore, RosterStore, IAs608PageResolver and IAttendanceAdmission through the pipeline. The caller must construct those dependencies first and keep them alive until after the owner is destroyed. The log must be open and the admission observer attached before attendance scans are expected to be admitted. These lifetime constraints are documented but are not runtime-enforced.

The owner forwards the existing config-after-commit pipeline API. Correct application ordering remains: durable config commit, owner/pipeline+admission update, then ClosureEngine schedule update, all in one serialized operation before any scan step can run. The owner does not own or update ClosureEngine and cannot stop unrelated code from violating that ordering.

All owner methods must run from one serialized application loop. There is no mutex or RTOS synchronization.

### 5. C3 is not closed — duplicate owners/low-level bypasses remain possible

This reusable owner is not yet a complete production composition root:
- No production main.cpp, scheduler, HTTP route, admin UI or service lifecycle instantiates exactly one As608AttendanceOwner.
- The class has no global singleton enforcement. Two owners can be constructed over the same Stream, potentially creating independent arbiters/links for one sensor. Production must prevent this at its composition boundary.
- Public standalone low-level As608AttendanceClient and As608EnrollmentSession constructors remain available for unit tests/adapters. Unrelated code can create a bypassing transport/session.
- The legacy injected-map As608Runtime constructor remains public.
- The owner intentionally exposes no enrollment workflow. E18/U5 page mapping and physical ReadIndexTable semantics remain unresolved.
- It does not serialize HTTP config/closure/storage work outside its own call path.

Do not describe As608AttendanceOwner as production-integrated. It is a reusable, narrow attendance composition that still needs application adoption and a single-owner root.

### 6. Test evidence

- Test-first compile-time API assertions failed against v0.2.53 because occupancy/enrollment/attendance accessors were mutable. The first pipeline compilation after switching to Runtime-bound construction also checks the new private friend/accessor path.
- The initial owner integration test failed to compile because the owner header did not yet exist.
- Final packaged verifier: `./tools/verify_v0.2.54.sh targeted` — exit 0, `TARGETED_V0_2_54=PASS`; total 590.06 seconds.
- Gate within verifier: `pio test -e native -f 'test_as608_gate'` — 34/34 passed, 247.18 seconds.
- Pipeline within verifier: `pio test -e native -f 'test_as608_pipeline'` — 10/10 passed, 257.67 seconds.
- Focused total 44/44.
- Static inventory 551 C++ registrations across 34 suites; 507 not run.
- No full native suite, SF2 Node suite, final-source ESP32 compile/link, final ELF inspection, physical sensor or storage power-loss test.

### 7. Decision and release safety

- HANDOFF_FINAL.md unchanged.
- docs/DECISIONS.md unchanged; E18 remains DELEGATED.
- No page-to-student mapping added.
- No flash, NVS erase/write, destructive sensor operation or physical hardware test performed.
- C3 remains blocking. This is not production firmware or a verified hardware build.
