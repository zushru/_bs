# Specialized source audit — v0.2.16

Date: 2026-11-06  
Baseline: uploaded `att_v0.2.15_2026-11-06.tar.gz`  
Baseline SHA-256: `c27f2d0f4932382df526e433fe76f085cb3c6e3bec9485a35b539e2f5029ea7b`

## Audit scope

- Read `docs/CONTEXT.md`, `docs/CRITICAL.md`, `HANDOFF_FINAL.md`, `docs/DECISIONS.md`, and `docs/UNVERIFIED.md` before editing.
- Reviewed the boot/provision lifecycle, `HealthFsm`, API window calculation, timetable rules, config validation, config apply gate, attendance scan pipeline, and closure sweep.
- Compared the working tree against the uploaded v0.2.15 archive.
- Recounted source `RUN_TEST()` registrations and verified archive/manifest integrity.
- No unit tests, compiler, PlatformIO command, ESP32 target build, or device operation was run, as requested.

## Implemented in v0.2.16

### 1. Config boot → HealthFsm adapter

New `updateHealthFromConfigBoot()` maps:

- `CFG_BOOT_READY` → `provisioned=true`, `cfgOk=true`.
- `CFG_BOOT_NEEDS_SETUP` → `provisioned=false`, `cfgOk=true`.
- `CFG_BOOT_COLD`, `CFG_BOOT_FAULT`, and unknown values → `provisioned=true`, `cfgOk=false`.

It preserves the caller's RTC, storage, sensor, and `reverified` inputs. COLD is not interpreted as blank NVS. The helper does not implement the setup-code gate, persisted lockout, provisioning UI, or production boot flow.

### 2. Fail-closed current-window derivation

New `deriveConfigWindowOpen()` validates the current config and requires a caller-trusted RTC epoch. It delegates weekday/window boundary logic to the existing `windowOpenNow()` implementation. On invalid config or untrusted time, it returns an error and sets `windowOpen=true`.

This helper answers only whether the current timetable has a scan window at the supplied local time. It does not establish historical closure state and does not apply the configured term bounds; outside-term results can be conservative (a timetable window may still be reported open).

### 3. Added regression source

`test/test_config_guard` contains seven authored cases for boot-state mapping, preservation of unrelated health faults, configured scan-window opening, and fail-closed error behavior. These tests were not executed.

## Specialized findings

### C17 — historical timetable version is not reconstructible

The persisted `Config` contains only the active timetable. `ClosureEngine::setSchedule()` replaces the live timetable. `ClosureEngine::step()` iterates historical dates from `termStart` through today using that current timetable and appends missing `EV_CLOSE` records.

After a schedule change, a later step or reboot can therefore synthesize a close for a session that was not scheduled on the historical date. A current-timetable scan cannot reliably determine `unclosedPast`.

A single last-change timestamp is insufficient if multiple schedule revisions must remain reconstructible. The design needs durable version history and a defined crash-consistent relationship between the selected config and its schedule history. E2 (A/B config layout) is DELEGATED; no schema or event model was changed. Production schedule changes must remain disabled.

### C18 — term-end is inconsistent across layers

Source inspection found:

- `dayChecks()` rejects only `day > termEnd`, accepting `day == termEnd` (inclusive).
- `As608AttendancePipeline::begin()` and `saveMatch_()` reject `day >= termEnd` (exclusive).
- `ClosureEngine` receives `termStart` but no `termEnd`; its historical close sweep continues through today without a term-end check.

This means a final-day manual mark can be accepted while a fingerprint attendance scan on that same date is rejected. Closure processing can also append `EV_CLOSE` events outside the configured term. The configuration spec does not choose inclusive versus exclusive semantics.

E26 is recorded as OPEN. No boundary interpretation or closure patch was guessed. Resolve E26, then align API, scan, closure, no-class, and day-view logic and add tests at `termEnd - 1`, `termEnd`, and `termEnd + 1`.

## Verification and packaging audit

- Source registrations: **318 across 19 suites** (counted from `RUN_TEST()` calls). This is not a passing-test count.
- User-reported verification: **295/295 on v0.2.14** only. The 16 v0.2.15 and 7 v0.2.16 registrations remain unexecuted.
- New module: `config_runtime_guard.h/.cpp`; new suite: `test/test_config_guard/test_main.cpp`.
- No existing tested core implementation was modified. In particular, `closure_engine.*`, `config_store.*`, `config_apply.*`, `config_boot.*`, and the NVS backend remain unchanged from the uploaded v0.2.15 baseline.
- `HANDOFF_FINAL.md` is byte-identical to the uploaded archive.
- Existing signed and delegated decisions were not changed. One new OPEN item, E26, records the unresolved term-end convention.
- No `.git/`, build output, or test cache is included in the release archive.

## Release limitations

This is a source development snapshot, not production firmware. There is no exact ESP32 build, no live firmware boot integration, no setup-code security enforcement, no NVS power-cut test, no LittleFS restore swap/recovery, and no sensor/device verification. C1 remains incomplete. Schedule changes must remain disabled until C17/E2 and C18/E26 are resolved.
