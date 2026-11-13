#include "nvs_ap_credential_store.h"
#include "esp32_nvs_startup_guard.h"
#include <string.h>

namespace att {
namespace {

esp_err_t nvsStartupError(const ConfigBootCoordinator& configBoot) {
  const Esp32NvsStartupStatus startup = Esp32NvsStartupGuard::status();
  if (!startup.safeForApplication) {
    return startup.firstInitResult == ESP_OK
        ? ESP_FAIL : static_cast<esp_err_t>(startup.firstInitResult);
  }
  if (!configBoot.ready() && !configBoot.needsSetup())
    return ESP_FAIL;
  return ESP_OK;
}

}  // namespace

const char* const Esp32NvsApCredentialStore::kNamespace = "attwifi";
const char* const Esp32NvsApCredentialStore::kKey = "ap_pass_v1";

void Esp32NvsApCredentialStore::clear_(void* value, size_t length) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(value);
  while (length--) *p++ = 0;
}

bool Esp32NvsApCredentialStore::validPassphrase_(const char* passphrase) {
  return ApCredentialBootstrap::validPassphrase(passphrase);
}

esp_err_t Esp32NvsApCredentialStore::readFromHandle_(
    nvs_handle_t handle, char out[AP_CREDENTIAL_BUFFER_SIZE]) {
  if (out == nullptr) return ESP_ERR_INVALID_ARG;
  clear_(out, AP_CREDENTIAL_BUFFER_SIZE);

  size_t length = 0;
  esp_err_t err = nvs_get_str(handle, kKey, nullptr, &length);
  if (err != ESP_OK) return err;
  // NVS string length includes the terminating NUL. WPA2 passphrases accepted
  // by this device are 8..63 printable bytes, so exact stored length is 9..64.
  if (length < 9 || length > AP_CREDENTIAL_BUFFER_SIZE) {
    return ESP_ERR_NVS_INVALID_LENGTH;
  }

  size_t actual = length;
  err = nvs_get_str(handle, kKey, out, &actual);
  if (err != ESP_OK) {
    clear_(out, AP_CREDENTIAL_BUFFER_SIZE);
    return err;
  }
  if (actual != length || out[length - 1] != '\0' ||
      !validPassphrase_(out)) {
    clear_(out, AP_CREDENTIAL_BUFFER_SIZE);
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  return ESP_OK;
}

ApCredentialStoreRead Esp32NvsApCredentialStore::load(
    char out[AP_CREDENTIAL_BUFFER_SIZE]) {
  if (out == nullptr) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return APCR_READ_ERROR;
  }
  clear_(out, AP_CREDENTIAL_BUFFER_SIZE);
  const esp_err_t startupErr = nvsStartupError(configBoot_);
  if (startupErr != ESP_OK) {
    lastError_ = startupErr;
    return APCR_READ_ERROR;
  }

  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    lastError_ = ESP_OK;
    return APCR_NOT_FOUND;
  }
  if (err != ESP_OK) {
    lastError_ = err;
    return APCR_READ_ERROR;
  }

  char candidate[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  err = readFromHandle_(handle, candidate);
  nvs_close(handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    clear_(candidate, sizeof candidate);
    lastError_ = ESP_OK;
    return APCR_NOT_FOUND;
  }
  if (err != ESP_OK) {
    clear_(candidate, sizeof candidate);
    lastError_ = err;
    return APCR_READ_ERROR;
  }

  memcpy(out, candidate, sizeof candidate);
  clear_(candidate, sizeof candidate);
  lastError_ = ESP_OK;
  return APCR_READ_OK;
}

ApCredentialStoreWrite Esp32NvsApCredentialStore::createIfAbsent(
    const char* passphrase) {
  if (!validPassphrase_(passphrase)) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return APCW_WRITE_ERROR;
  }
  const esp_err_t startupErr = nvsStartupError(configBoot_);
  if (startupErr != ESP_OK) {
    lastError_ = startupErr;
    return APCW_WRITE_ERROR;
  }

  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return APCW_WRITE_ERROR;
  }

  char existing[AP_CREDENTIAL_BUFFER_SIZE] = {0};
  err = readFromHandle_(handle, existing);
  if (err == ESP_OK) {
    clear_(existing, sizeof existing);
    nvs_close(handle);
    lastError_ = ESP_OK;
    return APCW_ALREADY_EXISTS;
  }
  clear_(existing, sizeof existing);
  if (err != ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    lastError_ = err;
    return APCW_WRITE_ERROR;
  }

  err = nvs_set_str(handle, kKey, passphrase);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  lastError_ = err;
  return err == ESP_OK ? APCW_CREATED : APCW_WRITE_ERROR;
}

}  // namespace att
