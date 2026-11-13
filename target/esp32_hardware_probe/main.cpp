#include <Arduino.h>
#include <Wire.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp32_bootstrap_random.h"
#include "esp32_oled.h"
#include "esp32_i2c_bus_owner.h"
#include "esp32_ds3231_register_io.h"
#include "esp32_secure_random.h"
#include "esp32_wifi_ap.h"
#include "ap_credential_bootstrap.h"
#include "probe_credential.h"
#include "esp32_nvs_startup_guard.h"

namespace {

const char kProbeSsid[] = "att";
const uint8_t kProbeChannel = 1;
const uint8_t kProbeMaxClients = 2;
const uint32_t kCredentialDisplayMs = 90000UL;
const uint32_t kStatusPeriodMs = 5000UL;
const uint32_t kI2cClockHz = 100000UL;

att::Esp32BootstrapRandom bootstrapRandom;
att::Esp32SecureRandom secureRandom;
att::Esp32WifiAp wifiAp(secureRandom);
att::Esp32Oled* oled = nullptr;

char probePassphrase[33] = {};
bool apStarted = false;

// Link-only references let the final-ELF inspector verify that the standard
// ESP-IDF bulk-erase APIs route through the shared partition-erase wrapper.
// None of these functions is called by the diagnostic probe.
using NvsEraseRef = esp_err_t (*)();
using NvsErasePartitionRef = esp_err_t (*)(const char*);
using NvsErasePartitionPtrRef = esp_err_t (*)(const esp_partition_t*);
volatile NvsEraseRef keepNvsFlashErase = &nvs_flash_erase;
volatile NvsErasePartitionRef keepNvsFlashErasePartition =
    &nvs_flash_erase_partition;
volatile NvsErasePartitionPtrRef keepNvsFlashErasePartitionPtr =
    &nvs_flash_erase_partition_ptr;
bool dhcpEventReported = false;
bool oledClearReported = false;
bool oledInitialized = false;
uint32_t credentialShownAt = 0;
uint32_t lastStatusAt = 0;

void secureClear(void* data, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(data);
  while (length-- > 0) *cursor++ = 0;
}

void scanI2cBus(att::II2cBusLock& busLock) {
  att::I2cBusLockGuard busGuard(busLock);
  if (!busGuard.acquired()) {
    Serial.println("I2C_SCAN=SKIPPED shared_bus_lock_failed");
    return;
  }

  bool foundOled = false;
  bool foundEeprom = false;
  bool foundRtc = false;
  unsigned count = 0;

  Serial.println("I2C_SCAN_BEGIN addresses=0x08..0x77");
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    const uint8_t result = Wire.endTransmission();
    if (result == 0) {
      ++count;
      Serial.printf("I2C_ACK=0x%02X\n", static_cast<unsigned>(address));
      if (address == 0x3C) foundOled = true;
      if (address == 0x57) foundEeprom = true;
      if (address == 0x68) foundRtc = true;
    }
  }
  Serial.printf("I2C_SCAN_END count=%u oled_3c=%u eeprom_57=%u rtc_68=%u\n",
                count, foundOled ? 1U : 0U, foundEeprom ? 1U : 0U,
                foundRtc ? 1U : 0U);
}

bool createEphemeralPassphrase() {
  uint8_t entropy[16] = {};
  const bool randomOk = bootstrapRandom.fill(entropy, sizeof entropy);
  const bool encoded = randomOk && att::encodeProbeCredential(
      entropy, sizeof entropy, probePassphrase, sizeof probePassphrase);
  secureClear(entropy, sizeof entropy);
  return encoded;
}

