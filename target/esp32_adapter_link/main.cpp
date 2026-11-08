#include <stddef.h>
#include <stdint.h>

#include "esp32_bootstrap_random.h"
#include "esp32_pin_auth_runtime.h"
#include "esp32_oled.h"
#include "esp32_nvs_startup_guard.h"
#include "ap_credential_setup.h"
#include "esp32_secure_random.h"
#include "esp32_wifi_ap.h"
#include "config_boot.h"
#include "nvs_ap_credential_store.h"
#include "nvs_config_backend.h"
#include "nvs_pin_attempt_store.h"

namespace {

// This is a compile/link-only smoke application. Global construction does not
// initialize Wi-Fi or NVS; it only verifies that the adapter types compose.
att::Esp32SecureRandom secureRandom;
att::Esp32BootstrapRandom bootstrapRandom;
att::Esp32WifiAp wifiAp(secureRandom);
att::Esp32NvsApCredentialStore apCredentialStore;
att::ApCredentialBootstrap apCredentialBootstrap(apCredentialStore,
                                                   bootstrapRandom);
att::Esp32NvsConfigBackend configBackend;
att::ConfigBootCoordinator configBoot(configBackend);
att::Esp32NvsPinAttemptStore pinAttemptStore("attpin");
att::Esp32PinAuthRuntime pinAuthRuntime("attpin");
att::Esp32Oled oled(Wire);
att::ApCredentialSetupCoordinator apCredentialSetup(
    configBoot, apCredentialBootstrap, oled, wifiAp);

// Keep direct references to adapter methods so the final link must resolve the
// ESP-IDF symbols referenced by their implementations. These pointers are
// read in setup(); no method is called and no hardware/storage action occurs.
using ApStartRef = bool (att::Esp32WifiAp::*)(const char*, size_t,
                                               const char*, size_t, int);
using ApStopRef = bool (att::Esp32WifiAp::*)();
using RandomFillRef = bool (att::Esp32SecureRandom::*)(uint8_t*, size_t);
using BootstrapFillRef = bool (att::Esp32BootstrapRandom::*)(uint8_t*, size_t);
using ApStoreLoadRef = att::ApCredentialStoreRead
    (att::Esp32NvsApCredentialStore::*)(char*);
using ApStoreCreateRef = att::ApCredentialStoreWrite
    (att::Esp32NvsApCredentialStore::*)(const char*);
using CredentialPrepareRef = att::ApCredentialResult
    (att::ApCredentialBootstrap::*)(att::ConfigBootState, const char*, char*);
using ConfigStartRef = att::ConfigBootResult (att::ConfigBootCoordinator::*)();
using ConfigProvisionRef = att::ConfigBootResult
    (att::ConfigBootCoordinator::*)(const att::Config&,
                                     const att::IConfigProvisionGate&);
using ConfigBeginRef = bool (att::Esp32NvsConfigBackend::*)();
using ConfigLoadRef = bool (att::Esp32NvsConfigBackend::*)(att::Config&,
                                                            att::CfgPick&);
using ConfigReadSlotRef = bool (att::Esp32NvsConfigBackend::*)(uint8_t,
                                                                uint8_t*);
using ConfigWriteSlotRef = bool (att::Esp32NvsConfigBackend::*)(
    uint8_t, const uint8_t*);
using ConfigReadMarkerRef = bool (att::Esp32NvsConfigBackend::*)(bool&);
using PinInitRef = bool (att::Esp32NvsPinAttemptStore::*)();
using PinLoadRef = bool (att::Esp32NvsPinAttemptStore::*)(uint16_t&);
using PinPersistRef = bool (att::Esp32NvsPinAttemptStore::*)(uint16_t);
using PinRuntimeStartRef = bool (att::Esp32PinAuthRuntime::*)(
    const att::ConfigBootCoordinator&, uint32_t);
using OledBeginRef = att::Esp32OledInitCode (att::Esp32Oled::*)();
using OledShowCredentialRef = att::Esp32OledActionCode
    (att::Esp32Oled::*)(const att::ApCredentialResult&, const char*, size_t);
using OledClearRef = att::Esp32OledActionCode (att::Esp32Oled::*)();
using OledRequestCredentialRef = bool (att::Esp32Oled::*)(
    const att::ApCredentialResult&, const char*, size_t);
using OledRequestBlankRef = bool (att::Esp32Oled::*)();
using ApHasAssignedIpRef = bool (att::Esp32WifiAp::*)() const;
using SetupStartRef = att::ApCredentialSetupCode
    (att::ApCredentialSetupCoordinator::*)();
using SetupProvisionRef = att::ApCredentialSetupCode
    (att::ApCredentialSetupCoordinator::*)(
        const att::Config&, const att::IConfigProvisionGate&,
        att::ConfigBootResult&);
using SetupRetryBlankRef = att::ApCredentialSetupCode
    (att::ApCredentialSetupCoordinator::*)();
using SetupMayProvisionRef = bool
    (att::ApCredentialSetupCoordinator::*)(
        const att::IConfigProvisionGate&) const;

volatile ApStartRef keepApStart = &att::Esp32WifiAp::start;
volatile ApStopRef keepApStop = &att::Esp32WifiAp::stop;
volatile RandomFillRef keepSecureRandomFill = &att::Esp32SecureRandom::fill;
volatile BootstrapFillRef keepBootstrapRandomFill =
    &att::Esp32BootstrapRandom::fill;
volatile ApStoreLoadRef keepApStoreLoad =
    &att::Esp32NvsApCredentialStore::load;
volatile ApStoreCreateRef keepApStoreCreate =
    &att::Esp32NvsApCredentialStore::createIfAbsent;
volatile CredentialPrepareRef keepCredentialPrepare =
    &att::ApCredentialBootstrap::prepare;
volatile ConfigStartRef keepConfigStart = &att::ConfigBootCoordinator::start;
volatile ConfigProvisionRef keepConfigProvision =
    &att::ConfigBootCoordinator::provision;
volatile ConfigBeginRef keepConfigBegin = &att::Esp32NvsConfigBackend::begin;
volatile ConfigLoadRef keepConfigLoad = &att::Esp32NvsConfigBackend::load;
volatile ConfigReadSlotRef keepConfigReadSlot =
    &att::Esp32NvsConfigBackend::readSlot;
volatile ConfigWriteSlotRef keepConfigWriteSlot =
    &att::Esp32NvsConfigBackend::writeSlot;
volatile ConfigReadMarkerRef keepConfigReadMarker =
    &att::Esp32NvsConfigBackend::readProvisionMarker;
volatile PinInitRef keepPinInit =
    &att::Esp32NvsPinAttemptStore::initializeFailureCount;
volatile PinLoadRef keepPinLoad =
    &att::Esp32NvsPinAttemptStore::loadFailureCount;
volatile PinPersistRef keepPinPersist =
    &att::Esp32NvsPinAttemptStore::persistFailureCount;
volatile PinRuntimeStartRef keepPinRuntimeStart =
    &att::Esp32PinAuthRuntime::start;
volatile OledBeginRef keepOledBegin = &att::Esp32Oled::begin;
volatile OledShowCredentialRef keepOledShowCredential =
    &att::Esp32Oled::showSetupCredential;
volatile OledClearRef keepOledClear = &att::Esp32Oled::clear;
volatile OledRequestCredentialRef keepOledRequestCredential =
    &att::Esp32Oled::requestCredentialDisplay;
volatile OledRequestBlankRef keepOledRequestBlank =
    &att::Esp32Oled::requestBlankFrame;
volatile ApHasAssignedIpRef keepApHasAssignedIp =
    &att::Esp32WifiAp::hasAssignedClientIp;
volatile SetupStartRef keepSetupStart =
    &att::ApCredentialSetupCoordinator::start;
volatile SetupProvisionRef keepSetupProvision =
    &att::ApCredentialSetupCoordinator::provision;
volatile SetupRetryBlankRef keepSetupRetryBlank =
    &att::ApCredentialSetupCoordinator::retryBlankFrame;
volatile SetupMayProvisionRef keepSetupMayProvision =
    &att::ApCredentialSetupCoordinator::mayProvision;

}  // namespace

