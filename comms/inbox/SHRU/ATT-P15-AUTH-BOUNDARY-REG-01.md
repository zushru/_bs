# ATT-P15-AUTH-BOUNDARY-REG-01 — adversarial setup-code boundary regressions

- Author: SHRU
- Status: COMPLETE
- Baseline: v0.2.95 development snapshot; manifest is stale
- Workspace: CANONICAL_SHARED_TREE
- Inspected: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; docs/SPECS/api_logic.md; lib/core/src/admin_session.h/.cpp; lib/core/src/pin_auth.h/.cpp; lib/core/src/pin_auth_session.h/.cpp; test/test_admin_session/test_main.cpp.
- Not inspected: concrete platform verifier/storage implementations or production route composition; execution was prohibited.

## Findings and change

- [INFO] Extended the local CounterStore test fake in the assigned file with failPersistCall, allowing one selected persistence call to fail while prior calls succeed. Existing global ok behavior is preserved.
- Added test_setup_code_malformed_input_is_rejected_before_persist_verify_and_action(): a six-byte candidate containing punctuation returns PAC_BAD_INPUT before attempt persistence, verifier or privileged action; the coordinator is not faulted.
- Added test_setup_code_verifier_error_latches_coordinator_and_never_runs_action(): PV_ERROR occurs only after the attempt count is persisted; it returns PAC_VERIFY_ERROR, latches the setup-code coordinator, and never invokes the action. A subsequent request returns PAC_NOT_READY without another persistence/verifier/action call.
- Added test_setup_code_counter_reset_persist_failure_never_runs_privileged_action(): first attempt persistence succeeds, verifier matches, and the selected second persistence (counter reset) fails. The test expects PAC_STORAGE_ERROR, a faulted coordinator and zero action calls. The fake's persisted value remains at the pre-verification increment.
- The three new tests are each registered exactly once. test_admin_session now has 30 source registrations; this is not a test pass.
- No verifier, credential, KDF, entropy, lifetime, counter-domain, route or production behavior was changed.
- Changed project path: test/test_admin_session/test_main.cpp only.
- Pre-edit copy: comms/staging/SHRU/ATT-P15-AUTH-BOUNDARY-REG-01/before/test_main.cpp
- Unified diff: comms/staging/SHRU/ATT-P15-AUTH-BOUNDARY-REG-01/patch.diff

## Verification

- Full changed-file read-back completed.
- Confirmed all three test definitions and RUN_TEST registrations exactly once; saved diff matches regenerated diff; added lines have no trailing whitespace.
- SHA-256 before: fe6b276361a362c7be9309b70f0edd0b88c3d371e27e82904472a942f671670e
- SHA-256 after: 554cfe36098ef1cfd986110b561362d7a2dbb4ec5cb1f6a3a1935d3bb022db04
- Tests, compiler, build, PlatformIO and hardware: NOT RUN, as assigned.
