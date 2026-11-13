#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fileset.h"

namespace att {

#if !defined(ESP_PLATFORM)
struct LittleFsFileSetTestOps;
#endif

// POSIX/VFS-backed IFileSet for the mounted ESP-IDF LittleFS filesystem.
// The mountpoint must already be mounted and owned by the application.
// All operations must be serialized by the caller on the device loop.
class LittleFsFileSet final : public IFileSet {
 public:
  explicit LittleFsFileSet(const char* mountpoint);
#if !defined(ESP_PLATFORM)
  // Native-only injection seam; the supplied table must outlive this instance.
  LittleFsFileSet(const char* mountpoint,
                  const LittleFsFileSetTestOps* testOps);
#endif
  bool valid() const { return valid_; }

  bool countChecked(size_t& out) override;
  // One-pass structural validation plus fixed-record file-size alignment.
  bool countCheckedWithFileSizeAlignment(size_t alignment, size_t& out);
  // Read-only CRC, type, global sequence, and per-file commit-boundary audit.
  bool validateRecordFiles(size_t fileCount, int16_t utcOffsetMin);
  bool monthKeyChecked(size_t index, uint32_t& out) override;
  bool fileSizeChecked(size_t index, size_t& out) override;
  bool readFile(size_t index, size_t offset, uint8_t* out, size_t count) override;
  bool appendFile(size_t index, const uint8_t* data, size_t count) override;
  bool addFile(uint32_t monthKey) override;

  // Destructive recovery is deliberately unavailable in this increment.
  bool truncateFile(size_t index, size_t length) override;
  bool quarantineCopy(size_t index) override;
  bool quarantineFrom(size_t index) override;

 private:
  static const size_t MOUNTPOINT_CAP = 32;
  static const size_t PATH_CAP = 80;
  char mountpoint_[MOUNTPOINT_CAP];
  bool valid_;
#if !defined(ESP_PLATFORM)
  const LittleFsFileSetTestOps* testOps_;
#endif

  bool makePath_(const char* suffix, char* out, size_t capacity) const;
  bool monthPath_(uint32_t key, char* out, size_t capacity) const;
  bool ensureLogDirectory_(bool create) const;
  bool scan_(size_t* count, uint32_t afterKey, uint32_t* nextKey,
             size_t fileSizeAlignment = 0) const;
  bool sizeAtKey_(uint32_t key, size_t& out) const;
};

}  // namespace att
