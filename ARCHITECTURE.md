# ATT system architecture

**Baseline:** v0.2.95 (2026-11-12)  
**Document type:** source-oriented architecture overview  
**Status:** describes the supplied development snapshot; proposed or unverified elements are labeled.  
**Authority:** product behavior remains governed by `HANDOFF_FINAL.md` and `docs/DECISIONS.md`; module contracts remain in `docs/SPECS/`.

## 1. System purpose and boundary

**Delivery objective (2026-11-13): production candidate.** The target is the complete attendance product, not a graduation/demo slice. The intended device must safely compose setup, authenticated administration, roster management, fingerprint enrollment and identification, attendance admission/commit, session closure, record review, backup/restore and approved reporting. Existing signed rules remain controlling; school-dependent SF2 choices remain gated rather than guessed.

Production-candidate status is evidence-gated: source presence, native tests, host simulations and target linking are separate evidence classes. Do not enable persistent mutation paths until their target storage commit and recovery behavior is verified. Do not claim release readiness without full workflow, restart/replay, fault-recovery, security and physical-hardware evidence.

ATT is an ESP32-based fingerprint attendance device. The intended product combines fingerprint capture/identification, attendance and roster logic, persistent event records, clock/session rules, an administrator-controlled network interface and local status/display hardware. The repository contains reusable firmware layers and tests for many of those responsibilities.

The supplied v0.2.95 baseline is **not a complete integrated product**. In particular, it has no production firmware composition root/scheduler that connects all the reusable components into one running device. It also has no completed production HTTP/API/dashboard integration. The SF2 folder is a separate browser-side reporting reference model, not a server or production exporter.

Do not confuse these four evidence levels:

- **Implemented:** source code exists.
- **Host-tested:** tests pass against host fakes/POSIX or pure JavaScript models.
- **ESP32 compile/link verified:** a specified target compiles and links; this does not establish runtime behavior.
- **Physically verified:** the exact hardware configuration was exercised and evidence recorded.

A component can be implemented and host-tested while remaining unintegrated and hardware-unverified.

## 2. Repository-level architecture

```text
Product requirements / decisions
             |
             v
    Hardware-independent core
             ^
       adapters / clients
       /       |        \
    AS608   ESP32 HAL   LittleFS adapter
       \       |        /
        future production composition root
                  |
          physical ESP32 system

SF2 browser reference models (separate workstream; no production API link)
```

The arrows above are conceptual subsystem relationships, not a verified runtime call graph. Exact includes, ownership and caller relationships must be checked in source before changing a dependency.

### Main directories

| Path | Responsibility | Current boundary |
|---|---|---|
| `lib/core/src/` | Hardware-independent domain logic and interfaces | Reusable core; no production firmware root. |
| `lib/as608/src/` | AS608 link/client, enrollment, scanning, attendance pipeline/admission and sensor arbitration | Reusable sensor/attendance components; production caller/root absent. |
| `lib/hal_esp32/src/` | ESP32-specific adapters and owner/startup gates | Adapters exist; not proof of complete production startup. |
| `lib/littlefs_file_set/src/` | `IFileSet` implementation, mount ownership and serialized access | Host/POSIX evidence exists; mounted-target durability and production composition remain open. |
| `test/` | PlatformIO native C++ tests | 46 suites; host behavior only unless a test states otherwise. |
| `target/esp32_adapter_link/` | Inert compile/link smoke composition | Compile/link evidence only; never flash. |
| `target/esp32_hardware_probe/` | Diagnostic hardware probe | Not attendance firmware. Review before any diagnostic deployment. |
| `sketch/` | Legacy hardware bring-up sketch | Historical diagnostic; known duplicate-template/credential cautions. |
| `web/sf2/` | Browser-side reference models and Node tests | Not a device API, dashboard or XLSX writer. |
| `docs/SPECS/` | Module-level contracts | Read the relevant spec before changing that module. |

## 3. Firmware subsystem responsibilities

### 3.1 Core domain (`lib/core/src/`)

