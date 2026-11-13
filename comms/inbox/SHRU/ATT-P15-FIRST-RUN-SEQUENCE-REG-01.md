# ATT-P15-FIRST-RUN-SEQUENCE-REG-01 — first-run credential/code ordering

- Author: SHRU
- Status: COMPLETE — TEST SOURCE AUTHORED; NOT RUN
- Baseline: v0.2.95 development snapshot
- Workspace: CANONICAL_SHARED_TREE
- Authority: E28/E33; docs/SPECS/ap_credential_setup.md; FirstRunSetupCodeSequence source contract
- Read: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; docs/SPECS/ap_credential_setup.md; setup_code_sequence.h/.cpp, ap_credential_setup.h/.cpp, ap_credential_display.h and credential_soft_ap.h; assigned test harness.

## Change

Only `test/test_ap_credential_setup/test_main.cpp` was modified. It now includes `setup_code_sequence.h`, adds a local fake `ISetupCodeIssuer` that records its call order/count, and adds eight first-run sequence tests:

1. Credential display precedes secured AP startup, which precedes setup-code issuance.
2. Without a DHCP client lease, the AP passphrase remains in the display fake, the issuer is not called, provisioning remains blocked, and blank-frame retry cannot advance setup.
3. The first DHCP assignment while AP remains READY permits exactly one setup-code issue; repeated issue attempts return already-issued.
4. AP startup failure faults the sequence, never calls the issuer and blocks provisioning.
5. AP readiness loss before code issue faults the sequence and blocks provisioning.
6. Setup-code issuer failure faults the sequence and blocks provisioning.
7. Repeated start while waiting does not destroy the waiting state; later DHCP assignment still allows exactly one issue.
8. Request authorization remains required after code issue. A denied request cannot write config; blank-frame retry while waiting cannot write config; an authorized request writes once, and post-provision blank-frame retry does not repeat storage mutation or permit a second provision.

No production source, interface, spec, shared stub, build configuration or other test file was changed.

## Provenance and static verification

- Pre-edit copy: `comms/staging/SHRU/ATT-P15-FIRST-RUN-SEQUENCE-REG-01/before/test_main.cpp`
- Unified diff: `comms/staging/SHRU/ATT-P15-FIRST-RUN-SEQUENCE-REG-01/patch.diff`
- Before SHA-256: `05aa9ef0b80108725570604908c95d1c7fb447f3f10bf5a9fd35f0a10c073281`
- After SHA-256: `73120e79a6e22f18c1f461c3d5d13e873fc932dc8eda40fc0850ece14fd137a9`
- Patch SHA-256: `cf2a37d64d6980115b666a1f1143a9cdc3e48edcf2bba08406a2c33418e46622`
- Full changed-file read-back completed; saved diff matches regenerated diff.
- Static source consistency: 26 definitions / 26 registrations, no duplicates or missing registrations; no trailing whitespace.

Tests, compiler, build, PlatformIO, flash and hardware: **NOT RUN**, as assigned. This is fake-based source coverage, not proof of real OLED, AP, DHCP or HTTP behavior.

Next: PISHDA independently reviews this test change before acceptance.
