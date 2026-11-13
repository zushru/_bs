#include "littlefs_serialized_fileset.h"

namespace att {
namespace {

class LockGuard {
 public:
  explicit LockGuard(ILittleFsStorageLock& lock)
      : lock_(lock), acquired_(lock_.acquire()) {}
  ~LockGuard() {
    if (acquired_) lock_.release();
  }
  bool acquired() const { return acquired_; }

  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  ILittleFsStorageLock& lock_;
  bool acquired_;
};

}  // namespace

LittleFsStorageMutex::LittleFsStorageMutex()
#if defined(ESP_PLATFORM)
    : mutex_(xSemaphoreCreateMutex())
#endif
{}

LittleFsStorageMutex::~LittleFsStorageMutex() {
#if defined(ESP_PLATFORM)
  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
#endif
}

bool LittleFsStorageMutex::valid() const {
#if defined(ESP_PLATFORM)
  return mutex_ != nullptr;
#else
  return true;
#endif
}

bool LittleFsStorageMutex::acquire() {
#if defined(ESP_PLATFORM)
  return mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
#else
  mutex_.lock();
  return true;
#endif
}

void LittleFsStorageMutex::release() {
#if defined(ESP_PLATFORM)
  if (mutex_) (void)xSemaphoreGive(mutex_);
#else
  mutex_.unlock();
#endif
}

SerializedFileSet::SerializedFileSet(
    IFileSet& files, ILittleFsStorageLock& lock)
    : files_(files), lock_(lock) {}

bool SerializedFileSet::countChecked(size_t& out) {
  out = 0;
  LockGuard guard(lock_);
  if (!guard.acquired()) return false;
  if (!files_.countChecked(out)) {
    out = 0;
    return false;
  }
  return true;
}

bool SerializedFileSet::monthKeyChecked(size_t index, uint32_t& out) {
  out = 0;
  LockGuard guard(lock_);
  if (!guard.acquired()) return false;
  if (!files_.monthKeyChecked(index, out)) {
    out = 0;
    return false;
  }
  return true;
}

bool SerializedFileSet::fileSizeChecked(size_t index, size_t& out) {
  out = 0;
  LockGuard guard(lock_);
  if (!guard.acquired()) return false;
  if (!files_.fileSizeChecked(index, out)) {
    out = 0;
    return false;
  }
  return true;
}

bool SerializedFileSet::readFile(size_t index, size_t offset, uint8_t* out,
                                 size_t count) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.readFile(index, offset, out, count);
}

bool SerializedFileSet::appendFile(size_t index, const uint8_t* data,
                                   size_t count) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.appendFile(index, data, count);
}

bool SerializedFileSet::truncateFile(size_t index, size_t length) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.truncateFile(index, length);
}

bool SerializedFileSet::addFile(uint32_t monthKey) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.addFile(monthKey);
}

bool SerializedFileSet::quarantineCopy(size_t index) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.quarantineCopy(index);
}

bool SerializedFileSet::quarantineFrom(size_t index) {
  LockGuard guard(lock_);
  return guard.acquired() && files_.quarantineFrom(index);
}

}  // namespace att
