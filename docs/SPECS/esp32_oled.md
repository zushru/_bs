# ESP32 OLED adapter — v0.2.44

Status: standalone hardware adapter; not wired into production boot/UI. The display and I2C bus have not been tested on the user's device.

## Fixed hardware contract

| Property | Value |
|---|---|
| Controller | SSD1306 |
| Resolution | 128 × 32 pixels (user confirmed) |
| I2C address | `0x3C` |
| SDA / SCL | GPIO21 / GPIO22 |
| Reset | No separate reset pin (`-1`) |
| Bus ownership | One board-level shared `TwoWire` owner; OLED must not call `Wire.begin()` |
| Transaction clock | 100 kHz during and after display transactions |

The bus is also reserved for the DS3231 (`0x68`) and EEPROM (`0x57`). The adapter receives `TwoWire&` by reference and calls the Adafruit driver with `periphBegin=false`. The future board composition must call `Wire.begin(21, 22)` exactly once before `Esp32Oled::begin()`. No other module should independently initialize the same bus.

## Pinned software dependencies

- Adafruit SSD1306 2.5.17
- Adafruit GFX Library 1.12.6
- Adafruit BusIO 1.17.4

The dependency chain is SSD1306 → GFX → BusIO as declared by the pinned `library.properties` files. The v0.2.44 target smoke config pins all three and keeps this target separate from the default native test environment.

## API and behavior

`Esp32Oled::begin()` calls:

```cpp
 display_.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
```

`EOI_BUFFER_READY` means the Adafruit library returned true after framebuffer allocation and initialization calls. It does not prove a physical display is connected or that initialization commands were accepted.

`showSetupCredential(result, passphrase, length)` is intentionally narrow:

- requires `result.code == APCB_OK` and `result.disclose == true`;
- requires exactly 32 lowercase hexadecimal bytes, matching signed E28 first-provision format;
- rejects malformed, uppercase, non-hex, short and oversized values rather than truncating;
- when the result is non-disclosing or invalid, it requests a blank frame so a previous secret is not intentionally left visible;
- renders four 8-pixel text rows: `Wi-Fi setup`, `WPA2 password:`, characters 0–15, characters 16–31;
- requests a display transfer, then clears the driver's software framebuffer; the OLED's visible frame remains until `clear()`.

The method does not modify the caller's password buffer and does not construct a second plaintext string. The pixel framebuffer is cleared immediately after the display transfer call returns.

`clear()` clears the framebuffer and calls `display()` to request a blank panel. No automatic timeout or acknowledgement button is implemented; the caller must decide when to clear the visible credential.

## Critical API limitation

Adafruit SSD1306 2.5.17 declares `begin()` as `bool`, but `display()` is `void`. `begin()` can report framebuffer allocation failure; it is not a reliable panel-presence test. A completed `display()` call does not expose `Wire.endTransmission()` status to this adapter. Therefore:

- `EOI_BUFFER_READY` is not a hardware-ready proof;
- `EOA_TRANSFER_REQUESTED` is not proof of I2C acknowledgement or visible pixels;
- the adapter alone cannot be treated as a verified credential-disclosure gate;
- production boot must not claim E28 disclosure complete solely because the method returned `EOA_TRANSFER_REQUESTED`.

Before production `NEEDS_SETUP` → `READY` transition, decide and implement a visible-display verification/acknowledgement contract that cannot accidentally complete if the panel is absent. E29 is SIGNED (`att`); E31 is DELEGATED (channel 1). E30 is DELEGATED to no remote reset/rotation; USB service recovery is not implemented.

## Verification

- `test_esp32_oled`: 13 registrations. The local sanitizer-enabled Unity-compatible stand-in run passed 13/13. It tests formatting/state calls through a fake Adafruit API, not the actual library or panel.
- SF2: 257/257 Node registrations passed in v0.2.44; all 25 `.mjs` modules pass `node --check`.
- The user reports the v0.2.43 ESP32 target smoke build succeeded with Espressif32 8.0.0, Arduino-ESP32 3.3.12 and Xtensa toolchain 14.2.0+20260121. v0.2.44 expands the smoke build with the three pinned Adafruit libraries; that expanded target build still needs user verification.
- No OLED hardware test, I2C bus-sharing test, panel write acknowledgement, visible credential review or full native PlatformIO run is claimed for v0.2.44.
