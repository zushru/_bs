# v0.2.84 critical implementation audit

Date: 2026-11-11

## Scope

Review of the native-build guard for the ESP-IDF-only LittleFS mount backend, `Esp32I2cBusOwner`, native owner tests, shared OLED/DS3231 lock use, hardware-probe RTC status read and inert smoke composition. Does not claim a production root or physical verification.

## Findings

1. **Native/target source boundary.** The ESP-IDF-only LittleFS mount implementation is now compiled only when `ESP_PLATFORM` is defined. This prevents native HAL builds from requiring ESP-IDF VFS headers; the pinned ESP32 target still compiles the real implementation.
2. **Single initialization point.** `begin()` is the only method that invokes `TwoWire::begin()`. It rejects negative pins, identical pins, zero clock and an invalid mutex before touching the bus. Repeated calls after success are idempotent and do not reinitialize.
3. **Fail-closed startup.** A failed `Wire.begin()` latches `attempted_` while `ready_` remains false. The owner refuses a second attempt because the framework may have partially initialized hardware. This is intentionally conservative; no automatic bus recovery is provided.
4. **Shared serialization.** `Esp32I2cBusOwner` implements `II2cBusLock`; its `acquire()` refuses before `ready_`, then delegates to one `I2cBusMutex`. OLED and DS3231 adapters receive the same owner object as their lock. The owner does not prevent other code from accessing global `Wire` directly, so architectural enforcement remains a composition-root responsibility.
5. **Probe is read-only.** The hardware probe performs a single DS3231 status-register read after bus initialization. It reports OSF and does not modify RTC registers. It does not read or interpret calendar fields, set time, or clear OSF.
6. **Smoke remains inert.** The adapter-link target constructs the owner and dependent adapters without calling `begin()` or performing I2C transactions.
7. **Test adequacy.** Four owner tests cover pin/clock forwarding, one-time initialization, terminal failure, invalid-argument no-touch behavior, readiness gating and shared lock identity. Existing 36 DS3231 and 16 OLED tests were rerun because their transport shares the Wire stub and owner lock contract.

## Verification

- `pio test -e native -f 'test_i2c_bus_owner' -f 'test_ds3231' -f 'test_esp32_oled'` — **56/56 passed**, exit 0.
- Direct Xtensa C++11 compilation of `i2c_bus_lock.cpp` and `esp32_i2c_bus_owner.cpp` against the pinned Arduino-ESP32 3.3.12 / ESP-IDF headers — **both passed**, exit 0; objects `/tmp/att_i2c_bus_lock.o` and `/tmp/att_esp32_i2c_bus_owner.o`. This is translation-unit compile evidence only.
- `pio run -j 1 -c platformio.esp32-smoke.ini -e esp32-adapter-link` — attempted twice; remote process sessions ended without a final summary and no final ELF exists. **Full build/link not verified.** Logs: `/tmp/att-v0.2.84-smoke.log`, `/tmp/att-v0.2.84-smoke-resume.log`.
- Hardware-probe build, ELF inspection, flash and physical-device tests were not run.

## Remaining blockers / adversarial review

- [UNVERIFIED: full Arduino-ESP32 3.3.12 build/link of the owner and changed compositions] The owner and lock translation units compile directly with the pinned Xtensa toolchain, but a completed PlatformIO target build and final ELF are still required.
- [UNVERIFIED: physical `Wire.begin()` failure behavior and retry safety] Bench test on the actual board; current implementation conservatively forbids retry.
- Any direct `Wire` call outside the shared lock can bypass serialization. Audit every future OLED/RTC/EEPROM call site before production integration.
- No recovery path exists for stuck SDA/SCL, bus timeout, peripheral reset or partial transaction. A failed transaction does not automatically reinitialize the bus.
- `ready_` is a startup-state flag, not an atomic cross-task state machine. Construct and initialize the owner before exposing clients to concurrent tasks; do not call `begin()` concurrently.
- No timezone convention is selected. Do not call `readUtcEpoch()` or `writeUtcEpoch()` from production until the RTC storage convention and offset migration behavior are explicitly decided.
- OSF must continue to make candidate time untrusted. The probe only reports the flag; it does not authorize clearing it.
- No production root, trusted-time health gate, system clock setter, attendance/log clock application or scheduler was added. C3 remains blocking.
