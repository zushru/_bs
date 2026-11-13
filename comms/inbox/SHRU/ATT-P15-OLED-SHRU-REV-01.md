# ATT-P15-OLED-SHRU-REV-01 — Independent review of setup-code OLED primitive

- Author: SHRU
- Status: COMPLETE
- Baseline: v0.2.95 development snapshot; current canonical tree, working-tree integrity not established
- Workspace: CANONICAL_SHARED_TREE
- Inspected:
  - `AGENTS.md`, `WORKING_MEMORY.md`, `index.md`, `AI_COORDINATION.md`
  - `docs/SPECS/esp32_oled.md`
  - `lib/hal_esp32/src/esp32_oled.h`
  - `lib/hal_esp32/src/esp32_oled.cpp`
  - `test/test_esp32_oled/test_main.cpp`
  - `test/test_esp32_oled/stubs/oled_test_state.h`
  - `test/test_esp32_oled/stubs/Adafruit_SSD1306.h`
  - `test/test_esp32_oled/stubs/Wire.h`
  - `comms/README.md`
- Not inspected: pinned Adafruit library implementation/package source, physical panel, I2C electrical behavior, unrelated OLED callers beyond the repository-wide symbol search.
 
## Findings

- [INFO] **No confirmed defect in `showSetupCode()`.** `esp32_oled.cpp:42-48` rejects null, any length other than six and every byte outside ASCII `0`–`9`. The valid path acquires the shared bus lock before touching the display (`:113-115`), writes the six caller bytes directly (`:117-123`), requests the transfer and clears the driver's software framebuffer before releasing the guard (`:125-129`). No second plaintext code buffer or caller-buffer mutation is apparent. The invalid path delegates to `clear()` and propagates lock/not-initialized failures (`:107-110`). The six-digit line starts at x=28; six built-in-font cells at 12 px each occupy 72 px, giving a centered 128 px line. Leading-zero, non-NUL-terminated, caller-preservation and valid/invalid lock-failure cases are present in the seven new tests.
- [MEDIUM] **The host fake does not prove pixel-level layout or framebuffer scrubbing.** `oled_test_state.h:9-30` stores character rows, not a 128×32 pixel buffer. `stubs/Adafruit_SSD1306.h:36-50` clears only those string rows and copies strings into `visibleLines`; `:62-78` reduces cursor coordinates to integer character row/column buckets. Consequently, assertions in `test_main.cpp:357-381` and `:464-475` establish the fake's text/state behavior, not exact pixel centering, glyph clipping, complete driver-buffer zeroization or panel output. The source's call to the real driver's `clearDisplay()` is correctly placed after `display()`, but this test double cannot validate the real implementation. The test name claiming pixel-framebuffer scrubbing is stronger than its fixture evidence.
- [LOW] **One documented invalid-byte category lacks a direct regression.** `docs/SPECS/esp32_oled.md:50-52` explicitly lists punctuation rejection. Existing tests exercise alphabetic, embedded-NUL and high-bit bytes (`test_main.cpp:417-430`), but do not include a punctuation byte. The range check makes rejection apparent in source; add one punctuation case when the test owner next edits this suite.
- [HIGH — integration boundary, not a primitive defect] **This remains display-only, not a disclosure acknowledgement.** The repository-wide `showSetupCode` search found the declaration/definition, docs and tests, but no production runtime caller. The API correctly returns `EOA_TRANSFER_REQUESTED`, and both header (`esp32_oled.h:59-66`) and specification (`esp32_oled.md:56,62-69`) state that this is not proof of I2C acknowledgement or visible pixels. Do not reinterpret the result as successful disclosure or use it to authorize a production state transition. The production caller and visibility/acknowledgement contract remain separate unresolved integration work.

## Required actions

1. No source correction is required for the reviewed primitive based on inspected evidence.
2. Keep `EOA_TRANSFER_REQUESTED` semantically narrow. Production integration must remain blocked from treating it as proof of user-visible disclosure until the separately specified acknowledgement contract is resolved.
3. Improve the OLED fake or add a real-driver/target-level check if pixel-level centering, clipping or full framebuffer wiping needs test evidence. Add one punctuation regression as a small coverage completion.

## Verification

- Read-only source/spec/test review and repository-wide `showSetupCode` search completed on the canonical path `/root/prototype/att`.
- No production non-test call site was found.
- Tests, compilers, builds, PlatformIO and hardware: **NOT RUN**, as required by this assignment.
- No implementation or test source files changed. This report is the only file written by SHRU.
- Reviewed-file SHA-256:
  - `lib/hal_esp32/src/esp32_oled.h`: `d298b72b948c46d7a6b9656e84367912a44e3b9f38a23ed34d5a9c0fab3440a6`
  - `lib/hal_esp32/src/esp32_oled.cpp`: `0b0ef1f0cc07e92878d053cac06c92d4ce1edd082a01548731c1a59f0cbb243e`
  - `test/test_esp32_oled/test_main.cpp`: `ca70dcfe29dfefb2124a12a64e809c5bf499dc57fd5d539c25fe2acd07709e16`
  - `test/test_esp32_oled/stubs/oled_test_state.h`: `4b8074b0cd25616f88a7f731a89421f40770fb9c84e237c133cb40672715db30`
  - `test/test_esp32_oled/stubs/Adafruit_SSD1306.h`: `a91a6da0c4cd0259d07a682681031e26b86e4defa50023835281f8c3555027bb`
