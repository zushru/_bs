#pragma once

#include "fileset.h"
#include "littlefs_file_set.h"
#include "littlefs_serialized_fileset.h"

namespace att {

class ILittleFsMountBackend {
 public:
  virtual ~ILittleFsMountBackend() {}
  // Must never format, erase, or repair storage.
  virtual bool mountWithoutFormatting(const char* mountpoint,
                                      const char* partitionLabel) = 0;
  // Unregister only a mount created by this backend instance.
  virtual bool unmountOwned() = 0;
};

enum LittleFsOwnerState {
  LITTLEFS_OWNER_NOT_STARTED = 0,
  LITTLEFS_OWNER_STARTING,
  LITTLEFS_OWNER_READY,
  LITTLEFS_OWNER_FAULT
};

// Owns the mount-to-IFileSet startup boundary. Call begin() once from the
// serialized application startup path. Storage is unavailable until the
// mount succeeds and the expected VFS root/layout is validated.
class LittleFsStorageOwner final {
 public:
  LittleFsStorageOwner(ILittleFsMountBackend& backend,
                       const char* mountpoint,
                       const char* partitionLabel,
                       int16_t utcOffsetMin);

  LittleFsStorageOwner(const LittleFsStorageOwner&) = delete;
  LittleFsStorageOwner& operator=(const LittleFsStorageOwner&) = delete;

  bool begin();
  LittleFsOwnerState state() const { return state_; }
  bool cleanupFailed() const { return cleanupFailed_; }
  IFileSet* fileSet();

 private:
  enum {
    MOUNTPOINT_CAP = 32,     // Matches LittleFsFileSet mountpoint capacity.
    PARTITION_LABEL_CAP = 17 // esp_partition_t::label (16) plus NUL.
  };

  ILittleFsMountBackend& backend_;
  LittleFsFileSet files_;
  LittleFsStorageMutex storageMutex_;
  SerializedFileSet serializedFiles_;
  char mountpoint_[MOUNTPOINT_CAP];
  char partitionLabel_[PARTITION_LABEL_CAP];
  int16_t utcOffsetMin_;
  bool configValid_;
  bool cleanupFailed_;
  LittleFsOwnerState state_;
};

}  // namespace att
