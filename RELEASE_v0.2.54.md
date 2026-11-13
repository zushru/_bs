# Release v0.2.54 — owned occupancy and attendance-only composition

Date: 2026-11-09  
Base: complete, verified v0.2.53 archive extraction  
Project root during development: /root/att  
Release status: development snapshot; C3 remains blocking

## Scope

This increment hardens Runtime API ownership and adds the first reusable attendance-only composition:

1. The preferred As608Runtime constructor now owns the Occupancy map. Public occupancy, enrollment-session and attendance-client accessors return const views only.
2. As608AttendancePipeline has a Runtime-bound constructor and is a friend of Runtime. As608StartupCoordinator alone can service setup, and the pipeline alone can service scans; callers no longer need to extract the raw attendance client.
3. As608AttendanceOwner privately composes one As608Runtime, As608StartupCoordinator and As608AttendancePipeline. It exposes startup, read-only status and checked attendance-pipeline operations, not the underlying runtime/client/link/FSM/session/sensor/reader.
4. The inert ESP32 adapter-link source now uses the preferred owned-map runtime constructor.

The owner is reusable composition, not production firmware. There is still no production main.cpp, scheduler, HTTP adapter or admin UI. Enrollment is intentionally not exposed pending resolution of E18/U5 and a reviewed enrollment workflow.

## Implementation details

### Runtime ownership

- Added As608Runtime(Stream&, RosterStore&, uint32_t sensorPassword), which constructs and owns the occupancy map used by the enrollment FSM and occupancy reader.
- Retained As608Runtime(Stream&, RosterStore&, Occupancy&, uint32_t) for low-level tests/adapters. Its caller still retains a mutable reference, so production should use the owned-map constructor.
- Public occupancy() returns const Occupancy&.
- Public enrollment() returns const As608EnrollmentSession&. Ordinary callers can inspect available()/held() but cannot invoke acquire(), sync() or abortPreparation() on the runtime-managed session.
- Public attendance() returns const As608AttendanceClient&. Its state can be inspected, but beginSetup(), stepSetup(), beginScan() and stepScan() are not callable through the Runtime accessor.
- Mutable occupancy access, raw enrollment sensor/reader/FSM access and enrollment-session servicing remain private to As608StartupCoordinator.
- As608AttendanceClient::invalidateReady() is now private to the startup coordinator.

### Source and test changes

- lib/hal_esp32/src/as608_runtime.h/.cpp — owned-map constructor, const-only occupancy/enrollment/attendance views and private friend access.
- lib/as608/src/as608_enroll_sensor.h/.cpp — read-only enrollment-session availability status.
- lib/as608/src/as608_attendance_client.h — restrict ready invalidation to startup coordinator.
- lib/hal_esp32/src/as608_startup.cpp — startup uses private Runtime access.
- lib/as608/src/as608_attendance_pipeline.h/.cpp — Runtime-bound constructor; pipeline obtains the private attendance client without exposing it to callers.
- lib/hal_esp32/src/as608_attendance_owner.h/.cpp — attendance-only composition.
- target/esp32_adapter_link/main.cpp — owned-map Runtime constructor.
- test/test_as608_gate/test_main.cpp — const-view compile-time assertions and owned-map/startup tests.
- test/test_as608_pipeline/test_main.cpp — Runtime-bound pipeline fixture and owner success/fault integration tests.

### Attendance-only composition

The Runtime-bound `As608AttendancePipeline` constructor obtains the attendance client through a private friend accessor, so callers no longer need a mutable `Runtime::attendance()` reference. `As608Runtime::attendance()` returns a const status-only view. The startup coordinator retains private setup access, and only the pipeline has private scan-service access.

Added:
- lib/hal_esp32/src/as608_attendance_owner.h
- lib/hal_esp32/src/as608_attendance_owner.cpp

As608AttendanceOwner privately owns:
- As608Runtime using its owned-occupancy constructor;
- As608StartupCoordinator;
- As608AttendancePipeline.

Its public API is limited to:
- beginStartup()/stepStartup() and startup READY/fault/capacity status;
- beginScan(epoch)/stepScan() and scan-active status;
- const occupancy and transport-fault views;
- existing pipeline config-after-commit forwarding.

