#include <Arduino.h>
#include <WebServer.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <new>
#include <stdio.h>
#include <Wire.h>
#include <string.h>

#include "ap_credential_bootstrap.h"
#include "ap_credential_setup.h"
#include "setup_code_sequence.h"
#include "admin_session.h"
#include "config_boot.h"
#include "esp32_bootstrap_random.h"
#include "esp32_i2c_bus_owner.h"
#include "esp32_ds3231_register_io.h"
#include "ds3231_clock.h"
#include "time_rules.h"
#include "month_set.h"
#include "log_store.h"
#include "event_group_audit.h"
#include "roster_store.h"
#include "as608_attendance_pipeline.h"
#include "health_fsm.h"
#include "config_runtime_guard.h"
#include "esp32_littlefs_mount.h"
#include "esp32_nvs_startup_guard.h"
#include "esp32_oled.h"
#include "esp32_pin_auth_runtime.h"
#include "esp32_clock_monotonic.h"
#include "esp32_ram_setup_code_verifier.h"
#include "esp32_mbedtls_pin_config_runtime.h"
#include "esp32_mbedtls_pin_writer.h"
#include "esp32_secure_random.h"
#include "esp32_wifi_ap.h"
#include "nvs_ap_credential_store.h"
#include "nvs_config_backend.h"
#include "littlefs_mount_owner.h"

