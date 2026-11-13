#pragma once

#include "ds3231_clock.h"
#include "i2c_bus_lock.h"

class TwoWire;

namespace att {

// Uses an already-initialized, externally-owned TwoWire bus. This adapter never
// calls Wire.begin(); GPIO21/GPIO22 are shared with the OLED and EEPROM.
class Esp32Ds3231RegisterIo : public IDs3231RegisterIo {
 public:
  Esp32Ds3231RegisterIo(TwoWire& wire, II2cBusLock& busLock)
      : wire_(wire), busLock_(busLock) {}

  bool readRegisters(uint8_t firstRegister, uint8_t* out,
                    uint8_t length) override;
  Ds3231WriteResult writeRegisters(uint8_t firstRegister,
                                    const uint8_t* data,
                                    uint8_t length) override;

 private:
  TwoWire& wire_;
  II2cBusLock& busLock_;
};

}  // namespace att