It does not expose raw runtime, attendance client, enrollment session/FSM, UART link, sensor adapter or occupancy reader. It intentionally has no enrollment operation. The external LogStore, RosterStore, IAs608PageResolver and IAttendanceAdmission supplied to the constructor are non-owning dependencies and must outlive the owner. The log must be open and its admission observer attached before scans are expected to be admitted.

The application must instantiate exactly one owner per physical sensor and service it from one serialized loop. This class does not enforce a global singleton, prevent unrelated code from constructing a standalone low-level client/transport, or add an RTOS mutex. Its config-after-commit methods preserve the existing caller contract: commit durable configuration first, update admission/pipeline second, then update ClosureEngine before further processing. The owner does not update ClosureEngine itself.

### Target source

target/esp32_adapter_link/main.cpp now uses As608Runtime(Serial2, sensorSmokeRoster, password) and no longer creates an external static Occupancy. The smoke application remains inert and must never be flashed.

## Test-first evidence

- Test-first compile-time API checks showed that v0.2.53 exposed mutable occupancy/enrollment/attendance references. The initial gate compile failed on occupancy/enrollment; after those accessors were changed, the added attendance const-view assertion failed against the intermediate tree, proving the raw attendance client was still mutable. All three assertions compile on final source.
- The attendance-owner test was added before its header/implementation. The pipeline suite failed compilation because as608_attendance_owner.h did not yet exist.
- After implementation, the new owner success test verifies scan rejection before READY, full synthetic startup, a checked page resolution, and durable EV_SCAN append through the pipeline.
- The owner fault test injects a non-OK authentication ACK and verifies startup fault, invalid occupancy, scan rejection and terminal owner behavior.

## Final targeted verification

Exact commands and final results:

- `./tools/verify_v0.2.54.sh targeted` — exit 0; `TARGETED_V0_2_54=PASS`; 590.06 seconds total.
- Within the verifier: `pio test -e native -f 'test_as608_gate'` — 34/34 passed; 247.18 seconds.
- Within the verifier: `pio test -e native -f 'test_as608_pipeline'` — 10/10 passed; 257.67 seconds.
- Combined targeted result: 44/44.

Static inventory:
- 551 C++ RUN_TEST registrations across 34 native suites.
- 44 registrations ran in this increment; 507 did not run.

## Explicitly not verified

- No full native regression.
- No SF2 Node suite or JavaScript syntax run; SF2 source is unchanged.
- No v0.2.54 ESP32 compile/link, final ELF inspection or NVS wrapper routing inspection. The v0.2.52 smoke build predates this source. [UNVERIFIED: build the inert ESP32 adapter-link target against the complete v0.2.54 tree and inspect its exact final ELF.]
- No flash, NVS erase/write, physical sensor/UART operation, Job 11, OLED, Wi-Fi, RTC, EEPROM, power loss, brownout or soak test.
- No production main.cpp, scheduler, HTTP routes, admin UI, reporting integration or enrollment service.

## Residual C3 blockers

1. As608AttendanceOwner is not instantiated by production firmware. A reusable type does not prove the application uses it.
2. The owner cannot enforce global singleton construction. Unrelated code can still construct multiple owners, standalone As608AttendanceClient/As608EnrollmentSession instances, or parallel StreamAs608Link/arbiter objects.
3. The legacy injected-map As608Runtime constructor remains public for low-level tests/adapters and leaves its caller with a mutable Occupancy reference. Production must use the owned-map constructor.
4. The owner requires non-owning LogStore, RosterStore, resolver and admission dependencies to outlive it; the production root must establish and test that lifetime ordering.
5. Enrollment is not exposed. E18 remains DELEGATED and U5 physical ReadIndexTable semantics remain unverified. Do not choose an ID/page mapping or build a production enrollment service by guessing.
6. No production serialized scheduler currently coordinates attendance with closure, config changes, storage, HTTP response lifetime or other sensor work. The owner documents the ordering contract but cannot enforce work outside its API.

C3 remains blocking. E18 remains DELEGATED. No student-ID/page mapping was introduced. HANDOFF_FINAL.md and docs/DECISIONS.md were not changed.
