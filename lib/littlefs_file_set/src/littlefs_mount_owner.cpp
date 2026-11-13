#include "littlefs_mount_owner.h"
#include "event_codec.h"

#include <string.h>

namespace att {
namespace {

bool copyBounded(char* destination, size_t capacity, const char* source) {
  if (!destination || !source || capacity == 0) return false;
  const size_t length = strlen(source);
  if (length == 0 || length >= capacity) return false;
  memcpy(destination, source, length + 1);
  return true;
}

}  // namespace

LittleFsStorageOwner::LittleFsStorageOwner(
    ILittleFsMountBackend& backend, const char* mountpoint,
    const char* partitionLabel, int16_t utcOffsetMin)
    : backend_(backend),
      files_(mountpoint),
      storageMutex_(),
      serializedFiles_(files_, storageMutex_),
      mountpoint_{0},
      partitionLabel_{0},
      utcOffsetMin_(utcOffsetMin),
      configValid_(false),
      cleanupFailed_(false),
      state_(LITTLEFS_OWNER_NOT_STARTED) {
  configValid_ = files_.valid() && storageMutex_.valid() &&
      utcOffsetMin_ >= -720 && utcOffsetMin_ <= 840 &&
      copyBounded(mountpoint_, sizeof mountpoint_, mountpoint) &&
      copyBounded(partitionLabel_, sizeof partitionLabel_, partitionLabel);
}

bool LittleFsStorageOwner::begin() {
  if (state_ == LITTLEFS_OWNER_READY) return true;
  if (state_ != LITTLEFS_OWNER_NOT_STARTED) return false;

  state_ = LITTLEFS_OWNER_STARTING;
  if (!configValid_ ||
      !backend_.mountWithoutFormatting(mountpoint_, partitionLabel_)) {
    state_ = LITTLEFS_OWNER_FAULT;
    return false;
  }

  // Validate the VFS root, canonical entries, record lengths, CRCs, types,
  // global sequence, and per-file commit boundaries. This is read-only.
  size_t existingFiles = 0;
  if (!files_.countCheckedWithFileSizeAlignment(REC_SIZE, existingFiles) ||
      !files_.validateRecordFiles(existingFiles, utcOffsetMin_)) {
    // The mount was created by this owner, but the data layout is not safe
    // to expose. Release only this backend's mount; never format or repair.
    cleanupFailed_ = !backend_.unmountOwned();
    state_ = LITTLEFS_OWNER_FAULT;
    return false;
  }
  (void)existingFiles;

  state_ = LITTLEFS_OWNER_READY;
  return true;
}

IFileSet* LittleFsStorageOwner::fileSet() {
  return state_ == LITTLEFS_OWNER_READY ? &serializedFiles_ : nullptr;
}

}  // namespace att
