# SPEC: enroll_fsm (lib/core/src/enroll_fsm.h/.cpp)

Status: the enrollment FSM and raw AS608 adapter are host-tested. v0.2.9 adds the shared sensor-bus runtime and attendance scan client, but the production scheduler, attendance event/log path, HTTP/UI and device build are not implemented. E23 is SIGNED. E18 remains DELEGATED and must not be changed without approval.

## User-approved product choices
- O9/O10: bulk enrollment uses an ascending queue of active roster IDs that do not have a template in the current sensor occupancy map. The roster entry must exist first.
- O11: post-enrollment verification is an ON/OFF choice selected for each enrollment batch. The choice is passed to `begin(capacity, verifyAfterEnroll)`; it is not persisted in `Config`, and no default is imposed by this core.
- The queue advances automatically after a student is enrolled, skipped, or a verification failure is explicitly accepted. A student can be skipped without changing the roster.

## Fixed limits and preconditions
- Maximum active-student queue: 45, matching P1.
- Sensor capacity must be 1..300 and must be read from the actual sensor adapter.
- `Occupancy.valid` must be true before a batch begins. The complete index read must have finished.
- Roster overflow refuses the batch. Active students assigned above sensor capacity also refuse it; the FSM does not silently omit them.
- The queue is a snapshot built from active roster IDs with no occupied template, ordered by increasing ID.
- Known/previously assigned IDs are never reallocated. No `replace` or delete path exists in this FSM.

## Sensor adapter contract
`IEnrollSensor` is intentionally abstract. It does not assert that a particular Adafruit_Fingerprint method exists.
- Response polling is asynchronous. `EIO_PENDING` means the command is still in flight; the FSM must remain in the same state and call the same method again to poll it. Pending is not an error and must not consume retry budget. The Arduino Stream TX write remains synchronous and its latency is unmeasured.
- `operationPending()` reports whether a command is in flight. Cancellation drains that command before starting the finger-release fence. `skipCurrent()` is rejected while any command is in flight so a queue transition cannot strand the UART transaction.
- `captureImage()` reports `EIO_NO_FINGER` while waiting, `EIO_OK` when an image is captured, and mapped image/transport/fatal errors otherwise.
- `convertImage(1|2)` converts the first or second sample.
- `searchTemplate(page, score)` MUST search the full occupied sensor library. `EIO_NO_MATCH` is the only clean "no duplicate" response. `EIO_OK` returns a physical page and score. Unknown protocol/library results must not be mapped to `EIO_NO_MATCH`.
- `createModel()` reports `EIO_MISMATCH` when the two captures do not match.
- `storeModel(page)` stores only at the supplied physical page. The adapter must never reinterpret the page as a student ID.
- The raw adapter in `lib/as608/src/as608_enroll_sensor.*` maps command response codes based on the diagnostic sketch and reads 32-byte index pages using `0x1F`. `As608OccupancyReader` keeps the occupancy invalid until every page succeeds and clears partial data on failure. Adafruit CircuitPython's `read_templates()` provides an LSB-first reference implementation; the target AS608's behavior remains unverified until Job 11 is compared with known physical pages. The transport bridge in `lib/hal_esp32/src/stream_as608_link.*` and ESP32 integration remain unverified until built and exercised on the device.

Student ID maps to sensor page through the existing `idToPage(id)` helper (`page = ID - 1`). This mapping is still subject to C9/U7 hardware verification above page 0.

## State flow
1. Wait for first finger, capture and convert into buffer 1.
2. Search the complete sensor library. A match blocks storage and reports the existing student ID and score. The administrator can try a different finger or skip the student.
3. On a clean no-match result, wait for the second placement, capture and convert into buffer 2, then create a model.
4. Immediately before storage, recheck that occupancy is still valid, the roster entry is still active, and the target slot is still empty.
5. Store at `idToPage(currentId)`. On a positive acknowledgement, update the in-memory occupancy map and increment the enrolled count.
6. If verification is enabled, wait until the finger is removed, then request a fresh scan. The full-library search must resolve to the newly enrolled page. A mismatch/no-match is shown as `EN_VERIFY_FAILED`; the template is NOT automatically deleted. The administrator may retry verification or explicitly continue with a warning.
7. When verification is off, advance the queue after a positive store acknowledgement. Keep polling until the enrollment finger is removed before accepting another student's sample or reporting `EN_DONE` and handing the sensor back to attendance scanning.

One `step()` call makes at most one sensor operation. `step()` itself does not run the web server. The current bridge polls a bounded number of RX bytes per step, but command transmission uses synchronous `Stream::write()` and may block while queuing the short packet. The production scheduler must call it at a controlled cadence and continue servicing unrelated work between calls; measure TX and loop latency before relying on responsiveness.