The core is intended to keep policy and data logic independent of Arduino/ESP32 details. Its source includes:

- Event representation and validation: `event_codec.*`, `event_group_audit.*`.
- Append/log/index and month-file coordination: `log_store.*`, `log_index.*`, `month_set.*`, `fileset.h`, `istorage.h`.
- Roster state, history and mutation boundary: `roster_store.*`, `roster_event_writer.*`, `enrolled_roster.*`, `roster_name_history.*`, `roster_snapshot.*`, `month_roster_projection.*`.
- Attendance/session/health policy: `status_derive.*`, `closure_engine.*`, `day_view.*`, `health_fsm.*`, `time_rules.*`, `clock_policy.*`.
- Configuration and provisioning: `config_store.*`, `config_boot.*`, `config_provision.*`, `config_apply.*`, `config_runtime_guard.*`.
- Authentication/session and request logic: `pin_auth.*`, `pin_auth_session.*`, `api_logic.*`.
- Backup/restore validation: `backup_format.*`, `restore_replay.*`.

These source files do not collectively constitute a running application. Their public contracts and tests must be inspected together. `RosterEventWriter` is a reusable planner-before-append boundary, not a production route or enforcement against direct `LogStore::append()` callers. Its exact observer and log generation must remain stable under one serialized owner.

### 3.2 AS608 (`lib/as608/src/`)

The AS608 layer owns the sensor-facing reusable operations:

- `as608_attendance_client.*`: attendance scan/client operation.
- `as608_attendance_pipeline.*`: connects scan outcomes to admission/attendance policy.
- `as608_enroll_sensor.*`: sensor enrollment operations.
- `as608_sensor_arbiter.*`: serializes shared sensor use and ownership transitions.
- `attendance_config_runtime.*`: applies/preflights relevant configuration against attendance behavior.
- `log_attendance_admission.cpp`: log-backed attendance admission boundary.

Enrollment and attendance must not issue concurrent conflicting sensor commands. The owner/arbiter and cancellation/finger-release fence rules are defined in the AS608 specifications and decision ledger. The exact physical mapping between logical learner IDs and AS608 template pages remains unresolved; do not assume a one-based/zero-based mapping.

### 3.3 ESP32 HAL (`lib/hal_esp32/src/`)

This layer adapts core and AS608 behavior to ESP32/Arduino facilities. It includes:

- AS608 UART/runtime/startup and reusable owner/gate components.
- NVS-backed config, PIN-attempt and AP-credential stores, plus startup-guard policy.
- Wi-Fi AP, secure/bootstrap random sources and OLED display.
- DS3231 clock/register I/O and shared I2C bus ownership/locking.
- LittleFS mount backend and application-attendance gate.

Important classes include `ApplicationAttendanceGate`, `As608AttendanceOwner`, `As608OwnerConfigRuntime`, `As608Startup`, `Esp32LittleFsMountBackend`, `LittleFsStorageOwner`, `Esp32NvsStartupGuard` and individual hardware adapters. Their presence does not show that a production `main()` constructs them in the correct order. That integration is blocker C3/C1.

### 3.4 LittleFS and log path

The reusable storage path is organized around `IFileSet`/`IStorage`, `LittleFsFileSet`, a mount/storage owner, a serialized file-set wrapper, `MonthSet` and `LogStore`.

LogStore::open() now performs its full-stream validation and observer-replay
passes in fixed batches of at most MAX_GROUP (16) records, using a 256-byte
stack buffer. MonthSet::read() may split a batch across adjacent month
files; record decoding, sequence checks and observer delivery remain ordered.
The trailing unfinished-group check remains record-at-a-time. Indexed LogStore::at() also revalidates the expected firstSeq + index sequence on every read; a read, decode or sequence failure invalidates the in-memory log and resets attached projections, while an out-of-range index remains a non-faulting rejection. This is a
source-level filesystem-call reduction, not a measured boot-time claim; it
adds no heap allocation, persistent-format change or target integration.

