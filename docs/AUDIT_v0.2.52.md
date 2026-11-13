# Specialized audit — v0.2.52

## Scope

- AS608 startup sequencing and sensor-owner transitions.
- Occupancy invalidation and full index refresh ordering.
- Consistency of the occupancy object shared by runtime, enrollment FSM, reader, coordinator and attendance event pipeline.
- Attendance readiness revocation after capacity mismatch.
- Failure release/retention behavior.
- Event-commit gating when occupancy validity changes.
- ESP32 smoke-link retention, verification evidence and release integrity.

## Findings

### 1. Startup sequence is fail-closed at the coordinator boundary

The coordinator acquires the enrollment reservation before sensor authentication and capacity read. It starts a complete occupancy refresh only after valid capacity is reported. The reader clears the map at refresh start and only sets `valid` after every required page succeeds. The coordinator checks occupancy validity and capacity again before releasing enrollment ownership. It then authenticates and reads capacity under the attendance owner, and requires the two reported capacities to match before returning READY.

Failure clears occupancy. An enrollment preparation lease is released only when the link is not poisoned, no command is pending, the reader is inactive and the FSM is idle. Transport poison retains ownership, consistent with the existing no-transaction-ID recovery rule.

### 2. Duplicate occupancy dependency — corrected

The initial constructor accepted both an `As608Runtime&` and a separately supplied `Occupancy&`. A caller could pass a different map from the one used by the runtime's reader and FSM, causing startup to validate the wrong object.

The coordinator now accepts only the runtime. The runtime retains the original `Occupancy&`, uses it to construct the reader and FSM, and exposes that exact reference to startup. Test-first compilation failed because the one-argument API/accessor were absent; the owning gate suite then passed 31/31.

### 3. Pipeline occupancy gate — corrected

The pipeline previously relied on attendance-client readiness and admission but did not require a trusted occupancy map. A caller could bypass the startup coordinator and begin the event pipeline with an invalid/uninitialized map.

The pipeline now receives the same `const Occupancy&` from the runtime. It rejects `begin()` when `occupancy.valid` is false and checks the flag again immediately before durable `EV_SCAN` append. Two new tests prove:
- invalid occupancy blocks scan acquisition even when the attendance client is manually setup-ready;
- occupancy invalidated after capture/convert/search start prevents event append and releases the scan lease.

The constructor change was tested first: compilation failed until the occupancy dependency was added. The final pipeline suite passed 8/8, including all six previous Present/Late, duplicate, resolver, storage-failure and no-finger cases.

This is not a full startup barrier. A caller can still invoke the raw attendance client and start scans outside the pipeline. A production root must encapsulate the runtime and enforce the startup sequence.

### 4. Authentication and mismatch failure coverage

A non-OK enrollment-side password ACK faults startup, clears occupancy, releases the drained enrollment reservation and leaves attendance unready. Partial index-page failure clears the incomplete bitmap and prevents attendance setup. A transport write fault leaves the poisoned sensor reserved. Capacity disagreement between enrollment and attendance setup revokes completed attendance readiness and clears occupancy.

Tests use synthetic UART ACKs and do not establish actual sensor protocol behavior.

### 5. E18 and index semantics remain unresolved

No decision ledger or handoff entry was changed. Occupancy still uses the existing LSB-first 0x1F bitmap interpretation and `pageToId(slot)` mapping. Host tests seed synthetic bits; they are not Job 11 results. Actual sensor index-command availability, 32-byte payload, bit order, page numbering, ID 300 and high-page search remain unverified. E18 stays DELEGATED.

### 6. Verification

- `./tools/verify_v0.2.52.sh targeted`: exit code 0; `TARGETED_V0_2_52=PASS`.
- `pio test -e native -f 'test_as608_gate'`: 31/31 passed.
- `pio test -e native -f 'test_as608_pipeline'`: 8/8 passed.
- Focused total: **39/39**.
- Static inventory: **546 C++ registrations across 34 suites**; 507 not run.
- Initial ESP32 smoke build: exit 0; RAM 63,676 B (19.4%); flash 926,476 B (70.7%); 1,882.09 s. This compiled/linked startup/HAL changes before the final occupancy-pipeline edit.
- Exact initial target ELF inspector: exit 0; `NVS_WRAPPER_ROUTING=PASS`.
- Attempted post-pipeline-edit target rebuild: deliberately terminated after 255.81 s, exit 143, because PlatformIO cleared cached target objects and started a clean dependency/framework rebuild. This is not a pass and is not represented as one.
- Full native suite, SF2 Node tests, diagnostic probe build and hardware tests were not run.

### 7. Product and safety boundaries

- `HANDOFF_FINAL.md` unchanged.
- `docs/DECISIONS.md` unchanged. E18 and all other decision statuses remain as they were.
- No production student-ID/page mapping was added.
- No real sensor command was sent; no flash or NVS operation was performed.
- C3 and C34 remain blocking.

## Residual risk summary

The startup coordinator establishes a host-tested ordering protocol, and the event pipeline now refuses to commit while the occupancy map is invalid. However, direct attendance-client APIs can bypass startup, actual index semantics are unknown, and production firmware still does not exist. This is not deployment-ready.