The single AS608 must be exclusively owned for the complete enrollment batch (SIGNED E23, Option A). Attendance scans must not issue commands to the sensor between FSM steps because they can overwrite the sensor's image/model buffers and corrupt the enrollment sequence. The intended application must instantiate one `As608Runtime` per physical sensor. That root creates one `As608SensorArbiter` and one `StreamAs608Link`, then wires both attendance and enrollment to those exact instances. It also creates the enrollment FSM, occupancy reader and session. The arbiter is single-thread only; multi-task access requires synchronization.

The enrollment scheduler must call `As608EnrollmentSession::acquire()` before authentication, capacity read, and index refresh. It must call `sync()` after each FSM step; the session releases only after `EN_DONE` or `EN_CANCELLED`, with no pending sensor command or active occupancy refresh. It must retain ownership through retries, refresh, and release faults. A setup failure while the FSM is still idle may use `abortPreparation()` only after all sensor work drains and the Stream link is not faulted. `cancel()` drains any in-flight sensor command before beginning a finger-release fence. Only a positive `EIO_NO_FINGER` response transitions to `EN_CANCELLED`. Three consecutive release-check transport/image errors enter `EN_CANCEL_RELEASE_FAULT`; the sensor remains reserved until `retryCancelRelease()` succeeds. If cancellation occurs while a store command is in flight, a positive store ACK is reflected in occupancy; any uncertain result invalidates occupancy and requires a complete refresh before attendance eligibility/closure resumes.

The attendance side uses `As608AttendanceClient`. It holds a separate short attendance lease across authentication/capacity setup and each capture -> convert -> full-library search attempt. This blocks enrollment from acquiring the sensor between separate commands. A match returns a physical sensor page and score; the client does not translate page to student ID (E18) or write an attendance event. If the Stream link is poisoned, the attendance lease remains held; the runtime must not be rebuilt until the sensor and ESP32 have been power-cycled. This scan-lifetime rule is E25 (DELEGATED, not yet user-reviewed).

## Recovery and safety behavior
- `EIO_NO_FINGER` is a normal wait condition and does not consume the retry budget.
- Three consecutive recoverable image/transport errors pause the current operation in `EN_RETRY_REQUIRED`; the administrator can retry or skip before storage. Three model mismatches also pause for a manual retry.
- A positive store acknowledgement updates the in-memory occupancy snapshot. Any non-OK store result is ambiguous: the module may have stored the model even if its reply was lost. The FSM invalidates occupancy and enters `EN_OCCUPANCY_REFRESH_REQUIRED`. It never blindly repeats `storeModel()`.
- The pre-store slot check is against the valid occupancy snapshot. It assumes the adapter prevents all other sensor writers for the whole batch; the FSM cannot detect an out-of-band sensor write that is not reflected in that snapshot.
- After the adapter completes a full index refresh, call `resumeAfterOccupancyRefresh()`. If the target is present, it is counted as stored and the queue continues after it. If absent, that student is re-queued. Previously skipped IDs are not re-added.
- A store/verify failure does not issue delete commands. Delete API support and its safe use are not verified.
- A fatal error outside the store operation enters `EN_SENSOR_FAULT`; the administrator must cancel/recover the sensor before starting another batch.

## Occupancy and closure integration
- `EnrolledRoster` holds a reference to `Occupancy`, so a successful store updates eligibility immediately in the same running process.
- On reboot, restore, sensor replacement, or an ambiguous store error, the adapter must refresh the complete sensor index before any closure can trust expected-student membership. If that refresh is incomplete, leave `Occupancy.valid == false`; `EnrolledRoster.expected()` then returns false for everybody and closures must not infer Absent from a stale map.
- The planned production adapter must also follow the project rule to refresh sensor occupancy before each closure tick that snapshots the roster. This FSM does not implement that scheduler integration.

## Explicit non-goals / unresolved hardware facts
- No HTTP endpoint, JSON schema, admin UI, buzzer/OLED behavior, production attendance scheduler, timetable/classification/log adapter, or firmware entry point is provided. The AS608 sensor-bus runtime exists, but the complete product pipeline is not wired.
- No `Adafruit_Fingerprint` signatures are assumed or cited here.
- AS608 index-table command availability, page size, and actual bit order remain hardware verification items despite the Adafruit CircuitPython reference. Full-library search coverage, ID 300/page 299, image error mapping, and write-ack behavior also remain unverified.
- Duplicate score is exposed for diagnostics only. No threshold or `minScore` policy is invented; C4 remains open.
- Templates are not included in backups. Re-enrollment remains the recovery path.

## Test coverage
The `test_enroll` suite registers 36 cases, `test_as608` 15, and `test_as608_gate` 26. Total source registrations are 222 across 12 suites in v0.2.9. The user reported 214/214 PlatformIO native tests passed on v0.2.8. The v0.2.9 26-case gate/runtime suite passes locally under C++11 `-Werror` with stand-in Unity; the full 222-case PlatformIO run and exact ESP32 core remain unverified. These host checks do not verify the physical sensor or complete product attendance pipeline.