Observer recovery is also fail-closed. A candidate observer is reset before
attachment and receives an ordered replay; if any replay read fails, the
LogStore invalidates the whole stream, resets every registered projection,
removes the failed candidate and returns false. Existing observers remain
registered in reset state. A later successful open() resets them again and
rebuilds them only from a complete replay. Production indexed-read consumers
must propagate false; the current day-view, roster-history, roster-prefix
and clock-pair readers do so. Reset callbacks are limited to clearing derived
state and must not re-enter storage.

The intended design is conservative: no automatic format; read-only validation/preflight before exposing usable storage; serialized storage operations; append-only event records; and fail-closed behavior on ambiguous/corrupt/incomplete data. A failed append is **indeterminate**, not proof that no bytes were written. A caller must not blindly retry the same logical event.

`ClockConfirmationService` also binds the readiness capability implemented by
`LittleFsStorageOwner`: it requires READY and verifies that its `MonthSet` uses
the owner's exact serialized `IFileSet`. This is an object-ownership check,
not a whole-transaction lock; the future application owner must serialize the
complete clock call with all storage writers. The service still has no
production caller.

Host POSIX tests and fake mount backends exercise important paths, but they do not prove the pinned ESP-IDF LittleFS VFS implementation, parent-directory durability, low-space behavior, a durable commit point, power-loss recovery or production mount ownership. Do not enable destructive repair/truncate/quarantine based on host behavior.

## 4. Intended runtime responsibilities versus current integration

The product needs one production composition root and a serialized scheduler/owner that coordinates at least:

1. ESP32/NVS startup guard and validated config boot.
2. Trusted clock/RTC read and health-state establishment.
3. No-format LittleFS mount/validation and storage-owner readiness.
4. `MonthSet`/`LogStore` and roster replay/semantic validation.
5. AS608 UART, sensor runtime/arbiter and template occupancy reconciliation.
6. Attendance admission through the application gate, live health state and log-backed commit.
7. Closure/session scheduler and correct event sequencing.
8. Authenticated administrative API/UI, request ownership/idempotency and safe error behavior.

This is a **required target architecture**, not an existing boot sequence. The current archive does not establish a production `main.cpp`, HTTP route owner, full scheduler, clock caller or end-to-end flow. Do not draw this as a completed stack or build demo claims around it.

### 4.1 Recommended application owner — PARTIALLY INTEGRATED; FULL GRAPH NOT IMPLEMENTED

**Recommendation:** use the existing Arduino `setup()` / `loop()` thread as the
one serialized application owner. Do not introduce separate FreeRTOS tasks for
HTTP handlers, attendance, enrollment, clock changes or storage writes in the
first production composition.

This is the lowest-risk fit for the current source contracts:

- `WebServer.handleClient()` is called synchronously from `loop()`, so its route
  callbacks already run on the same thread as the application loop.
- `AdminSessionBoundary`, `PinAuthCoordinator`, `ConfigApplyCoordinator`,
  `ClockConfirmationService`, `ApplicationAttendanceGate`, `As608Runtime` and
  the attendance pipeline explicitly require serialized main-loop calls or do
  not add cross-task synchronization.
- `Esp32WifiAp` event callbacks only signal their event group; they must not be
  expanded into application-state mutation callbacks.
- The I2C lock and serialized file-set wrapper protect narrow resources. They
  do not serialize a complete clock, config, attendance or roster transaction.

The target now has a static `Application` object and synchronous HTTP dispatch
on Arduino's `loop()`, so this is an existing single-threaded shell, not merely
a proposal. It owns the E33 verifier and services expiry around HTTP dispatch.
On READY, it now also constructs the live PBKDF2 PIN runtime at E35's fixed
2,048 iterations, then binds `AdminSessionBoundary` to the exact `attsec`
coordinator, verifier and AP-owned RNG. It constructs `AdminSetupCodeBoundary`
only when the separately started `attsetup` runtime is ready, binding that
exact coordinator and the RAM-only setup-code verifier. NEEDS_SETUP has no PIN
verifier/session boundary; READY-missing `attsetup` has no P15 boundary. This
is owner composition only: no protected HTTP route calls these objects.
NEEDS_SETUP now constructs a session-free `FirstRunSetupCodeBoundary` and the
serialized target loop issues/reissues the RAM code only after DHCP and expiry
service. No HTTP setup submission is exposed and no privileged HTTP operation
is enabled.

