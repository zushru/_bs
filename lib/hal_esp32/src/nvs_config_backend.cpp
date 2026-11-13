#include "nvs_config_backend.h"
#include "esp32_nvs_startup_guard.h"
#include <string.h>

namespace att {
namespace {
const char* const NVS_NAMESPACE = "attcfg";
const char* const SLOT_KEYS[2] = {"slot_a", "slot_b"};
const char* const PROVISIONED_KEY = "provisioned";
const uint8_t PROVISIONED_VALUE[4] = {'A', 'T', 'T', 'P'};

// true = valid marker (or safely created); false = I/O/corruption.
bool ensureProvisionMarker(nvs_handle_t handle, esp_err_t& error) {
  uint8_t marker[sizeof PROVISIONED_VALUE];
  size_t length = 0;
  error = nvs_get_blob(handle, PROVISIONED_KEY, nullptr, &length);
  if (error == ESP_ERR_NVS_NOT_FOUND) {
    error = nvs_set_blob(handle, PROVISIONED_KEY, PROVISIONED_VALUE,
                         sizeof PROVISIONED_VALUE);
    return error == ESP_OK;
  }
  if (error != ESP_OK) return false;
  if (length != sizeof marker) {
    error = ESP_ERR_NVS_INVALID_LENGTH;
    return false;
  }
  size_t actual = sizeof marker;
  error = nvs_get_blob(handle, PROVISIONED_KEY, marker, &actual);
  if (error != ESP_OK) return false;
  if (actual != sizeof marker || memcmp(marker, PROVISIONED_VALUE, sizeof marker) != 0) {
    error = ESP_FAIL;
    return false;
  }
  return true;
}

// A valid marker means provisioning occurred before; when both slots are blank,
// that is a fault rather than first boot. The marker is also checked on valid boots.
bool hasProvisionMarker(bool& present, esp_err_t& error) {
  present = false;
  nvs_handle_t handle;
  error = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (error == ESP_ERR_NVS_NOT_FOUND) { error = ESP_OK; return true; }
  if (error != ESP_OK) return false;

  uint8_t marker[sizeof PROVISIONED_VALUE];
  size_t length = 0;
  error = nvs_get_blob(handle, PROVISIONED_KEY, nullptr, &length);
  if (error == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    error = ESP_OK;
    return true;
  }
  if (error != ESP_OK || length != sizeof marker) {
    nvs_close(handle);
    if (error == ESP_OK) error = ESP_ERR_NVS_INVALID_LENGTH;
    return false;
  }
  size_t actual = sizeof marker;
  error = nvs_get_blob(handle, PROVISIONED_KEY, marker, &actual);
  nvs_close(handle);
  if (error != ESP_OK) return false;
  if (actual != sizeof marker || memcmp(marker, PROVISIONED_VALUE, sizeof marker) != 0) {
    error = ESP_FAIL;
    return false;
  }
  present = true;
  return true;
}

bool persistProvisionMarker(esp_err_t& error) {
  nvs_handle_t handle;
  error = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (error != ESP_OK) return false;
  const bool markerOk = ensureProvisionMarker(handle, error);
  if (markerOk && error == ESP_OK) error = nvs_commit(handle);
  nvs_close(handle);
  return markerOk && error == ESP_OK;
}

void blankSlot(uint8_t out[CFG_SLOT_SIZE]) {
  memset(out, 0xFF, CFG_SLOT_SIZE);
}
}  // namespace

bool Esp32NvsConfigBackend::begin() {
  if (ready_) return true;

  // Arduino-ESP32 initializes the default NVS partition inside initArduino(),
  // before setup(). Reuse that one startup result; never retry initialization
  // here after its guarded whole-partition erase fallback was denied.
  const Esp32NvsStartupStatus startup = Esp32NvsStartupGuard::status();
  if (!startup.safeForApplication) {
    lastError_ = static_cast<esp_err_t>(startup.firstInitResult);
    if (lastError_ == ESP_OK) lastError_ = ESP_FAIL;
    ready_ = false;
    return false;
  }

  lastError_ = ESP_OK;
  ready_ = true;
  return true;
}

bool Esp32NvsConfigBackend::load(Config& out, CfgPick& pick) {
  if (!ready_) {
    pick = CfgPick{CB_FAULT, -1, 0};
    lastError_ = ESP_ERR_NVS_NOT_INITIALIZED;
    return false;
  }
  if (!readConfigSlots(*this, out, pick)) {
    if (lastError_ == ESP_OK) lastError_ = ESP_FAIL;  // Decode/slot-selection fault.
    return false;
  }

  bool provisioned = false;
  esp_err_t err = ESP_OK;
  if (!hasProvisionMarker(provisioned, err)) {
    pick = CfgPick{CB_FAULT, -1, 0};
    lastError_ = err;
    return false;
  }
  if (pick.boot == CB_UNPROVISIONED && provisioned) {
    pick = CfgPick{CB_FAULT, -1, 0};
    lastError_ = ESP_FAIL;
    return false;
  }
  // Migrate configurations created before the marker existed. If the marker
  // cannot be committed, do not start normal operation without the loss guard.
  if (pick.boot == CB_OK && !provisioned && !persistProvisionMarker(err)) {
    pick = CfgPick{CB_FAULT, -1, 0};
    lastError_ = err;
    return false;
  }
  lastError_ = ESP_OK;
  return true;
}

bool Esp32NvsConfigBackend::readSlot(uint8_t slot, uint8_t out[CFG_SLOT_SIZE]) {
  if (!ready_ || slot > 1 || out == nullptr) {
    lastError_ = ready_ ? ESP_ERR_INVALID_ARG : ESP_ERR_NVS_NOT_INITIALIZED;
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    blankSlot(out);  // Missing namespace means both config keys are unprovisioned.
    lastError_ = ESP_OK;
    return true;
  }
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  size_t length = 0;
  err = nvs_get_blob(handle, SLOT_KEYS[slot], nullptr, &length);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    blankSlot(out);
    lastError_ = ESP_OK;
    return true;
  }
  if (err != ESP_OK) {
    nvs_close(handle);
    lastError_ = err;
    return false;
  }
  if (length != CFG_SLOT_SIZE) {
    nvs_close(handle);
    lastError_ = ESP_ERR_NVS_INVALID_LENGTH;
    return false;  // A present but malformed key is corruption, never a blank slot.
  }

