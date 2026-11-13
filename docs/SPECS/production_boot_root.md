# Production boot root — prototype slice

Status: **implemented as a read-only boot shell; not a production release**.

## Target and ownership

- PlatformIO environment: `att-boot-shell` in `platformio.attendance.ini`.
- Source root: `target/esp32_attendance/main.cpp`.
- Platform: pinned `espressif32@8.0.0`, Arduino framework, ESP32 Dev Module, 240 MHz board default, 4 MB flash.
- The local `Application` object is constructed inside Arduino `setup()` so `Esp32I2cBusOwner` creates its FreeRTOS lock only after framework initialization.
- One serialized Arduino loop owns the config coordinator, AP lifecycle and `WebServer`; no AP lifecycle operations are exposed over HTTP.

## Startup order

1. Require `Esp32NvsStartupGuard::status().safeForApplication`. No application-level NVS erase, retry or format path exists.
2. Start `Esp32NvsConfigBackend` through `ConfigBootCoordinator`; only `CFG_BOOT_NEEDS_SETUP` and `CFG_BOOT_READY` may continue.
3. Initialize the shared I2C bus once at SDA 21 / SCL 22, 100 kHz, then initialize the fixed 128x32 SSD1306 OLED at `0x3C`.
4. On `READY` only, read the DS3231 calendar/status using the signed UTC-register convention (E32, offset `0`). Classify bus/read failure, invalid calendar, OSF set and pre-2020 epoch separately. This probe never sets the RTC or clears OSF and does not block AP startup. `NEEDS_SETUP` skips it.
5. Start the single `Esp32PinAuthRuntime` using seconds from `esp_timer_get_time()`. The NVS attempt counter is initialized only in `NEEDS_SETUP`; any existing value is preserved. In `READY`, a missing, corrupt or unreadable counter is a boot fault and is never recreated. An available OLED receives an `AUTH / FAULT` transfer request, then AP/HTTP startup stops. This loads the durable attempt state but does not enable any route.
6. On `READY` only, mount the signed `spiffs` partition without formatting and run the read-only `LittleFsStorageOwner` per-file layout/semantic validation. Then construct `MonthSet` over its serialized `IFileSet` and call only `LogStore::open()` for complete cross-month stream validation/replay. Require successful replay before publishing `baselineEpoch`; any mount, layout or replay failure blocks AP/HTTP. No file is created, appended, truncated, repaired or quarantined. `NEEDS_SETUP` skips attendance storage entirely.
7. Evaluate `HealthFsm` from config boot, storage/replay readiness, RTC presence/OSF/calendar/floor/log-baseline facts and explicit `clockTrusted`. The boot shell always supplies `clockTrusted=false`, so a plausible candidate remains `H_CLOCK_FAULT` until a future explicit confirmation workflow exists. Sensor health is not ready because no sensor owner is composed.
8. On `NEEDS_SETUP`, delegate secret generation, durable create-if-absent storage, readback, OLED disclosure and WPA2 AP startup to `ApCredentialSetupCoordinator`. OLED/bus failure prevents first-boot AP startup. On `READY`, validate/readback the separate `attwifi` AP credential against `Config::wifiPass`, clear the temporary buffer on every path, and start the AP only on exact match.
9. Start the read-only HTTP status service only after the AP reports ready.

## HTTP contract


- `GET /`: responsive static status page; it distinguishes `NEEDS_SETUP` from `READY`.
- `GET /api/status`: read-only JSON containing boot state, RTC candidate/classification, log replay result/count/baseline, HealthFsm state, `clock_trusted:false`, and explicit feature-disabled flags.
- Other methods: HTTP 405. Unknown GET paths: HTTP 404.
- No CORS, no reflected input, no browser storage, no external assets, no credentials or learner information.
- Security headers include no-store, nosniff, no-referrer and a restrictive CSP.

## Deliberately disabled

Provisioning POST, HTTP PIN verification, browser sessions/token issuance, admin routes, configuration changes, attendance scan, AS608 enrollment, scheduler, manual corrections, closure, backups, reporting metadata and XLSX/SF2 export remain disabled. Core source now includes `AdminSetupCodeBoundary` for the P15 setup-code-gated endpoints; it is not usable without a concrete `ISetupCodeVerifier`, a production entropy-source binding for the generic generator and durable credential storage, the E33 lockout-counter mapping, OLED disclosure, and request-bound HTTP callers. The generic session `authorize()` method continues to deny these endpoints. `Esp32PinAuthRuntime` only loads the durable attempt counter and owns the core coordinator; no HTTP handler calls it. Source now includes the writer-only private A/B PIN-credential update path, `Esp32MbedTlsAdminPinWriter`, and a boot-bound `Esp32MbedTlsPinConfigRuntime` that refreshes the live verifier after commit and disables its accessor on boot fault. None is composed in `main.cpp`. Core source now has an `AdminSessionBoundary::changeAdminPin()` seam joining exact request-token/health authorization to current-PIN verification and the injected writer; success revokes sessions and indeterminate failure latches the boundary. This is not a route or production composition. The writer/runtime require the same injected PBKDF2 iteration count; E35 fixes it at 2,048. No PIN-change route is enabled. LittleFS is mounted only on `READY` in no-format read-only mode for per-file validation and full `MonthSet`/`LogStore::open()` replay. READY boot now then attaches and validates `EventGroupAudit`, replays the operational `RosterStore` and attaches the exact-config `LogAttendanceAdmission` observer. E27’s stricter `RosterTransitionAudit` remains a monthly-reporting guard and is not an operational-readiness check. No storage mutation or durability claim is enabled. Existing READY devices missing `attsetup` require E36’s explicit non-destructive USB migration; it must not initialize the counter silently at ordinary boot. The boot shell must not be represented as a complete attendance system.

## Remaining gates

- Use E35’s 2,048-iteration PBKDF2 policy and compose the existing PIN writer with a real config-runtime adapter; then wire request-bound login, lockout and response-confirmed session issuance without weakening `IConfigProvisionGate`.
- Wire the generic P15 generator to an approved entropy source; implement credential storage, OLED disclosure and acknowledgment, a concrete verifier and approved lockout-counter mapping; then wire request-bound provisioning and the complete configuration wizard. The source boundary alone does not authorize these routes.
- The shell now compares a valid UTC candidate with the replayed log baseline and feeds the result into HealthFsm. `HealthInputs::clockTrusted` remains false because explicit clock confirmation and `ClockPolicy` application are not integrated. Build the authorized confirmation/write/replay path before attendance is enabled.
- Compose AS608 owner and `LinearAs608PageResolver`; verify U5 bit-order/page behavior on the actual sensor.
- Resolve user-reported vs signed partition discrepancy before any flash or LittleFS mount.
- Implement transactional attendance persistence/restore and the R18 encrypted reporting companion protocol before real learner identity reporting.
- Add a production runtime loop with scheduler, scan admission, output feedback, safe admin operations and end-to-end tests.
