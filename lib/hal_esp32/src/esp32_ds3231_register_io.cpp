#include "esp32_ds3231_register_io.h"

#include <string.h>

#include "Wire.h"

namespace att {
namespace {
const uint8_t kDs3231Address = 0x68;
const uint8_t kTimeRegister = 0x00;
const uint8_t kStatusRegister = 0x0F;
const uint8_t kTimeRegisterCount = 7;
}

bool Esp32Ds3231RegisterIo::readRegisters(uint8_t firstRegister,
                                         uint8_t* out, uint8_t length) {
  if (out == nullptr ||
      !((firstRegister == kTimeRegister && length == kTimeRegisterCount) ||
        (firstRegister == kStatusRegister && length == 1))) return false;

  I2cBusLockGuard busGuard(busLock_);
  if (!busGuard.acquired()) return false;

  uint8_t candidate[kTimeRegisterCount] = {};
  wire_.beginTransmission(kDs3231Address);
  if (wire_.write(firstRegister) != 1) {
    (void)wire_.endTransmission(true);
    return false;
  }
  if (wire_.endTransmission(false) != 0) return false;

  const size_t received = wire_.requestFrom(
      kDs3231Address, static_cast<size_t>(length), true);
  if (received != length) {
    while (wire_.available() > 0) (void)wire_.read();
    return false;
  }

  bool complete = true;
  for (uint8_t i = 0; i < length; ++i) {
    if (wire_.available() <= 0) {
      complete = false;
      break;
    }
    const int value = wire_.read();
    if (value < 0) {
      complete = false;
      break;
    }
    candidate[i] = static_cast<uint8_t>(value);
  }
  if (!complete) {
    while (wire_.available() > 0) (void)wire_.read();
    return false;
  }
  memcpy(out, candidate, length);
  return true;
}

Ds3231WriteResult Esp32Ds3231RegisterIo::writeRegisters(
    uint8_t firstRegister, const uint8_t* data, uint8_t length) {
  if (data == nullptr ||
      !((firstRegister == kTimeRegister && length == kTimeRegisterCount) ||
        (firstRegister == kStatusRegister && length == 1)))
    return DS3231_WRITE_NOT_STARTED;

  I2cBusLockGuard busGuard(busLock_);
  if (!busGuard.acquired()) return DS3231_WRITE_NOT_STARTED;

  wire_.beginTransmission(kDs3231Address);
  bool buffered = wire_.write(firstRegister) == 1;
  for (uint8_t i = 0; buffered && i < length; ++i)
    buffered = wire_.write(data[i]) == 1;

  // Always close the transaction, including the short-buffer path. If some
  // bytes were buffered before a failure, the RTC may have accepted a prefix.
  const uint8_t result = wire_.endTransmission(true);
  return buffered && result == 0 ? DS3231_WRITE_OK
                                  : DS3231_WRITE_UNCERTAIN;
}

}  // namespace att
