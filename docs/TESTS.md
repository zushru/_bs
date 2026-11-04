# TESTS

## Suites (PlatformIO env `native`, Unity)
| Suite | Host count | Device-confirmed | Notes |
|---|---|---|---|
| test_core | 20 | yes | Handoff Core A |
| test_coreb | 36 | yes | Handoff Core B |
| test_coreb2 | 11 | yes | Two tests changed by fix_b2_tests.py |
| test_config | 12 | yes | config_store |
| test_monthset | 10 | yes | month_set |
| test_roster | 9 | yes | roster_store |
| test_backup | 9 | yes BEFORE template cut | Current file not run on the device |
| test_hardening | 15 | yes BEFORE template cut | Current file not run on the device |
| test_api | 20 | yes | api_logic |
| test_enroll | 7 | NO | enrolled_roster |
| TOTAL | 149 | last full device run: 142 | 142 = the set before the template cut and test_enroll |

## Last device report (your message after the archive)
- test_hardening failed to compile on your working tree because its log_store.h was the handoff baseline (C14). The other suites passed on your tree. Those passes do not prove the r2 tree; rerun from a full extraction.

## Commands
- On the device: `cd att && pio test -e native 2>&1 | tail -14`
- Expect `149 test cases: 149 succeeded` once the current suites are run on the device.

## Host sandbox method (how the 149 was produced)
- Unity replaced by a 60-line stand-in (not the real library). Equivalent assertions only.
- Compiled with `g++ -std=c++17 -Wall -Wextra`, one suite at a time, with lib/core/src/*.cpp.
- A clean-room check started from the handoff baseline, applied patches, and ran all suites.
- Mutation testing: api_logic (20 of 21 mutants caught, the survivor is equivalent), enrolled_roster (8 of 8 caught).
- Syntax check of sketch/sketch_compact_minimal_fixed.ino against hand-written API stubs: passes. Stub signatures are guesses; the device build is the real check.

## Not tested anywhere
- Real mbedtls AES-GCM and PBKDF2 (ICipher is a toy in tests).
- LittleFS IFileSet driver, power loss, atomic rename.
- AS608 index table read, ID 300 page, code 23 handling, sensor wipe.
- Restore end to end, closure after a config change (C1).
