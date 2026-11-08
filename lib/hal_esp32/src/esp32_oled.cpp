#include "esp32_oled.h"

namespace att {
namespace {
const uint32_t kI2cClockHz = 100000UL;
}

Esp32Oled::Esp32Oled(TwoWire& sharedBus)
    : display_(kWidth, kHeight, &sharedBus, -1, kI2cClockHz, kI2cClockHz),
      initialized_(false) {}

Esp32OledInitCode Esp32Oled::begin() {
  if (initialized_) return EOI_ALREADY_INITIALIZED;

  // The shared bus is owned by the future board-level I2C composition root.
  // periphBegin=false prevents Adafruit from reinitializing/reconfiguring it.
  if (!display_.begin(SSD1306_SWITCHCAPVCC, kAddress, true, false)) {
    return EOI_BUFFER_ALLOC_FAILED;
  }

  initialized_ = true;
  resetFramebuffer_();
  display_.display();
  return EOI_BUFFER_READY;
}

bool Esp32Oled::validSetupPassphrase_(const char* passphrase,
                                      size_t length) {
  if (passphrase == nullptr || length != kSetupPassphraseLength) return false;
  for (size_t i = 0; i < length; ++i) {
    const char ch = passphrase[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
      return false;
    }
  }
  return true;
}

void Esp32Oled::resetFramebuffer_() {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
}

Esp32OledActionCode Esp32Oled::showSetupCredential(
    const ApCredentialResult& result, const char* passphrase,
    size_t passphraseLength) {
  if (!initialized_) return EOA_NOT_INITIALIZED;

  // A non-disclosure result or invalid credential must not leave an older
  // setup secret visible after the caller moves to another boot state.
  if (result.code != APCB_OK || !result.disclose) {
    return clear() == EOA_TRANSFER_REQUESTED ? EOA_NOT_DISCLOSABLE
                                             : EOA_NOT_INITIALIZED;
  }
  if (!validSetupPassphrase_(passphrase, passphraseLength)) {
    clear();
    return EOA_INVALID_CREDENTIAL;
  }

  resetFramebuffer_();
  display_.setCursor(0, 0);
  display_.print("Wi-Fi: att");
  display_.setCursor(0, 8);
  display_.print("WPA2 password:");

  display_.setCursor(16, 16);
  for (size_t i = 0; i < 16; ++i) {
    display_.write(static_cast<uint8_t>(passphrase[i]));
  }
  display_.setCursor(16, 24);
  for (size_t i = 16; i < kSetupPassphraseLength; ++i) {
    display_.write(static_cast<uint8_t>(passphrase[i]));
  }

  // The library's display() returns void. Make the transfer request, then
  // erase the rendered bitmap from RAM while leaving the panel image intact.
  display_.display();
  display_.clearDisplay();
  return EOA_TRANSFER_REQUESTED;
}

Esp32OledActionCode Esp32Oled::clear() {
  if (!initialized_) return EOA_NOT_INITIALIZED;
  resetFramebuffer_();
  display_.display();
  return EOA_TRANSFER_REQUESTED;
}

bool Esp32Oled::requestCredentialDisplay(const ApCredentialResult& result,
                                         const char* passphrase,
                                         size_t passphraseLength) {
  return showSetupCredential(result, passphrase, passphraseLength) ==
         EOA_TRANSFER_REQUESTED;
}

bool Esp32Oled::requestBlankFrame() {
  return clear() == EOA_TRANSFER_REQUESTED;
}

}  // namespace att
