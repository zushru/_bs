# ATT-P15-MONOTONIC-CROSS-OP-REG-01

Status: READY FOR INDEPENDENT REVIEW (test source only).

- Canonical tree: `/root/prototype/att`; edited in place.
- Exact changed source path: `test/test_esp32_ram_setup_code_verifier/test_main.cpp` only.
- Before-image: `comms/staging/SHRU/ATT-P15-MONOTONIC-CROSS-OP-REG-01/before/test_main.cpp`.
- Unified diff: `comms/staging/SHRU/ATT-P15-MONOTONIC-CROSS-OP-REG-01/patch.diff`.
- Added registration: `RUN_TEST(test_verify_detects_cross_operation_monotonic_regression_after_mismatch)`.
- Static inventory: 26 test definitions / 26 registrations; duplicate definitions: 0; duplicate registrations: 0; unregistered definitions: []; registrations without definitions: [].

Regression sequence: issue a code at the fixture's initial monotonic time; observe time 150 through a wrong-code verification and confirm the live code remains active; then move time backwards to 149 (still later than issue time) and submit the correct code. The test requires PV_ERROR, a latched fault, inactive verifier, wiped code buffer, exactly one blank-request attempt (measured at the I2C lock acquisition boundary), no successful display transfer after the denied lock, and no additional RNG calls. This distinguishes the cross-operation monotonic invariant from a check against issue time alone.

SHA-256:
- Before source: `dd9238639268d9ea444088091675add3e3366964cc3a920633904c09c98af2ec`
- After source: `655be70baf23c94e3e0d8397a91d7616c7536870725bf8825e0cffed62df041a`
- Patch: `f8a36d69ae8e07202f24243f2841e1b0f299c3f15c8f02b4dcbe560e17c10470`

No tests, compilers, builds, PlatformIO, flashing or hardware were run. No other source paths were changed by this task. The regression is source-authored and awaits independent review.
