#pragma once

#include <stdint.h>

#include <Wire.h>

#include "i2c_bus_lock.h"

namespace att {

// Board-level owner for the shared OLED / DS3231 / EEPROM TwoWire instance.
// Construct after Arduino/FreeRTOS initialization. A failed Wire.begin() is
// terminal for this owner: its partial hardware state is unknown, so it must
// not blindly retry or permit clients to transact.
class Esp32I2cBusOwner final : public II2cBusLock {
 public:
  explicit Esp32I2cBusOwner(TwoWire& wire)
      : wire_(wire), attempted_(false), ready_(false) {}

  Esp32I2cBusOwner(const Esp32I2cBusOwner&) = delete;
  Esp32I2cBusOwner& operator=(const Esp32I2cBusOwner&) = delete;

  bool begin(int sda, int scl, uint32_t clockHz);
  bool ready() const { return ready_; }
  // Clients receive this readiness-gated lock, never the raw mutex.
  II2cBusLock& lock() { return *this; }
  bool lockValid() const { return lock_.valid(); }
  bool acquire() override;
  void release() override;

 private:
  TwoWire& wire_;
  I2cBusMutex lock_;
  bool attempted_;
  bool ready_;
};

}  // namespace att
