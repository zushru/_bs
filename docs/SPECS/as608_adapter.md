# SPEC: AS608 adapter, shared ownership, and sensor-bus startup (v0.2.55)

Status: raw enrollment and attendance command adapters, full occupancy reader, shared arbiter, attendance scan lease, enrollment lifecycle lease and sensor-bus composition root are host-tested. They have not been built on the user's ESP32 or verified against the physical sensor.

## Components

- `lib/as608/src/as608_enroll_sensor.*`: raw AS608 command packet builder/parser, enrollment-facing adapter and fail-closed occupancy reader. `As608EnrollSensor` accepts a command owner; it defaults to `AS608_OWNER_ENROLLMENT`.
- `lib/as608/src/as608_attendance_client.*`: non-blocking attendance setup and scan sequence. It constructs the raw adapter with `AS608_OWNER_ATTENDANCE`. Setup authenticates and reads capacity. A scan performs capture (`0x01`), convert (`0x02`, buffer 1), and full-library search (`0x04`). It returns a physical page and score; it does not map to student ID or write an attendance event.
- `lib/as608/src/as608_sensor_arbiter.*`: shared attendance/enrollment command arbiter, attendance scan lease and enrollment lifecycle lease.
- `lib/hal_esp32/src/stream_as608_link.*`: owner-tagged bounded-RX `Stream` transport; TX uses synchronous `Stream::write()` and may block.
- `lib/hal_esp32/src/as608_runtime.*`: low-level sensor-bus composition. It creates one `As608SensorArbiter` and one `StreamAs608Link`, then constructs all attendance/enrollment objects against those same instances. Its preferred constructor owns the `Occupancy` map and exposes only `const Occupancy&`; only `As608StartupCoordinator` can obtain mutable access. A legacy injected-map constructor remains for low-level tests/adapters and retains the caller-owned mutable reference. It also owns the enrollment FSM.
- `lib/hal_esp32/src/as608_attendance_owner.*`: preferred reusable attendance-only composition. It owns one preferred runtime, startup coordinator and checked event pipeline and exposes no raw sensor or enrollment controls.
- `lib/hal_esp32/src/as608_owner_config_runtime.*`: `IConfigApplyRuntime` adapter connecting the preferred attendance owner to `ConfigApplyCoordinator`; applies owner pipeline/admission state, then the external `ClosureEngine` schedule after durable config-slot write and read-back verification.
- `lib/hal_esp32/src/as608_startup.*`: serialized startup coordinator. It authenticates as enrollment owner, reads capacity, refreshes all index pages under the enrollment reservation, releases that reservation only after a complete trusted refresh, authenticates as attendance owner, checks matching capacity, and reports READY only after both phases succeed. All failure paths invalidate occupancy; transport poison retains sensor ownership.
- `test/test_as608_gate/test_main.cpp`: 34 ownership, transport, enrollment session, runtime read-only API, attendance client, startup coordinator and composition cases.
- `test/test_as608_pipeline/test_main.cpp`: 11 event-pipeline, attendance-owner and owner-bound config-runtime cases, including owner startup fault, synthetic match-to-EV_SCAN and A/B config-apply composition.

The runtime is not the whole product composition root. `As608AttendanceOwner` now composes runtime + startup + checked event pipeline for attendance-only use, but no production `main.cpp` or scheduler instantiates it. This module does not implement HTTP/JSON routes, admin UI, OLED, buzzer, NVS or LittleFS drivers.

## Ownership and scheduling contract

- `As608AttendanceOwner` is the intended attendance-only production composition for one physical AS608. Instantiate it once in the application root; it internally creates one `As608Runtime`, one `StreamAs608Link` and one arbiter. The class cannot enforce global singleton construction, so do not instantiate a second owner or separate low-level link/client for the same sensor.
- The runtime wires `As608AttendanceClient`, `As608EnrollSensor`, `As608EnrollmentSession` and `StreamAs608Link` to the same arbiter/link instances. The transport rejects attendance-owner commands unless the attendance lifecycle lease is held; owner tags alone are not sufficient. Tests verify that a raw attendance command is denied without a lease, enrollment blocks attendance lease acquisition, and an attendance lease blocks enrollment acquisition.
- A raw attendance-owner transaction is rejected unless an attendance lifecycle lease is held. An attendance lease covers authentication + capacity setup, or one whole capture -> convert -> search scan. This prevents enrollment from acquiring the sensor between two command ACKs. On a normal terminal result (match, no-match, no-finger or recoverable error), the lease releases after the command has been consumed.
- If the Stream link becomes poisoned, the attendance client enters `ST_FAULT` and deliberately keeps its lease held. This fails closed; runtime reconstruction is not a recovery procedure unless the physical sensor and ESP32 have first been power-cycled.
- The enrollment session still acquires before authentication/capacity/index refresh and releases only at `EN_DONE`/`EN_CANCELLED`, with no pending command or active occupancy refresh. Setup abort is permitted only while the FSM is idle and all work is drained.
- The arbiter is single-thread/main-loop only. No mutex is provided. Do not call it from multiple tasks.

