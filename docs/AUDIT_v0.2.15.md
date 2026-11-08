# Specialized source audit — v0.2.15

Date: 2026-11-06  
Scope: one-shot config boot/provision coordinator and ESP32 NVS backend contract.  
Execution: static source/documentation/package audit only. No compiler, PlatformIO test, firmware build or device test was run, per the user's instruction.

## Implementation reviewed

`ConfigBootCoordinator` wraps the platform boot backend and the existing `ConfigProvisioner`:

1. `start()` is accepted only from COLD and immediately latches FAULT as its fail-closed default.
2. Backend `begin()` must succeed before `load()` is called.
3. A successful unprovisioned load transitions to NEEDS_SETUP, but does not expose a `Config` pointer or slot selection.
4. A valid A/B selection must have `CB_OK`, active slot 0 or 1, nonzero generation, and a valid Config before READY is exposed.
5. Provisioning is accepted only from NEEDS_SETUP and retains the injected `IConfigProvisionGate` requirement.
6. Authorization and invalid-config rejection remain retryable because they happen before storage mutation. All other provision errors latch FAULT.
7. After the provisioner reports success, the coordinator calls the backend's canonical `load()` path again and compares active slot, generation and the complete Config before transitioning to READY.
8. `config()` returns null and `selection()` leaves its output unchanged unless READY.

`Esp32NvsConfigBackend` now implements `IConfigBootBackend` through its existing `begin()`/`load()` methods. Production NVS behavior was not changed in this increment.

## Static audit checks

- All 16 new `RUN_TEST` registrations have unique test function names and a corresponding function body.
- The documented suite inventory sums to 311 registrations across 18 suites.
- The user-reported v0.2.14 baseline is recorded as 295/295 passed; the 16 v0.2.15 registrations are not reported as passing.
- `HANDOFF_FINAL.md` and `docs/DECISIONS.md` are unchanged from v0.2.14.
- No E18/O6/E12/E13/E15/E16/E17/E22 or other DELEGATED decision was changed.
- No `nvs_flash_erase`, automatic NVS format/erase path, LittleFS operation, key derivation, HTTP endpoint schema, or setup/PIN security implementation was introduced.
- The config boot spec documents that the caller must map NEEDS_SETUP to `H_UNPROVISIONED` and FAULT to `H_CONFIG_FAULT` before constructing normal runtime objects.
- Manifest and extracted archive integrity were checked after packaging.

## Findings and limits

- **No production boot caller exists yet.** The class makes the safe sequence available, but only a future firmware entry point can enforce that it is constructed before all consumers and map its state into `HealthFsm`.
- **No setup-code security implementation exists.** `IConfigProvisionGate` remains a caller contract; this class does not implement C6, lockout persistence, or PIN-change rules.
- **NVS encryption remains unresolved (C16).** The config contains the AP passphrase and PIN hash/salt.
- **NVS power-loss behavior remains unverified (U39).** Successful `nvs_commit()` and reload in source are not a physical power-cut proof.
- **No exact ESP32 build was run.** The new pure-core class and inheritance change have not been compiled against the pinned Arduino-ESP32/ESP-IDF toolchain.
- **The 16 new native tests are authored only.** They have not been executed. The 295/295 result applies to v0.2.14, as reported by the user.
- This does not complete C1. Health wiring, trusted config-gate inputs, target build, provisioning security, NVS encryption decision and power-cut testing remain open.

## Audit disposition

No static blocker was identified in the new coordinator's intended state transitions. This is a development snapshot, not a verified firmware boot implementation. The next integration step is to instantiate this coordinator before runtime services and wire READY/NEEDS_SETUP/FAULT into the actual serialized firmware boot path and `HealthFsm`.
