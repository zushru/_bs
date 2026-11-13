# Specialized audit — v0.2.53

## Audit scope

- Whether the attendance client owned by As608Runtime can be manually initialized before the startup coordinator finishes occupancy refresh.
- Whether startup still reaches attendance setup after the occupancy refresh.
- Whether the pipeline test actually exercises coordinator startup, rather than setting up the client directly.
- Whether the new API accidentally claims to close independent-client, enrollment or raw-scan bypasses.
- Release evidence integrity and unchanged delegated decisions.

## Findings

### 1. Runtime-managed attendance and enrollment gates

As608Runtime constructs its As608AttendanceClient using a private four-argument constructor with runtimeManaged=true. Its public three-argument constructor delegates with runtimeManaged=false and remains available for isolated low-level use. As608Runtime also constructs As608EnrollmentSession through its private runtime-managed constructor; the public four-argument session constructor remains an independent low-level path.

For the runtime-owned instance:
- Public beginSetup() and beginScan() return false.
- Public stepSetup() and stepScan() return fatal; stepScan() also resets page/score outputs.
- These public rejections happen before lease changes, transport polling or UART transmission.
- Only As608StartupCoordinator can call the private setup-start/setup-step methods.
- Only As608AttendancePipeline can call the private scan-start/scan-step methods.
- Startup uses its private path only after enrollment-owner authentication, capacity read, complete occupancy refresh and enrollment-reservation release.
- `As608StartupCoordinator::step()` explicitly returns READY/FAULT in terminal states; the gate test polls once more after READY and confirms occupancy and attendance readiness remain intact.

This is a narrow runtime-composition boundary. Ordinary callers holding runtime.attendance() cannot independently start/setup/poll attendance or steal a pipeline scan. Runtime enrollment acquire() refuses until startup READY. Public abortPreparation() cannot release the boot reservation while startup is pending; the coordinator uses a private startup-only abort path. After READY, normal runtime session acquire/abort becomes available. Raw enrollment sensor/reader/FSM getters are private to As608StartupCoordinator. Standalone three-argument attendance clients and four-argument enrollment sessions remain unrestricted low-level paths.

### 2. Test-first behavior is demonstrated

Test-first gate regressions showed runtime.attendance().beginSetup() was accepted before startup, public stepSetup() could advance coordinator-owned setup, runtime.enrollment().acquire() was accepted before READY, and public abortPreparation() could release the boot reservation. The pipeline regression showed public stepScan() could poll a scan started by the pipeline. Each behavior was reproduced before its corresponding guard. Final gate suite passed 33/33.

The existing runtime/arbiter test was adjusted to prove the expected private coordinator path still begins startup with enrollment-owner authentication, rather than assuming public attendance setup remains possible.

### 3. Pipeline tests now exercise startup

The pipeline test fixture now constructs As608StartupCoordinator and drives synthetic UART ACKs through:
1. enrollment-owner authentication;
2. enrollment capacity read;
3. occupancy index refresh;
4. attendance-owner authentication;
5. attendance capacity read and READY.

The fixture no longer manually initializes the runtime-owned attendance client. The unresolved-page test also uses the coordinator. The Present pipeline test additionally calls the public raw stepScan() while a pipeline scan is active and verifies that the call returns fatal without changing the last UART instruction or consuming the pipeline operation. All UART replies and storage remain synthetic.

### 4. Critical residual bypasses — C3 remains open

This release does not prove mandatory startup for every possible caller:
- The standalone three-argument As608AttendanceClient and four-argument As608EnrollmentSession constructors remain public and allow independent low-level control with separately created links/arbiters.
- The caller constructing As608Runtime retains the original mutable Occupancy& passed to its constructor and can mutate that map outside the runtime; production must own it privately.
- Runtime-managed enrollment acquire/boot-abort are gated correctly, and raw FSM/sensor/reader getters are private, but there is no production enrollment service/pipeline that safely exposes post-READY enrollment operations.
- The application still has no production composition root or scheduler. There is no firmware owner that hides parallel transports and serializes all sensor calls.

The next structural requirement is an encapsulating production owner. It should expose enrollment and attendance only after startup READY, route attendance solely through the checked event pipeline, prevent second links/arbiters, and serialize config/closure with sensor work. A production composition test must prove pending/faulted startup blocks every entry point.

### 5. Delegated sensor semantics remain untouched

No decision ledger or handoff entry changed. E18 remains DELEGATED. The implementation does not add a page-to-student mapping. The physical 0x1F command, 32-byte payload, bit order, page numbering, Job 5/9 discrepancy, ID 300 and high-page behavior remain unverified. The tests use synthetic UART frames and synthetic occupancy bits.

### 6. Verification

- Test-first reproductions: the gate suite failed on pre-READY enrollment acquire and public boot-reservation abort; earlier gate regressions reproduced setup servicing bypass. The pipeline suite reproduced public scan-step polling. These were deliberate expected failures, not release passes.
- Final gate: `pio test -e native -f 'test_as608_gate'` — 33/33 passed; exit 0.
- Final pipeline: `pio test -e native -f 'test_as608_pipeline'` — 8/8 passed; exit 0.
- Focused total: 41/41.
- Static inventory: 548 C++ registrations across 34 suites; 507 registrations were not run.
- Full native regression, SF2 Node tests, ESP32 target build, final ELF inspection and hardware verification were not run.
- v0.2.52 target compile/link evidence predates the v0.2.53 source and is not carried forward as a final-source pass.

## Product and release safety

- HANDOFF_FINAL.md unchanged.
- docs/DECISIONS.md unchanged; E18 remains DELEGATED.
- No flash, erase, destructive sensor operation or NVS mutation performed.
- C3 remains blocking; this release is not production firmware or a verified hardware build.