## Startup coordinator API

1. For the attendance-only production path, construct exactly one `As608AttendanceOwner`; it owns the Runtime, startup coordinator and checked pipeline. The lower-level coordinator API below is for tests/controlled compositions and must not be used to create a second transport for the same sensor.
2. Call `begin()` once after configuration/storage boot gates have succeeded. A false return means the startup reservation could not be acquired and no startup transaction was started.
3. Call `step()` from the serialized main loop until `AS608_STARTUP_READY` or `AS608_STARTUP_FAULT`. Do not permit scans or enrollment while startup is pending or faulted.
4. Startup refreshes occupancy from every required index page before allowing the attendance-owner authentication/capacity phase. The attendance setup is accepted only if its capacity equals the enrollment-side capacity.
5. A failed startup coordinator is terminal for that instance. If the link is poisoned, retain the sensor reservation and require a full power-cycle of the ESP32 and sensor before reconstructing the runtime. A non-poison protocol failure may release ownership safely, but the coordinator remains faulted.

**Runtime lifecycle boundary (v0.2.53):** `As608Runtime` constructs its attendance client in runtime-managed mode. Its public `beginSetup()`/`beginScan()` return false, and public `stepSetup()`/`stepScan()` return fatal without acquiring/polling the sensor or sending UART data. Only private methods callable by `As608StartupCoordinator` can advance setup after full occupancy refresh; only `As608AttendancePipeline` can start/service scans and reach the event commit path. Regressions prove the public methods cannot steal setup or scan transactions.

**Runtime ownership (v0.2.54):** the preferred `As608Runtime(Stream&, RosterStore&, password)` constructor owns its occupancy map and exposes only `const Occupancy&`. Its enrollment accessor returns `const As608EnrollmentSession&`, and its attendance accessor returns `const As608AttendanceClient&`, so callers can inspect state but cannot acquire/sync/abort enrollment or start/step the runtime-managed attendance client. The old injected-map constructor remains for low-level tests/adapters and retains its caller-owned mutable reference; production should not use it. Raw enrollment FSM/sensor/reader access remains private to startup.

**Residual integration limits:** standalone low-level `As608AttendanceClient` and `As608EnrollmentSession` constructors remain public, and unrelated code can construct parallel clients/transports or multiple runtime/owner objects. The v0.2.54 `As608AttendanceOwner` is the intended attendance composition. The Runtime public attendance accessor is const-only; only startup/pipeline friends can obtain its mutable client. The owner is not yet instantiated by a production main/scheduler; it adds no RTOS mutex or global singleton enforcement. It exposes no enrollment operation until E18 and the enrollment workflow are resolved.

## Attendance owner API (v0.2.55)

`As608AttendanceOwner` is the preferred attendance-only composition. It owns one runtime (which owns the map), startup coordinator and event pipeline, and exposes only `beginStartup()`/`stepStartup()` and READY/fault/capacity status; `beginScan(epoch)`/`stepScan()` and scan-active status; a read-only occupancy view, transport-fault status and the existing config-after-commit pipeline forwarding hooks.

It does not expose `As608Runtime`, `As608AttendanceClient`, `As608EnrollmentSession`, the raw `StreamAs608Link`, or enrollment FSM/sensor/reader objects. A caller must create the `LogStore`, `RosterStore`, page resolver and admission policy before the owner and keep all of them alive for the owner's lifetime. The log must be open and its admission observer attached before scans are admitted.

The owner must be instantiated once per physical sensor by the application root and serviced from one serialized loop. The class cannot enforce global singleton construction or prevent a second low-level transport from being created elsewhere. Its config hooks preserve the ordering contract: durable config commit first, then owner/pipeline+admission update, then `ClosureEngine::setSchedule()` before processing resumes. The owner itself does not update `ClosureEngine`; v0.2.55 supplies `As608OwnerConfigRuntime` for this bridge.

Enrollment is intentionally not exposed. E18 remains DELEGATED, U5's physical index semantics remain unverified, and the project still has no production firmware root or scheduler.

## Attendance client API

For an isolated standalone client, call `beginScan()` when a scan attempt is due, then call `stepScan(page, score)` until terminal. `AS608_ATT_NO_FINGER` ends that one attempt; the caller starts a new attempt on the next schedule tick. The runtime-managed client intentionally rejects these public raw-scan methods; production code must call the checked attendance pipeline instead. Treat `page` as a physical sensor page, not a student ID. Do not call `pageToId()` or `idToPage()` inside this adapter. E18 remains DELEGATED and the Job 5/9 discrepancy remains unresolved.

The clients must be serviced from one serialized loop. Each step starts or polls at most one sensor operation. `Stream::write()` can still block synchronously.

## Attendance pipeline occupancy prerequisite

