# Focused audit — v0.2.17 native test-runner correction

Date: 2026-11-06  
Baseline: `att_v0.2.16_2026-11-06.tar.gz`  
Baseline SHA-256: `f18fa5d32bda42aa601c7baace94bf82133811b7cb6a93d0476de1edde1a36c5`

## Reported failure

The user reported that all v0.2.16 suites passed except `test_config_guard`, which failed during linking:

```text
/usr/bin/ld: .../Scrt1.o: in function `_start':
(.text+0x1c): undefined reference to `main'
(.text+0x20): undefined reference to `main'
collect2: error: ld returned 1 exit status
```

The failure occurred before a test executable was produced. Therefore, none of the seven `test_config_guard` assertions ran. This was not an assertion failure and is not evidence of a defect in the production helpers.

## Root cause

`test/test_config_guard/test_main.cpp` ended with an Arduino-style `void setup()` entry point. The PlatformIO `native` test executables in this repository use `int main()` (one existing suite uses `int main(int argc, char** argv)`). The native linker requires `main`, and there was no such symbol in the failed suite.

A static scan of the v0.2.16 archive confirmed that `test_config_guard` was the only `test_main.cpp` without a `main()` definition.

## Change

- Replaced `void setup()` with `int main()`.
- Retained the same seven `RUN_TEST()` statements and their order.
- Changed the final `UNITY_END()` statement to `return UNITY_END();`.
- No production source, test assertion, config schema, signed handoff, or decision was changed.

## Audit results

| Check | Result |
|---|---|
| `test_main.cpp` files | 19 |
| Files exposing `main()` after patch | 19 |
| Files relying on `setup()` only | 0 |
| Source `RUN_TEST()` registrations | 318 (unchanged) |
| Production implementation files changed | 0 |
| `HANDOFF_FINAL.md` | Unchanged |
| Signed/delegated decisions | Unchanged |
| Tests/compiler/PlatformIO | Not run, per instruction |
| Archive and manifest | Rebuilt and integrity-checked |

## Verification required

Run the focused suite first, then the complete suite:

```sh
pio test -e native -f test_config_guard -vvv
pio test -e native 2>&1 | tail -14
```

Expected focused result: 7 test cases succeeded. Expected full source registration total: 318 test cases. These are expectations, not claimed results.

The user's prior report that the other v0.2.16 suites passed is retained as user-reported evidence. v0.2.17 itself remains unverified until the corrected PlatformIO run is supplied.
