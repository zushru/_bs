# Release v0.2.83: DS3231 clock HAL foundation

Date: 2026-11-11  
Baseline: user-uploaded `att_v0.2.82_2026-11-11.tar.gz`  
Status: development snapshot. Host behavior received focused manual verification;
PlatformIO target build and physical RTC behavior are not verified.

## Increment

Adds a DS3231 calendar/UTC conversion module and a bounded ESP32 `TwoWire`
transport. It supports 12/24-hour register decoding, BCD/calendar validation,
Gregorian leap years, the century bit, explicit UTC-offset conversion, separate
OSF reporting, explicit OSF clearing with alarm-flag preservation/read-back,
and uncertain-write semantics. The transport is injected with an already-owned
bus and never initializes `Wire`.

The shared native `Wire.h` stub now includes the DS3231 fake transport, and the
inert ESP32 adapter-link composition retains the new RTC methods. The smoke
application does not access the bus or RTC.

## Files changed

- `lib/hal_esp32/src/ds3231_clock.cpp/.h`
- `lib/hal_esp32/src/esp32_ds3231_register_io.cpp/.h`
- `test/test_ds3231/test_main.cpp`
- `test/test_ds3231/stubs/Wire.h`
- `test/test_esp32_oled/stubs/Wire.h`
- `target/esp32_adapter_link/main.cpp`
- `target/esp32_adapter_link/README.md`
- `docs/SPECS/ds3231_clock.md`
- `docs/AUDIT_v0.2.83.md`
- release, changelog, test, README, working-memory and manifest metadata

## Verification actually performed

The container did not have PlatformIO (`pio` was unavailable), so the PlatformIO
Unity suites and ESP32 target build could not be run. Two manual host compiles
used a temporary Unity-compatible assertion shim; this is not a PlatformIO/Unity
runner result.

- DS3231 suite: 33 registrations, compiled as C++11 with
  `-Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer`.
  Result: **33/33 passed**, process exit 0.
- Directly affected OLED suite, using the shared stub include path and the same
  compiler/sanitizers: **13/13 passed**, process exit 0.
- The RTC suite's fake transport models a short write where seconds/minutes
  are already visible before the next byte fails; `writeUtcEpoch()` returns
  `DS3231_IO_UNCERTAIN`. This is a modeled failure path, not physical RTC proof.
- A red regression run produced three failures for the newly-latched alarm flag,
  missing OSF verification, and failed status read-back. The implementation
  passed all three after fixing those cases.

Exact host compile command (from the extracted project root):

```sh
ROOT="$PWD"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I/tmp/att_ds3231_red \
  -I"$ROOT/test/test_esp32_oled/stubs" -I"$ROOT/lib/hal_esp32/src" \
  "$ROOT/test/test_ds3231/test_main.cpp" \
  "$ROOT/lib/hal_esp32/src/ds3231_clock.cpp" \
  "$ROOT/lib/hal_esp32/src/esp32_ds3231_register_io.cpp" \
  -o /tmp/test_ds3231 && /tmp/test_ds3231
```

The temporary shim at `/tmp/att_ds3231_red/unity.h` is session-local and is not
included in the project archive. The same compile pattern was used for
`test/test_esp32_oled/test_main.cpp` plus `lib/hal_esp32/src/esp32_oled.cpp`;
its include path used `test/test_esp32_oled/stubs`.

## Deliberately not run

- `pio test -e native -f 'test_ds3231'` and
  `pio test -e native -f 'test_esp32_oled'`: PlatformIO unavailable.
- `pio run -c platformio.esp32-smoke.ini -e esp32-adapter-link`: not run; target
  compile/link against the real pinned Arduino-ESP32 headers remains required.
- Full native N1, SF2 Node suites, hardware-probe build, device flash, RTC bus
  transactions, RTC time retention, OSF behavior, power-cut/brownout, sensor,
  OLED, EEPROM, AP or power-bank behavior: not run.

## Critical boundaries

This is a reusable HAL component, not production clock integration. No main,
scheduler, board-level I2C owner, RTC health gate, system-clock setter, log
clock-apply caller or startup path was added. RTC UTC-vs-local storage policy is
not selected: call with offset `0` to store UTC, or pass the same validated
configured offset for local wall-time storage. A future caller must treat OSF
as invalid time, explicitly clear it only after setting accepted time, and
reconcile uncertain writes. C3 remains blocking.

Never flash the inert ESP32 adapter-link target. Do not infer physical RTC
behavior from these host tests.
