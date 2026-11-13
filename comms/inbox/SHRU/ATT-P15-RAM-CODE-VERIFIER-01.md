# ATT-P15-RAM-CODE-VERIFIER-01 — RAM-only setup-code verifier

- Author: SHRU
- Status: COMPLETE — SOURCE AND REGRESSIONS AUTHORED; NOT RUN
- Baseline: v0.2.95 development snapshot
- Workspace: CANONICAL_SHARED_TREE
- Authority: E33 in docs/DECISIONS.md (SIGNED, 2026-11-13)
- Read: AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md, HANDOFF_FINAL.md, docs/DECISIONS.md, docs/SPECS/api_logic.md, docs/SPECS/esp32_oled.md, docs/SPECS/ap_credential_setup.md; pin-auth, setup-code sequence, secure-random, monotonic-clock, OLED and Wi-Fi AP interfaces/implementations; existing secure-random and OLED test harness/stubs.

## Implementation

Exactly three assigned project paths were created/changed:

- `lib/hal_esp32/src/esp32_ram_setup_code_verifier.h`
- `lib/hal_esp32/src/esp32_ram_setup_code_verifier.cpp`
- `test/test_esp32_ram_setup_code_verifier/test_main.cpp`

The verifier implements both `ISetupCodeVerifier` and `ISetupCodeIssuer`. It takes the `Esp32SecureRandom` instance by reference (the production composition must pass the same instance owned by `Esp32WifiAp`), an `IClockMonotonicSource`, and the existing `Esp32Oled`.

- Uses the existing unbiased `generateSetupCode()`; requires the RF-ready gate before issue.
- Holds exactly one six-byte ASCII code plus terminator in RAM; no persistent code/hash, KDF, HTTP route, or production caller was added.
- Starts the 600,000,000-microsecond lifetime after the OLED transfer request. A duplicate issue is refused while active.
- Rejects on elapsed time >= 600,000,000 us. Expiry wipes the code and requests a blank frame before any replacement code is generated/displayed.
- Performs a fixed six-position accumulated comparison. Mismatch preserves the active code; match consumes/wipes it before requesting a blank frame.
- Clock failure/regression, RF-gate loss, random-source failure, or OLED/lock failure latches the verifier faulted, clears internal code bytes with a volatile wipe, and attempts a blank-frame request. A blank-transfer failure prevents a successful verification result/action.

## Regression source

16 native test registrations cover RF-not-ready, RF loss after issue, injected random-source failure, valid issue, leading zeroes, duplicate issue, mismatch retention, malformed candidates, one-time success, exact expiry boundary, blank-before-rotation ordering, clock failure before and after display, clock regression, issue-display lock failure, match-clear failure and expiry-clear failure.

The tests use native fakes/stubs. They cannot establish actual RNG entropy quality, physical OLED visibility, I2C acknowledgement, or target timing.

## Static verification

- Full read-back of all three changed files: completed.
- Static test-source consistency: 16 definitions / 16 registrations; no duplicates, missing registrations or unregistered definitions.
- Trailing whitespace: none.
- SHA-256:
  - Header: `0c4f3f174351afc4c5376025d42b2b1d765574b2e657089e8f7ebc6c79d7ec7a`
  - Source: `c37ed8fd9c9b8fc5e9fd31758eacda4972fe61f4dae39d1f9a7b570bda4209ff`
  - Tests: `7e6a02acee809bc97d566dd2afaed91f8694e6c181fbb36ff9c66b15f3d29893`

Tests, compilers, builds, PlatformIO, flashing and hardware: **NOT RUN**, as explicitly required by the assignment.

## Limits / handoff

- No production root/caller is present or added. The future caller must invoke issue only after the existing first-run sequence confirms AP readiness and DHCP assignment; it must supply the same RF-gated random instance used by `Esp32WifiAp`.
- `EOA_TRANSFER_REQUESTED` and `Esp32Oled::clear()` do not prove physical visibility/blanking. If the shared I2C lock fails, the prior panel image may remain even though the internal code is wiped and verification fails closed.
- The ESP-IDF `esp_fill_random()` API returns `void`; the production adapter can enforce its RF lifecycle gate but cannot report a hardware RNG failure from that API. The verifier's generic `fill()==false` path is covered with an injected test source.
- The expiry check is performed on verifier/issuer calls; no timer or production service loop was added.
- The repository manifest is already stale per WORKING_MEMORY.md; it was not regenerated.
- Next: PISHDA independently reviews the exact diff/source and decides acceptance. No integration or production-readiness claim is made.
