#include "esp32_bootstrap_random.h"
#include <bootloader_random.h>
#include <esp_random.h>

namespace att {

bool Esp32BootstrapRandom::fill(uint8_t* out, size_t length) {
  if (out == nullptr || length == 0) return false;
  // ESP-IDF 5.5.5 documents this SAR-ADC entropy source for true random output
  // without RF. It MUST be disabled before esp_wifi_start() or ADC/I2S use.
  bootloader_random_enable();
  esp_fill_random(out, length);
  bootloader_random_disable();
  return true;
}

}  // namespace att
