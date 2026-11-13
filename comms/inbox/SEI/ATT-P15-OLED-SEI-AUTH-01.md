# ATT-P15-OLED-SEI-AUTH-01 — P15 setup-code boundary audit

- Author: SEI
- Status: PARTIAL
- Baseline: v0.2.95 development snapshot (source baseline 2026-11-12). MANIFEST.sha256 is stale and was not used as attestation.
- Workspace: CANONICAL_SHARED_TREE (read via RDC, device localhost). Not reconciled against PISHDA's view.
- Inspected: docs/DECISIONS.md; docs/CRITICAL.md; docs/SPECS/esp32_oled.md; docs/SPECS/api_logic.md; docs/SPECS/ap_credential_setup.md; docs/SPECS/ap_credential_bootstrap.md; lib/hal_esp32/src/esp32_oled.h; lib/hal_esp32/src/esp32_oled.cpp; comms/README.md; bootstrap set (AGENTS.md, WORKING_MEMORY.md, index.md, AI_COORDINATION.md).
- Not inspected: HANDOFF_FINAL.md (controlling P15 wording; PARTIAL status depends on it). Source of generateSetupCode, PinLockout/PinAuthCoordinator, Esp32BootstrapRandom and Esp32SecureRandom (findings below rely on spec text). The seven new OLED test bodies (QYUPAL scope). docs/SPECS/INDEX.md, ARCHITECTURE.md.

## Findings

- [HIGH] Single-factor admin takeover path. api_logic.md authorize table: PIN_RESET requires no token, only the setup code, in states OK, CLOCK and SENSOR. A setup-code holder can reset the admin PIN, log in with it, and reach token-gated admin routes. Admin security therefore reduces to setup-code guessing. The six-digit length appears in the generator and the task text, but no DECISIONS.md row for P15 states length, entropy or lifetime. P15 is recorded only as placement and operations. Confirm the exact wording in HANDOFF_FINAL.md.

- [HIGH] Guess budget is rate-limited, not capped. Per E12 as written: 5 failures give 60 s, doubling, capped at 300 s, with no total limit. Steady state is about 5 guesses per 300 s, roughly 1 guess per minute, indefinitely. Exhausting 10^6 codes takes about 694 days worst case and about 347 days expected. The remote attacker population is anyone holding the WPA2 passphrase, which is permanent and cannot be rotated under E30. The rate limit is the only control. No durable lock or total cap is specified.

- [HIGH] Setup code and WPA2 passphrase share one panel with no frame ownership. E28 requires the passphrase to be re-displayable on every NEEDS_SETUP boot until provisioning completes. showSetupCode() and showSetupCredential() both overwrite the same panel, and neither records what is visible. A composition that shows the setup code during NEEDS_SETUP silently removes the passphrase the operator needs to join the AP. A rule is needed: one composed screen, or sequential frames with a named owner.

- [HIGH] The return value cannot prove visibility. showSetupCode() returns EOA_TRANSFER_REQUESTED, the same enum value as the credential path. Per esp32_oled.md, display() is void and Wire status is not exposed. A caller that treats this as "operator saw the code" is wrong. E30 forbids reset, so a durably issued code that never reached the panel could strand setup. No visible-display acknowledgement contract exists. esp32_oled.md leaves it OPEN.

- [MEDIUM] Issuance lifetime and counter domain are undefined. Not specified: per-boot (RAM) versus durable (NVS) issuance; single-use versus reuse; whether one code serves SETUP, RESTORE, PIN_RESET and ROLLOVER over a year; whether regenerating a code may reset the counter (it must not). Counter domain: if shared with the PIN, setup-code guessing can lock out PIN login (denial of service). If separate, PIN lockout does nothing against PIN_RESET, which bypasses the PIN entirely. Either choice must be made explicitly.

- [MEDIUM] Entropy source choice conflicts and is unrecorded. Esp32BootstrapRandom is documented as pre-radio only and explicitly not for anything beyond the E28 credential. Esp32SecureRandom is RF-gated, reserved for token RNG, and needs a production Wi-Fi caller that does not exist. The generation time relative to AP start selects the adapter, and that choice is not recorded anywhere.