`As608AttendancePipeline` receives the runtime's occupancy reference. `begin()` refuses to acquire the attendance scan lease while `occupancy.valid` is false. `saveMatch_()` checks the flag again immediately before appending `EV_SCAN`. Runtime-managed clients reject public `beginSetup()`/`beginScan()` and return fatal from public `stepSetup()`/`stepScan()` without polling the transport. Only `As608StartupCoordinator` can service setup; only `As608AttendancePipeline` can start and service scans. The preferred runtime constructor owns the map and exposes it read-only; the legacy injected-map constructor still leaves a mutable reference with its caller and is not the production path. Standalone low-level clients/sessions can also be constructed independently; the production root must use one owner and avoid parallel transports.

## Packet and command behavior

Command packet: `EF 01`, address `FF FF FF FF`, packet type `01`, length, instruction, parameters, checksum. ACK packet type is `07`. The parser verifies frame header, address, packet type, exact length and checksum.

| Instruction | Purpose | Parameters |
|---|---|---|
| `0x13` | Verify existing password | Four big-endian password bytes |
| `0x0F` | Read system parameters/capacity | None |
| `0x01` | Capture image / detect finger | None |
| `0x02` | Convert image | Buffer 1 or 2 |
| `0x04` | Search template | Buffer 1, start page 0, count = reported capacity |
| `0x05` | Create model | None |
| `0x06` | Store model | Buffer 1, physical page |
| `0x1F` | Read occupancy index page | Page 0..3 |

The adapter never changes the stored sensor password. It uses the password supplied by the caller. Search page range covers 0..capacity-1 by construction, but high-page search coverage remains unverified. The store adapter accepts a physical page unchanged; the enrollment FSM is responsible for its existing delegated ID/page conversion.

## Error and recovery behavior

- A capture ACK code `0x02` maps to no finger. Search ACK `0x09` maps to no match. Other mappings remain provisional and are based on the diagnostic sketch, not pinned vendor docs.
- A fully consumed malformed or invalid ACK maps to a retryable/fatal adapter result as currently coded. Transport timeout, partial write, malformed frame length or overflow poisons the Stream link to prevent a late ACK from being misattributed.
- After link poison, the attendance lease remains held and the enrollment session remains unable to acquire the sensor. Do not attempt in-place recovery.
- Occupancy refresh remains fail-closed: partial maps are discarded and validity is restored only after all required pages succeed.

## Deliberate omissions / blockers

1. U5: target sensor `0x1F` availability, 32-byte payload and bit order remain unverified. The Adafruit CircuitPython implementation is a reference only, not proof for this module.
2. E18: ID-to-page mapping is DELEGATED, not reviewed. This adapter returns physical pages and does not choose a mapping. Resolve the Job 5/9 discrepancy using known sensor pages before production attendance mapping.
3. The runtime is not yet called by a production firmware `main.cpp` or scheduler. The attendance result is not classified by timetable and is not written into the attendance log. Enrollment HTTP/UI and administrative recovery are missing.
4. No exact ESP32 Arduino core build. [UNVERIFIED: compile against the target's exact ESP32 core and confirm `Stream` signatures/behavior.]
5. `Stream::write()` is synchronous; TX latency and main-loop p99/max have not been measured.
6. No physical sensor verification for command mapping, status codes, index table, ID 300, high-page search, enrollment or cancellation.

## Reference cross-check (not a dependency)

Adafruit CircuitPython Fingerprint source defines `_TEMPLATEREAD = 0x1F`, reads a 44-byte ACK for each 256-slot page and scans 32 payload bytes LSB-first. The Adafruit Arduino C++ library exposes `getTemplateCount()` but no public index-table reader. These upstream sources are not pinned and do not establish compatibility of the user's exact sensor.


## Owner-bound configuration runtime (v0.2.55)

`As608OwnerConfigRuntime` implements `IConfigApplyRuntime` for the preferred `As608AttendanceOwner`:

- `canApply(next)` delegates to the owner. It refuses changes until startup READY and while the owner pipeline or attendance sensor is active. Config validation and admission-parameter preflight are also required. The pipeline additionally refuses a UTC-offset change while `RosterStore` retains its construction-time offset; no live roster replay/rebuild exists yet.
- `applyAfterCommit(next)` first calls `owner.setConfigAfterCommit(next)`, which changes pipeline Config and admission Params. It then calls `ClosureEngine::setSchedule(next.params, next.tt, next.termStart, next.termEnd)`.
- Use it as the runtime argument to `ConfigApplyCoordinator`. Do not call `applyAfterCommit()` as a substitute for the coordinator: the coordinator owns generation, inactive-slot selection, durable write and read-back validation.
- The full transaction, owner calls, closure operations and subsequent scheduler work must be serialized on one application loop. The adapter adds no mutex and cannot prevent a scan starting between `canApply()` and the post-commit apply.
- The caller must ensure the owner and ClosureEngine were built against the same selected boot Config and share the intended LogStore/RosterStore identity; both are non-owning references whose lifetime the adapter cannot enforce. A post-commit apply failure leaves persistent config newer than RAM; ConfigApplyCoordinator latches a fault and processing must stop until reboot/reload. There is no rollback.
- v0.2.55 host integration verifies the coordinator path with in-memory A/B slots and synthetic UART. It does not verify the real NVS backend, flash atomicity, power-loss recovery or ESP32 target compatibility.

