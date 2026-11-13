# Config A/B commit/apply coordinator (v0.2.13)

Status: hardware-independent coordinator plus an ESP-IDF NVS backend are authored. The user reported 295/295 native tests passed on v0.2.14, covering the v0.2.11–v0.2.13 suites. v0.2.15 adds a boot/provision wrapper; exact target build and power-cut behavior remain unverified.

## Boot sequence

1. Construct `Esp32NvsConfigBackend` and `ConfigBootCoordinator`; call `ConfigBootCoordinator::start()`. The coordinator calls backend begin then canonical load. It exposes no config unless the selected A/B slot is valid.
2. On `CFG_BOOT_NEEDS_SETUP`, run the first-run wizard and call `ConfigBootCoordinator::provision()` with a real setup/health gate. The coordinator delegates to `ConfigProvisioner`, which rechecks the durable `ATTP` marker and both slots, writes only slot A at generation 1, and verifies the slot bytes. The boot coordinator then reloads through the canonical backend path and compares config/slot/generation before entering READY.
3. On `CFG_BOOT_FAULT`, do not construct any normal runtime or substitute defaults. Map the fault to `H_CONFIG_FAULT`; on NEEDS_SETUP, map to `H_UNPROVISIONED`. The boot coordinator does not own `HealthFsm` or implement C6 security.
4. Only after `CFG_BOOT_READY`, construct live services from the exact config and selected slot/generation. Then construct `ConfigApplyCoordinator` with the same backend, runtime bridge, config and selected selection. Full firmware boot composition remains unimplemented.

## First configuration provisioning

`ConfigApplyCoordinator` deliberately cannot update a `CB_UNPROVISIONED` device. First setup is a distinct path implemented by `ConfigProvisioner` in `lib/core/src/config_provision.*`.

- It requires `IConfigProvisionGate::mayProvision()` to return true. The production implementation must only do so after the setup-code flow and health permissions have passed; this interface does not enforce C6 by itself.
- It accepts only a `CB_UNPROVISIONED` boot result, then re-reads both slots to reject a stale result or a concurrent/foreign write.
- It validates the initial config, writes only slot A with sequence 1, reads both slots back, and verifies exact config equality plus slot B still blank.
- A false write is indeterminate even if bytes may have persisted. A post-write read/verification failure is also latched. The caller must set config fault and require reboot/reload; no in-place retry is allowed.
- On success, construct live services from that exact config and returned `CfgPick`, then construct `ConfigApplyCoordinator`. Do not run scan/enrollment before the setup/security/health state is established.

The NVS backend creates/commits the `ATTP` marker together with first slot write. A marker only detects deletion of both slot keys if the marker survives; whole-partition erase clears it too.

## NVS layout and semantics

- Default NVS partition label: `nvs` (as defined by the product partition table).
- Namespace: `attcfg`. Config keys: `slot_a`, `slot_b`. Each value must be exactly `CFG_SLOT_SIZE` (192) bytes. A `provisioned` 4-byte marker (`ATTP`) is committed with the first successful slot write. A valid legacy config without a marker is migrated by writing/committing the marker during boot before normal operation is allowed.
- `load()` checks the marker on every boot. Both blank slots plus a valid marker is `CB_FAULT`, preventing a wiped pair of slot keys from silently appearing as first boot. A malformed marker faults even if a config slot is valid. A valid legacy config without a marker is migrated; failure to persist the marker blocks normal boot. The marker cannot detect full-partition erase because that erases the marker too. The first-provision path uses the provision-only `IConfigProvisionBackend::readProvisionMarker()` contract to check it independently before accepting blank slots; marker-read errors fail closed.
- `readSlot()` uses a read-only handle. A missing namespace or key is represented as 192 bytes of `0xFF`, which is the core blank-slot encoding. A present blob with the wrong length is an error, not blank. Other NVS read errors propagate as backend failure. The caller's output buffer is unchanged on read error.
- `writeSlot()` accepts only slot 0 or 1, opens read/write, calls `nvs_set_blob()`, calls `nvs_commit()` only after a successful set, and closes the handle on every opened path. A false return is indeterminate: the coordinator must latch fault because the data may have persisted despite an error.
- The coordinator separately reopens the inactive slot through `readSlot()`, decodes it, and checks both generation and config equality before changing live state.
- No operation calls `nvs_flash_erase()`, `nvs_flash_erase_partition()`, or any format/reset path.

