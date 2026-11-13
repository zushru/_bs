#pragma once

#include "ap_credential_bootstrap.h"
#include "config_boot.h"
#include <nvs.h>

namespace att {

// Device-local NVS record for the SoftAP WPA2 passphrase. It intentionally
// uses namespace "attwifi", not the PIN counter namespace (E12) or config
// namespace. Every method checks both the pre-setup NVS startup snapshot and
// the supplied ConfigBootCoordinator. Only READY/NEEDS_SETUP permits access;
// COLD/FAULT fails before nvs_open(). The coordinator is non-owned and must
// outlive this store. All methods run on the serialized boot/application task.
// No automatic erase/reformat or overwrite recovery is allowed.
class Esp32NvsApCredentialStore : public IApCredentialStore {
 public:
  explicit Esp32NvsApCredentialStore(const ConfigBootCoordinator& configBoot)
      : configBoot_(configBoot), lastError_(ESP_OK) {}

  ApCredentialStoreRead load(char out[AP_CREDENTIAL_BUFFER_SIZE]) override;
  ApCredentialStoreWrite createIfAbsent(const char* passphrase) override;

  esp_err_t lastError() const { return lastError_; }

 private:
  const ConfigBootCoordinator& configBoot_;
  static const char* const kNamespace;
  static const char* const kKey;

  esp_err_t readFromHandle_(nvs_handle_t handle,
                            char out[AP_CREDENTIAL_BUFFER_SIZE]);
  static void clear_(void* value, size_t length);
  static bool validPassphrase_(const char* passphrase);

  esp_err_t lastError_;
};

}  // namespace att
