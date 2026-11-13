# Release v0.2.76 — fail-closed AP/PIN NVS adapter startup boundary

Date: 2026-11-10  
Baseline: v0.2.75  
Status: development snapshot; not production firmware and not hardware-verified.

## Scope

C1/C34 adapter-boundary hardening. Every public operation of Esp32NvsApCredentialStore and Esp32NvsPinAttemptStore now checks the pre-setup Esp32NvsStartupGuard snapshot before opening NVS. An unsafe snapshot fails closed before any NVS open, set or commit. The adapters preserve firstInitResult when it is a non-ESP_OK error and use ESP_FAIL for an unsafe snapshot whose first init result was ESP_OK.

The AP credential read path still clears the caller's buffer on failure. PIN-attempt read failure leaves the caller's output unchanged. Existing storage keys, namespaces, formats and provisioning semantics are unchanged.

This is not a config-boot readiness token. If Arduino startup was safe, a direct caller can still invoke these low-level adapters before ConfigBootCoordinator::start() reaches READY or NEEDS_SETUP. The future production root must enforce config boot ordering before using AP/PIN stores and every other application storage owner. No production root exists yet.

No erase policy, format path, recovery protocol, signed/delegated decision, or product behavior changed.

## Changed source and tests

Implementation and public contract:
- lib/hal_esp32/src/nvs_ap_credential_store.cpp
- lib/hal_esp32/src/nvs_ap_credential_store.h
- lib/hal_esp32/src/nvs_pin_attempt_store.cpp
- lib/hal_esp32/src/nvs_pin_attempt_store.h

Tests:
- test/test_nvs_ap_credential/test_main.cpp
- test/test_nvs_config/test_main.cpp

Compile/link smoke probe:
- target/esp32_adapter_link/main.cpp (a non-invoking link anchor keeps the non-virtual PIN initializer present in the final ELF)

Tooling and documentation:
- tools/verify_v0.2.76.sh
- RELEASE_v0.2.76.md
- docs/AUDIT_v0.2.76.md
- Updated README, release pointer, changelog, context, critical issues, unverified items, tests, roadmap, config-boot spec, working memory and manifest.

## Test-first and focused verification

The regressions were added before implementation.

Red runs:
- pio test -e native -f 'test_nvs_ap_credential': new unsafe-startup assertion failed because the unguarded adapter reached NVS and reported the missing key. Existing cases passed. PlatformIO emitted SIGHUP after Unity's failure output; this is failure-reproduction evidence, not a normal completed run.
- pio test -e native -f 'test_nvs_config': new PIN unsafe-startup assertion failed because the adapter reached NVS. Existing cases passed. PlatformIO emitted SIGHUP after Unity's failure output; this is failure-reproduction evidence, not a normal completed run.

Final green runs:
- pio test -e native -f 'test_nvs_ap_credential' — 16/16 passed, exit 0. Log: /tmp/att_v0276_green_test_nvs_ap_credential.log.
- pio test -e native -f 'test_nvs_config' — 47/47 passed, exit 0. Log: /tmp/att_v0276_green_test_nvs_config.log.
- Selected total: 63/63 across two distinct suites.
- Current inventory: 632 C++ registrations across 35 suites; SF2 remains 257 Node registrations across 12 files. The other 569 C++ registrations were not run.

## Target compile/link and exact ELF checks

The inert `esp32-adapter-link` target built successfully with:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link -j 2
```

The first build completed in 1,076.82 s but its first ELF check exposed that `initializeFailureCount()` was garbage-collected despite the original discarded-value link anchor. The smoke anchor was corrected to force a live volatile member-function-pointer read/compare without calling the method. The incremental rebuild completed in 100.83 s. Final RAM: 67,356/327,680 B (20.6%); application flash: 935,792/1,310,720 B (71.4%).

Final ELF SHA-256: `3e5989be6ef00ed708159e7a17bd70618f7c8b2955a6e597bc5465301bf359ec`. Final BIN SHA-256: `20138030906d1f619be41736c5303b091c0ab8462906d56be52ecd17a6f252c7`.

`./tools/verify_v0.2.76.sh elf "$PWD/.pio/build/esp32-adapter-link/firmware.elf"` passed 12/12 NVS wrapper routes, retained all seven checked NVS backend/guard/AP/PIN methods, and retained all nine LittleFsFileSet methods; exit 0. The exact logs and toolchain are in `docs/AUDIT_v0.2.76.md`. This proves compile/link/static ELF properties only; the inert target was not flashed.

## Deliberately not run

- Full pio test -e native — N1 remains deferred until the planned integration checkpoint.
- SF2 npm test — no SF2 source changed.
- Diagnostic hardware-probe build or run.
- Flashing, runtime NVS, NVS corruption, format/erase, mounted LittleFS/VFS operations, power-cut/brownout, sensor, AP/OLED/RTC, power-bank or physical-device tests.

## Remaining blockers

C34 remains blocking. The erase guard still covers only its narrow wrapped whole-DATA_NVS erase path; chunked ranges, DATA_NVS_KEYS and direct lower-level flash erase paths are not comprehensively blocked. The adapter checks only reject an unsafe Arduino startup snapshot; they do not enforce ConfigBootCoordinator readiness or prove that the production root orders storage owners correctly. No production main/scheduler, integrated HTTP/auth boundary, LittleFS mount owner, or hardware evidence exists.

C13.2 remains a proposal only and is not approved for implementation. C3/C6/C2 remain blocking. E18/U5 and all physical hardware claims remain unresolved/unverified.
