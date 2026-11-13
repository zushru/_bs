# ATT ESP32 attendance target — boot shell

This target is the first production-oriented composition root. It is deliberately limited to a protected AP and a canonical read-only local dashboard. It does not implement attendance or administration yet.

## Build

```sh
pio run -c platformio.attendance.ini -e att-boot-shell -j 2
```

## Canonical dashboard

The single source is web/dashboard/index.html; UI/API boundaries are documented in web/dashboard/README.md. tools/embed_dashboard.py generates dashboard_html.h, and the attendance PlatformIO environment runs that generator before compilation. Edit the HTML source, not the generated header. The embedded page uses only the existing same-origin GET /api/status endpoint. It keeps the six reference navigation destinations, but pages without real production APIs are explicitly unavailable. There are no sample students, mock attendance records, simulated actions, credential fields or write requests.


The target uses the signed 4 MB partition table in `partitions_att.csv`:

- NVS: `0x9000`, `0x5000`
- OTA metadata: `0xE000`, `0x2000`
- Factory application: `0x10000`, `0x180000`
- SPIFFS/LittleFS data partition: `0x190000`, `0x270000`

On `CFG_BOOT_READY`, the target mounts the signed `spiffs` partition without formatting or grow-on-mount, requests `read_only=true` at the LittleFS VFS boundary, validates the existing layout read-only, and replays the complete `MonthSet`/`LogStore` stream. `CFG_BOOT_NEEDS_SETUP` skips attendance storage. Neither path writes, truncates, repairs, quarantines or formats attendance files. The VFS flag is boot-shell-only defense in depth; its enforcement on the exact pinned target has not been runtime-verified, and it is not the writable mount design for future attendance operation. This source-level mount/replay does not establish target VFS durability or physical power-loss safety.

## Boot behavior

1. The NVS startup guard must report exactly one successful, non-erasing Arduino NVS initialization. A failed or ambiguous startup is terminal; no erase/recovery fallback is attempted.
2. `ConfigBootCoordinator` must load either `NEEDS_SETUP` or a validated `READY` configuration. Other states remain offline.
3. The shared I2C bus and OLED are initialized. First boot requires the OLED credential transfer to be requested successfully.
4. On `READY` only, the DS3231 calendar is read as UTC (offset zero). This is diagnostic only: the shell does not set the RTC, clear OSF, or trust the candidate clock. `NEEDS_SETUP` skips the RTC read.
5. The persistent PIN-attempt runtime starts before AP or HTTP. It uses monotonic time and loads the durable counter. A missing/corrupt/unreadable counter on `READY` is a boot fault; this shell does not expose the runtime through an HTTP route.
6. On `READY` only, the target mounts the signed `spiffs` partition at `/littlefs` without formatting. It validates the existing layout read-only, constructs `MonthSet` and `LogStore` over the storage owner's serialized file set, and performs a complete read-only log replay. A partition mismatch, mount/layout failure, or replay failure blocks AP and HTTP startup. `NEEDS_SETUP` skips attendance storage entirely.
7. The health state is evaluated from config, storage/replay and RTC observations. Clock trust remains false and no sensor owner is composed, so attendance is not enabled.
8. On `NEEDS_SETUP`, the AP credential coordinator creates a 128-bit random 32-character hex WPA2 credential in its dedicated `attwifi` namespace, reads it back, requests OLED disclosure, then starts the fixed `att` AP. The same credential is disclosed on every setup-required boot until provisioning succeeds.
9. On `READY`, the persisted `attwifi` credential must match the validated config credential. Mismatch or storage ambiguity prevents AP startup.
10. The HTTP server starts only after the AP is ready and exposes only `GET /` and `GET /api/status`. Other methods return 405; unknown GET paths return 404. No credential, roster, attendance or config payload is exposed.

## Current scope

- No setup/provision POST route.
- No PIN verifier, KDF writer, browser session or admin authentication route.
- No attendance scanning, enrollment, corrections, closure or scheduler.
- On `CFG_BOOT_READY`, a no-format LittleFS mount, read-only layout validation and complete log replay run before AP/HTTP startup; `NEEDS_SETUP` skips attendance storage. No attendance-file mutation, repair or recovery is enabled.
- No reporting identity companion, XLSX/SF2 export or report route.
- The dashboard explicitly states that attendance and privileged functions are disabled. Its read-only status fields reflect only the public status response; no attendance, roster, timetable, admin or report data is fabricated.

Do not claim this is a usable attendance release. It is a boot/network composition milestone that preserves fail-closed behavior while the required authentication and application services are completed.

## First hardware check

Do not flash while the existing soak is running. Before a separately authorized flash, verify the current hardware partition map. The previously uploaded device diagnostic reported `spiffs` at `0x290000/0x160000`, which does not match this signed layout. This target must not be flashed to that device with an assumed partition table; resolve the discrepancy first. This build does not erase flash or perform a hardware upload.


## First-run request adapter status

The strict proposed JSON request schema and a fixed-buffer raw-body handler candidate are documented in [`docs/SPECS/first_run_setup_request.md`](../../docs/SPECS/first_run_setup_request.md). They are source-authored only; there is no active setup POST route and the boot shell remains GET-only. The pinned Arduino-ESP32 WebServer multipart parser currently ignores its total Content-Length argument and may allocate unbounded per-field strings before the handler. Do not enable the candidate route until that pre-handler parser path is fixed or replaced by a bounded parser.
