#include "i2c_bus_lock.h"

namespace att {

I2cBusMutex::I2cBusMutex()
#if defined(ESP_PLATFORM)
    : mutex_(xSemaphoreCreateMutex())
#endif
{}

I2cBusMutex::~I2cBusMutex() {
#if defined(ESP_PLATFORM)
  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
#endif
}

bool I2cBusMutex::valid() const {
#if defined(ESP_PLATFORM)
  return mutex_ != nullptr;
#else
  return true;
#endif
}

bool I2cBusMutex::acquire() {
#if defined(ESP_PLATFORM)
  return mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
#else
  mutex_.lock();
  return true;
#endif
}

void I2cBusMutex::release() {
#if defined(ESP_PLATFORM)
  if (mutex_) (void)xSemaphoreGive(mutex_);
#else
  mutex_.unlock();
#endif
}

}  // namespace att