namespace {

const uint32_t kI2cClockHz = 100000UL;
const uint8_t kSdaPin = 21;
const uint8_t kSclPin = 22;
const char kLittleFsMountPoint[] = "/littlefs";

enum RtcStartupObservation : uint8_t {
  RTC_STARTUP_NOT_CHECKED = 0,
  RTC_STARTUP_BUS_UNAVAILABLE,
  RTC_STARTUP_READ_FAILED,
  RTC_STARTUP_INVALID_CALENDAR,
  RTC_STARTUP_OSF_SET,
  RTC_STARTUP_BELOW_FLOOR,
  RTC_STARTUP_BELOW_LOG,
  RTC_STARTUP_CANDIDATE_UNTRUSTED
};

const char kNeedsSetupHtml[] PROGMEM =
    "<!doctype html><html lang='en'><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='robots' content='noindex,nofollow'>"
    "<title>ATT setup</title><style>"
    "body{font:16px system-ui,sans-serif;margin:0;background:#f3f5f7;color:#17212b}"
    "main{max-width:560px;margin:10vh auto;padding:24px;background:#fff;border-radius:14px}"
    "h1{margin-top:0} .state{font-weight:700;color:#875400}"
    "p{line-height:1.5} small{color:#596673}</style><main>"
    "<h1>ATT</h1><p class='state'>Initial setup required</p>"
    "<p>The device has started its protected setup network. Use the Wi-Fi "
    "credential shown on the device OLED.</p>"
    "<p>Configuration changes are not enabled in this build. No attendance "
    "is being recorded.</p><p><small>Read-only system status.</small></p>"
    "<p><a href='/api/status'>Device status</a></p></main></html>";

const char kConfiguredHtml[] PROGMEM =
    "<!doctype html><html lang='en'><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='robots' content='noindex,nofollow'>"
    "<title>ATT status</title><style>"
    "body{font:16px system-ui,sans-serif;margin:0;background:#f3f5f7;color:#17212b}"
    "main{max-width:560px;margin:10vh auto;padding:24px;background:#fff;border-radius:14px}"
    "h1{margin-top:0} .state{font-weight:700;color:#17623b}"
    "p{line-height:1.5} small{color:#596673}</style><main>"
    "<h1>ATT</h1><p class='state'>Configuration loaded</p>"
    "<p>The stored configuration passed boot validation. Event groups were "
    "structurally checked, roster history was replayed and the log-backed "
    "attendance-admission observer was attached. Attendance remains disabled.</p>"
    "<p>The DS3231 candidate is UTC. Storage durability has not been certified. "
    "The RTC observation is "
    "diagnostic only and does not establish trusted time. Administrator "
    "authentication, attendance scanning, changes and SF2 export remain "
    "disabled in this boot-shell build.</p>"
    "<p><small>Read-only system status.</small></p>"
    "<p><a href='/api/status'>Device status</a></p></main></html>";

const char kNeedsSetupJson[] PROGMEM =
    "{\"boot\":\"needs_setup\",\"ap\":\"ready\","
    "\"storage\":\"not_started\",\"rtc_observation\":\"not_checked\","
    "\"clock_trusted\":false,\"attendance_enabled\":false,"
    "\"admin_routes_enabled\":false,\"config_provisioning_enabled\":false,"
    "\"sf2_export_enabled\":false}";

class Esp32ApplicationRestart : public att::IApplicationRestart {
 public:
  void restartAfterProvisioning() override { esp_restart(); }
};

struct Application {
  att::Esp32NvsConfigBackend configBackend;
  att::ConfigBootCoordinator configBoot;
  att::Esp32PinAuthRuntime pinAuthRuntime;
  att::Esp32PinAuthRuntime setupCodeAuthRuntime;
  att::Esp32NvsApCredentialStore apCredentialStore;
  att::Esp32BootstrapRandom bootstrapRandom;
  att::ApCredentialBootstrap apCredentialBootstrap;
  att::Esp32SecureRandom runtimeRandom;
  att::Esp32MbedTlsInitialPinPreparer initialPinPreparer;
  att::Esp32WifiAp wifiAp;
  att::Esp32I2cBusOwner i2cBus;
  att::Esp32Ds3231RegisterIo rtcRegisterIo;
  att::Ds3231Clock rtcClock;
  att::Esp32Oled oled;
  att::Esp32ClockMonotonicSource monotonicClock;
  att::Esp32RamSetupCodeVerifier setupCodeVerifier;
  alignas(att::Esp32MbedTlsPinConfigRuntime)
      uint8_t pinConfigRuntimeMemory[sizeof(att::Esp32MbedTlsPinConfigRuntime)];
  att::Esp32MbedTlsPinConfigRuntime* pinConfigRuntime;
  alignas(att::AdminSessionBoundary)
      uint8_t adminSessionsMemory[sizeof(att::AdminSessionBoundary)];
  att::AdminSessionBoundary* adminSessions;
  alignas(att::AdminSetupCodeBoundary)
      uint8_t adminSetupCodeBoundaryMemory[sizeof(att::AdminSetupCodeBoundary)];
  att::AdminSetupCodeBoundary* adminSetupCodeBoundary;
  att::ApCredentialSetupCoordinator apSetup;
  alignas(att::FirstRunSetupCodeSequence)
      uint8_t firstRunSequenceMemory[sizeof(att::FirstRunSetupCodeSequence)];
  att::FirstRunSetupCodeSequence* firstRunSequence;
  alignas(att::FirstRunSetupCodeBoundary)
      uint8_t firstRunSetupBoundaryMemory[sizeof(att::FirstRunSetupCodeBoundary)];
  att::FirstRunSetupCodeBoundary* firstRunSetupBoundary;
  att::FirstRunProvisioningRestartGate firstRunRestart;
  Esp32ApplicationRestart applicationRestart;
  att::Esp32LittleFsMountBackend storageMountBackend;
  alignas(att::LittleFsStorageOwner)
      uint8_t storageOwnerMemory[sizeof(att::LittleFsStorageOwner)];
  att::LittleFsStorageOwner* storageOwner;
  alignas(att::MonthSet) uint8_t monthSetMemory[sizeof(att::MonthSet)];
  att::MonthSet* months;
  alignas(att::LogStore) uint8_t logStoreMemory[sizeof(att::LogStore)];
  att::LogStore* log;
  att::EventGroupAudit eventGroupAudit;
  alignas(att::RosterStore)
      uint8_t rosterMemory[sizeof(att::RosterStore)];
  att::RosterStore* roster;
  alignas(att::LogAttendanceAdmission)
      uint8_t attendanceAdmissionMemory[sizeof(att::LogAttendanceAdmission)];
  att::LogAttendanceAdmission* attendanceAdmission;
  att::HealthFsm health;
  WebServer http;
  att::ConfigBootState bootState;
  bool oledReady;
  bool apReady;
  bool httpReady;
  bool storageReady;
  bool eventGroupAuditReady;
  bool rosterHistoryReady;
  bool attendanceAdmissionAttached;
  RtcStartupObservation rtcObservation;
  bool rtcPresent;
  bool rtcLostPower;
  bool rtcCalendarValid;
  uint32_t rtcEpochUtc;
  bool logReplayReady;
  int64_t nextProvisionClearRetryUs;
  uint32_t logRecordCount;
  uint32_t logBaselineEpoch;
  bool faulted;

