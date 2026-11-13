#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <nvs_flash.h>

#include "esp32_bootstrap_random.h"
#include "esp32_pin_auth_runtime.h"
#include "esp32_oled.h"
#include "i2c_bus_lock.h"
#include "esp32_i2c_bus_owner.h"
#include "ds3231_clock.h"
#include "esp32_ds3231_register_io.h"
#include "ap_credential_setup.h"
#include "esp32_secure_random.h"
#include "esp32_wifi_ap.h"
#include "config_boot.h"
#include "nvs_ap_credential_store.h"
#include "nvs_config_backend.h"
#include "nvs_pin_attempt_store.h"
#include "esp32_nvs_startup_guard.h"
#include "as608_runtime.h"
#include "as608_startup.h"
#include "as608_attendance_owner.h"
#include "as608_owner_config_runtime.h"
#include "application_attendance_gate.h"
#include "roster_snapshot.h"
#include "littlefs_file_set.h"
#include "littlefs_mount_owner.h"
#include "esp32_littlefs_mount.h"

namespace {

// This is a compile/link-only smoke application. Global construction does not
// initialize Wi-Fi or NVS; it only verifies that the adapter types compose.
att::Esp32SecureRandom secureRandom;
att::Esp32BootstrapRandom bootstrapRandom;
att::Esp32WifiAp wifiAp(secureRandom);
att::Esp32NvsConfigBackend configBackend;
att::ConfigBootCoordinator configBoot(configBackend);
att::Esp32NvsApCredentialStore apCredentialStore(configBoot);
att::ApCredentialBootstrap apCredentialBootstrap(apCredentialStore,
                                                   bootstrapRandom);
att::Esp32PinAuthRuntime pinAuthRuntime("attpin", configBoot);

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
using PinRuntimeStartRef = bool (att::Esp32PinAuthRuntime::*)(uint32_t);
using Ds3231ReadRef = att::Ds3231Code
    (att::Ds3231Clock::*)(int16_t, att::Ds3231Reading&) const;
using Ds3231WriteRef = att::Ds3231Code
    (att::Ds3231Clock::*)(uint32_t, int16_t);
using Ds3231ClearOsfRef = att::Ds3231Code (att::Ds3231Clock::*)();
using Ds3231RegisterReadRef = bool (att::Esp32Ds3231RegisterIo::*)(
    uint8_t, uint8_t*, uint8_t);
using Ds3231RegisterWriteRef = att::Ds3231WriteResult
    (att::Esp32Ds3231RegisterIo::*)(uint8_t, const uint8_t*, uint8_t);
using I2cOwnerBeginRef = bool (att::Esp32I2cBusOwner::*)(int, int, uint32_t);
using I2cLockAcquireRef = bool (att::I2cBusMutex::*)();
using I2cLockReleaseRef = void (att::I2cBusMutex::*)();
volatile I2cOwnerBeginRef keepI2cOwnerBegin = &att::Esp32I2cBusOwner::begin;
volatile I2cLockAcquireRef keepI2cLockAcquire = &att::I2cBusMutex::acquire;
volatile I2cLockReleaseRef keepI2cLockRelease = &att::I2cBusMutex::release;
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
using NvsStartupStatusRef = att::Esp32NvsStartupStatus (*)();

// Application gate, preferred attendance owner and config runtime link anchors.
// They are not constructed here; this smoke target remains inert.
using AttendanceOwnerStartupRef = bool (att::As608AttendanceOwner::*)();
using AttendanceOwnerScanBeginRef = bool (att::As608AttendanceOwner::*)(uint32_t);
using AttendanceOwnerScanStepRef = att::AttendancePipelineResult
    (att::As608AttendanceOwner::*)();
using AttendanceOwnerConfigCheckRef = bool
    (att::As608AttendanceOwner::*)(const att::Config&) const;
using AttendanceGateStartupReadyRef =
    bool (att::ApplicationAttendanceGate::*)() const;
using AttendanceGateAttendanceReadyRef =
    bool (att::ApplicationAttendanceGate::*)() const;
using AttendanceGateConfigMatchRef = bool
    (att::ApplicationAttendanceGate::*)(const att::Config&) const;
using AttendanceGateClosureMatchRef = bool
    (att::ApplicationAttendanceGate::*)(const att::ClosureEngine&) const;
// Qualified calls in these link-only shims bypass virtual member-pointer
// encoding and create direct references to both concrete runtime methods.
bool ownerConfigCanApplyLinkShim(att::As608OwnerConfigRuntime& runtime,
                                 const att::Config& next) {
  return runtime.att::As608OwnerConfigRuntime::canApply(next);
}
bool ownerConfigApplyLinkShim(att::As608OwnerConfigRuntime& runtime,
                              const att::Config& next) {
  return runtime.att::As608OwnerConfigRuntime::applyAfterCommit(next);
}
using OwnerConfigMethodLinkProbe = bool (*)(
    att::As608OwnerConfigRuntime&, const att::Config&);
volatile OwnerConfigMethodLinkProbe keepOwnerConfigCanApply =
    &ownerConfigCanApplyLinkShim;
volatile OwnerConfigMethodLinkProbe keepOwnerConfigApply =
    &ownerConfigApplyLinkShim;

volatile AttendanceOwnerStartupRef keepAttendanceOwnerStartup =
    &att::As608AttendanceOwner::beginStartup;
volatile AttendanceOwnerScanBeginRef keepAttendanceOwnerScanBegin =
    &att::As608AttendanceOwner::beginScan;
volatile AttendanceOwnerScanStepRef keepAttendanceOwnerScanStep =
    &att::As608AttendanceOwner::stepScan;
volatile AttendanceOwnerConfigCheckRef keepAttendanceOwnerConfigCheck =
    &att::As608AttendanceOwner::canSetConfigAfterCommit;
volatile AttendanceGateStartupReadyRef keepAttendanceGateStartupReady =
    &att::ApplicationAttendanceGate::readyForSensorStartup;
volatile AttendanceGateAttendanceReadyRef keepAttendanceGateAttendanceReady =
    &att::ApplicationAttendanceGate::readyForAttendance;
volatile AttendanceGateConfigMatchRef keepAttendanceGateConfigMatch =
    &att::ApplicationAttendanceGate::matchesConfig;
volatile AttendanceGateClosureMatchRef keepAttendanceGateClosureMatch =
    &att::ApplicationAttendanceGate::matchesClosure;


// AS608 link anchors force the UART Stream bridge, raw command adapter,
// attendance client, enrollment session/FSM and single-owner runtime to compile
// and link against the pinned Arduino-ESP32 target. No anchor is invoked.
using RosterAuditConsumeRef = bool (att::RosterTransitionAudit::*)(
    const att::Event&, int16_t);
volatile RosterAuditConsumeRef keepRosterAuditConsume =
    &att::RosterTransitionAudit::consume;

// LittleFsFileSet is compiled/linked against the pinned ESP32 VFS toolchain,
// but this smoke target never mounts LittleFS or invokes any file operation.
using FileSetCountRef = bool (att::LittleFsFileSet::*)(size_t&);
using FileSetMonthKeyRef = bool (att::LittleFsFileSet::*)(size_t, uint32_t&);
using FileSetFileSizeRef = bool (att::LittleFsFileSet::*)(size_t, size_t&);
using FileSetReadRef = bool (att::LittleFsFileSet::*)(
    size_t, size_t, uint8_t*, size_t);
using FileSetAppendRef = bool (att::LittleFsFileSet::*)(
    size_t, const uint8_t*, size_t);
using FileSetAddRef = bool (att::LittleFsFileSet::*)(uint32_t);
using FileSetTruncateRef = bool (att::LittleFsFileSet::*)(size_t, size_t);
using FileSetQuarantineRef = bool (att::LittleFsFileSet::*)(size_t);
volatile FileSetCountRef keepFileSetCount = &att::LittleFsFileSet::countChecked;
volatile FileSetMonthKeyRef keepFileSetMonthKey =
    &att::LittleFsFileSet::monthKeyChecked;
volatile FileSetFileSizeRef keepFileSetFileSize =
    &att::LittleFsFileSet::fileSizeChecked;
volatile FileSetReadRef keepFileSetRead = &att::LittleFsFileSet::readFile;
volatile FileSetAppendRef keepFileSetAppend = &att::LittleFsFileSet::appendFile;
volatile FileSetAddRef keepFileSetAdd = &att::LittleFsFileSet::addFile;
volatile FileSetTruncateRef keepFileSetTruncate =
    &att::LittleFsFileSet::truncateFile;
volatile FileSetQuarantineRef keepFileSetQuarantineCopy =
    &att::LittleFsFileSet::quarantineCopy;
volatile FileSetQuarantineRef keepFileSetQuarantineFrom =
    &att::LittleFsFileSet::quarantineFrom;
using LittleFsMountRef = bool (att::Esp32LittleFsMountBackend::*)(
    const char*, const char*);
using LittleFsUnmountRef = bool (att::Esp32LittleFsMountBackend::*)();
using StorageOwnerBeginRef = bool (att::LittleFsStorageOwner::*)();
using StorageOwnerFileSetRef = att::IFileSet* (att::LittleFsStorageOwner::*)();
volatile LittleFsMountRef keepLittleFsMount =
    &att::Esp32LittleFsMountBackend::mountWithoutFormatting;
volatile LittleFsUnmountRef keepLittleFsUnmount =
    &att::Esp32LittleFsMountBackend::unmountOwned;
volatile StorageOwnerBeginRef keepLittleFsOwnerBegin =
    &att::LittleFsStorageOwner::begin;
volatile StorageOwnerFileSetRef keepLittleFsOwnerFileSet =
    &att::LittleFsStorageOwner::fileSet;
using RuntimeAttendanceRef = const att::As608AttendanceClient&
    (att::As608Runtime::*)() const;
using RuntimeEnrollmentRef = const att::As608EnrollmentSession&
    (att::As608Runtime::*)() const;
using RuntimeFaultedRef = bool (att::As608Runtime::*)() const;
using RuntimeOccupancyStoreRef = const att::Occupancy&
    (att::As608Runtime::*)() const;
using StartupCtorRef = void (*)(void*, att::As608Runtime&);
using StartupBeginRef = bool (att::As608StartupCoordinator::*)();
using StartupStepRef = att::As608StartupStatus (att::As608StartupCoordinator::*)();
using LinkStartRef = int (att::StreamAs608Link::*)(
    att::As608CommandOwner, const uint8_t*, size_t, uint32_t);
using LinkPollRef = int (att::StreamAs608Link::*)(
    att::As608CommandOwner, uint8_t*, size_t, size_t&);
using SensorAuthenticateRef = att::EnrollIo (att::As608EnrollSensor::*)();
using SensorCapacityRef = att::EnrollIo (att::As608EnrollSensor::*)(uint16_t&);
using SensorIndexRef = att::EnrollIo (att::As608EnrollSensor::*)(uint8_t, uint8_t*);
using SensorCaptureRef = att::EnrollIo (att::As608EnrollSensor::*)();
using SensorConvertRef = att::EnrollIo (att::As608EnrollSensor::*)(uint8_t);
using SensorSearchRef = att::EnrollIo (att::As608EnrollSensor::*)(uint16_t&, uint16_t&);
using SensorCreateRef = att::EnrollIo (att::As608EnrollSensor::*)();
using SensorStoreRef = att::EnrollIo (att::As608EnrollSensor::*)(uint16_t);
using OccupancyBeginRef = bool (att::As608OccupancyReader::*)();
using OccupancyStepRef = att::EnrollIo (att::As608OccupancyReader::*)();
using AttendanceSetupBeginRef = bool (att::As608AttendanceClient::*)();
using AttendanceSetupStepRef = att::As608AttendanceResult
    (att::As608AttendanceClient::*)();
using AttendanceScanBeginRef = bool (att::As608AttendanceClient::*)();
using AttendanceScanStepRef = att::As608AttendanceResult
    (att::As608AttendanceClient::*)(uint16_t&, uint16_t&);
using EnrollmentAcquireRef = bool (att::As608EnrollmentSession::*)();
using EnrollmentSyncRef = bool (att::As608EnrollmentSession::*)();
using EnrollmentAbortRef = bool (att::As608EnrollmentSession::*)();
using EnrollBeginRef = att::EnrollStart (att::EnrollFsm::*)(uint16_t, bool);
using EnrollStepRef = void (att::EnrollFsm::*)();
using EnrollCancelRef = void (att::EnrollFsm::*)();
using NvsEraseRef = esp_err_t (*)();
using NvsErasePartitionRef = esp_err_t (*)(const char*);
using NvsErasePartitionPtrRef = esp_err_t (*)(const esp_partition_t*);

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
volatile Ds3231ReadRef keepDs3231Read = &att::Ds3231Clock::readUtcEpoch;
volatile Ds3231WriteRef keepDs3231Write = &att::Ds3231Clock::writeUtcEpoch;
volatile Ds3231ClearOsfRef keepDs3231ClearOsf =
    &att::Ds3231Clock::clearOscillatorStopFlag;
volatile Ds3231RegisterReadRef keepDs3231RegisterRead =
    &att::Esp32Ds3231RegisterIo::readRegisters;
volatile Ds3231RegisterWriteRef keepDs3231RegisterWrite =
    &att::Esp32Ds3231RegisterIo::writeRegisters;
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
volatile NvsStartupStatusRef keepNvsStartupStatus =
    &att::Esp32NvsStartupGuard::status;
volatile RuntimeAttendanceRef keepRuntimeAttendance =
    &att::As608Runtime::attendance;
volatile RuntimeEnrollmentRef keepRuntimeEnrollment =
    &att::As608Runtime::enrollment;
volatile RuntimeFaultedRef keepRuntimeFaulted =
    &att::As608Runtime::transportFaulted;
volatile RuntimeOccupancyStoreRef keepRuntimeOccupancyStore =
    &att::As608Runtime::occupancy;
volatile StartupBeginRef keepStartupBegin =
    &att::As608StartupCoordinator::begin;
volatile StartupStepRef keepStartupStep =
    &att::As608StartupCoordinator::step;
volatile LinkStartRef keepLinkStart = &att::StreamAs608Link::startTransaction;
volatile LinkPollRef keepLinkPoll = &att::StreamAs608Link::pollTransaction;
volatile SensorAuthenticateRef keepSensorAuthenticate =
    &att::As608EnrollSensor::authenticate;
volatile SensorCapacityRef keepSensorCapacity = &att::As608EnrollSensor::readCapacity;
volatile SensorIndexRef keepSensorIndex = &att::As608EnrollSensor::readIndexPage;
volatile SensorCaptureRef keepSensorCapture = &att::As608EnrollSensor::captureImage;
volatile SensorConvertRef keepSensorConvert = &att::As608EnrollSensor::convertImage;
volatile SensorSearchRef keepSensorSearch = &att::As608EnrollSensor::searchTemplate;
volatile SensorCreateRef keepSensorCreate = &att::As608EnrollSensor::createModel;
volatile SensorStoreRef keepSensorStore = &att::As608EnrollSensor::storeModel;
volatile OccupancyBeginRef keepOccupancyBegin = &att::As608OccupancyReader::begin;
volatile OccupancyStepRef keepOccupancyStep = &att::As608OccupancyReader::step;
volatile AttendanceSetupBeginRef keepAttendanceSetupBegin =
    &att::As608AttendanceClient::beginSetup;
volatile AttendanceSetupStepRef keepAttendanceSetupStep =
    &att::As608AttendanceClient::stepSetup;
volatile AttendanceScanBeginRef keepAttendanceScanBegin =
    &att::As608AttendanceClient::beginScan;
volatile AttendanceScanStepRef keepAttendanceScanStep =
    &att::As608AttendanceClient::stepScan;
volatile EnrollmentAcquireRef keepEnrollmentAcquire =
    &att::As608EnrollmentSession::acquire;
volatile EnrollmentSyncRef keepEnrollmentSync =
    &att::As608EnrollmentSession::sync;
volatile EnrollmentAbortRef keepEnrollmentAbort =
    &att::As608EnrollmentSession::abortPreparation;
volatile EnrollBeginRef keepEnrollBegin = &att::EnrollFsm::begin;
volatile EnrollStepRef keepEnrollStep = &att::EnrollFsm::step;
volatile EnrollCancelRef keepEnrollCancel = &att::EnrollFsm::cancel;
// Link-only anchors: these APIs are retained for final-ELF route inspection;
// setup() never invokes them and this smoke application never erases NVS.
volatile NvsEraseRef keepNvsFlashErase = &nvs_flash_erase;
volatile NvsErasePartitionRef keepNvsFlashErasePartition =
    &nvs_flash_erase_partition;
volatile NvsErasePartitionPtrRef keepNvsFlashErasePartitionPtr =
    &nvs_flash_erase_partition_ptr;

}  // namespace

