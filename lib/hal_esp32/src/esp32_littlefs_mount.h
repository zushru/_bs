#pragma once

#include "littlefs_mount_owner.h"

namespace att {

// ESP-IDF VFS backend. Registration is no-format and rejects an already
// mounted partition so a second owner cannot silently adopt another path.
class Esp32LittleFsMountBackend final : public ILittleFsMountBackend {
 public:
  Esp32LittleFsMountBackend();
  Esp32LittleFsMountBackend(const Esp32LittleFsMountBackend&) = delete;
  Esp32LittleFsMountBackend& operator=(
      const Esp32LittleFsMountBackend&) = delete;
  Esp32LittleFsMountBackend(Esp32LittleFsMountBackend&&) = delete;
  Esp32LittleFsMountBackend& operator=(
      Esp32LittleFsMountBackend&&) = delete;

  bool mountWithoutFormatting(const char* mountpoint,
                              const char* partitionLabel) override;
  bool unmountOwned() override;

 private:
  bool mounted_;
  char partitionLabel_[17];
};

}  // namespace att