bool verifyApConfiguration(const char* expectedPassphrase,
                           size_t expectedPassphraseLength) {
  wifi_config_t config = {};
  const esp_err_t readResult = esp_wifi_get_config(WIFI_IF_AP, &config);
  if (readResult != ESP_OK) {
    Serial.printf("AP_CONFIG_READ=FAIL esp_err=%d\n", static_cast<int>(readResult));
    secureClear(&config, sizeof config);
    return false;
  }

  const bool ssidMatches = config.ap.ssid_len == sizeof(kProbeSsid) - 1 &&
      memcmp(config.ap.ssid, kProbeSsid, sizeof(kProbeSsid) - 1) == 0;
  const bool passwordMatches = expectedPassphrase != nullptr &&
      expectedPassphraseLength == 32 &&
      memcmp(config.ap.password, expectedPassphrase,
             expectedPassphraseLength) == 0 &&
      config.ap.password[expectedPassphraseLength] == '\0';
  const bool channelMatches = config.ap.channel == kProbeChannel;
  const bool authMatches = config.ap.authmode == WIFI_AUTH_WPA2_PSK;
  const bool cipherMatches = config.ap.pairwise_cipher == WIFI_CIPHER_TYPE_CCMP;
  const bool maxClientsMatch = config.ap.max_connection == kProbeMaxClients;
  const bool visibleSsid = config.ap.ssid_hidden == 0;
  const bool allMatch = ssidMatches && passwordMatches && channelMatches &&
      authMatches && cipherMatches && maxClientsMatch && visibleSsid;

  Serial.printf(
      "AP_CONFIG_VERIFY=%s ssid=%u password=%u channel=%u wpa2_psk=%u "
      "ccmp=%u max_clients=%u ssid_visible=%u\n",
      allMatch ? "PASS" : "FAIL", ssidMatches ? 1U : 0U,
      passwordMatches ? 1U : 0U, channelMatches ? 1U : 0U,
      authMatches ? 1U : 0U, cipherMatches ? 1U : 0U,
      maxClientsMatch ? 1U : 0U, visibleSsid ? 1U : 0U);
  secureClear(&config, sizeof config);
  return allMatch;
}