  uint8_t tmp[CFG_SLOT_SIZE];
  size_t actual = sizeof tmp;
  err = nvs_get_blob(handle, SLOT_KEYS[slot], tmp, &actual);
  nvs_close(handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }
  if (actual != CFG_SLOT_SIZE) {
    lastError_ = ESP_ERR_NVS_INVALID_LENGTH;
    return false;
  }

  memcpy(out, tmp, sizeof tmp);
  lastError_ = ESP_OK;
  return true;
}

bool Esp32NvsConfigBackend::readProvisionMarker(bool& present) {
  present = false;
  if (!ready_) {
    lastError_ = ESP_ERR_NVS_NOT_INITIALIZED;
    return false;
  }
  esp_err_t err = ESP_OK;
  if (!hasProvisionMarker(present, err)) {
    lastError_ = err;
    return false;
  }
  lastError_ = ESP_OK;
  return true;
}

bool Esp32NvsConfigBackend::writeSlot(uint8_t slot,
                                      const uint8_t data[CFG_SLOT_SIZE]) {
  if (!ready_ || slot > 1 || data == nullptr) {
    lastError_ = ready_ ? ESP_ERR_INVALID_ARG : ESP_ERR_NVS_NOT_INITIALIZED;
    return false;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    lastError_ = err;
    return false;
  }

  err = nvs_set_blob(handle, SLOT_KEYS[slot], data, CFG_SLOT_SIZE);
  if (err == ESP_OK && !ensureProvisionMarker(handle, err)) {
    // Do not commit a slot if its provision marker is missing/corrupt or cannot
    // be written. The coordinator treats this as indeterminate and latches fault.
  }
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  lastError_ = err;
  return err == ESP_OK;
}

}  // namespace att
