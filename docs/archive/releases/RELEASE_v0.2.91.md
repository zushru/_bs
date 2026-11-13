# v0.2.91 — C1/C34 NVS access-order gate

**Status: development-only. C1/C34 remain BLOCKING; no production firmware root exists.** This increment closes the accidental early-call path through the AP credential and PIN-attempt adapter APIs. It is not a complete NVS or flash-erase firewall.

## Baseline and integrity

- Root: `/root/att`; baseline README marker `v0.2.90 (2026-11-11)`.
- Baseline `MANIFEST.sha256`: 439 entries, verified before source edits. No Git metadata.
- Protected files were not changed: `HANDOFF_FINAL.md` SHA-256 `4e231004b90538651361846a0a3fc574270376c850a35bcfc2e7021da52fff1d`; `docs/DECISIONS.md` SHA-256 `52856953239e90c4e834d777a7be04db2d1b16bee3aab7ca5cec6e8d711297e3`.

## Change

- `Esp32NvsApCredentialStore` and `Esp32NvsPinAttemptStore` now require a non-owning `ConfigBootCoordinator` reference at construction. Each public operation checks `Esp32NvsStartupGuard::status()` and requires that same coordinator to be `READY` or `NEEDS_SETUP` before `nvs_open()`; `COLD`/`FAULT` returns `ESP_FAIL` without NVS access.
- The coordinator reference must outlive each adapter. The production root must pass its single authoritative coordinator instance; this is an API-level guard against accidental early calls, not protection against raw NVS calls or deliberate construction/use of another coordinator.
- `Esp32PinAuthRuntime` accepts the boot coordinator at construction, binds its private attempt store to the same object, and now exposes `start(nowS)` rather than accepting a potentially different coordinator at start time.
- The inert smoke composition now declares `Esp32NvsConfigBackend` and `ConfigBootCoordinator` before the dependent adapters so global object lifetimes are valid. Its setup still performs no NVS operations.
- No NVS key, namespace, schema, marker, commit ordering, startup wrapper, signed rule or delegated E12 counter-domain choice changed.

## Files changed

Production/HAL and target composition:
- `lib/hal_esp32/src/nvs_ap_credential_store.h`
- `lib/hal_esp32/src/nvs_ap_credential_store.cpp`
- `lib/hal_esp32/src/nvs_pin_attempt_store.h`
- `lib/hal_esp32/src/nvs_pin_attempt_store.cpp`
- `lib/hal_esp32/src/esp32_pin_auth_runtime.h`
- `lib/hal_esp32/src/esp32_pin_auth_runtime.cpp`
- `target/esp32_adapter_link/main.cpp`

Tests:
- `test/test_nvs_ap_credential/test_main.cpp`
- `test/test_nvs_config/test_main.cpp`

Documentation/release records:
- `README.md`, `RELEASE.md`, `RELEASE_v0.2.91.md`
- `docs/AUDIT_v0.2.91.md`, `docs/CHANGELOG.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/DEVELOPMENT_ROADMAP.md`, `docs/SPECS/config_boot.md`, `docs/TESTS.md`, `docs/UNVERIFIED.md`, `docs/WORKING_MEMORY.md`
- `MANIFEST.sha256` (regenerated below)

## Test-first and focused results

Red command:

```sh
pio test -e native -f 'test_nvs_ap_credential' -f 'test_nvs_config'
```

The new compile-time assertions failed as intended because AP store was default-constructible and PIN store accepted a namespace alone. Log: `/tmp/att_c1_nvs_gate_red.log`; no Unity runtime cases ran in red phase.

Green command:

```sh
pio test -e native -f 'test_nvs_ap_credential' -f 'test_nvs_config'
```

Result: **65/65 passed, exit 0** (`test_nvs_ap_credential` 17/17; `test_nvs_config` 48/48). This includes new tests proving both APIs reject `COLD` and `FAULT` without opening NVS, existing unsafe Arduino-startup rejection, and the unchanged READY/NEEDS_SETUP counter/credential semantics. Log: `/tmp/att_c1_nvs_gate_green3.log`.

Current inventory: **728 C++ RUN_TEST registrations across 39 suites**. The other 663 registrations were not run. Full native N1, SF2, and `test_config_boot` were not run; the latter was not directly changed, and the adapter tests exercise the state interface they consume.

## ESP32 target attempt

Exact command:

```sh
pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

Pinned target packages included Espressif32 8.0.0, Arduino-ESP32 3.3.12, ESP-IDF 5.5.5, Xtensa 14.2.0+20260121, and esptool 5.4.0. `target/esp32_adapter_link/main.cpp.o` compiled with the new constructors/runtime signature; bootloader and partition images generated; Wire, SPI and Adafruit BusIO compiled/archived. The process was terminated after 7m37s during third-party Adafruit GFX compilation to conserve the phone. It had not reached the project HAL TUs or final link. No `firmware.elf` or `firmware.bin` exists. Log: `/tmp/att_c1_nvs_gate_esp32.log`. The build process was terminated and no PIO/SCons/Xtensa child remained. This is incomplete target evidence, not a build pass.

## Critical residuals

1. The gate protects only these two C++ adapter APIs. Code with direct access to ESP-IDF NVS can bypass them. A caller could also bind to a different coordinator; the real application root must construct one authoritative instance and pass it to the adapters.
2. `ConfigBootCoordinator` state reads are intended for the serialized boot/application task; no new cross-task synchronization was added.
3. The shared `Esp32NvsStartupGuard` still blocks only its exact full DATA_NVS partition `esp_partition_erase_range()` route. Chunked erases, DATA_NVS_KEYS, direct lower-level flash erase APIs and alternate routes are not a complete firewall.
4. No production main/scheduler composes config boot, AP credential bootstrap, PIN runtime, services and health fault mapping. The inert smoke target must never be flashed.
5. No NVS runtime, power-cut/brownout, first-boot physical setup, or physical device test was run. No destructive NVS operation was performed.

## Deliberately not run

- `pio test -e native` (full N1), SF2 Node tests, hardware-probe build, final ESP32 link/ELF inspection, flash, NVS corruption/erase, power-cut/brownout, and physical tests.

## Disposition

This increment closes the specific accidental early-call bypass through AP/PIN NVS adapter APIs. **C1/C34 remain blocking** until the production root uses one authoritative coordinator, all application NVS owners are ordered behind it, the broader erase surface is addressed, and exact target/runtime verification is completed. Never flash the inert adapter-link target.