The static `Application` object must ultimately own the complete mutable service graph.
`setup()` performs one-shot boot and route registration; `loop()` is the only
place that advances services or handles requests. Route lambdas are adapters,
not owners: parse bounded input, invoke the long-lived boundary/service and
serialize its result. Do not spawn a worker task or create auth/session/store
objects per request. If a future requirement truly needs another task, submit
commands to one owner queue with explicit bounded-capacity and cancellation
semantics; do not add unrelated per-subsystem mutexes.

```text
ESP32 Arduino main thread — sole mutable application owner
  setup(): NVS guard -> ConfigBoot -> construct READY graph -> validate -> AP/HTTP
  loop():
    P15 expiry service -> WebServer.handleClient() -> route adapter -> typed service
    P15 expiry service again after synchronous request dispatch
    first-run/AP state progression
    AS608 startup / scan / enrollment state machines
    closure + health scheduler
    status/state publication

ESP-IDF Wi-Fi/IP callbacks -> event-group signals only
```

### 4.2 Stable object graph and boot ordering — PROPOSED

The composition root should construct one long-lived instance of each owner and
bind exact object identities. Suggested dependency order:

1. Run `Esp32NvsStartupGuard` before constructing `Application`; start the
   `ConfigBootCoordinator` and accept only `CFG_BOOT_NEEDS_SETUP` or
   `CFG_BOOT_READY`. Do not construct runtime services from default config.
2. Initialize the shared I2C/OLED and the single `Esp32SecureRandom` /
   `Esp32WifiAp` pair. The random source passed to the AP must be the exact same
   object later used for session tokens, PIN salt generation and the P15 code.
   It is not ready until the AP_START event opens its RF gate.
3. Start two separate persistent attempt runtimes, `attsec` and `attsetup`,
   before exposing secret-check routes. The target now constructs and starts
   both namespace-bound runtimes before storage/AP/HTTP. Existing `attsec`
   failure remains a global boot fault. An `attsetup` failure is scoped to P15:
   a READY device missing it retains only the read-only status AP; setup-code
   actions remain unavailable. Do not auto-migrate or initialize READY state.
   On NEEDS_SETUP, the target constructs `FirstRunSetupCodeSequence` and,
   only when the exact `attsetup` runtime is ready, a session-free
   `FirstRunSetupCodeBoundary`. The AP password is displayed before AP start;
   the serialized loop issues/reissues the code only after AP readiness and the
   AP's DHCP-assignment latch, servicing expiry first. The boundary preflights
   the existing AP credential and normalized Config before code verification,
   then enforces durable attempt/reset ordering and the H_UNPROVISIONED gate.
   No HTTP submission route or post-provision READY graph transition exists.
   On READY, the target now creates `Esp32MbedTlsPinConfigRuntime` at E35's
   fixed 2,048 iterations and binds `AdminSessionBoundary` to the exact
   `attsec` coordinator, live verifier and AP-owned RNG. It creates
   `AdminSetupCodeBoundary` only if the exact `attsetup` runtime is ready,
   binding the exact session owner, setup coordinator and RAM verifier. These
   boundaries are not yet called by HTTP routes.
4. On NEEDS_SETUP, do not touch attendance storage. Compose the E28/E33 path:
   AP credential frame before secured AP startup, AP_START/READY and DHCP
   assignment, then RAM-only code issue/reissue on the serialized loop after
   expiry service. First-run setup uses the session-free
   `FirstRunSetupCodeBoundary`, not `AdminSetupCodeBoundary`; it preflights the
   exact AP-start credential, non-PIN Config fields and six-digit PIN
   syntax/strength before code verification. After durable attempt/reset and
   H_UNPROVISIONED authorization, it derives the initial PIN salt/hash with the
   RF-gated AP RNG and E35's fixed 2,048-round PBKDF2; then the canonical Config
   provisioner performs the one durable first write. No HTTP caller or
   post-provision READY graph transition exists yet.