void setup() {
  // Guard is observed during framework startup, before setup().
  if (!att::esp32NvsStartupStatus().mayStartApplication) return;

  // Volatile reads retain the function references for link checking. This
  // smoke application intentionally performs no AP, entropy, or NVS operation.
  (void)keepApStart;
  (void)keepApStop;
  (void)keepSecureRandomFill;
  (void)keepBootstrapRandomFill;
  (void)keepApStoreLoad;
  (void)keepApStoreCreate;
  (void)keepCredentialPrepare;
  (void)keepConfigStart;
  (void)keepConfigProvision;
  (void)keepConfigBegin;
  (void)keepConfigLoad;
  (void)keepConfigReadSlot;
  (void)keepConfigWriteSlot;
  (void)keepConfigReadMarker;
  (void)keepPinInit;
  (void)keepPinLoad;
  (void)keepPinPersist;
  (void)keepPinRuntimeStart;
  (void)keepOledBegin;
  (void)keepOledShowCredential;
  (void)keepOledClear;
  (void)keepOledRequestCredential;
  (void)keepOledRequestBlank;
  (void)keepApHasAssignedIp;
  (void)keepSetupStart;
  (void)keepSetupProvision;
  (void)keepSetupRetryBlank;
  (void)keepSetupMayProvision;
}

void loop() {}
