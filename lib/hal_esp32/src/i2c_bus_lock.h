#pragma once

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <mutex>
#endif

namespace att {

// Shared application-level lock for every client of one TwoWire instance.
// Wire protects individual transactions, but its RX buffer is consumed after
// requestFrom() releases the framework mutex. Keep the lock through read().
class II2cBusLock {
 public:
  virtual ~II2cBusLock() {}
  virtual bool acquire() = 0;
  virtual void release() = 0;
};

// One mutex shared by every client of a single physical TwoWire bus.
// On ESP32 it is created after Arduino/FreeRTOS initialization by the owner.
class I2cBusMutex final : public II2cBusLock {
 public:
  I2cBusMutex();
  ~I2cBusMutex() override;
  I2cBusMutex(const I2cBusMutex&) = delete;
  I2cBusMutex& operator=(const I2cBusMutex&) = delete;

  bool valid() const;
  bool acquire() override;
  void release() override;

 private:
#if defined(ESP_PLATFORM)
  SemaphoreHandle_t mutex_;
#else
  std::mutex mutex_;
#endif
};

class I2cBusLockGuard {
 public:
  explicit I2cBusLockGuard(II2cBusLock& lock)
      : lock_(lock), acquired_(lock_.acquire()) {}
  ~I2cBusLockGuard() {
    if (acquired_) lock_.release();
  }
  bool acquired() const { return acquired_; }

  I2cBusLockGuard(const I2cBusLockGuard&) = delete;
  I2cBusLockGuard& operator=(const I2cBusLockGuard&) = delete;

 private:
  II2cBusLock& lock_;
  bool acquired_;
};

}  // namespace att
