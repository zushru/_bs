# Release v0.2.38 — 2026-11-08

Status: **C6 token-session success action and RF-gated ESP32 RNG adapter.** Development snapshot only; not production firmware and not hardware-verified. Replace the complete `att/` tree; never merge individual files into an older tree.

## Changes

1. Added `ISecureRandomSource` and `TokenSessionSuccessAction` in `lib/core/src/pin_auth_session.*`. It requests exactly 16 random bytes after the coordinator has durably reset the attempt counter. A read-only `TokenTable::canIssue()` preflight rejects null, all-zero and duplicate candidates without changing active sessions.
2. The token is copied once into the HTTP response buffer. The handler must call `confirmResponseSent(monotonicNowS)` only after the complete response write succeeds. Only then does the action call `TokenTable::issue()` and apply LRU eviction. The monotonic timestamp is sampled at confirmation, so the 900-second idle lifetime starts when the token becomes active, not when PIN verification began. Aborted/abandoned responses clear the pending token and leave existing sessions unchanged. The handler must erase its temporary token copy. This is only confirmation that the server-side write API reports the full response accepted/written; it is not an end-to-end acknowledgement that the phone application received or stored the token.
3. **Serialized-use limitation:** the eventual handler must not yield to another `TokenTable` mutation between `run()` preflight and `confirmResponseSent(monotonicNowS)`. If that cannot be guaranteed by the actual server scheduler, replace the preflight/commit contract with an explicit reservation mechanism before integration. A post-write `issue()` failure must be treated as an authentication/service fault, not a successful login. No production handler exists yet.
4. Added `Esp32SecureRandom` in `lib/hal_esp32/src/esp32_secure_random.*`. It wraps ESP-IDF `esp_fill_random()` and refuses use until the caller signals that Wi-Fi RF has started; it refuses again after RF stop. The adapter cannot independently verify Wi-Fi state. The caller must signal start only after the actual AP/RF start call succeeds.
5. Added 11 token-session, 5 ESP32 RNG and 2 `TokenTable::canIssue()` C++ test registrations. Current inventory: **431 registrations across 25 suites**. The 18 new tests are unrun. The 413/413 v0.2.36 native result covers only the prior 413-case baseline.

## Verification status

- User-reported v0.2.36 native baseline: **413/413 passed across 23 suites**. v0.2.38 adds 18 unrun registrations; do not report 431/431 as a passing v0.2.38 result.
- SF2 Node suite: **257/257 passed locally in v0.2.37**; v0.2.38 makes no SF2 changes.
- No v0.2.38 C++ compiler/PlatformIO test, ESP32 target build or hardware verification was run.
- The archive manifest/checksum and clean extraction are verified separately; those checks do not compile or execute firmware.

## Boundaries and remaining blockers

- E12 remains DELEGATED; no choice is made about whether PIN and setup-code failures share a counter domain.
- PIN PBKDF2-SHA256 verifier/writer and PIN KDF work factor/salt policy remain unimplemented/unselected. Do not guess these or conflate them with backup KDF C12.
- No production boot composition, Wi-Fi RF lifecycle caller, HTTP/JSON adapter, protected route wiring or full fault/session revocation integration exists. C6 remains blocking.
- Espressif documents `esp_fill_random()` as sharing `esp_random()` entropy constraints; true-random output requires a continuously active entropy source such as Wi-Fi/Bluetooth. The RF-ready flag is caller notification, not a direct hardware-state check. The actual Arduino-ESP32/ESP-IDF version is not pinned in `platformio.ini`.
- No signed/delegated product policy, hardware mapping, backup scope, persisted config schema or O8 decision changed. O8 remains config + roster + log; no reporting PII persistence is permitted without the approved recovery path.
- No LittleFS power-loss behavior, restore swap/recovery, official XLSX output, physical sensor behavior, soak or brownout test is verified.

## Required verification

From a clean full v0.2.38 extraction:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_api -vvv 2>&1 | tee api-test.log
pio test -e native -f test_pin_auth_session -vvv 2>&1 | tee token-session-test.log
pio test -e native -f test_esp32_secure_random -vvv 2>&1 | tee esp32-rng-test.log
pio test -e native 2>&1 | tee native-test.log
```

Then pin and build the exact ESP32 target. Integrate RF notifications only after successful AP start and verify the synchronous TokenTable response lifecycle against the actual server/scheduler. No hardware claim should be made until the user confirms the device output.
