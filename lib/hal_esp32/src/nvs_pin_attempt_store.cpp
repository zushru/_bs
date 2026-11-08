#include "nvs_pin_attempt_store.h"
#include <string.h>

namespace att {
namespace {
const char* const FAILURE_KEY = "pin_fails";
const char* const CONFIG_NAMESPACE = "attcfg";
}  // namespace

Esp32NvsPinAttemptStore::Esp32NvsPinAttemptStore(const char* nvsNamespace)
    : namespace_{0}, configured_(false), lastError_(ESP_OK) {
  if (nvsNamespace == nullptr) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return;
  }
  const size_t length = strlen(nvsNamespace);
  if (length == 0 || length > 15 ||
      strcmp(nvsNamespace, CONFIG_NAMESPACE) == 0) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return;
  }
  memcpy(namespace_, nvsNamespace, length + 1);
  configured_ = true;
}

bool Esp32NvsPinAttemptStore::loadFailureCount(uint16_t& out) {
  if (!configured_) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(namespace_, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  uint16_t value = 0;
  err = nvs_get_u16(handle, FAILURE_KEY, &value);
  nvs_close(handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  out = value;
  lastError_ = ESP_OK;
  return true;
}

bool Esp32NvsPinAttemptStore::initializeFailureCount() {
  if (!configured_) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  uint16_t existing = 0;
  err = nvs_get_u16(handle, FAILURE_KEY, &existing);
  if (err == ESP_OK) {
    err = ESP_FAIL;  // Existing state, including a lockout, must never be reset.
  } else if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = nvs_set_u16(handle, FAILURE_KEY, 0);
  }
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  uint16_t persisted = 1;
  if (!loadFailureCount(persisted)) return false;
  if (persisted != 0) {
    lastError_ = ESP_FAIL;
    return false;
  }
  lastError_ = ESP_OK;
  return true;
}

bool Esp32NvsPinAttemptStore::persistFailureCount(uint16_t failures) {
  if (!configured_) {
    lastError_ = ESP_ERR_INVALID_ARG;
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(namespace_, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  uint16_t current = 0;
  err = nvs_get_u16(handle, FAILURE_KEY, &current);
  if (err == ESP_OK) err = nvs_set_u16(handle, FAILURE_KEY, failures);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  // Reopen a fresh read-only handle: a successful queued set is not enough.
  uint16_t persisted = 0;
  if (!loadFailureCount(persisted)) return false;
  if (persisted != failures) {
    lastError_ = ESP_FAIL;
    return false;
  }
  lastError_ = ESP_OK;
  return true;
}

}  // namespace att
