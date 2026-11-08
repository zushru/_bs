# SPEC: AS608 adapter, shared ownership, and sensor-bus composition (v0.2.9)

Status: raw enrollment and attendance command adapters, full occupancy reader, shared arbiter, attendance scan lease, enrollment lifecycle lease and sensor-bus composition root are host-tested. They have not been built on the user's ESP32 or verified against the physical sensor.

## Components

- `lib/as608/src/as608_enroll_sensor.*`: raw AS608 command packet builder/parser, enrollment-facing adapter and fail-closed occupancy reader. `As608EnrollSensor` accepts a command owner; it defaults to `AS608_OWNER_ENROLLMENT`.
- `lib/as608/src/as608_attendance_client.*`: non-blocking attendance setup and scan sequence. It constructs the raw adapter with `AS608_OWNER_ATTENDANCE`. Setup authenticates and reads capacity. A scan performs capture (`0x01`), convert (`0x02`, buffer 1), and full-library search (`0x04`). It returns a physical page and score; it does not map to student ID or write an attendance event.
- `lib/as608/src/as608_sensor_arbiter.*`: shared attendance/enrollment command arbiter, attendance scan lease and enrollment lifecycle lease.
- `lib/hal_esp32/src/stream_as608_link.*`: owner-tagged bounded-RX `Stream` transport; TX uses synchronous `Stream::write()` and may block.
- `lib/hal_esp32/src/as608_runtime.*`: sensor-bus composition root. It creates one `As608SensorArbiter` and one `StreamAs608Link`, then constructs all attendance/enrollment objects against those same instances. It also owns the enrollment FSM and wires it to the supplied roster and occupancy objects.
- `test/test_as608_gate/test_main.cpp`: 26 ownership, transport, enrollment session, attendance client and composition integration cases.

The runtime is not the whole product composition root. It does not implement the production scheduler, attendance classification/event/log write, HTTP/JSON routes, admin UI, OLED, buzzer, NVS or LittleFS drivers. No production `main.cpp` exists yet.

## Ownership and scheduling contract

- `As608Runtime` is the intended single entry point for one physical AS608. Do not construct a second `StreamAs608Link` or arbiter for that sensor.
- The runtime wires `As608AttendanceClient`, `As608EnrollSensor`, `As608EnrollmentSession` and `StreamAs608Link` to the same arbiter/link instances. The transport rejects attendance-owner commands unless the attendance lifecycle lease is held; owner tags alone are not sufficient. Tests verify that a raw attendance command is denied without a lease, enrollment blocks attendance lease acquisition, and an attendance lease blocks enrollment acquisition.
- A raw attendance-owner transaction is rejected unless an attendance lifecycle lease is held. An attendance lease covers authentication + capacity setup, or one whole capture -> convert -> search scan. This prevents enrollment from acquiring the sensor between two command ACKs. On a normal terminal result (match, no-match, no-finger or recoverable error), the lease releases after the command has been consumed.
- If the Stream link becomes poisoned, the attendance client enters `ST_FAULT` and deliberately keeps its lease held. This fails closed; runtime reconstruction is not a recovery procedure unless the physical sensor and ESP32 have first been power-cycled.
- The enrollment session still acquires before authentication/capacity/index refresh and releases only at `EN_DONE`/`EN_CANCELLED`, with no pending command or active occupancy refresh. Setup abort is permitted only while the FSM is idle and all work is drained.
- The arbiter is single-thread/main-loop only. No mutex is provided. Do not call it from multiple tasks.

## Attendance client API

1. Call `beginSetup()` once after constructing the runtime.
2. Call `stepSetup()` repeatedly until `AS608_ATT_READY` or an error. It holds the attendance lease across password authentication and capacity read.
3. Call `beginScan()` when a scan attempt is due.
4. Call `stepScan(page, score)` until a terminal result. `AS608_ATT_NO_FINGER` ends that one attempt; the caller starts a new attempt on the next schedule tick.
5. Treat `page` as a physical sensor page, not a student ID. Do not call `pageToId()` or `idToPage()` inside this adapter. E18 remains DELEGATED and the Job 5/9 discrepancy remains unresolved.

The client must be serviced from one serialized loop. Each step starts or polls at most one sensor operation. `Stream::write()` can still block synchronously.

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