void setup() {
  // Construct the link-only sensor composition after initArduino(), avoiding
  // cross-translation-unit static initialization against the global Serial2.
  // Constructors only wire references/in-memory state; no UART method is called.
  // Serial2 is never begun and the sentinel password is never transmitted.
  static att::Esp32I2cBusOwner sharedI2cBus(Wire);
  volatile bool sharedI2cBusReady = sharedI2cBus.lockValid();
  (void)sharedI2cBusReady;
  static att::Esp32Oled oled(Wire, sharedI2cBus.lock());
  static att::Esp32Ds3231RegisterIo ds3231Registers(Wire, sharedI2cBus.lock());
  static att::Ds3231Clock ds3231Clock(ds3231Registers);
  static att::ApCredentialSetupCoordinator apCredentialSetup(
      configBoot, apCredentialBootstrap, oled, wifiAp);
  (void)&ds3231Clock;
  static att::RosterStore sensorSmokeRoster(480);
  // Retain the roster-history semantic audit in the final target link and BSS
  // so the new fixed per-ID state is checked against the real ESP32 ABI.
  static att::RosterTransitionAudit rosterAuditSmoke;
  volatile bool rosterAuditValid = rosterAuditSmoke.valid();
  (void)rosterAuditValid;
  // Storage-owner construction allocates its FreeRTOS mutex only.
  // begin() is deliberately not called: this target never registers/mounts
  // LittleFS or reads, writes, formats, or erases partition data.
  static att::Esp32LittleFsMountBackend littleFsMountBackend;
  static att::LittleFsStorageOwner littleFsOwner(
      littleFsMountBackend, "/littlefs", "spiffs", 480);
  volatile bool littleFsOwnerReady =
      littleFsOwner.state() == att::LITTLEFS_OWNER_READY;
  (void)littleFsOwnerReady;
  static att::LittleFsFileSet littleFsSmoke("/littlefs");
  volatile bool littleFsPathValid = littleFsSmoke.valid();
  (void)littleFsPathValid;
  static att::As608Runtime sensorRuntime(Serial2, sensorSmokeRoster, 0U);
  static att::As608StartupCoordinator sensorStartup(sensorRuntime);
  (void)&sensorRuntime;
  (void)&sensorStartup;

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
  // Force the non-virtual provisioning initializer's member-function pointer
  // into a live relocation; a discarded-value expression alone was optimized
  // away and allowed --gc-sections to remove the method from the final ELF.
  const PinInitRef pinInitLinkProbe = keepPinInit;
  volatile bool pinInitLinkProbeValid = pinInitLinkProbe != nullptr;
  (void)pinInitLinkProbeValid;
  (void)keepPinLoad;
  (void)keepPinPersist;
  (void)keepPinRuntimeStart;
  const OledBeginRef oledBeginLinkProbe = keepOledBegin;
  volatile bool oledBeginLinkProbeValid = oledBeginLinkProbe != nullptr;
  (void)oledBeginLinkProbeValid;
  (void)keepOledShowCredential;
  (void)keepOledClear;
  (void)keepOledRequestCredential;
  (void)keepOledRequestBlank;
  (void)keepI2cOwnerBegin;
  (void)keepI2cLockAcquire;
  (void)keepI2cLockRelease;
  // Retain DS3231 conversion and transport code without starting I2C or
  // issuing RTC transactions in this inert compile/link target.
  const Ds3231ReadRef ds3231ReadLinkProbe = keepDs3231Read;
  volatile bool ds3231ReadLinkProbeValid = ds3231ReadLinkProbe != nullptr;
  (void)ds3231ReadLinkProbeValid;
  const Ds3231WriteRef ds3231WriteLinkProbe = keepDs3231Write;
  volatile bool ds3231WriteLinkProbeValid = ds3231WriteLinkProbe != nullptr;
  (void)ds3231WriteLinkProbeValid;
  const Ds3231ClearOsfRef ds3231ClearOsfLinkProbe = keepDs3231ClearOsf;
  volatile bool ds3231ClearOsfLinkProbeValid =
      ds3231ClearOsfLinkProbe != nullptr;
  (void)ds3231ClearOsfLinkProbeValid;
  const Ds3231RegisterReadRef ds3231RegisterReadLinkProbe =
      keepDs3231RegisterRead;
  volatile bool ds3231RegisterReadLinkProbeValid =
      ds3231RegisterReadLinkProbe != nullptr;
  (void)ds3231RegisterReadLinkProbeValid;
  const Ds3231RegisterWriteRef ds3231RegisterWriteLinkProbe =
      keepDs3231RegisterWrite;
  volatile bool ds3231RegisterWriteLinkProbeValid =
      ds3231RegisterWriteLinkProbe != nullptr;
  (void)ds3231RegisterWriteLinkProbeValid;
  (void)keepApHasAssignedIp;
  (void)keepSetupStart;
  (void)keepSetupProvision;
  (void)keepSetupRetryBlank;
  (void)keepSetupMayProvision;
  (void)keepNvsStartupStatus;
  // Force each new application-owner member function into a live relocation.
  // Plain discarded-value reads are insufficient under --gc-sections.
  const AttendanceOwnerStartupRef ownerStartupLinkProbe =
      keepAttendanceOwnerStartup;
  volatile bool ownerStartupLinkProbeValid =
      ownerStartupLinkProbe != nullptr;
  (void)ownerStartupLinkProbeValid;
  const AttendanceOwnerScanBeginRef ownerScanBeginLinkProbe =
      keepAttendanceOwnerScanBegin;
  volatile bool ownerScanBeginLinkProbeValid =
      ownerScanBeginLinkProbe != nullptr;
  (void)ownerScanBeginLinkProbeValid;
  const AttendanceOwnerScanStepRef ownerScanStepLinkProbe =
      keepAttendanceOwnerScanStep;
  volatile bool ownerScanStepLinkProbeValid =
      ownerScanStepLinkProbe != nullptr;
  (void)ownerScanStepLinkProbeValid;
  const AttendanceOwnerConfigCheckRef ownerConfigLinkProbe =
      keepAttendanceOwnerConfigCheck;
  volatile bool ownerConfigLinkProbeValid = ownerConfigLinkProbe != nullptr;
  (void)ownerConfigLinkProbeValid;
  const AttendanceGateStartupReadyRef gateStartupLinkProbe =
      keepAttendanceGateStartupReady;
  volatile bool gateStartupLinkProbeValid = gateStartupLinkProbe != nullptr;
  (void)gateStartupLinkProbeValid;
  const AttendanceGateAttendanceReadyRef gateAttendanceLinkProbe =
      keepAttendanceGateAttendanceReady;
  volatile bool gateAttendanceLinkProbeValid =
      gateAttendanceLinkProbe != nullptr;
  (void)gateAttendanceLinkProbeValid;
  const AttendanceGateConfigMatchRef gateConfigLinkProbe =
      keepAttendanceGateConfigMatch;
  volatile bool gateConfigLinkProbeValid = gateConfigLinkProbe != nullptr;
  (void)gateConfigLinkProbeValid;
  const AttendanceGateClosureMatchRef gateClosureLinkProbe =
      keepAttendanceGateClosureMatch;
  volatile bool gateClosureLinkProbeValid = gateClosureLinkProbe != nullptr;
  (void)gateClosureLinkProbeValid;
  const OwnerConfigMethodLinkProbe ownerConfigCanApplyLinkProbe =
      keepOwnerConfigCanApply;
  volatile bool ownerConfigCanApplyLinkProbeValid =
      ownerConfigCanApplyLinkProbe != nullptr;
  (void)ownerConfigCanApplyLinkProbeValid;
  const OwnerConfigMethodLinkProbe ownerConfigApplyLinkProbe =
      keepOwnerConfigApply;
  volatile bool ownerConfigApplyLinkProbeValid =
      ownerConfigApplyLinkProbe != nullptr;
  (void)ownerConfigApplyLinkProbeValid;
  (void)keepRosterAuditConsume;
  (void)keepFileSetCount;
  (void)keepFileSetMonthKey;
  (void)keepFileSetFileSize;
  (void)keepFileSetRead;
  (void)keepFileSetAppend;
  (void)keepFileSetAdd;
  (void)keepFileSetTruncate;
  (void)keepFileSetQuarantineCopy;
  (void)keepFileSetQuarantineFrom;
  (void)keepLittleFsMount;
  (void)keepLittleFsUnmount;
  // Discarded-value reads of these member-function references were removed
  // by section garbage collection. Copy each volatile pointer into a live
  // probe so the final ELF retains both owner methods and their relocations.
  const StorageOwnerBeginRef ownerBeginLinkProbe = keepLittleFsOwnerBegin;
  volatile bool ownerBeginLinkProbeValid = ownerBeginLinkProbe != nullptr;
  (void)ownerBeginLinkProbeValid;
  const StorageOwnerFileSetRef ownerFileSetLinkProbe = keepLittleFsOwnerFileSet;
  volatile bool ownerFileSetLinkProbeValid = ownerFileSetLinkProbe != nullptr;
  (void)ownerFileSetLinkProbeValid;
  (void)keepRuntimeAttendance;
  (void)keepRuntimeEnrollment;
  (void)keepRuntimeFaulted;
  (void)keepRuntimeOccupancyStore;
  (void)keepStartupBegin;
  (void)keepStartupStep;
  (void)keepLinkStart;
  (void)keepLinkPoll;
  (void)keepSensorAuthenticate;
  (void)keepSensorCapacity;
  (void)keepSensorIndex;
  (void)keepSensorCapture;
  (void)keepSensorConvert;
  (void)keepSensorSearch;
  (void)keepSensorCreate;
  (void)keepSensorStore;
  (void)keepOccupancyBegin;
  (void)keepOccupancyStep;
  (void)keepAttendanceSetupBegin;
  (void)keepAttendanceSetupStep;
  (void)keepAttendanceScanBegin;
  (void)keepAttendanceScanStep;
  (void)keepEnrollmentAcquire;
  (void)keepEnrollmentSync;
  (void)keepEnrollmentAbort;
  (void)keepEnrollBegin;
  (void)keepEnrollStep;
  (void)keepEnrollCancel;
  (void)keepNvsFlashErase;
  (void)keepNvsFlashErasePartition;
  (void)keepNvsFlashErasePartitionPtr;
}

void loop() {}
