# ATT-P15-OLED-TST-PUNCT-01 — OLED setup-code punctuation regression

- Author: SHRU
- Status: COMPLETE
- Baseline: v0.2.95 development snapshot; manifest is stale
- Workspace: CANONICAL_SHARED_TREE
- Inspected: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md; docs/SPECS/esp32_oled.md; lib/hal_esp32/src/esp32_oled.h and esp32_oled.cpp; test/test_esp32_oled/test_main.cpp; OLED test state and Adafruit SSD1306 stubs.
- Not inspected: actual SSD1306 library internals, physical panel and I2C acknowledgement.

## Findings and change

- [INFO] Added test_setup_code_rejects_ascii_punctuation_and_blanks_previous_frame(). It first renders a valid code, submits six bytes containing '-', expects EOA_INVALID_SETUP_CODE, checks that a blank-frame transfer was requested and verifies the fake's visible and working text rows are blank. It does not claim pixel-level or physical visibility evidence.
- The new test is registered exactly once. test_esp32_oled now has 28 source registrations; this is not a test pass.
- Changed project path: test/test_esp32_oled/test_main.cpp only.
- Pre-edit copy: comms/staging/SHRU/ATT-P15-OLED-TST-PUNCT-01/before/test_main.cpp
- Unified diff: comms/staging/SHRU/ATT-P15-OLED-TST-PUNCT-01/patch.diff

## Verification

- Full changed-file read-back completed.
- Confirmed one test definition and one RUN_TEST registration; saved diff matches regenerated diff; added lines have no trailing whitespace.
- SHA-256 before: ca70dcfe29dfefb2124a12a64e809c5bf499dc57fd5d539c25fe2acd07709e16
- SHA-256 after: 778e84fb1d2faa8ab7b7d4a9406e519089432c20139dc9af4076753ef6f52bc5
- Tests, compiler, build, PlatformIO and hardware: NOT RUN, as assigned.