5. On READY, construct the exact storage chain:
   `LittleFsStorageOwner` -> its serialized `IFileSet` -> `MonthSet` ->
   `LogStore`. Open/validate the full log before enabling writers. Then attach
   long-lived projections/auditors to that exact `LogStore`: `RosterStore`
   (using the validated config UTC offset), `LogAttendanceAdmission`,
   `EventGroupAudit`, and `ClosureEngine`'s `LogIndex`. Check every attach
   result plus `RosterStore::historyComplete()` and the event-group audit before
   constructing any attendance gate. E27's `RosterTransitionAudit` is a
   reporting-projection guard only; do not apply it to operational boot or live
   roster readiness, because same-day lifecycle events remain operationally
   permitted under E27. Run it when building the monthly reporting projection.
6. Construct `ApplicationAttendanceGate` over those exact objects and one
   `As608AttendanceOwner` over the exact log, roster, config and gate. Do not
   create a parallel AS608 runtime, stream link, sensor arbiter or occupancy
   map. The owner must outlive every scan/enrollment operation.
7. Only after validated storage/projections, clock/health state, auth runtimes,
   and the relevant service owners are ready should the authenticated route
   families be registered. Keep the current read-only boot shell as-is until
   that production composition is separately implemented and verified.

`LogStore::open()` alone is not sufficient: current `target/esp32_attendance`
opens the log but does not yet attach this full projection graph. Observer
objects must remain alive for the entire period in which `LogStore` can notify
them. The observer limit is finite (`MAX_OBSERVERS`); check it as part of
composition rather than assuming attachment cannot fail.

### 4.3 Authentication and route boundaries — PROPOSED

HTTP parsing must produce typed request values. Core/adapter boundaries receive
the exact token decoded from the current request, never caller-supplied
`tokenValid`, `setupCodeOk` or health booleans. Use one long-lived instance of
each boundary:

| Request class | Required boundary / action |
|---|---|
| Public status | Keep only the existing exact `/` and `/api/status` read-only handlers and their allowlisted fields. |
| Login | `AdminSessionBoundary::beginLogin()`; send the full token response, then call `confirmLoginResponseSent()`. On serialization/socket failure call `abortLoginResponse()`. |
| Ordinary authenticated endpoints | `AdminSessionBoundary::authorize()` with the endpoint enum, current HealthFsm state, exact current-request token and monotonic seconds; only then call the typed API builder/service. |
| PIN change | `AdminSessionBoundary::changeAdminPin()`; current PIN is required and the request token/health check precedes verification. |
| First-run setup | `FirstRunSetupCodeBoundary::provision()` in NEEDS_SETUP; session-free code-only, normalized-config/AP-credential preflight, H_UNPROVISIONED checked after code verification/reset. No HTTP caller yet. |
| PIN reset, restore, rollover | `AdminSetupCodeBoundary::authenticateAndRun()` on the provisioned admin graph. Preserve E33: PIN_RESET/RESTORE are code-only; ROLLOVER checks this request's live token before code verification. |
| Clock confirmation | `AdminClockConfirmation::confirm()`; it derives health from the exact `ClockConfirmationService` owner and verifies the current request token before invoking the service. |
| Roster writes | Ordinary `EP_ROSTER_SET` authorization, then `rosterSet` / `rosterDeactivate` / `rosterActivate` planning, then one append only when the builder returns `A_OK` with a nonzero event count. |
| Config writes | Ordinary `EP_CONFIG_PUT` authorization and `ConfigApplyCoordinator`; use its own current generation, preflight, A/B commit, read-back and runtime-after-commit ordering. |
| Attendance / enrollment | A narrow `As608AttendanceOwner` façade and the application scheduler, never raw `As608Runtime` access from a route. The current owner exposes startup and scan operations but no complete enrollment-route façade yet. |

This table defines component ownership, not new URI names or JSON schemas.
Those route contracts remain to be implemented and tested. Do not register a
route merely because an endpoint enum or service class exists.

