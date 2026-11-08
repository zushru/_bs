#include "esp32_wifi_ap.h"
#include <esp_err.h>
#include <esp_netif_defaults.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <string.h>
#include <stdlib.h>

namespace att {

struct Esp32WifiAp::CallbackContext {
  EventGroupHandle_t events;
  esp_netif_t* apNetif;
};

namespace {
void secureClear(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

bool alreadyInitialized(esp_err_t result) {
  return result == ESP_OK || result == ESP_ERR_INVALID_STATE;
}
}  // namespace

Esp32WifiAp::Esp32WifiAp(Esp32SecureRandom& random)
    : random_(random), apNetif_(nullptr), startEvents_(nullptr),
      wifiInitialized_(false), wifiStarted_(false), eventRegistered_(false),
      ipEventRegistered_(false), callbackContext_(nullptr), started_(false),
      faulted_(false), code_(EWAP_COLD) {}

bool Esp32WifiAp::validInputs_(const char* ssid, size_t ssidLength,
                               const char* passphrase,
                               size_t passphraseLength, int channel) {
  if (ssid == nullptr || ssidLength == 0 || ssidLength > 32 ||
      passphrase == nullptr || passphraseLength < 8 ||
      passphraseLength > 63 || channel < 1 || channel > 13) {
    return false;
  }
  for (size_t i = 0; i < ssidLength; ++i) {
    if (ssid[i] == '\0') return false;
  }
  for (size_t i = 0; i < passphraseLength; ++i) {
    const uint8_t ch = static_cast<uint8_t>(passphrase[i]);
    if (ch < 0x20 || ch > 0x7E) return false;
  }
  return true;
}

void Esp32WifiAp::onWifiEvent_(void* arg, esp_event_base_t eventBase,
                               int32_t eventId, void* eventData) {
  (void)eventData;
  if (arg == nullptr || eventBase != WIFI_EVENT || eventId != WIFI_EVENT_AP_START) {
    return;
  }
  // Use the separately allocated EventGroup as callback context, not `this`.
  // If unregister fails, cleanup deliberately retains the group; the event
  // handler can therefore never dereference a destroyed Esp32WifiAp object.
  CallbackContext* context = static_cast<CallbackContext*>(arg);
  if (context != nullptr && context->events != nullptr) {
    xEventGroupSetBits(context->events, kApStartedBit);
  }
}

void Esp32WifiAp::onIpEvent_(void* arg, esp_event_base_t eventBase,
                             int32_t eventId, void* eventData) {
  if (arg == nullptr || eventBase != IP_EVENT ||
      eventId != IP_EVENT_AP_STAIPASSIGNED || eventData == nullptr) {
    return;
  }
  CallbackContext* context = static_cast<CallbackContext*>(arg);
  const ip_event_ap_staipassigned_t* assigned =
      static_cast<const ip_event_ap_staipassigned_t*>(eventData);
  if (context != nullptr && context->events != nullptr &&
      context->apNetif != nullptr && assigned->esp_netif == context->apNetif) {
    // Store only the fact that a client obtained a DHCP lease. Do not retain
    // the event's MAC address or IP address in application memory.
    xEventGroupSetBits(context->events, kClientIpAssignedBit);
  }
}

bool Esp32WifiAp::hasAssignedClientIp() const {
  if (!ready() || startEvents_ == nullptr) return false;
  return (xEventGroupGetBits(startEvents_) & kClientIpAssignedBit) != 0;
}

// Teardown is deliberately ordered. If stop or handler removal fails, leave
// still-referenced resources allocated and latch the owner in FAULTED state.
bool Esp32WifiAp::cleanup_() {
  if (wifiStarted_) {
    if (esp_wifi_stop() != ESP_OK) return false;
    wifiStarted_ = false;
  }

  if (ipEventRegistered_) {
    if (esp_event_handler_unregister(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED,
                                     &Esp32WifiAp::onIpEvent_) != ESP_OK) {
      return false;
    }
    ipEventRegistered_ = false;
  }

  if (eventRegistered_) {
    if (esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_START,
                                     &Esp32WifiAp::onWifiEvent_) != ESP_OK) {
      return false;
    }
    eventRegistered_ = false;
  }

  if (startEvents_ != nullptr) {
    vEventGroupDelete(startEvents_);
    startEvents_ = nullptr;
  }
  // Handler callbacks use this heap context, never the owner object. It is
  // retained whenever either unregister operation fails above.
  free(callbackContext_);
  callbackContext_ = nullptr;

  if (wifiInitialized_) {
    if (esp_wifi_deinit() != ESP_OK) return false;
    wifiInitialized_ = false;
  }