void clearProbeScreen(const char* reason) {
  if (!oledInitialized || oledClearReported || oled == nullptr) return;
  const att::Esp32OledActionCode result = oled->clear();
  Serial.printf("OLED_CLEAR_REQUEST=%u reason=%s (not panel acknowledgement)\n",
                static_cast<unsigned>(result), reason);
  oledClearReported = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  (void)keepNvsFlashErase;
  (void)keepNvsFlashErasePartition;
  (void)keepNvsFlashErasePartitionPtr;
  Serial.println("ATT_HARDWARE_PROBE=v0.2.48");
  Serial.println("PROBE_ONLY=1 no_attendance_or_config_namespace_access=1");
  Serial.println("Wi-Fi PHY may read/write its separate NVS namespace: phy.");
  Serial.println("No HTTP server, attendance scan, config writes, or intentional NVS erase.");

  // Construct the FreeRTOS mutex after initArduino(); every probe I2C client
  // uses this same lock, including the direct address scan and OLED adapter.
  static att::Esp32I2cBusOwner sharedI2cBus(Wire);
  static att::Esp32Oled oledDevice(Wire, sharedI2cBus.lock());
  static att::Esp32Ds3231RegisterIo rtcRegisters(Wire, sharedI2cBus.lock());
  oled = &oledDevice;
  if (!sharedI2cBus.lockValid()) {
    Serial.println("I2C_BUS_LOCK=FAIL AP ABORTED");
    return;
  }

  const att::Esp32NvsStartupStatus nvsStatus =
      att::Esp32NvsStartupGuard::status();
  if (!nvsStatus.safeForApplication) {
    Serial.printf(
        "SAFE_NVS_INIT=FAIL observed=%u init_calls=%lu first_esp_err=%ld "
        "erase_blocked=%u; NO ERASE/RETRY; AP ABORTED\n",
        nvsStatus.initObserved ? 1U : 0U,
        static_cast<unsigned long>(nvsStatus.initCallCount),
        static_cast<long>(nvsStatus.firstInitResult),
        nvsStatus.nvsEraseBlocked ? 1U : 0U);
    return;
  }
  Serial.printf("SAFE_NVS_INIT=PASS init_calls=%lu erase_guard=ARMED\n",
                static_cast<unsigned long>(nvsStatus.initCallCount));

  const bool wireReady = sharedI2cBus.begin(21, 22, kI2cClockHz);
  Serial.printf("I2C_BUS_BEGIN=%s sda=21 scl=22 clock_hz=%lu\n",
                wireReady ? "PASS" : "FAIL",
                static_cast<unsigned long>(kI2cClockHz));
  if (wireReady) {
    scanI2cBus(sharedI2cBus.lock());
    uint8_t rtcStatus = 0;
    const bool rtcStatusRead = rtcRegisters.readRegisters(0x0F, &rtcStatus, 1);
    Serial.printf("RTC_STATUS_READ=%s osf=%u (read-only; time policy untouched)\n",
                  rtcStatusRead ? "PASS" : "FAIL",
                  rtcStatusRead && (rtcStatus & 0x80u) != 0 ? 1U : 0U);
  }

  if (wireReady) {
    const att::Esp32OledInitCode oledResult = oled->begin();
    oledInitialized = oledResult == att::EOI_BUFFER_READY ||
                      oledResult == att::EOI_ALREADY_INITIALIZED;
    Serial.printf("OLED_BEGIN=%u framebuffer_ready=%u "
                  "(not physical panel acknowledgement)\n",
                  static_cast<unsigned>(oledResult), oledInitialized ? 1U : 0U);
  } else {
    Serial.println("OLED_BEGIN=SKIPPED shared_i2c_bus_not_started");
  }

  if (!createEphemeralPassphrase()) {
    Serial.println("PROBE_CREDENTIAL=FAIL bootstrap_rng_or_hex_encoding");
    clearProbeScreen("credential_failure");
    return;
  }

  const att::ApCredentialResult disclosure = {att::APCB_OK, true};
  if (oledInitialized) {
    const att::Esp32OledActionCode displayResult =
        oled->showSetupCredential(disclosure, probePassphrase, 32);
    Serial.printf("OLED_CREDENTIAL_TRANSFER=%u "
                  "(request only; inspect actual pixels)\n",
                  static_cast<unsigned>(displayResult));
    if (displayResult == att::EOA_TRANSFER_REQUESTED) {
      credentialShownAt = millis();
    }
  } else {
    Serial.println("OLED_CREDENTIAL_TRANSFER=SKIPPED panel_adapter_not_initialized");
  }

  // This is an ephemeral diagnostic secret. It is never written to NVS or
  // Config. USB serial is a deliberate credential fallback for this probe.
  Serial.printf("PROBE_SSID=%s channel=%u\n", kProbeSsid,
                static_cast<unsigned>(kProbeChannel));
  Serial.printf("PROBE_PASSWORD=%s (temporary; do not reuse)\n", probePassphrase);

  const bool started = wifiAp.start(kProbeSsid, sizeof(kProbeSsid) - 1,
                                    probePassphrase, 32, kProbeChannel);
  const bool adapterReady = started && wifiAp.ready();
  const bool configMatches = adapterReady &&
      verifyApConfiguration(probePassphrase, 32);
  secureClear(probePassphrase, sizeof probePassphrase);

  if (!adapterReady || !configMatches) {
    Serial.printf("AP_START=FAIL adapter_code=%u ready=%u config_match=%u\n",
                  static_cast<unsigned>(wifiAp.code()),
                  wifiAp.ready() ? 1U : 0U, configMatches ? 1U : 0U);
    if (adapterReady) {
      const bool stopped = wifiAp.stop();
      Serial.printf("AP_STOP_AFTER_VERIFY_FAILURE=%s code=%u\n",
                    stopped ? "PASS" : "FAIL",
                    static_cast<unsigned>(wifiAp.code()));
    }
    clearProbeScreen("ap_start_or_config_verify_failure");
    return;
  }

  apStarted = true;
  Serial.println("AP_START=PASS ssid=att channel=1 adapter_ready=1 config_verified=1");
  Serial.println("Connect a phone to this AP; DHCP is reported when this AP netif assigns an IP.");
  Serial.println("No DHCP report means association/lease did not complete or the event was missed.");
}

void loop() {
  if (apStarted && !dhcpEventReported && wifiAp.hasAssignedClientIp()) {
    Serial.println("DHCP_ASSIGNMENT_EVENT=1 (historical lease event; not authentication/current presence)");
    dhcpEventReported = true;
  }

  if (apStarted && !oledClearReported && credentialShownAt != 0 &&
      static_cast<uint32_t>(millis() - credentialShownAt) >=
          kCredentialDisplayMs) {
    clearProbeScreen("90_second_timeout");
  }

  if (apStarted && static_cast<uint32_t>(millis() - lastStatusAt) >=
                       kStatusPeriodMs) {
    lastStatusAt = millis();
    Serial.printf("AP_STATUS ready=%u dhcp_ever_assigned=%u\n",
                  wifiAp.ready() ? 1U : 0U,
                  wifiAp.hasAssignedClientIp() ? 1U : 0U);
  }
  delay(50);
}
