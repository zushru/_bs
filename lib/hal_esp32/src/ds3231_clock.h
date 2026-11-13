#pragma once

#include <stdint.h>

namespace att {

// DS3231 register access is injected so calendar conversion and validity rules
// can be tested without an RTC or an initialized I2C bus.
enum Ds3231WriteResult : uint8_t {
  DS3231_WRITE_OK = 0,
  DS3231_WRITE_NOT_STARTED,  // lock/argument gate refused before Wire access
  DS3231_WRITE_UNCERTAIN     // Wire may have accepted none, some, or all bytes
};

class IDs3231RegisterIo {
 public:
  virtual ~IDs3231RegisterIo() {}
  virtual bool readRegisters(uint8_t firstRegister, uint8_t* out,
                             uint8_t length) = 0;
  virtual Ds3231WriteResult writeRegisters(uint8_t firstRegister,
                                            const uint8_t* data,
                                            uint8_t length) = 0;
};

enum Ds3231Code : uint8_t {
  DS3231_OK = 0,
  DS3231_INVALID_ARGUMENT,
  DS3231_IO_ERROR,       // read failed; no output value is exposed
  DS3231_IO_UNCERTAIN,   // write may have committed partially or completely
  DS3231_OSF_STILL_SET,  // oscillator-stop flag remained/set during clear
  DS3231_INVALID_TIME,
  DS3231_IO_NOT_STARTED  // write refused before any Wire operation
};

struct Ds3231Reading {
  uint32_t epochUtc;
  bool oscillatorStopped;  // OSF is evidence; a read never clears it.
};

// DS3231 registers contain no timezone metadata. Every conversion therefore
// requires an explicit offset: pass 0 when storing UTC, or the same validated
// configured offset used to write/read configured local wall time. No default
// PHT offset or implicit timezone policy is selected by this HAL.
class Ds3231Clock {
 public:
  explicit Ds3231Clock(IDs3231RegisterIo& io) : io_(io) {}

  Ds3231Code readUtcEpoch(int16_t utcOffsetMin, Ds3231Reading& out) const;

  // Writes only the seven calendar registers. It deliberately does not clear
  // OSF; callers must do that explicitly after accepting the new clock value.
  Ds3231Code writeUtcEpoch(uint32_t epochUtc, int16_t utcOffsetMin);

  // Explicitly clear only OSF while preserving other writable status flags.
  Ds3231Code clearOscillatorStopFlag();

 private:
  IDs3231RegisterIo& io_;
};

}  // namespace att
