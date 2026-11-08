# Config boot and first-provision coordinator

Status: hardware-independent source added in v0.2.15; v0.2.16 adds a boot-state-to-HealthFsm mapper. New native cases are authored but unexecuted; exact ESP32 build and target boot wiring are not verified.

## Purpose

Prevent the firmware from constructing normal runtime services from default or partially loaded configuration. The coordinator owns the one-shot transition from cold boot to a validated existing config, explicit setup-required state, or fail-closed boot fault.

## Backend contract

`IConfigBootBackend` combines:

- `begin()`: initialize the platform storage backend without erasing or formatting it.
- `load(Config&, CfgPick&)`: load the canonical A/B selection, distinguishing blank/unprovisioned from I/O/corruption faults.
- `IConfigProvisionBackend`: read/write slots and inspect the durable first-provision marker.

The ESP32 implementation is `Esp32NvsConfigBackend`. It uses the default NVS partition and does not erase it. Target compatibility and power-cut behavior remain unverified.

## State transitions

| State | Meaning | Config exposed? | Allowed next action |
|---|---|---:|---|
| COLD | `start()` not called | No | `start()` |
| READY | A valid slot and selection were loaded or first provision was verified and reloaded | Yes | Construct normal runtime |
| NEEDS_SETUP | Backend loaded successfully and both config slots are unprovisioned | No | Setup flow, then `provision()` |
| FAULT | Backend init/load, slot validation, provision storage or post-provision reload failed/ambiguous | No | Stop normal boot; reboot/recovery path |

`start()` is one-shot. It latches FAULT before any backend side effect, and only explicitly changes to READY or NEEDS_SETUP after the complete checks.

`provision()` is accepted only from NEEDS_SETUP. It delegates to `ConfigProvisioner`, which requires an injected authorization gate, rechecks the durable marker and both slots, writes A/generation 1, and verifies the bytes. The boot coordinator then calls `load()` again and checks that the selected slot/generation and complete config match the provision result before exposing READY.

Only `NOT_AUTHORIZED` and `INVALID_CONFIG` are treated as correctable setup rejections. All other provision errors latch FAULT because storage state may be unknown or stale.

## Caller obligations

1. Construct the backend and coordinator before any pipeline, closure, sensor, web or display runtime that consumes config.
2. Never substitute `defaultConfig()` when the coordinator is not READY.
3. On NEEDS_SETUP, run the signed OLED setup flow and enforce C6/E12 requirements before calling `provision()`. `IConfigProvisionGate` is a boundary, not an implementation of setup-code lockout. v0.2.31 adds `PinAuthCoordinator` for the ordering primitive, but this boot coordinator does not construct or wire it.
4. Map NEEDS_SETUP to `H_UNPROVISIONED`; map FAULT to `H_CONFIG_FAULT` and stop normal operation. The coordinator does not own `HealthFsm` because RTC/storage inputs are supplied by the eventual application root.
5. Construct `ConfigApplyCoordinator` only after READY, and pass exactly the config and `CfgPick` returned by this coordinator.
6. All calls are serialized on the main loop. No cross-task safety is provided.

## Explicit non-goals

- Does not construct or wire `PinAuthCoordinator`; concrete setup-code/PIN verification, durable attempt-counter storage, PBKDF2 credential writing, HTTP route enforcement and health permissions remain absent (C6).
- Does not choose NVS encryption/key provisioning (C16).
- Does not prove NVS power-loss atomicity or full-partition erase detection.
- Does not create production `main.cpp`, sensor scheduler, RTC adapter, LittleFS driver, or web adapter.

## Health state handoff helper (v0.2.16)

`updateHealthFromConfigBoot()` maps READY to `provisioned=true/cfgOk=true`, NEEDS_SETUP to `provisioned=false/cfgOk=true`, and COLD/FAULT to `provisioned=true/cfgOk=false`. It preserves RTC, storage, sensor and re-verification inputs. COLD is not treated as blank NVS. The application must still call it in the actual boot path and must not construct runtime services until READY. It does not implement setup-code security or a production `main`.
