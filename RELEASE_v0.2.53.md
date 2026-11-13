# Release v0.2.53 — runtime-owned sensor lifecycle gates

Date: 2026-11-09  
Base: complete, verified v0.2.52 archive extraction  
Project root during development: /root/att  
Release status: development snapshot; C3 remains blocking

## Scope

This increment closes runtime-managed setup/scan service bypasses and the pre-startup enrollment reservation bypass. Callers could previously initialize or poll attendance outside the coordinator/pipeline, poll a pipeline-owned scan, and acquire the runtime enrollment session before the occupancy map was refreshed.

The runtime-owned attendance client is now constructed in runtime-managed mode. Its public beginSetup()/beginScan() refuse, and public stepSetup()/stepScan() return fatal without changing leases, polling the transport or sending UART data. As608StartupCoordinator can start and service setup through private methods only after enrollment-side authentication, capacity read and full occupancy refresh have completed and enrollment ownership has been released. As608AttendancePipeline alone can start and service runtime-owned scans through private methods, preserving the roster/admission/EV_SCAN path.

The public three-argument As608AttendanceClient and four-argument As608EnrollmentSession constructors remain available for isolated low-level use. Runtime-managed enrollment acquire is denied before READY; only the coordinator can acquire the private boot reservation. Once the full startup sequence succeeds, normal runtime enrollment acquisition is enabled. Raw enrollment FSM/sensor/reader getters are private to the coordinator. These restrictions do not stop callers from constructing separate standalone clients/sessions or parallel transports. The Runtime constructor also receives a mutable Occupancy& whose caller retains its own reference.

## Source and test changes

- lib/as608/src/as608_attendance_client.h
- lib/as608/src/as608_attendance_client.cpp
- lib/as608/src/as608_attendance_pipeline.cpp
- lib/as608/src/as608_enroll_sensor.h
- lib/as608/src/as608_enroll_sensor.cpp
- lib/hal_esp32/src/as608_runtime.h
- lib/hal_esp32/src/as608_runtime.cpp
- lib/hal_esp32/src/as608_startup.cpp
- test/test_as608_gate/test_main.cpp
- test/test_as608_pipeline/test_main.cpp

The gate regressions assert that runtime-managed public attendance setup/scan entry and polling methods cannot acquire a lease, advance setup, poll scan state or emit UART bytes. The enrollment regression proves runtime.enrollment().acquire() is refused before READY without UART activity, public abortPreparation() cannot release the startup-owned reservation, and normal acquisition/abort becomes available after READY. Raw runtime FSM/sensor/reader getters are private to the startup coordinator. The pipeline suite proves an external raw stepScan() cannot steal or poll a scan started by the checked pipeline.

The pipeline test fixture and its standalone unresolved-page case now drive the full As608StartupCoordinator sequence with synthetic UART ACKs. They no longer manually set up the runtime-owned attendance client.

## Verification evidence

Test-first regressions (deliberate expected failures, not release passes):
- `test_as608_gate`: public `runtime.attendance().beginSetup()` was accepted before startup on v0.2.52; a later regression showed public `stepSetup()` could service coordinator-owned setup; the enrollment-session regressions showed pre-READY `acquire()` and public `abortPreparation()` could take/release the boot reservation. Each expected failure was observed before its corresponding guard.
- `test_as608_pipeline`: public `stepScan()` polled a scan started by the checked pipeline before the private service path was added.
- The remote runner reported the expected single assertion failure, then emitted SIGHUP in those test-first runs. Those nonzero runs are defect reproductions only.

Final targeted tests:
- `pio test -e native -f 'test_as608_gate'` — 33/33 passed; exit 0; 20.484 seconds.
- `pio test -e native -f 'test_as608_pipeline'` — 8/8 passed; exit 0; 18.477 seconds.
- Combined focused result: 41/41.

Inventory:
- 548 C++ RUN_TEST registrations across 34 test suites.
- 41 registrations ran in this increment; 507 did not run.
- The inventory was counted from test/*.cpp source registrations, not inferred from the focused suite output.

## Explicitly not verified

- No full native regression.
- No SF2 Node suite or JavaScript syntax suite; those sources are unchanged.
- No v0.2.53 ESP32 compile/link or final-ELF inspection. The v0.2.52 smoke build/ELF result predates this release's constructor/setup-authority source changes and must not be presented as evidence for v0.2.53.
- No flash, NVS erase/write, physical sensor/UART operation, Job 11, OLED, Wi-Fi, RTC, EEPROM, power loss, brownout or soak test.
- No production firmware root, scheduler or HTTP/UI integration exists.

## Residual C3 blockers

This change is intentionally narrower than a complete startup/lifecycle boundary:
1. Runtime-managed enrollment acquisition is gated until READY; the public session cannot abort the boot reservation while startup is pending, and raw FSM/sensor/reader getters are private to startup. However, no production enrollment service/pipeline is implemented, so the feature is not yet exposed as a supported post-READY workflow.
2. The standalone three-argument As608AttendanceClient and four-argument As608EnrollmentSession constructors still permit low-level operations when paired with independently created transports/arbiters.
3. Runtime-managed raw scan methods are now restricted to As608AttendancePipeline, but the standalone three-argument As608AttendanceClient constructor still permits direct setup/scan outside the checked event path. Production must not expose independently constructed clients or parallel transports.
4. The constructor caller retains the original mutable Occupancy& and can manipulate it outside the runtime. There is no production composition root or serialized scheduler to prevent parallel transports or guarantee that every raw sensor operation uses the one runtime.

A production owner must encapsulate these resources, expose only approved lifecycle operations after startup READY, and serialize enrollment, scan pipeline, configuration and closure work. C3 remains blocking.

E18 remains DELEGATED. No student-ID/page mapping was introduced. U5 physical index command/payload/bit-order semantics remain unverified. HANDOFF_FINAL.md and docs/DECISIONS.md were not changed.
