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

## Term-end boundary rule (C18 / E26)

On 2026-11-13, under the user's explicit delegation of engineering decisions, PISHDA selected **inclusive** `termEnd`: the configured final local calendar day belongs to the term. The source contract is `0 <= termStart <= termEnd <= 31446`; one-day terms are valid. The 31446 ceiling is stricter than the 16-bit day field: epoch timestamps are uint32 Unix seconds, so 2106-02-05 (day 31446) is the last full local day representable at every allowed UTC offset.

The source now applies this boundary in API mark/no-class validation, AS608 scan admission and match commit, closure tick/session creation and outage clipping, config validation, first-run setup parsing, and `ClosureEngine` config matching/rebinding. `apiDayInInclusiveTerm()` is the shared predicate that every future day-view/read adapter must call before `buildDayView()`; there is no production day-view route today. Regression source was added, but no tests were run in this increment. C18 is therefore source-implemented, not verified or release-cleared. This decision is DELEGATED, not SIGNED.

C17 remains independent and blocking: `ClosureEngine` still has no persisted timetable-effective history. Do not enable schedule-changing requests or claim historical closure correctness until E2/C17 are resolved.

## Serialized-call contract

- Call boot mapping only after `ConfigBootCoordinator::start()` has returned, unless deliberately using the COLD -> `H_CONFIG_FAULT` fail-closed mapping.
- Call window derivation on the serialized application loop using the current validated config and a timestamp whose clock trust comes from the health/RTC path.
- If the result is not `CFG_WINDOW_OK`, do not call `ConfigApplyCoordinator::apply()` for a schedule change.
- Do not fabricate `ConfigGateIn.unclosedPast=false`. C17 must be resolved before production schedule changes are enabled.
