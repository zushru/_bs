#include <Arduino.h>
#include <Wire.h>
#include <esp_err.h>
#include <esp_wifi.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp32_bootstrap_random.h"
#include "esp32_oled.h"
#include "esp32_nvs_startup_guard.h"
#include "esp32_secure_random.h"
#include "esp32_wifi_ap.h"
#include "ap_credential_bootstrap.h"
#include "probe_credential.h"

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
att::Esp32Oled oled(Wire);

char probePassphrase[33] = {};
bool apStarted = false;
bool dhcpEventReported = false;
bool oledClearReported = false;
bool oledInitialized = false;
uint32_t credentialShownAt = 0;
uint32_t lastStatusAt = 0;

void secureClear(void* data, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(data);
  while (length-- > 0) *cursor++ = 0;
}

void scanI2cBus() {
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
  if (!oledInitialized || oledClearReported) return;
  const att::Esp32OledActionCode result = oled.clear();
  Serial.printf("OLED_CLEAR_REQUEST=%u reason=%s (not panel acknowledgement)\n",
                static_cast<unsigned>(result), reason);
  oledClearReported = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("ATT_HARDWARE_PROBE=v0.2.48");
  Serial.println("PROBE_ONLY=1 no_attendance_or_config_namespace_access=1");
  Serial.println("Wi-Fi PHY may read/write its separate NVS namespace: phy.");
  Serial.println("No HTTP server, attendance scan, config writes, or intentional NVS erase.");

  const att::Esp32NvsStartupStatus nvsStatus =
      att::esp32NvsStartupStatus();
  if (!nvsStatus.mayStartApplication) {
    Serial.printf(
        "SAFE_NVS_INIT=FAIL observed=%u esp_err=%d erase_blocked=%u; "
        "NO APP START; AP ABORTED\n",
        nvsStatus.initObserved ? 1U : 0U,
        static_cast<int>(nvsStatus.firstInitResult),
        nvsStatus.fullNvsEraseBlocked ? 1U : 0U);
    return;
  }
  Serial.println("SAFE_NVS_INIT=PASS erase_guard=ARMED");

  const bool wireReady = Wire.begin(21, 22, kI2cClockHz);
  Serial.printf("I2C_BUS_BEGIN=%s sda=21 scl=22 clock_hz=%lu\n",
                wireReady ? "PASS" : "FAIL",
                static_cast<unsigned long>(kI2cClockHz));
  if (wireReady) scanI2cBus();

  if (wireReady) {
    const att::Esp32OledInitCode oledResult = oled.begin();
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
        oled.showSetupCredential(disclosure, probePassphrase, 32);
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
