# Specialized audit — v0.2.44 OLED adapter

Date: 2026-11-08  
Baseline: `att_v0.2.43_2026-11-08.tar.gz`  
Baseline SHA-256: `7781655fd5dcd3faa9f65422fae1959a00c32630ec6dd1d6b0b04a086e1f1d31`

## Executive result

The 128x32 SSD1306 adapter is narrow and isolated. It uses the existing shared I2C bus, does not initialize or reconfigure `Wire`, only renders the signed E28 disclosure result and exact 32-character lowercase-hex credential, and clears its software framebuffer after requesting the display transfer. It is **not** a production disclosure gate: Adafruit's `display()` returns `void`, and no caller or physical-visible acknowledgement exists.

No production boot flow, AP behavior, config schema, core logic, sensor implementation, SF2 source, or signed policy changed.

## Source delta

Compared with a clean extraction of v0.2.43, before adding this audit and release metadata:

- 12 existing files modified.
- 7 implementation/spec/test files added.
- 0 files removed.
- No existing `lib/core`, `lib/as608`, enrollment FSM, or SF2 source file modified.
- No previous test registration changed; 13 new registrations in `test_esp32_oled` bring the source inventory from 487/29 to 500/30.

The native `platformio.ini` change is only the test-stub include path needed by the new test translation unit. The separate ESP32 smoke config adds pinned Adafruit libraries. The target smoke app retains OLED method references but does not call them or initialize I2C.

## Code review

### Shared-bus ownership

- The adapter receives `TwoWire&` and passes it to `Adafruit_SSD1306`.
- `begin()` passes `periphBegin=false`; the adapter never calls `Wire.begin()` or sets SDA/SCL pins.
- The API contract requires the future board-level owner to initialize the shared bus first, exactly once, on GPIO21/GPIO22.
- The driver asks the Adafruit library to operate at 100 kHz before and after each transaction. This is an unmeasured conservative setting; DS3231/EEPROM coexistence at this rate remains a device test.

### Credential handling

- Only `{APCB_OK, disclose=true}` is accepted.
- The adapter requires exactly 32 lowercase hexadecimal bytes; it rejects null, short, oversized, uppercase and non-hex input without truncation.
- It renders four 8-pixel rows: a title, a password label, and two 16-character halves of the passphrase. The two credential rows fit horizontally at the chosen font/offset.
- It does not create another plaintext key string and does not modify the caller's passphrase buffer.
- After `display()` returns, the adapter clears its pixel framebuffer. The display panel retains its last frame until `clear()` is called. No timeout or physical acknowledgment is implemented because neither is part of the signed E28 interface and the caller does not yet exist.
- A non-disclosure result, bootstrap failure or malformed credential requests a blank frame to avoid intentionally leaving a previous key visible.

### Return-value semantics

- `EOI_BUFFER_READY` means the Adafruit `begin()` call returned true. It is not proof of an attached or working panel.
- `EOA_TRANSFER_REQUESTED` means the void `display()` call returned. It is not proof of I2C ACK, visible pixels or a successful clear.
- No caller may use either result as a physical-display acknowledgement or allow `CFG_BOOT_NEEDS_SETUP` to reach READY solely on that basis.

### Remaining implementation risks

- The production board-level I2C initialization and serialized UI/boot caller are absent.
- The OLED screen cannot display the AP SSID because E29 remains OPEN.
- The passphrase remains visible on the physical panel until a future caller invokes `clear()`; a disclosure duration/acknowledgement policy is not established.
- The 100 kHz bus choice and actual panel readability have not been measured.
- The actual Adafruit library headers/linkage are not compiled in this environment; the expanded target build is required.

## Verification performed in this environment

| Check | Result | Scope |
|---|---:|---|
| OLED focused suite | 13 tests, 67 assertions, 0 failures | `g++` C++11 + `-Wall -Wextra -Werror` + ASan/UBSan + temporary Unity-compatible fake + fake Adafruit/Wire API |
| Smoke main syntax | Passed | Host `g++ -fsyntax-only` against project stubs; not ESP32 compilation |
| SF2 Node suite | 257/257 passed | Local Node run; SF2 source unchanged |
| SF2 module syntax | 25/25 passed | `node --check` |
| Full native PlatformIO | Not run | PlatformIO CLI not installed here |
| Expanded ESP32 target compile/link | Not run | PlatformIO CLI not installed here; v0.2.43 pre-OLED smoke build was user-confirmed passed |
| OLED / shared I2C hardware | Not run | No physical device access |

The prior v0.2.42 user-reported PlatformIO run covers 487 registrations across 29 suites, not the 13 new v0.2.44 registrations.

## Critical disposition

- **C6 remains blocking**: production boot composition, PIN KDF/verifier, HTTP/JSON routes and auth enforcement are absent.
- **C32 remains blocking**: no integrated OLED-visible credential acknowledgement/disclosure path exists.
- **E28 remains SIGNED** and unchanged.
- **E29 remains OPEN** (production SSID source/format).
- **E30 remains OPEN** (reset/rotation/recovery).
- No delegates or signed choices were changed.

## Required user verification

From a clean extraction of the complete v0.2.44 archive:

```sh
sha256sum -c MANIFEST.sha256
set -o pipefail
pio test -e native -f test_esp32_oled -vvv 2>&1 | tee oled-test.log
pio test -e native 2>&1 | tee native-test.log
(cd web/sf2 && npm test)
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link 2>&1 | tee esp32-target-smoke.log
```

The target smoke image must not be flashed as production firmware. After successful target linkage, device checks must confirm OLED readability, shared-bus operation at 100 kHz, and DS3231/EEPROM coexistence. The physical disclosure/acknowledgement design remains a separate blocker.