  Application()
      : configBoot(configBackend),
        pinAuthRuntime("attsec", configBoot),
        setupCodeAuthRuntime("attsetup", configBoot),
        apCredentialStore(configBoot),
        apCredentialBootstrap(apCredentialStore, bootstrapRandom),
        initialPinPreparer(configBoot, runtimeRandom),
        wifiAp(runtimeRandom),
        i2cBus(Wire),
        rtcRegisterIo(Wire, i2cBus.lock()),
        rtcClock(rtcRegisterIo),
        oled(Wire, i2cBus.lock()),
        monotonicClock(),
        setupCodeVerifier(runtimeRandom, monotonicClock, oled),
        pinConfigRuntime(nullptr),
        adminSessions(nullptr),
        adminSetupCodeBoundary(nullptr),
        apSetup(configBoot, apCredentialBootstrap, oled, wifiAp),
        firstRunSequence(nullptr),
        firstRunSetupBoundary(nullptr),
        storageOwner(nullptr),
        months(nullptr),
        log(nullptr),
        eventGroupAudit(),
        roster(nullptr),
        attendanceAdmission(nullptr),
        http(80),
        bootState(att::CFG_BOOT_COLD),
        oledReady(false),
        apReady(false),
        httpReady(false),
        storageReady(false),
        eventGroupAuditReady(false),
        rosterHistoryReady(false),
        attendanceAdmissionAttached(false),
        rtcObservation(RTC_STARTUP_NOT_CHECKED),
        rtcPresent(false),
        rtcLostPower(false),
        rtcCalendarValid(false),
        rtcEpochUtc(0),
        logReplayReady(false),
        nextProvisionClearRetryUs(0),
        logRecordCount(0),
        logBaselineEpoch(0),
        faulted(false) {}
};

const char* rtcStartupObservationText(RtcStartupObservation observation) {
  switch (observation) {
    case RTC_STARTUP_NOT_CHECKED: return "not_checked";
    case RTC_STARTUP_BUS_UNAVAILABLE: return "bus_unavailable";
    case RTC_STARTUP_READ_FAILED: return "read_failed";
    case RTC_STARTUP_INVALID_CALENDAR: return "invalid_calendar";
    case RTC_STARTUP_OSF_SET: return "oscillator_stopped";
    case RTC_STARTUP_BELOW_FLOOR: return "below_minimum_epoch";
    case RTC_STARTUP_BELOW_LOG: return "below_log_baseline";
    case RTC_STARTUP_CANDIDATE_UNTRUSTED:
      return "utc_candidate_untrusted";
    default: return "unknown";
  }
}

Application* gApp = nullptr;

void secureClear(void* value, size_t length) {
  volatile uint8_t* cursor = static_cast<volatile uint8_t*>(value);
  while (length-- > 0) *cursor++ = 0;
}

uint32_t monotonicSeconds() {
  const int64_t elapsedUs = esp_timer_get_time();
  if (elapsedUs <= 0) return 0;
  return static_cast<uint32_t>(
      static_cast<uint64_t>(elapsedUs) / 1000000ULL);
}

void observeRtcStartup(Application& app) {
  if (app.bootState != att::CFG_BOOT_READY) return;
  if (!app.i2cBus.ready()) {
    app.rtcObservation = RTC_STARTUP_BUS_UNAVAILABLE;
    return;
  }

  // E32: DS3231 calendar registers store UTC. Offset zero is mandatory here;
  // PHT conversion stays at the application boundary through Config.params.
  att::Ds3231Reading reading = {0, false};
  const att::Ds3231Code result = app.rtcClock.readUtcEpoch(0, reading);
  if (result == att::DS3231_IO_ERROR) {
    app.rtcObservation = RTC_STARTUP_READ_FAILED;
    return;
  }

  // INVALID_TIME is returned only after both register reads succeeded, so the
  // RTC answered even though its calendar fields cannot be accepted.
  app.rtcPresent = true;
  if (result == att::DS3231_INVALID_TIME) {
    app.rtcObservation = RTC_STARTUP_INVALID_CALENDAR;
    return;
  }
  if (result != att::DS3231_OK) {
    app.rtcObservation = RTC_STARTUP_READ_FAILED;
    app.rtcPresent = false;
    return;
  }

  app.rtcCalendarValid = true;
  app.rtcLostPower = reading.oscillatorStopped;
  app.rtcEpochUtc = reading.epochUtc;
  if (reading.oscillatorStopped) {
    app.rtcObservation = RTC_STARTUP_OSF_SET;
  } else if (reading.epochUtc < att::EPOCH_2020) {
    app.rtcObservation = RTC_STARTUP_BELOW_FLOOR;
  } else {
    // A plausible UTC candidate is still untrusted until it is reconciled with
    // the replayed log baseline and the explicit clock-confirmation workflow.
    app.rtcObservation = RTC_STARTUP_CANDIDATE_UNTRUSTED;
  }
}

const char* healthStateText(att::HState state) {
  switch (state) {
    case att::H_OK: return "ok";
    case att::H_UNPROVISIONED: return "unprovisioned";
    case att::H_CLOCK_FAULT: return "clock_fault";
    case att::H_STORAGE_FAULT: return "storage_fault";
    case att::H_CONFIG_FAULT: return "config_fault";
    case att::H_SENSOR_FAULT: return "sensor_fault";
    default: return "unknown";
  }
}

void sendConfiguredStatus(Application& app) {
  char payload[512] = {};
  const int length = snprintf(
      payload, sizeof payload,
      "{\"boot\":\"ready\",\"ap\":\"ready\","
      "\"storage\":\"layout_validated\",\"log_replay\":\"%s\","
      "\"log_records\":%lu,\"log_baseline_utc\":%lu,\"event_group_audit\":\"%s\",\"roster_history\":\"%s\",\"attendance_admission\":\"%s\","
      "\"rtc_observation\":\"%s\",\"rtc_candidate_utc\":%lu,"
      "\"health\":\"%s\",\"clock_trusted\":false,"
      "\"attendance_enabled\":false,\"admin_routes_enabled\":false,"
      "\"config_provisioning_enabled\":false,\"sf2_export_enabled\":false}",
      app.logReplayReady ? "ok" : "fault",
      static_cast<unsigned long>(app.logRecordCount),
      static_cast<unsigned long>(app.logBaselineEpoch),
      app.eventGroupAuditReady ? "ok" : "fault",
      app.rosterHistoryReady ? "ok" : "fault",
      app.attendanceAdmissionAttached ? "attached" : "fault",
      rtcStartupObservationText(app.rtcObservation),
      static_cast<unsigned long>(app.rtcEpochUtc),
      healthStateText(app.health.state()));
  if (length < 0 || static_cast<size_t>(length) >= sizeof payload) {
    app.http.send(500, "application/json", "{}");
    return;
  }
  app.http.send(200, "application/json", payload);
}

void addReadOnlyHeaders(WebServer& server) {
  server.sendHeader("Cache-Control", "no-store, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.sendHeader(
      "Content-Security-Policy",
      "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; "
      "form-action 'none'; frame-ancestors 'none'");
}

void registerReadOnlyRoutes(Application& app) {
  app.http.on("/", HTTP_GET, [&app]() {
    addReadOnlyHeaders(app.http);

    if (app.bootState == att::CFG_BOOT_NEEDS_SETUP) {
      app.http.send_P(200, "text/html; charset=utf-8", kNeedsSetupHtml);
    } else {
      app.http.send_P(200, "text/html; charset=utf-8", kConfiguredHtml);
    }
  });

  app.http.on("/api/status", HTTP_GET, [&app]() {
    addReadOnlyHeaders(app.http);
    if (app.bootState == att::CFG_BOOT_NEEDS_SETUP) {
      app.http.send_P(200, "application/json", kNeedsSetupJson);
    } else {
      sendConfiguredStatus(app);
    }
  });

  app.http.onNotFound([&app]() {
    addReadOnlyHeaders(app.http);
    if (app.http.method() != HTTP_GET) {
      app.http.send(405, "text/plain; charset=utf-8", "Method not allowed");
    } else {
      app.http.send(404, "text/plain; charset=utf-8", "Not found");
    }
  });

  app.http.begin();
  app.httpReady = true;
}

bool startStorage(Application& app) {
  const att::Config* config = app.configBoot.config();
  if (app.storageReady && app.storageOwner != nullptr &&
      app.months != nullptr && app.log != nullptr && app.logReplayReady &&
      app.eventGroupAuditReady && app.rosterHistoryReady &&
      app.attendanceAdmissionAttached)
    return true;
  if (config == nullptr || app.storageOwner != nullptr ||
      app.months != nullptr || app.log != nullptr) return false;

  // Placement construction defers the timezone-dependent owner until after
  // validated config boot. The owner and its read-only mount live for the
  // lifetime of the static Application object.
  app.storageOwner = new (app.storageOwnerMemory) att::LittleFsStorageOwner(
      app.storageMountBackend, kLittleFsMountPoint,
      att::kSignedLittleFsPartitionLabel, config->params.utcOffsetMin);
  if (!app.storageOwner->begin()) return false;

  IFileSet* files = app.storageOwner->fileSet();
  if (app.storageOwner->state() != att::LITTLEFS_OWNER_READY || !files)
    return false;

  // This MonthSet/LogStore path is replay-only in the boot shell. Never call
  // setMonth(), append(), truncate(), planAt() or repair() from this target.
  app.months = new (app.monthSetMemory) att::MonthSet(*files);
  app.log = new (app.logStoreMemory) att::LogStore(*app.months);
  const att::OpenInfo opened = app.log->open();
  if (!opened.ok || app.log->faulted()) return false;
  app.logReplayReady = true;
  app.logRecordCount = opened.records;
  app.logBaselineEpoch = opened.baselineEpoch;

  // Structural event-group validation is separate from CRC/sequence replay.
  if (!app.eventGroupAudit.attach(*app.log) ||
      !app.eventGroupAudit.readyFor(*app.log, opened) ||
      !app.log->hasObserver(&app.eventGroupAudit)) return false;
  app.eventGroupAuditReady = true;

  // Rebuild the operational roster from the same authoritative log prefix.
  // Do not run RosterTransitionAudit here: E27 is a monthly-reporting guard,
  // not a reason to reject operational same-day lifecycle history.
  app.roster = new (app.rosterMemory) att::RosterStore(config->params.utcOffsetMin);
  if (!app.log->attach(app.roster) ||
      !app.log->hasObserver(app.roster) ||
      !app.roster->historyComplete()) return false;
  app.rosterHistoryReady = true;

  // Attach the log-backed no-class/closed-session admission projection now,
  // before any future scan owner can be constructed. It remains read-only here.
  app.attendanceAdmission = new (app.attendanceAdmissionMemory)
      att::LogAttendanceAdmission(*app.log, config->params);
  if (!app.attendanceAdmission->attached() ||
      !app.attendanceAdmission->usesLog(*app.log) ||
      !app.attendanceAdmission->usesParams(config->params) ||
      !app.log->hasObserver(app.attendanceAdmission) ||
      !app.eventGroupAudit.readyFor(*app.log, opened)) return false;
  app.attendanceAdmissionAttached = true;

  app.storageReady = true;
  return true;
}

void updateRuntimeHealth(Application& app) {
  const bool setupRequired = app.bootState == att::CFG_BOOT_NEEDS_SETUP;
  const bool storageOk = setupRequired ||
      (app.storageReady && app.logReplayReady && app.log != nullptr &&
       app.eventGroupAuditReady && app.rosterHistoryReady &&
       app.attendanceAdmissionAttached && app.roster != nullptr &&
       !app.log->faulted());
  const bool epochBelowLog = app.rtcCalendarValid && app.logReplayReady &&
      app.logBaselineEpoch != 0 && app.rtcEpochUtc < app.logBaselineEpoch;
  const bool epochBelowFloor = !app.rtcCalendarValid ||
      app.rtcEpochUtc < att::EPOCH_2020;

  att::HealthInputs inputs = {
      true,
      app.rtcPresent,
      app.rtcLostPower,
      epochBelowFloor,
      epochBelowLog,
      storageOk,
      true,
      false,  // sensorOk: no sensor owner is composed in this boot shell
      false,  // reverified: no explicit verification workflow ran
      false}; // clockTrusted: no accepted clock-confirmation workflow exists
  (void)att::updateHealthFromConfigBoot(app.health, app.configBoot, inputs);

  if (epochBelowLog && !app.rtcLostPower)
    app.rtcObservation = RTC_STARTUP_BELOW_LOG;
}

bool startConfiguredAp(Application& app) {
  const att::Config* config = app.configBoot.config();
  if (config == nullptr) return false;

  char passphrase[att::AP_CREDENTIAL_BUFFER_SIZE] = {};
  const att::ApCredentialResult credential =
      app.apCredentialBootstrap.prepare(att::CFG_BOOT_READY,
                                        config->wifiPass, passphrase);
  if (credential.code != att::APCB_OK || credential.disclose) {
    secureClear(passphrase, sizeof passphrase);
    return false;
  }

  const size_t length = strlen(passphrase);
  const bool started = app.wifiAp.start(
      att::kProductionSoftApSsid,
      sizeof(att::kProductionSoftApSsid) - 1,
      passphrase, length, att::kProductionSoftApChannel);
  secureClear(passphrase, sizeof passphrase);
  return started && app.wifiAp.ready();
}

void startApplication(Application& app) {
  const att::ConfigBootResult boot = app.configBoot.start();
  app.bootState = boot.state;
  if (boot.state != att::CFG_BOOT_NEEDS_SETUP &&
      boot.state != att::CFG_BOOT_READY) {
    app.faulted = true;
    return;
  }

  // E35: construct the live salted PBKDF2 verifier only from the exact
  // validated READY config. NEEDS_SETUP has no admin PIN credential yet.
  if (app.bootState == att::CFG_BOOT_READY) {
    app.pinConfigRuntime = new (app.pinConfigRuntimeMemory)
        att::Esp32MbedTlsPinConfigRuntime(
            app.configBoot, att::kE35AdminPinPbkdf2Iterations);
    if (!app.pinConfigRuntime->ready() ||
        app.pinConfigRuntime->verifier() == nullptr) {
      app.faulted = true;
      return;
    }
  }

  const bool busReady = app.i2cBus.begin(kSdaPin, kSclPin, kI2cClockHz);
  if (busReady) {
    const att::Esp32OledInitCode oledResult = app.oled.begin();
    app.oledReady = oledResult == att::EOI_BUFFER_READY ||
                    oledResult == att::EOI_ALREADY_INITIALIZED;
  }

  // READY boot only: read the DS3231 UTC calendar and OSF without changing
  // RTC state. This is diagnostic only; no trusted clock is established.
  observeRtcStartup(app);

  // Load both durable lockout domains before exposing either AP state.
  // NEEDS_SETUP may initialize an absent counter in each distinct namespace.
  // READY must find attsec for PIN auth; any P15 action separately requires
  // an already-present, ready attsetup runtime and may never auto-create it.
  if (!app.pinAuthRuntime.start(monotonicSeconds())) {
    if (app.oledReady) (void)app.oled.showAuthFault();
    app.faulted = true;
    return;
  }

  // E33: P15 uses its own attsetup counter, never the attsec PIN counter.
  // E36: a READY device missing this namespace leaves the P15 runtime faulted;
  // no setup-code action may use it. Keep the read-only status AP available:
  // migration is a separate supervised USB operation, not ordinary boot.
  // This shell has no setup-code issue/verify route; failure is scoped to P15
  // and does not silently initialize, reset or erase READY state.
  (void)app.setupCodeAuthRuntime.start(monotonicSeconds());

  // NEEDS_SETUP uses a distinct session-free EP_SETUP boundary. The AP may
  // remain status-only if attsetup is unavailable; do not expose code actions.
  if (app.bootState == att::CFG_BOOT_NEEDS_SETUP) {
    app.firstRunSequence = new (app.firstRunSequenceMemory)
        att::FirstRunSetupCodeSequence(app.apSetup, app.setupCodeVerifier);
    if (app.setupCodeAuthRuntime.ready()) {
      att::PinAuthCoordinator* setupAuth =
          app.setupCodeAuthRuntime.coordinator();
      if (setupAuth == nullptr) {
        app.faulted = true;
        return;
      }
      app.firstRunSetupBoundary = new (app.firstRunSetupBoundaryMemory)
          att::FirstRunSetupCodeBoundary(
              *setupAuth, app.setupCodeVerifier, *app.firstRunSequence,
              app.initialPinPreparer);
    }
  }

  // Bind the READY-only request/session graph to the exact long-lived owners.
  // The PIN session boundary can exist only after the durable attsec runtime
  // and the live PBKDF2 verifier are both ready. P15 gets a boundary only when
  // the separate attsetup runtime also loaded successfully. No route is exposed
  // by this boot shell, and NEEDS_SETUP has no administrator PIN verifier yet.
  if (app.bootState == att::CFG_BOOT_READY) {
    att::PinAuthCoordinator* pinAuth = app.pinAuthRuntime.coordinator();
    att::Esp32MbedTlsPinVerifier* pinVerifier =
        app.pinConfigRuntime != nullptr
            ? app.pinConfigRuntime->verifier() : nullptr;
    if (pinAuth == nullptr || pinVerifier == nullptr) {
      app.faulted = true;
      return;
    }
    app.adminSessions = new (app.adminSessionsMemory)
        att::AdminSessionBoundary(*pinAuth, *pinVerifier, app.runtimeRandom);

    if (app.setupCodeAuthRuntime.ready() &&
        !app.setupCodeVerifier.faulted()) {
      att::PinAuthCoordinator* setupAuth =
          app.setupCodeAuthRuntime.coordinator();
      if (setupAuth == nullptr) {
        app.faulted = true;
        return;
      }
      app.adminSetupCodeBoundary = new (app.adminSetupCodeBoundaryMemory)
          att::AdminSetupCodeBoundary(
              *app.adminSessions, *setupAuth, app.setupCodeVerifier);
    }
  }

  // Storage is started only for a validated existing configuration. A
  // partition-layout, mount or read-only log-validation failure blocks AP
  // and HTTP startup. NEEDS_SETUP never touches the attendance partition.
  if (app.bootState == att::CFG_BOOT_READY && !startStorage(app)) {
    updateRuntimeHealth(app);
    if (app.oledReady) (void)app.oled.showStorageFault();
    app.faulted = true;
    return;
  }

  // NEEDS_SETUP treats attendance storage as not applicable. READY reaches
  // this point only after the no-write MonthSet/LogStore replay succeeded.
  updateRuntimeHealth(app);

  if (app.bootState == att::CFG_BOOT_NEEDS_SETUP) {
    if (!busReady || !app.oledReady) {
      app.faulted = true;
      return;
    }
    if (app.firstRunSequence == nullptr ||
        app.firstRunSequence->start() != att::FSC_WAITING_FOR_CLIENT) {
      app.faulted = true;
      return;
    }
    app.apReady = app.wifiAp.ready();
  } else {
    app.apReady = startConfiguredAp(app);
    if (!app.apReady) {
      app.faulted = true;
      return;
    }
  }

  if (app.apReady) registerReadOnlyRoutes(app);
}

}  // namespace

