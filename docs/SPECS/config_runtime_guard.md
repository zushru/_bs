# Config runtime guard (v0.2.16)

Status: hardware-independent helpers. All seven regression cases are included in `test_config_guard`; the user-supplied v0.2.36 full native run reports 413/413 passing cases across all 23 suites. This is host-test evidence only. The helper is not a production boot or HTTP adapter.

## Boot state to HealthFsm

`updateHealthFromConfigBoot()` copies only the config/provisioning facts into a caller-supplied `HealthInputs` value, then calls `HealthFsm::update()`.

| ConfigBootCoordinator state | Health inputs set by helper | Result, assuming all other health inputs are healthy |
|---|---|---|
| `CFG_BOOT_READY` | `provisioned=true`, `cfgOk=true` | `H_OK` |
| `CFG_BOOT_NEEDS_SETUP` | `provisioned=false`, `cfgOk=true` | `H_UNPROVISIONED` |
| `CFG_BOOT_COLD` or `CFG_BOOT_FAULT` | `provisioned=true`, `cfgOk=false` | `H_CONFIG_FAULT` |

COLD is deliberately not interpreted as blank NVS. The caller must call `ConfigBootCoordinator::start()` before normal boot proceeds. The helper does not implement the setup-code verifier, durable lockout adapter, provisioning wizard, or production runtime construction. v0.2.31 adds a generic `PinAuthCoordinator`, but no concrete NVS/PBKDF2 adapter or boot wiring exists. It preserves all RTC, storage, sensor and `reverified` inputs supplied by the caller.

## Current scan-window fact

`deriveConfigWindowOpen()` validates the selected config and requires a trusted RTC epoch. It delegates the boundary calculation to the existing `windowOpenNow()` implementation, which evaluates weekday and scan-window rules but does not apply `termStart`/`termEnd`. This can conservatively report an open timetable window outside the configured term; it must not be interpreted as a full term-eligibility result. On any failure it sets `windowOpen=true` and returns an error, so a caller that ignores the status still blocks a schedule change.

The helper intentionally does **not** claim to derive `unclosedPast`.

## Blocking historical-schedule defect (C17)

The persisted v0.2.15 config has no effective-from timestamp or timetable version history. `ClosureEngine::setSchedule()` replaces the engine's current `Params`/`Timetable`; `ClosureEngine::step()` later scans historical days from `termStart` through today using that current timetable. After a schedule change, this can create an `EV_CLOSE` for a historical session that was not enabled on that historical date. Reboot has the same risk because the selected current config is used to reconstruct the engine.

Therefore, deriving `unclosedPast` by scanning historical dates against today's timetable is unsafe. It may invent closures or misclassify sessions from an older timetable. v0.2.16 does not add such a helper and does not enable schedule config writes.

A durable fix requires a decision on how to persist the schedule-effective boundary/history. The config slot layout choice E2 is DELEGATED; changing it without approval is prohibited. A single last-change timestamp is insufficient if multiple schedule revisions must remain reconstructible. Candidate designs include a bounded versioned schedule-history structure with effective-from days, or durable schedule-change events with explicit transaction/recovery semantics. The chosen design must define how history and the active config remain consistent across power loss. Do not implement either until the user approves the direction and its failure ordering.

## Term-end boundary finding (C18)

The current source has a second boundary inconsistency: `dayChecks()` accepts `day == termEnd`, the attendance pipeline rejects it, and `ClosureEngine` has no term-end input. The config specification does not define inclusive versus exclusive semantics. v0.2.16 does not guess. Decide that rule first, then align API, scan, closure and day-view behavior with boundary tests.

## Serialized-call contract

- Call boot mapping only after `ConfigBootCoordinator::start()` has returned, unless deliberately using the COLD -> `H_CONFIG_FAULT` fail-closed mapping.
- Call window derivation on the serialized application loop using the current validated config and a timestamp whose clock trust comes from the health/RTC path.
- If the result is not `CFG_WINDOW_OK`, do not call `ConfigApplyCoordinator::apply()` for a schedule change.
- Do not fabricate `ConfigGateIn.unclosedPast=false`. C17 must be resolved before production schedule changes are enabled.