  if (apNetif_ != nullptr) {
    // The convenience esp_netif_destroy_default_wifi() returns void and
    // destroys the netif even if its internal driver/handler clear fails.
    // Keep the netif alive on an indeterminate detach result instead.
    if (esp_wifi_clear_default_wifi_driver_and_handlers(apNetif_) != ESP_OK) {
      return false;
    }
    esp_netif_destroy(apNetif_);
    apNetif_ = nullptr;
  }
  return true;
}

bool Esp32WifiAp::failStart_(Esp32WifiApCode failureCode) {
  random_.notifyRfStopped();
  started_ = false;
  faulted_ = true;
  code_ = cleanup_() ? failureCode : EWAP_CLEANUP_FAILED;
  return false;
}

bool Esp32WifiAp::start(const char* ssid, size_t ssidLength,
                        const char* passphrase, size_t passphraseLength,
                        int channel) {
  if (faulted_) {
    code_ = EWAP_FAULTED;
    return false;
  }
  if (started_) {
    code_ = EWAP_ALREADY_ACTIVE;
    return false;
  }

  random_.notifyRfStopped();
  if (!validInputs_(ssid, ssidLength, passphrase, passphraseLength, channel)) {
    code_ = EWAP_INVALID_ARGUMENT;
    return false;
  }

  startEvents_ = xEventGroupCreate();
  if (startEvents_ == nullptr) return failStart_(EWAP_INIT_FAILED);
  callbackContext_ = static_cast<CallbackContext*>(malloc(sizeof(*callbackContext_)));
  if (callbackContext_ == nullptr) return failStart_(EWAP_INIT_FAILED);
  callbackContext_->events = startEvents_;
  callbackContext_->apNetif = nullptr;

  // Arduino's normal softAP() path starts AP mode before it applies the caller
  // config. This owner uses the IDF sequence instead: initialize, set mode,
  // install the full secured AP config, then start RF.
  if (!alreadyInitialized(esp_netif_init())) {
    return failStart_(EWAP_INIT_FAILED);
  }
  if (!alreadyInitialized(esp_event_loop_create_default())) {
    return failStart_(EWAP_INIT_FAILED);
  }

  // Build the same default AP netif as esp_netif_create_default_wifi_ap(),
  // but use its checked lower-level operations. The convenience constructor
  // uses assert()/ESP_ERROR_CHECK and can abort instead of returning an error.
  // Spell out the AP config fields rather than using
  // ESP_NETIF_DEFAULT_WIFI_AP(), whose pinned header expands to a C
  // designated-initializer macro. This remains C++11-compatible.
  esp_netif_config_t apNetifConfig = {};
  apNetifConfig.base = ESP_NETIF_BASE_DEFAULT_WIFI_AP;
  apNetifConfig.driver = nullptr;
  apNetifConfig.stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP;
  apNetif_ = esp_netif_new(&apNetifConfig);
  if (apNetif_ == nullptr) return failStart_(EWAP_INIT_FAILED);
  callbackContext_->apNetif = apNetif_;
  if (esp_netif_attach_wifi_ap(apNetif_) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }
  if (esp_wifi_set_default_wifi_ap_handlers() != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }

  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&initConfig) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }
  wifiInitialized_ = true;

  // The application config/NVS is authoritative; do not let the Wi-Fi driver
  // silently retain a stale AP password in its own flash namespace.
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }
  if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }

  wifi_config_t config;
  memset(&config, 0, sizeof config);
  memcpy(config.ap.ssid, ssid, ssidLength);
  config.ap.ssid_len = static_cast<uint8_t>(ssidLength);
  memcpy(config.ap.password, passphrase, passphraseLength);
  config.ap.channel = static_cast<uint8_t>(channel);
  config.ap.ssid_hidden = 0;
  config.ap.max_connection = 2;
  config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  config.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
  config.ap.ftm_responder = false;

  const esp_err_t configResult = esp_wifi_set_config(WIFI_IF_AP, &config);
  secureClear(&config, sizeof config);
  if (configResult != ESP_OK) return failStart_(EWAP_CONFIG_FAILED);

  // Register the DHCP assignment observer before the radio starts. The
  // v5.5.5 event includes the netif pointer, so unrelated netif leases are
  // ignored. No client MAC/IP is copied into persistent application state.
  if (esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED,
                                 &Esp32WifiAp::onIpEvent_, callbackContext_) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }
  ipEventRegistered_ = true;
  if (esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START,
                                 &Esp32WifiAp::onWifiEvent_, callbackContext_) != ESP_OK) {
    return failStart_(EWAP_INIT_FAILED);
  }
  eventRegistered_ = true;

  if (esp_wifi_start() != ESP_OK) {
    return failStart_(EWAP_START_FAILED);
  }
  wifiStarted_ = true;

  const EventBits_t events = xEventGroupWaitBits(
      startEvents_, kApStartedBit, pdTRUE, pdFALSE,
      pdMS_TO_TICKS(kStartTimeoutMs));
  if ((events & kApStartedBit) == 0) {
    return failStart_(EWAP_START_TIMEOUT);
  }

  // Only the AP_START event, following successful config-before-start, opens
  // the RF entropy gate. The callback itself never mutates random_.
  random_.notifyRfStarted();
  started_ = true;
  code_ = EWAP_READY;
  return true;
}

bool Esp32WifiAp::stop() {
  if (faulted_) {
    code_ = EWAP_FAULTED;
    return false;
  }
  if (!started_) {
    code_ = EWAP_NOT_ACTIVE;
    return false;
  }

  random_.notifyRfStopped();
  if (!cleanup_()) {
    faulted_ = true;
    code_ = EWAP_STOP_FAILED;
    return false;
  }

  started_ = false;
  code_ = EWAP_STOPPED;
  return true;
}

}  // namespace att
