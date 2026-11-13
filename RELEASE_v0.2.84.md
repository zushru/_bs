# Release v0.2.84: shared I2C bus owner and read-only RTC probe

Date: 2026-11-11  
Baseline: working tree after v0.2.83 DS3231 HAL increment  
Status: development snapshot. Focused native suites passed; ESP32 target build did not complete in this session. No hardware behavior is claimed.

## Increment

Adds an `ESP_PLATFORM` guard around the ESP-IDF-only LittleFS mount backend so the shared HAL library can also compile in native test environments. Adds `Esp32I2cBusOwner` as the board-level owner of the shared `TwoWire` bus and its mutex. It initializes the injected bus once with caller-supplied SDA, SCL and clock settings. OLED, DS3231 and future EEPROM clients receive the same readiness-gated lock; no client can acquire it before successful initialization. A failed `Wire.begin()` is terminal for that owner because partial driver state is unknown and blind retry is unsafe.

The diagnostic hardware probe now constructs the OLED and DS3231 adapters against this same owner. After successful bus startup it performs a read-only DS3231 status-register read and reports OSF. It does not set time, clear OSF, or select UTC-versus-local RTC storage policy. The inert smoke target constructs the owner and retains its link reference but does not initialize or access I2C.

## Verification actually performed

Command:

```sh
pio test -e native -f 'test_i2c_bus_owner' -f 'test_ds3231' -f 'test_esp32_oled'
```

Result: **56/56 passed**, exit 0. Suites: owner 4/4, DS3231 36/36, OLED 16/16. This is focused native evidence, not full N1 regression or hardware proof.

The pinned ESP32 smoke build was attempted with:

```sh
pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link
```

The retained log `/tmp/att-v0.2.84-smoke.log` reached compilation of the Adafruit BusIO library, but the remote process session disappeared without a final PlatformIO summary and no `firmware.elf` was produced. Treat the target build as **INCOMPLETE / NOT VERIFIED**, not pass or compiler failure. The two changed I2C-owner translation units were then compiled directly with the pinned Xtensa compiler and Arduino-ESP32 3.3.12 / ESP-IDF headers:

```sh
CXX=/root/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-g++
SYS=/root/.platformio/packages/framework-arduinoespressif32-esp32-libs
CORE=/root/.platformio/packages/framework-arduinoespressif32
$CXX -std=gnu++11 -Wall -Wextra -Werror=return-type -DESP_PLATFORM \
  -iprefix "$SYS/include/" @"$SYS/flags/includes" \
  -I"$SYS/dio_qspi/include" -I"$CORE/libraries/Wire/src" \
  -I"$CORE/cores/esp32" -I"$CORE/variants/esp32" -Ilib/hal_esp32/src \
  -c lib/hal_esp32/src/i2c_bus_lock.cpp -o /tmp/att_i2c_bus_lock.o
# Same command for esp32_i2c_bus_owner.cpp -> /tmp/att_esp32_i2c_bus_owner.o
```

Both objects were produced successfully as Xtensa relocatable ELF files, exit 0. This proves those two translation units compile against the pinned target headers; it does **not** prove full target link, integration, final ELF or runtime behavior. The hardware-probe target was not built. No flash or physical I2C/RTC operation was performed.

## Critical boundaries

- No production firmware root or scheduler exists. The owner is integrated only in the diagnostic probe and inert link smoke application.
- All clients of the physical bus must use the exact owner lock. Direct `Wire` calls that bypass it can still interleave.
- The owner does not implement I2C bus recovery after a stuck bus or peripheral fault.
- A failed `Wire.begin()` permanently disables that owner instance; recovery requires a controlled owner/device restart, not an automatic retry.
- RTC time conversion, OSF clearing, trusted-time gating, `ClockPolicy`, log clock application and the RTC UTC-versus-local convention remain unintegrated and unresolved.
- The status-only probe is not evidence of valid calendar registers, oscillator accuracy, backup-battery retention or timekeeping through power loss.
- The smoke target is inert and must never be flashed. The hardware probe is diagnostic firmware, not attendance firmware.
