# ESP32 adapter compile/link smoke application

This is not production firmware and must not be used as an attendance-device
build. It exists only to compile and link the current ESP32 AP, entropy, NVS,
PIN-attempt, AP credential bootstrap/setup coordinator, DHCP event integration and SSD1306 OLED adapters against the
selected Arduino-ESP32 / ESP-IDF / Adafruit package set.

The OLED adapter is configured for the project's 128x32 SSD1306 at I2C address
`0x3C`. It accepts an injected `TwoWire` bus and does not call `Wire.begin()`;
the future board-level I2C owner must initialize the shared bus once on SDA 21
and SCL 22 before calling `Esp32Oled::begin()`.

The smoke application also constructs an inert `ApCredentialSetupCoordinator`
and retains direct references to the coordinator, OLED bridge, and AP
DHCP-assignment methods. Authorization is supplied to `provision()` per request;
no authorization object is retained in the coordinator. Construction performs
no boot, provisioning, display, or AP calls.

The application deliberately does **not**:

- initialize or start the Wi-Fi driver or SoftAP;
- enable the bootstrap entropy source or request random bytes;
- initialize, write, erase, or migrate NVS;
- initialize or transmit to the I2C bus or OLED;
- accept a setup code, PIN, or network request.

The `setup()` function only reads volatile member-function references to keep
the adapter implementations and their ESP-IDF/Adafruit dependencies in the
final link. Global constructors only retain ordinary object state/references.

Build from the project root:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

Pinned display libraries: Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, and
Adafruit BusIO 1.17.4. These versions are compile/link dependencies only; the
actual panel, I2C bus, framebuffer allocation, and rendering are not tested by
this smoke image. A successful build is not proof of hardware behavior. Do not
flash this smoke image as production firmware.
