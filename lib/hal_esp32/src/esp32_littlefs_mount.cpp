#include "esp32_littlefs_mount.h"

// This backend is compiled only by the ESP-IDF target. The HAL library is
// also built by native suites, where ESP-IDF VFS headers do not exist.
#if defined(ESP_PLATFORM)
#include <esp_err.h>
#include <esp_littlefs.h>
#include <esp_vfs.h>
#include <string.h>

namespace att {

Esp32LittleFsMountBackend::Esp32LittleFsMountBackend()
    : mounted_(false), partitionLabel_{0} {}

bool Esp32LittleFsMountBackend::mountWithoutFormatting(
    const char* mountpoint, const char* partitionLabel) {
  if (!mountpoint || mountpoint[0] != '/' || !partitionLabel ||
      partitionLabel[0] == '\0')
    return false;

  const size_t pathLength = strlen(mountpoint);
  if (pathLength < 2 || pathLength > ESP_VFS_PATH_MAX ||
      mountpoint[pathLength - 1] == '/')
    return false;

  const size_t labelLength = strlen(partitionLabel);
  if (labelLength >= sizeof(((esp_partition_t*)0)->label)) return false;

  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partitionLabel);
  if (!partition ||
      (partition->subtype != ESP_PARTITION_SUBTYPE_DATA_SPIFFS &&
       partition->subtype != ESP_PARTITION_SUBTYPE_DATA_LITTLEFS) ||
      partition->readonly || partition->encrypted)
    return false;

  // Do not accept an existing mount: its base path may belong to another
  // owner. Registration returns INVALID_STATE on duplicate partition/path.
  if (esp_littlefs_mounted(partitionLabel)) return false;

  esp_vfs_littlefs_conf_t config = {};
  config.base_path = mountpoint;
  config.partition_label = partitionLabel;
  config.partition = nullptr;
  config.format_if_mount_failed = false;
  // This firmware entry point is a read-only boot shell. Enforce that at
  // the VFS boundary as well as at the call sites; a future writable
  // production owner must use a separately reviewed mount configuration.
  config.read_only = true;
  config.dont_mount = false;
  config.grow_on_mount = false;

  if (esp_vfs_littlefs_register(&config) != ESP_OK) return false;
  memcpy(partitionLabel_, partitionLabel, labelLength + 1);
  mounted_ = true;
  return true;
}

bool Esp32LittleFsMountBackend::unmountOwned() {
  if (!mounted_ || partitionLabel_[0] == '\0') return false;
  if (esp_vfs_littlefs_unregister(partitionLabel_) != ESP_OK) return false;
  mounted_ = false;
  partitionLabel_[0] = '\0';
  return true;
}

}  // namespace att

#endif  // ESP_PLATFORM
