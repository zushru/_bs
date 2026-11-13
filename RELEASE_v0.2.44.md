# Release v0.2.44 — 2026-11-08

Status: **standalone 128x32 SSD1306 adapter and expanded ESP32 adapter link-smoke target**. Development snapshot only; not production firmware and not hardware-verified. Install only by replacing the complete project tree with this archive. Do not merge single files into an older tree.

## Scope

The user confirmed the OLED resolution is **128×32**. The project hardware record already specifies I²C address `0x3C`, SDA GPIO21 and SCL GPIO22; the display shares the bus with the DS3231 (`0x68`) and EEPROM (`0x57`).

### Changes

1. Added `Esp32Oled`, a standalone Adafruit SSD1306 adapter fixed to 128×32 at `0x3C`. It takes an injected shared `TwoWire&`, does not call `Wire.begin()`, and does not reconfigure the bus pins. The board-level owner must initialize the shared bus once before calling `begin()`.
2. Configured display transactions for 100 kHz. This is a conservative shared-bus setting, not a measured board-level requirement.
3. Added a setup credential screen with `Wi-Fi setup`, `WPA2 password:`, and the exact 32-character lowercase-hex E28 passphrase split into two 16-character rows. It accepts only `ApCredentialResult{APCB_OK, true}` and an exactly 32-byte lowercase-hex buffer. Malformed values are rejected rather than truncated.
4. Non-disclosure, bootstrap failure and invalid-secret paths request a blank frame so an older password is not intentionally left displayed after the state changes. After a credential display request, the driver clears its RAM framebuffer but leaves the physical screen content until `clear()` is called. The caller's source buffer is not changed and the adapter does not allocate a second formatted plaintext password.
5. Extended `platformio.esp32-smoke.ini` with Adafruit SSD1306 2.5.17, Adafruit GFX Library 1.12.6 and Adafruit BusIO 1.17.4. The smoke translation unit retains references to the OLED methods so the expanded target build will compile/link the actual pinned library and adapter. The smoke app remains inert: it does not initialize or transmit on I²C, start the AP, request entropy or touch NVS.
6. Added 13 native test registrations for initialization parameters, shared-bus ownership, begin failure/retry, disclosure gating, exact formatting, stale-screen clearing, malformed credentials, caller-buffer preservation and framebuffer clearing.

## Important limitations

- Adafruit `begin()` reports framebuffer allocation/initialization-call status, not confirmed panel presence.
- Adafruit `display()` returns `void`; this adapter cannot observe I²C ACK status or verify that the screen actually showed/cleared pixels. `EOA_TRANSFER_REQUESTED` is deliberately not a hardware-ready result.
- No production boot/UI caller exists. No acknowledgement/visible-display contract, timeout, or credential-clear gesture was added. Do not transition `CFG_BOOT_NEEDS_SETUP` to `CFG_BOOT_READY` solely because `showSetupCredential()` returned `EOA_TRANSFER_REQUESTED`.
- E29 (production SSID naming) and E30 (credential reset/rotation/recovery) remain OPEN. This increment does not start or change the AP.

## Verification

- `test_esp32_oled`: **13/13 tests, 67 assertions, 0 failures** in a local `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined` run with a temporary Unity-compatible and Adafruit API stand-in. This is not PlatformIO/Unity and not a build against the actual Adafruit headers.
- The ESP32 smoke translation unit passes host `g++ -fsyntax-only` with the existing project test stubs. This is only host syntax evidence.
- SF2 Node suite: **257/257 passed** locally; all 25 `.mjs` modules passed `node --check`. No SF2 source changed.
- Source inventory: **500 C++ `RUN_TEST` registrations across 30 suites**. The user reports the previous v0.2.42 full PlatformIO run passed 487 registrations across 29 suites. The v0.2.44 full native PlatformIO run has not been performed in this environment.
- The user reports the v0.2.43 ESP32 smoke build succeeded with Espressif32 8.0.0, Arduino-ESP32/framework libraries 3.3.12, esptool 5.4.0 and Xtensa toolchain 14.2.0+20260121. This previous build predates the OLED and its Adafruit dependencies. The expanded v0.2.44 target build remains pending.
- No physical OLED, shared I²C bus, DS3231/EEPROM coexistence, visible password, or hardware-clear behavior was tested.

## Required verification from a clean v0.2.44 extraction

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_oled -vvv 2>&1 | tee oled-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-target-smoke.log
```

Record the resolved Adafruit/platform/framework/toolchain package versions and the complete build summary. The target smoke image is for compile/link validation only; do not flash it as production firmware. After that target build, the next hardware check is to verify the shared bus at 100 kHz, OLED readability, and RTC/EEPROM coexistence.

## Decisions and blockers

- **E29 remains OPEN:** production SoftAP SSID source/naming format.
- **E30 remains OPEN:** credential reset, rotation and recovery.
- **C6 remains blocking:** no production boot composition, display acknowledgement path, PIN KDF/verifier, HTTP/JSON routes or protected-route enforcement.
- The OLED adapter does not satisfy the signed E28 disclosure requirement by itself because panel visibility cannot be confirmed through the current Adafruit API.