## Transaction order

1. Validate/gate using the coordinator-owned generation.
2. Perform side-effect-free runtime preflight.
3. Encode generation `current + 1`; refuse `UINT32_MAX` rather than wrap.
4. Write only the inactive slot; backend calls `nvs_set_blob()` then `nvs_commit()`.
5. Reopen/read back/decode/verify generation and content.
6. Apply the config to `As608AttendancePipeline`/`LogAttendanceAdmission`, then call `ClosureEngine::setSchedule()`. No main-loop processing may interleave these calls.
7. Update coordinator RAM state only after all prior steps succeed.

## Failure rules

- `nvs_flash_init()` error: config load unavailable; do not erase, auto-format, or construct runtime with defaults.
- Slot read error or malformed-length present blob: `CB_FAULT`, never blank/unprovisioned.
- Write/set/commit false: indeterminate; latch config fault and stop normal processing until reboot re-reads both slots. An error may still leave a valid newer slot on flash; boot selection decides.
- Read-back I/O, decode, generation or content mismatch latches fault. Runtime is not updated.
- Runtime application unexpectedly fails after verified commit: persistent state is newer than RAM; caller must enter `H_CONFIG_FAULT` and stop normal processing. Reboot must reload both slots.
- The backend does not itself update `HealthFsm`; firmware must map any initialization/load/coordinator fault into `H_CONFIG_FAULT` and disable scan/enroll/manual operations according to `HealthFsm::perms()`.

## Important limits

- This is a standalone ESP-IDF adapter source, not a wired `main.cpp`; there is no production firmware entry point or exact `prod` PlatformIO environment yet.
- `ConfigGateIn.windowOpen` and `unclosedPast` are caller-provided facts. `deriveConfigWindowOpen()` now derives only the current timetable window from a trusted RTC epoch. C17 prevents a safe historical `unclosedPast` derivation until schedule history is persisted; do not pass `false` as a guess. E26 now defines an inclusive term end in source, but its current regressions are NOT RUN and no production day-view route uses `apiDayInInclusiveTerm()`. Production schedule changes must remain disabled until C17/E2 and E26 verification are resolved.
- The config blob contains the Wi-Fi passphrase and PIN hash/salt. This backend calls ordinary NVS APIs and does not itself configure encryption. Actual encryption depends on the final ESP-IDF build configuration and key provisioning, neither of which exists in this snapshot (C16). Do not assume physical-flash confidentiality.
- `nvs_commit()` success is the required NVS API commit call, not proof of behavior under every supply-failure timing. Run power-cut/reboot tests on the exact hardware.
- API semantics were checked against ESP-IDF v5.5.5 documentation. The recorded PlatformIO framework package `4.30312.261024` maps to Arduino-ESP32 3.3.12 based on ESP-IDF 5.5.5; no target build was executed.
- The coordinator does not persist PIN changes or Wi-Fi password changes as independent operations. Their dedicated endpoints need separate C6 enforcement.

## v0.2.16 gate-fact status

`deriveConfigWindowOpen()` now safely derives the current scan-window fact from the validated current config and a caller-verified RTC epoch; error paths set `windowOpen=true`. `unclosedPast` is deliberately not automated. C17 shows that the current timetable is not necessarily the timetable that applied to prior dates, because no schedule-effective boundary/history is persisted. Do not pass `unclosedPast=false` based only on the currently loaded timetable. E2 is DELEGATED and must be approved before changing config slot encoding or adding schedule-history records.