**HTTP body parsing boundary (source-authored, not compiled):** the pinned
attendance PlatformIO environment now includes a hash-pinned pre-build patch
for Arduino-ESP32 WebServer. All POST/PUT/PATCH/DELETE requests must
match an explicit canRaw() handler or are rejected before parsing. Raw
handlers run before multipart parsing; duplicate Content-Length and all
Transfer-Encoding are rejected; raw body size is capped at 768 bytes with a
5,000 ms whole-body deadline. This makes the stock multipart String
accumulator unreachable through the project's mutation request path. The
patch is applied only during the future attendance build and restores the
original framework file on normal build-process exit. Since the package is
shared, no other PlatformIO build may run concurrently; recover the retained
backup before an unrelated build after forced termination. The patch has not been
run or compiled, no raw handler is registered in main.cpp, and the target
still exposes only GET routes. This is not evidence of runtime HTTP safety.

### 4.4 Serialized write and failure discipline — REQUIRED

- A route may plan/validate an operation, but only the application owner may
  append or mutate config/RTC/health/session state. A roster request must use
  the planner first; never expose generic `LogStore::append()` to a request
  handler as a roster-write shortcut.
- `LogStore::append()` notifies `LogObserver::onRecord()` only after storage
  append succeeds, and observer callbacks return `void`. Therefore an append
  success cannot report whether a semantic projection rejected the event.
  After roster append, verify the roster remains trusted and the expected
  state change is visible before returning success. If not, latch the
  application fault and keep attendance/mutation disabled; the committed bytes
  cannot be rolled back through the observer interface.
- If append returns false, treat persistence as indeterminate. Stop the
  operation, fault the relevant owner and reopen/reconcile before any future
  attempt. Never blindly retry a logical event.
- `ConfigApplyCoordinator` must retain its durable A/B write -> read-back ->
  runtime apply order. If runtime apply fails after commit, persistent config
  is newer than RAM; stop until reboot/reload. Apply the attendance owner's
  config and `ClosureEngine::setSchedule()` in the same serialized operation,
  with no sensor/closure step between those updates.
- `ClockConfirmationService::confirm()` is one serialized transaction across
  log/month selection, RTC write/read-back and health state. A separate clock
  mutex is not enough. Do not retry ambiguous clock results.
- The `ReqRing` is RAM-only same-boot request deduplication, not durable
  exactly-once behavior across reboot or uncertain appends (U112 remains open).

### 4.5 Cooperative loop responsibilities — PROPOSED

The loop should call bounded, stateful service steps rather than run long
sensor workflows inside HTTP callbacks. At minimum it must explicitly own:

1. `WebServer.handleClient()` and request-bound boundary dispatch.
2. First-run AP/DHCP/setup-code state progression, including expiry/blanking
   service for the displayed setup code.
3. `As608AttendanceOwner.stepStartup()` until occupancy refresh and sensor
   authentication/capacity checks complete.
4. A dedicated scan-window scheduler that calls `beginScan(nowEpoch)` only
   when the configured scan window and health/admission gates allow it, then
   repeatedly calls `stepScan()` and restarts only after a terminal result.
   The owner currently exposes scan primitives, not this scheduler.
5. An enrollment queue/service that advances one sensor operation at a time
   through the owner. The current owner does not expose the complete
   enrollment FSM through a production façade; do not reach into its private
   runtime to bypass ownership.
6. Clock/health transition handling and `ClosureEngine::onBoot()`,
   `onHealth()`, `onTick()` / `step()` with an explicit persisted tick source.
   The reusable step primitive now has a source-only one-local-day budget,
   a processed-day high-water and a next-session-end wait. LogIndex keeps its
   existing 800 CLOSE references sorted for O(log n) closed-day lookup, with
   bounded O(n) insertion and no added index RAM. Four new registrations across
   these closure/index improvements are NOT RUN. No production scheduler calls
   the engine, the durable tick source is not composed, and C17 historical
   timetable-effective history remains unresolved. No closure writes are enabled.

