# v0.2.83 critical implementation audit

Date: 2026-11-11

## Scope

This audit covers the new DS3231 conversion module, ESP32 register transport,
shared native `Wire.h` fake and inert smoke-link anchors. It does not cover a
production root or physical clock behavior.

## Findings addressed

1. **No implicit timezone.** The chip has no timezone metadata. Every conversion
   requires an explicit offset; zero means the chip registers contain UTC. The
   HAL does not choose a production convention or change signed P10/PHT.
2. **Calendar validity.** BCD nibbles, reserved bits, hour mode, hour range,
   date/month range, Gregorian leap day and century bit are checked before
   exposing an epoch. Conversion does not depend on host timezone APIs.
3. **OSF is not hidden.** Reads surface OSF separately and never clear it. The
   explicit clear operation preserves the other status bits. The future caller
   must not trust a candidate epoch while OSF is set.
4. **Partial write honesty.** Both time-register writes and OSF writes return
   `DS3231_IO_UNCERTAIN` if transport reports failure. The transport closes the
   transaction after a local buffer short-write. The fake models a visible
   prefix and the clock API reports uncertainty; physical acceptance is not
   verified.
5. **Status-register race.** The datasheet makes A1F/A2F write-zero-to-clear.
   The clear path writes ones to those bits so an alarm latched between the
   status read and write is preserved, then reads status back. A post-write I/O
   failure is uncertain; a set OSF returns `DS3231_OSF_STILL_SET`.
6. **Shared bus ownership.** The transport accepts an injected `TwoWire&`, uses
   address `0x68`, permits only the seven time registers or status register,
   uses a repeated-start read and never calls `Wire.begin()`.
7. **Smoke target remains inert.** The ESP32 adapter-link target only constructs
   the adapter and retains link references. No RTC method or bus operation is
   called from the smoke app.

## Verification evidence

- DS3231 host suite: **33/33** passed in a manual C++11 build with warnings as
  errors, AddressSanitizer and UBSan. The runner used a temporary
  Unity-compatible shim, not PlatformIO Unity.
- Existing OLED suite sharing the `Wire.h` stub: **13/13** passed under the
  same manual host approach.
- Current inventory: **700 C++ `RUN_TEST` registrations across 38 suites**;
  DS3231 adds 33. SF2 remains 270 Node registrations across 12 files.
- PlatformIO was unavailable. No PlatformIO native test, target build, ELF
  inspection, target-specific compiler result or physical-device test exists
  for v0.2.83.

## Critical review / remaining risks

- The HAL is not wired to `ConfigBootCoordinator`, `ClockPolicy`, system time,
  log replay or attendance. It cannot make a clock trusted by itself.
- The board-level I2C owner does not exist. Bus startup, pin configuration,
  timing, cross-device serialization and recovery after a bus fault remain open.
- The UTC-vs-local RTC convention is deliberately unresolved. Local-wall-time
  mode requires the same offset on every read/write and a policy for offset
  changes; UTC mode requires passing zero consistently.
- The HAL has no mutex. Concurrent OLED/RTC/EEPROM access would race unless the
  future root serializes all bus transactions.
- Host fake tests do not establish actual `TwoWire` runtime behavior, DS3231
  transaction timing, oscillator behavior, backup-battery retention, OSF
  setting/clearing, power-loss behavior or RTC accuracy.
- The temporary host shim is not the actual Unity implementation. The PIO
  suite still needs to run in the project toolchain.
- The smoke target source was updated but not compiled. Its member-function
  anchors and dependencies still require a pinned ESP32 target build.

## Not run / blockers

No PlatformIO native test, ESP32 target build/link, ELF inspection, hardware
probe, device flash, physical RTC/OLED/EEPROM, power cut, brownout, power-bank,
full native or SF2 test was run. No production main, scheduler, boot sequence,
clock-policy caller or I2C bus owner was added.

C3 remains blocking. A production clock caller must use the validated config,
handle OSF, enforce trusted UTC, and handle all `ClockApplyResult` values. On
`CLOCK_APPLY_COMMIT_UNCERTAIN`, it must stop writes, reopen/replay and reconcile
before retrying. This HAL does not change that contract.
