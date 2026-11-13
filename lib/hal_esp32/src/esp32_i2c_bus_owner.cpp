#include "esp32_i2c_bus_owner.h"

namespace att {

bool Esp32I2cBusOwner::acquire() {
  return ready_ && lock_.acquire();
}

void Esp32I2cBusOwner::release() {
  if (ready_) lock_.release();
}

bool Esp32I2cBusOwner::begin(int sda, int scl, uint32_t clockHz) {
  if (ready_) return true;
  if (attempted_ || sda < 0 || scl < 0 || sda == scl || clockHz == 0 ||
      !lock_.valid()) return false;

  attempted_ = true;
  ready_ = wire_.begin(sda, scl, clockHz);
  return ready_;
}

}  // namespace att
