#pragma once

#include <stddef.h>
#include <stdint.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include "esp32_secure_random.h"
#include "credential_soft_ap.h"

namespace att {

enum Esp32WifiApCode : uint8_t {
  EWAP_COLD,
  EWAP_READY,
  EWAP_ALREADY_ACTIVE,
  EWAP_INVALID_ARGUMENT,
  EWAP_INIT_FAILED,
  EWAP_CONFIG_FAILED,
  EWAP_START_FAILED,
  EWAP_START_TIMEOUT,
  EWAP_CLEANUP_FAILED,
  EWAP_NOT_ACTIVE,
  EWAP_STOPPED,
  EWAP_STOP_FAILED,
  EWAP_FAULTED
};

// Sole owner of the production SoftAP lifecycle and RF-ready notification
// consumed by Esp32SecureRandom. This uses ESP-IDF directly so AP credentials
// are installed before esp_wifi_start() can transmit any AP beacon.
//
// Calls and all RNG use must remain on one serialized task. The event callback
// receives only the independently allocated EventGroup, never `this`; it only
// signals that group and never touches the ordinary-bool RNG gate. If handler
// removal fails, the group is deliberately retained so the callback context
// remains valid until reboot. Keep the owner and RNG alive for the AP lifetime.
// The caller supplies credentials; this adapter invents neither.
class Esp32WifiAp : public ICredentialSoftAp {
 public:
  explicit Esp32WifiAp(Esp32SecureRandom& random);
  Esp32WifiAp(const Esp32WifiAp&) = delete;
  Esp32WifiAp& operator=(const Esp32WifiAp&) = delete;

  // ssidLength is bytes (1..32); passphraseLength is bytes (8..63).
  // Passphrase bytes must be printable ASCII, matching Config::wifiPass.
  // Channel is explicit and must be 1..13. SSID is broadcast; WPA2-PSK/CCMP
  // are configured directly in wifi_config_t; max connections is fixed at 2.
  // Must be the sole initializer/owner of the ESP-IDF Wi-Fi driver. Do not
  // call Arduino WiFi.mode()/softAP() or another esp_wifi_init() first.
  bool start(const char* ssid, size_t ssidLength,
             const char* passphrase, size_t passphraseLength,
             int channel) override;

  // Controlled shutdown only. The signed product rule is AP always-on; do not
  // expose stop/restart as a user-facing or HTTP operation.
  // RNG readiness is cleared before the driver stop is attempted.
  bool stop();

  bool ready() const override { return started_ && !faulted_ && random_.rfStarted(); }
  // A latched, non-PII acknowledgement that the AP DHCP server assigned an
  // address to a client in this AP run. It is cleared by every new start.
  bool hasAssignedClientIp() const override;
  bool started() const { return started_; }
  bool faulted() const { return faulted_; }
  Esp32WifiApCode code() const { return code_; }

 private:
  static const EventBits_t kApStartedBit = BIT0;
  static const EventBits_t kClientIpAssignedBit = BIT1;
  static const uint32_t kStartTimeoutMs = 5000;
  struct CallbackContext;

  static bool validInputs_(const char* ssid, size_t ssidLength,
                           const char* passphrase, size_t passphraseLength,
                           int channel);
  static void onWifiEvent_(void* arg, esp_event_base_t eventBase,
                           int32_t eventId, void* eventData);
  static void onIpEvent_(void* arg, esp_event_base_t eventBase,
                         int32_t eventId, void* eventData);
  bool cleanup_();
  bool failStart_(Esp32WifiApCode failureCode);

  Esp32SecureRandom& random_;
  esp_netif_t* apNetif_;
  EventGroupHandle_t startEvents_;
  bool wifiInitialized_;
  bool wifiStarted_;
  bool eventRegistered_;
  bool ipEventRegistered_;
  CallbackContext* callbackContext_;
  bool started_;
  bool faulted_;
  Esp32WifiApCode code_;
};

}  // namespace att
