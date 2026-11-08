#pragma once

#include "ap_credential_bootstrap.h"
#include <nvs.h>

namespace att {

// Device-local NVS record for the SoftAP WPA2 passphrase. It intentionally
// uses namespace "attwifi", not the PIN counter namespace (E12) or config
// namespace. NVS must already be initialized by Esp32NvsConfigBackend::begin.
// All methods are synchronous and must run on the serialized boot/application
// task. No automatic erase/reformat or overwrite recovery is allowed.
class Esp32NvsApCredentialStore : public IApCredentialStore {
 public:
  Esp32NvsApCredentialStore() : lastError_(ESP_OK) {}

  ApCredentialStoreRead load(char out[AP_CREDENTIAL_BUFFER_SIZE]) override;
  ApCredentialStoreWrite createIfAbsent(const char* passphrase) override;

  esp_err_t lastError() const { return lastError_; }

 private:
  static const char* const kNamespace;
  static const char* const kKey;

  esp_err_t readFromHandle_(nvs_handle_t handle,
                            char out[AP_CREDENTIAL_BUFFER_SIZE]);
  static void clear_(void* value, size_t length);
  static bool validPassphrase_(const char* passphrase);

  esp_err_t lastError_;
};

}  // namespace att
