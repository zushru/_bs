# ESP32 adapter compile/link smoke application

This is not production firmware and must not be used as an attendance-device
build. It exists only to compile and link the current ESP32 AP, entropy, NVS,
PIN-attempt, AP credential bootstrap/setup coordinator, DHCP event integration,
SSD1306 OLED adapters, AS608 runtime, roster-transition audit, and LittleFS
storage stack (`LittleFsFileSet`, `SerializedFileSet`, `LittleFsStorageOwner`,
and `Esp32LittleFsMountBackend`) against the selected Arduino-ESP32 / ESP-IDF /
Adafruit package set. `LittleFsStorageOwner` requires an explicit validated UTC
offset; this smoke-only fixture supplies PHT (480 minutes). The roster audit object is retained in the smoke image so
its fixed per-ID state is included in the actual target ABI/BSS measurement.
The LittleFS stack is retained only for target compile/link compatibility; the
smoke image does not mount a filesystem or perform file operations. The owner
constructor allocates its FreeRTOS mutex, but `begin()` is not called, so no
partition is registered or accessed. The adapter's path metadata helper uses
POSIX lstat() on host builds and stat() under ESP_PLATFORM: the pinned ESP-IDF
newlib headers do not declare lstat(), and the ESP LittleFS backend has no
symbolic-link entry type. Host symlink-rejection regressions remain in
test_littlefs_fileset.

The shared `Esp32I2cBusOwner` owns one `TwoWire` instance and one mutex for the
OLED, DS3231 and future EEPROM clients. It initializes the bus once on SDA 21 /
SCL 22 at the board-selected clock. A failed `Wire.begin()` is terminal for
that owner because partial driver state is unknown. The smoke target constructs
the owner and adapters but never calls its `begin()` method or accesses I2C.
The OLED remains configured for 128x32 SSD1306 at address `0x3C`.

The smoke application also retains compile/link-only method references for
`ApplicationAttendanceGate`, `As608AttendanceOwner`, and
`As608OwnerConfigRuntime`. These methods are not invoked and those objects are
not constructed here; the anchors only force the corresponding translation
units and dependencies through the ESP32 compiler/linker.

The smoke application also constructs an inert `ApCredentialSetupCoordinator`
and retains direct references to the coordinator, OLED bridge, and AP
DHCP-assignment methods. Authorization is supplied to `provision()` per request;
no authorization object is retained in the coordinator. Construction performs
no boot, provisioning, display, or AP calls.

The application deliberately does **not**:

- initialize or start the Wi-Fi driver or SoftAP;
- enable the bootstrap entropy source or request random bytes;
- initialize, write, erase, or migrate NVS;
- initialize or transmit to the I2C bus, RTC or OLED;
- register or mount LittleFS, enumerate its entries, or perform file I/O;
- accept a setup code, PIN, or network request.

The `setup()` function constructs smoke-only object compositions and reads live
member-function link probes to retain the adapter implementations and their
ESP-IDF/Adafruit dependencies in the final ELF. Constructing the storage owner
allocates one FreeRTOS mutex; it does not call `begin()`. No mount or partition
access occurs.

Build from the project root:

```sh
pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

Pinned display libraries: Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, and
Adafruit BusIO 1.17.4. These versions are compile/link dependencies only; the
actual panel, I2C bus, framebuffer allocation, and rendering are not tested by
this smoke image. A successful build is not proof of hardware behavior. Do not
flash this smoke image as production firmware.