- [MEDIUM] Stored verifier for a 20-bit secret is offline-invertible. Any stored hash of a six-digit code can be inverted by enumeration regardless of KDF iteration count. Under R11 this is acceptable only because physical flash access is outside the threat model. The design should state that dependency explicitly. The online rate limit is then the only control.

- [MEDIUM] Primitive overclaim risk. showSetupCode() has no state gating, no issuance binding and no caller identity. Any caller in any state can display any six digits. It is a display primitive, not authorization, and must not be wired as "disclosure complete." Consider a result type distinct from credential transfer.

- [LOW] Residual secret copies. clearDisplay() zeroes the driver framebuffer. The caller's `code` buffer is untouched. Adafruit and Wire transmit staging is not scrubbed by this code and is likely to retain the digits briefly. Not verified on target. The caller must scrub its own buffer.

- [INFO] Guarantees the primitive can give: exactly six ASCII digits; explicit length; no NUL dependence; leading zeroes preserved; no truncation; no second plaintext copy; framebuffer cleared after the transfer request; invalid input requests a blank frame. Layout fits 128x32: label at size 1, six digits at size 2 at x=28, y=16, centered and within 32 rows. Guarantees it cannot give: visibility, authorization, uniqueness, lifetime, or correspondence to any verifier.

- [INFO] Seven P15 regressions are authored and NOT RUN. Bodies not inspected here.

## Decisions required (user authority; not resolved by SEI)

1. Exact P15 setup-code length, format, entropy, lifetime, issuance cadence and single-use rule, confirmed against HANDOFF_FINAL.md.
2. Whether single-factor PIN_RESET (F1/F2) is accepted. Alternatives: longer code; total cap requiring supervised USB recovery (E30 service path, not implemented); token requirement on PIN_RESET (likely circular, since PIN_RESET serves a forgotten PIN).
3. Separate or shared lockout counter domain.
4. Entropy adapter and generation timing relative to AP start.
5. Panel ownership model and visible-display acknowledgement contract.

## Required actions

- Do not wire showSetupCode() to any route or composition root until decisions 1–5 are recorded in DECISIONS.md.
- Do not persist a setup code until durability, lifetime and acknowledgement are decided.
- Confirm PinLockout's cap behaviour against E12 in source. Not inspected in this audit.

## Open question for the user

If the setup code and the WPA2 passphrase appear on the same panel at the same time, what does the code protect against that the passphrase does not?

## Verification

- Commands/tests run: none. Read-only audit. Only write: this report.
- Not run: all native tests, PlatformIO builds, compilers, flash, hardware.

## Revision 2026-11-13: HANDOFF_FINAL.md read; status now COMPLETE

- Status: PARTIAL → COMPLETE for this audit. Test-body review remains QYUPAL's scope.
- F1 correction: the six-digit length is user-stated ([U], HANDOFF §3 Security), not an assumption. Single-factor PIN_RESET is therefore signed behaviour. Its acceptance is a user decision, not a defect.
- HANDOFF §3 states the setup code is required for first run, restore, PIN reset and year rollover. It does not state per-operation issuance, lifetime or durability. Still OPEN.
- HANDOFF §3 and H8 accept that a leaked Wi-Fi passphrase exposes the admin token over plain HTTP. The setup code travels the same HTTP path. A passphrase holder who captures traffic learns the code. The code therefore guards against a passphrase holder who guesses, not one who sniffs.
- HANDOFF §3 says the Wi-Fi passphrase is "known only to admin". E28 displays it on the OLED. HANDOFF open question 7 (who else knows it) is unanswered and decisive for this threat model.
- SEI judgement (recommendations, not recorded decisions):
  (a) Separate lockout counter domains for PIN and setup code. H8 already accepts shared-PIN lockout by any client; sharing would extend that to the setup code.
  (b) Regenerate the code in RAM per display. Regeneration must not reset the persisted counter.
  (c) Code entry is the operator acknowledgement. Typing the code back proves it was read, so no panel-acknowledgement contract is needed.
  (d) Sequential display: passphrase until the first client is assigned, then setup code. Avoids overwriting the passphrase during join.
  (e) Entropy: Esp32SecureRandom at display time, after RF start. Esp32BootstrapRandom is pre-radio only.
- User-only decisions: acceptance of single-factor PIN_RESET; answer to open question 7.
