# Release v0.2.46 — 2026-11-08

**Development snapshot. Not production firmware.**

## Changes

1. **E29 signed:** first-provision AP SSID is fixed to `att`. **E31 delegated:** channel 1 is the assistant-selected fixed default; regulatory-country acceptance remains unverified. `ApCredentialSetupCoordinator::start()` takes no SSID/channel arguments, so a future caller cannot accidentally expose a MAC, school, or owner identifier or choose a different first-provision network identity. The lower-level `Esp32WifiAp` remains parameterized as an adapter.
2. The OLED setup credential screen now labels the network `Wi-Fi: att` above the WPA2 passphrase. The existing OLED rendering test was updated to assert the label and exact write count.
3. **E30 delegated:** no remote passphrase rotation, reset endpoint, touch/boot reset gesture, or automatic NVS erase/overwrite. Credential mismatch/corruption/loss remains fail-closed. A physically supervised USB service recovery procedure is not implemented. No AP recovery action may erase attendance/config data. Any future rotation must atomically reconcile the NVS credential and authoritative Config slot and prove rollback/power-cut behavior.
4. Updated the ESP32 smoke target's retained member-function pointer for the new no-argument coordinator start API.

## Verification required

No compiler, PlatformIO test run, target build, or hardware test was executed while preparing this release. The source inventory remains 520 C++ test registrations across 31 suites; the modified setup/OLED source and tests must be verified in PlatformIO. The v0.2.44 user-reported 500/500 native pass and v0.2.44 target link do not cover these changes.

From a clean extraction, run:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native 2>&1 | tee native-test-v0246.log
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-smoke-v0246.log
```

Do not flash the smoke binary as production firmware. It is compile/link-only and inert.

## Still blocked / unverified

- C6: no production `src/` composition, setup-code verifier, PIN verifier/KDF, HTTP/JSON routes, or request/session-bound auth construction.
- E30 USB service recovery is not implemented.
- Physical OLED visibility, shared I2C coexistence, AP DHCP behavior, channel acceptance, and first-beacon WPA2-PSK/CCMP remain unverified.
- C1/C17/E2 and C18/E26 still block production schedule changes; C2/C13 block restore; C3/E18/U5 block production enrollment/attendance; C8 soak/brownout remains undone.

See `docs/AUDIT_v0.2.46.md`, `docs/CONTEXT.md`, `docs/CRITICAL.md`, `docs/DECISIONS.md`, and `docs/UNVERIFIED.md`.
