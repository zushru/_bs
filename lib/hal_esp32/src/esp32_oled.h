#pragma once

#include <stddef.h>
#include <stdint.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#include "ap_credential_display.h"
#include "i2c_bus_lock.h"

namespace att {

enum Esp32OledInitCode : uint8_t {
  EOI_BUFFER_READY,
  EOI_BUFFER_ALLOC_FAILED,
  EOI_ALREADY_INITIALIZED,
  EOI_BUS_LOCK_FAILED
};

enum Esp32OledActionCode : uint8_t {
  EOA_TRANSFER_REQUESTED,
  EOA_NOT_INITIALIZED,
  EOA_NOT_DISCLOSABLE,
  EOA_INVALID_CREDENTIAL,
  EOA_BUS_LOCK_FAILED
};

// Shared-bus SSD1306 adapter for the project's fixed 128x32 / 0x3C OLED.
// The caller must initialize the shared TwoWire bus once (SDA 21, SCL 22)
// before begin(). This adapter never calls Wire.begin() or changes bus pins.
// The constructor and methods must be called from the serialized UI/boot task.
class Esp32Oled : public IApCredentialDisplay {
 public:
  static const uint8_t kWidth = 128;
  static const uint8_t kHeight = 32;
  static const uint8_t kAddress = 0x3C;
  static const size_t kSetupPassphraseLength = 32;

  Esp32Oled(TwoWire& sharedBus, II2cBusLock& busLock);
  Esp32Oled(const Esp32Oled&) = delete;
  Esp32Oled& operator=(const Esp32Oled&) = delete;

  // EOI_BUFFER_READY means Adafruit_SSD1306 allocated its framebuffer and
  // issued initialization commands. It is not proof that a panel is present.
  Esp32OledInitCode begin();

  // Only the signed NEEDS_SETUP disclosure result and the exact E28 32-byte
  // lowercase-hex credential are accepted. The secret is rendered as two
  // complete 16-character rows. The framebuffer is wiped after the transfer
  // request; the physical panel remains visible until clear() is called.
  // Adafruit_SSD1306::display() returns void, so this reports a transfer
  // request, not a confirmed I2C write or visible panel state.
  Esp32OledActionCode showSetupCredential(const ApCredentialResult& result,
                                          const char* passphrase,
                                          size_t passphraseLength);

  // Clears the framebuffer and requests a blank frame on the panel. The
  // Adafruit API provides no write result, so hardware clearing is unverified.
  Esp32OledActionCode clear();

  // IApCredentialDisplay bridge used by the ordered first-provision
  // coordinator. True means only that the Adafruit transfer was requested.
  bool requestCredentialDisplay(const ApCredentialResult& result,
                                const char* passphrase,
                                size_t passphraseLength) override;
  bool requestBlankFrame() override;

  bool initialized() const { return initialized_; }

 private:
  static bool validSetupPassphrase_(const char* passphrase, size_t length);
  void resetFramebuffer_();

  Adafruit_SSD1306 display_;
  II2cBusLock& busLock_;
  bool initialized_;
};

}  // namespace att
