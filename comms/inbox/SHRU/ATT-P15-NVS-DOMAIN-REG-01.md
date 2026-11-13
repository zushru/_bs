# ATT-P15-NVS-DOMAIN-REG-01 — runtime-level lockout counter isolation

- Author: SHRU
- Status: COMPLETE — TEST SOURCE AUTHORED; NOT RUN
- Baseline: v0.2.95 development snapshot
- Workspace: CANONICAL_SHARED_TREE
- Authority: E33 in docs/DECISIONS.md (SIGNED, 2026-11-13); docs/SPECS/api_logic.md
- Read: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; docs/SPECS/api_logic.md; Esp32PinAuthRuntime, Esp32NvsPinAttemptStore and PinAuthCoordinator interfaces/implementations; the assigned NVS fake and existing runtime/store regressions.

## Change

Only `test/test_nvs_config/test_main.cpp` was modified. Added two tests:

1. `test_pin_auth_runtimes_load_and_mutate_only_their_own_namespace`
   - Seeds provisioned READY state with `attsec=2` and `attsetup=4`.
   - Starts two separate `Esp32PinAuthRuntime` instances against the same `ConfigBootCoordinator`.
   - Asserts each coordinator loads its own persisted counter.
   - Drives a valid-format failed authentication through only the `attsec` coordinator.
   - Reads both namespaces back and asserts `attsec=3`, `attsetup=4`; the verifier runs once and the success action never runs.

2. `test_pin_auth_runtime_ready_missing_setup_counter_fails_without_initialization`
   - Seeds only `attsec` on a provisioned READY config.
   - Asserts `attsec` starts normally but `attsetup` fails with `ESP_ERR_NVS_NOT_FOUND`, is faulted/not ready, and exposes a null coordinator.
   - Asserts no NVS set or commit calls occur and the working `attsec` runtime remains ready with its count unchanged.

No production code, fake NVS implementation, shared harness, specification or build configuration was changed.

## Provenance and static verification

- Pre-edit copy: `comms/staging/SHRU/ATT-P15-NVS-DOMAIN-REG-01/before/test_main.cpp`
- Unified diff: `comms/staging/SHRU/ATT-P15-NVS-DOMAIN-REG-01/patch.diff`
- Before SHA-256: `d008dc01903d3ed76746d3dd1b78eaead75045510fed12393906422320e061bf`
- After SHA-256: `4b220edfc5297e3629648d7463d3ee291dea96ec768aa5c3a4157f9f12f44ae5`
- Patch SHA-256: `5bf86526e4890e7e96bf042e9e6808114b97613c05db1af373b7913945418535`
- File read-back completed. Generated diff matches the saved patch.
- Static source consistency: 50 test definitions / 50 registrations, no duplicates or missing registrations; no trailing whitespace.

Tests, compiler, build, PlatformIO, flash and hardware: **NOT RUN**, as assigned. These are source regressions only; no runtime behavior is claimed as executed.

Next: PISHDA independently reviews the exact test change before acceptance/integration.
