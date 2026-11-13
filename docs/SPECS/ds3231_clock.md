# DS3231 clock HAL contract

Status: reusable HAL implementation contract introduced in v0.2.83. This is not
an approved production clock policy and does not select the device's canonical
RTC storage convention.

## Hardware and ownership

- RTC: DS3231, fixed 7-bit I2C address `0x68`.
- Shared bus: board-level owner must initialize the shared `TwoWire` once on
  SDA GPIO21 / SCL GPIO22 before any OLED, RTC or EEPROM operation.
- `Esp32Ds3231RegisterIo` receives an existing `TwoWire&`; it never calls
  `Wire.begin()`, changes bus pins, changes bus clock, or owns synchronization.
- The transport accepts only the seven time registers `0x00..0x06` and the
  status register `0x0F`. Reads use a register-pointer write followed by a
  repeated-start read. Writes use one bounded transaction.
- All shared-bus access must be serialized by the future application/bus owner.
  This adapter contains no mutex and is not safe for concurrent callers.

## Time representation and offset

The DS3231 stores calendar fields but no timezone. `readUtcEpoch()` and
`writeUtcEpoch()` therefore require an explicit `utcOffsetMin` on every call,
validated to the existing project range `-720..+840` minutes.

- Pass `0` if the DS3231 registers are to contain UTC.
- Pass the same validated configured offset on both read and write if registers
  are to contain configured local wall time.
- A caller must not silently change the offset used to interpret already-stored
  local wall time. A configuration/time-zone change needs an explicit migration
  decision at the application layer.
- This adapter does not decide whether production stores UTC or local wall time,
  does not implement DST, and does not change signed P10/PHT (`UTC+8`) policy.

Conversion is independent of libc timezone state. It handles 12-hour and
24-hour register encoding, BCD validation, Gregorian leap years, the DS3231
century bit, and the project's `uint32_t` Unix-epoch boundary. The calendar
register convention is century bit clear = 2000–2099 and set = 2100–2199; this
is the convention written by this adapter. The adapter does not enforce the
application's minimum attendance date (currently governed elsewhere).

## Validity and failure semantics

- `readUtcEpoch()` reads time registers and status separately. It leaves its
  output unchanged on bad arguments, I2C failure, invalid BCD/calendar fields,
  or an unrepresentable UTC epoch.
- `Ds3231Reading::oscillatorStopped` exposes OSF (status bit 7) separately from
  the candidate epoch. A candidate read while OSF is set is not trusted time;
  the future caller must fail closed until a valid time is deliberately set and
  OSF is explicitly cleared.
- A read never clears OSF. `writeUtcEpoch()` also deliberately does not clear it.
- `clearOscillatorStopFlag()` is the only operation that clears OSF. It reads
  status first, clears bit 7, writes `1` to A1F/A2F (bits 0–1) so a newly latched
  alarm flag is not accidentally cleared, preserves the other status bits, and
  reads status back to verify OSF remains clear.
- A failed read before the status write returns `DS3231_IO_ERROR`. A failed
  status write or failed read-back returns `DS3231_IO_UNCERTAIN`; the operation
  may have changed the RTC but cannot be confirmed. A read-back with OSF still
  set returns `DS3231_OSF_STILL_SET`, and the time remains untrusted.
- `DS3231_IO_ERROR` denotes a failed read/no trusted output. `DS3231_IO_UNCERTAIN`
  denotes an indeterminate write or post-write verification. The caller must
  re-read and reconcile before retrying; it must not assume a failed operation
  left the clock unchanged.
- The transport closes a transaction even when its local transmit buffer fills
  partway through. A prefix may already have been transmitted; the clock layer
  reports that outcome as uncertain. Host fake visibility is not hardware proof.

## Production integration remains blocked

No production root, serialized scheduler, board-level I2C owner, RTC startup
policy, system-clock setter, health gate, or `ClockPolicy` caller is added here.
The future caller must:

1. Start `ConfigBootCoordinator` and obtain a validated configuration before
   selecting the offset passed to this API.
2. Initialize the shared I2C bus exactly once, then serialize OLED/RTC/EEPROM
   access.
3. Treat OSF as untrusted time; set/validate a clock before explicitly clearing
   OSF.
4. Feed only a trusted UTC epoch to `ClockCtx` and handle all four
   `ClockApplyResult` values. On `CLOCK_APPLY_COMMIT_UNCERTAIN`, stop writes,
   reopen/replay the log and reconcile before retrying.
5. Define what happens when configured UTC offset changes if the RTC contains
   local wall time.

The DS3231 register definitions, BCD encoding and OSF semantics are from the
Analog Devices DS3231 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf.
The `TwoWire` calls are matched to the pinned Arduino-ESP32 3.3.12 header:
https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/Wire/src/Wire.h.