Keep callbacks free of direct I2C/UART/filesystem work when a state-machine
step can own it. Measure synchronous PBKDF2, log/file and closure work before
choosing loop cadence; E35 fixes the PIN work factor at 2,048 iterations, but
its target latency still needs measurement. A future
multi-task server would need one shared owner/queue for *all* session, config,
RTC, roster, attendance, closure and storage mutations, not independent locks.

### 4.6 Production integration gates — NOT CLOSED

This proposal does not complete C3/C6/C13/U61. In particular:

- E35 fixes the existing salted PBKDF2-HMAC-SHA256 admin-PIN work factor at
  2,048 iterations for both verification and credential creation. The target
  still needs latency and denial-of-service measurement before release.
- Existing READY devices missing the E33 `attsetup` counter remain fail-closed.
  E36 requires explicit, physically supervised USB migration that preserves all
  existing security, attendance and sensor state; no migration utility exists.
- The RAM setup-code verifier now exposes `serviceExpiry()`: at the 600-second
  monotonic boundary it invalidates and wipes the code before one blank-frame
  transfer request, and fails closed on clock failure/regression, RF-gate loss
  or blank-transfer failure. Nine focused regressions are authored but NOT RUN.
  No production loop calls this service yet, so periodic OLED blanking is not
  integrated; a transfer request is not proof of physical panel visibility.
- The attendance owner lacks a public enrollment façade and scan-window
  scheduler; no production enrollment/automatic-scan claim is justified.
- LittleFS target durability and the signed/device partition discrepancy
  remain unresolved. Host/POSIX behavior is not target durability proof.
- U61 requires all production roster routes to use the E34 planner and verify
  observer/projection postconditions; generic raw append remains bypassable.
- U112 durable exactly-once/reconciliation is still absent.

The present target remains a read-only status shell. READY boot now attaches
`EventGroupAudit`, replays `RosterStore`, and attaches
`LogAttendanceAdmission`; these projections are startup prerequisites only and
do not enable writes or attendance. The full production composition remains
unfinished. This document is an engineering composition proposal, not an
authorization to enable privileged routes.

## 5. Data and persistence concepts

The current module design uses an append-oriented event/log model, month file sets, configuration slots and roster history. Event encoding, group framing, sequence continuity, closure payloads and replay semantics are specified in the core specs and handoff. Persistent-format changes require explicit compatibility review and must never be slipped into a deadline patch.

Important invariants include:

- Absence is derived only from trusted closed-session coverage and expected-roster evidence; missing data is not absence.
- Present/Late are tied to scan-time policy, with the exact rules defined by the decision ledger.
- Roster deactivation is not deletion; historical identity/lifecycle data affects report-month projection.
- A write failure may leave visible bytes or files. Reopen/reconcile before any retry that could duplicate a logical event.
- Ambiguous/corrupt/incomplete streams must fail closed; do not replay only a plausible prefix or silently truncate.
- Restore replay validation is not equivalent to a transactional swap or reboot recovery mechanism.

## 6. Hardware architecture

The HAL targets an ESP32 Dev Module using PlatformIO/Arduino and includes interfaces/adapters for:

- AS608 fingerprint sensor over UART.
- DS3231 RTC and shared I2C bus ownership.
- OLED display (SSD1306/GFX libraries).
- Wi-Fi SoftAP and DHCP behavior.
- NVS-backed configuration, credential and PIN-attempt state.
- LittleFS-backed records and roster data.
- ESP32 random sources for security/setup paths.

The exact pin mapping, power, sensor template behavior, display, AP/DHCP and RTC behavior must be confirmed against `HANDOFF_FINAL.md`, the hardware specs and current device. Never infer electrical wiring from code alone. The legacy sketch has a demonstration SoftAP credential and a known duplicate-template issue; it is not production firmware.

## 7. Security boundaries

The repository contains core PIN lockout/session/token helpers, NVS persistence adapters, RNG adapters and AP components. That is not proof that every production route is protected. C6 remains blocking because the production HTTP adapter/routes, request-bound authorization, PIN verifier/KDF writer and session/fault enforcement are not fully integrated.