void setup() {
  const att::Esp32NvsStartupStatus nvs =
      att::Esp32NvsStartupGuard::status();
  if (!nvs.safeForApplication) return;

  static Application application;
  gApp = &application;
  startApplication(application);
}

void loop() {
  if (gApp != nullptr) {
    // E33: service RAM-only setup-code expiry on the same serialized owner
    // before a request can verify it and again after synchronous HTTP returns.
    // First-run issue/reissue is advanced below only after the DHCP latch.
    if (!gApp->setupCodeVerifier.serviceExpiry()) {
      gApp->faulted = true;
    }
    // First-run issuance is advanced only by the serialized owner. The
    // sequence checks the AP's own latched DHCP assignment before display.
    if (!gApp->faulted &&
        gApp->bootState == att::CFG_BOOT_NEEDS_SETUP &&
        gApp->firstRunSequence != nullptr &&
        gApp->firstRunSetupBoundary != nullptr &&
        !gApp->firstRunSetupBoundary->faulted() &&
        !gApp->firstRunSetupBoundary->completed() &&
        !gApp->setupCodeVerifier.faulted()) {
      (void)gApp->firstRunSequence->issueAfterClientAssigned();
    }
    if (gApp->httpReady && gApp->apReady && !gApp->faulted) {
      gApp->http.handleClient();
      if (!gApp->setupCodeVerifier.serviceExpiry()) {
        gApp->faulted = true;
      }
    }
    // A committed config is not enough: never reboot while the OLED may
    // still show the AP credential. Retry only the blank transfer, throttled.
    if (gApp->firstRunSetupBoundary != nullptr &&
        gApp->firstRunSetupBoundary->completed() &&
        !gApp->firstRunSetupBoundary->restartReady() &&
        gApp->firstRunSequence != nullptr) {
      const int64_t nowUs = esp_timer_get_time();
      if (nowUs >= gApp->nextProvisionClearRetryUs) {
        gApp->nextProvisionClearRetryUs = nowUs + 1000000LL;
        (void)gApp->firstRunSequence->retryBlankFrame();
      }
    }
    // Run only after handleClient() returns: the HTTP response has left its
    // handler and the OLED blank transfer has completed.
    if (gApp->firstRunSetupBoundary != nullptr &&
        gApp->firstRunRestart.service(*gApp->firstRunSetupBoundary,
                                      gApp->applicationRestart)) {
      gApp->faulted = true;
    }
  }
  delay(2);
}