The signed security boundary and residual risks are recorded in the handoff/decision ledger. Do not silently upgrade the threat model, expose reporting PII, invent a PIN KDF, or add a new credential/reset path to make a demo easier. The current NVS erase guard is a narrow guard over specific call surfaces, not a universal flash erase firewall.

## 8. SF2 reporting workstream

`web/sf2/` contains pure JavaScript reference models for attendance cell mapping, totals, learner row layout/pagination, month roster projection, reporting identity, snapshot schema/adaptation/join and a browser-side snapshot coordinator model. Its Node suite currently has 270 registrations across 12 files.

SF2 remains independent from a production device API. It does not provide a durable server-side coordinator, authoritative reviewer verification, complete reporting PII persistence, approved template writer, production dashboard or final XLSX export. Browser/model tests establish only their local rules. School/LIS-dependent requirements remain in `docs/sf2/TEACHER_QUESTIONS.md` and the SF2 decision/specification set.

## 9. Build and verification targets

| Command/config | Purpose | Forbidden inference |
|---|---|---|
| `pio test -e native` | Full C++ host test suite | Does not prove ESP32 runtime or hardware. |
| `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2` | Inert adapter compile/link + selected symbol checks | **Never flash.** Not attendance firmware. |
| `pio run -c platformio.esp32-probe.ini -e esp32-hardware-probe -j 2` | Diagnostic hardware-probe build | Not a production attendance image. Flash only as an explicitly authorized diagnostic after source/wiring review. |
| `cd web/sf2 && npm test` | SF2 Node reference-model suite | Does not prove device API, reporting authority or workbook fidelity. |

Pinned target details live in the PlatformIO configs. Keep native, smoke and probe environments separate.

## 10. Change protocol

Before changing a subsystem, read its specification from `docs/SPECS/INDEX.md`, inspect the implementation and public header, trace all callers, inspect owning/dependent tests, and identify active blocker/decision IDs. Changes to product rules, sensor mapping, persisted formats, recovery, authentication, PII or reporting authority require the approval/delegation documented in `docs/DECISIONS.md`.

Update the module spec only when its contract changes; update `ARCHITECTURE.md` only when subsystem boundaries, ownership, dependency or integration state changes; update `WORKING_MEMORY.md` only when the current state changes. Release evidence must state precisely what was run and what was not.


### First-run provision rebootstrap boundary

`ConfigBootCoordinator::provision()` makes its own state READY after durable read-back, but the static target `Application` was composed under the original NEEDS_SETUP branch. It must not hot-construct READY-only services after an HTTP mutation. The target's one-shot restart gate now waits for first-run boundary completion and `ACSS_PROVISIONED`, retries a pending OLED blank transfer no more than once per second, and invokes `esp_restart()` only after the synchronous `WebServer::handleClient()` call returns. A fresh startup then reloads NVS and constructs the ordinary READY graph. This source-only gate is currently unreachable because the boot shell still has only GET `/` and GET `/api/status`; the future setup route remains a separate blocker. Physical OLED clearing and ESP32 reboot/reload behavior remain unverified.


### First-run request parsing is a separate integration gate

`first_run_setup_request.*` defines a strict 768-byte JSON schema for the complete non-secret initial Config plus the E33 setup code and new admin PIN. `esp32_first_run_http_handler.*` is a fixed-buffer raw-body adapter candidate that delegates to the existing `FirstRunSetupCodeBoundary`; neither is currently wired into `main.cpp`.

The pinned Arduino-ESP32 WebServer `Parsing.cpp` uses the raw-body callback only for non-multipart content. Its multipart `_parseForm()` path ignores its total Content-Length argument and appends lines to per-field `String` values before the request handler. The adapter cannot reject such a body before those allocations. Accordingly, the production target remains GET-only. A route must not be registered until the framework-level multipart path has a strict total body/time/field bound before allocation, or the target uses a bounded replacement parser. This is a source-inspected blocker, not runtime evidence.
