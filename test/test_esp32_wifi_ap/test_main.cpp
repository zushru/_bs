#include <unity.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <esp_random.h>
#include "esp32_wifi_ap.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

using namespace att;

enum Call : uint8_t {
  CALL_NETIF_INIT = 1,
  CALL_EVENT_LOOP,
  CALL_NETIF_NEW,
  CALL_NETIF_ATTACH,
  CALL_DEFAULT_HANDLERS,
  CALL_WIFI_INIT,
  CALL_WIFI_STORAGE,
  CALL_WIFI_MODE,
  CALL_WIFI_CONFIG,
  CALL_HANDLER_REGISTER,
  CALL_WIFI_START,
  CALL_WIFI_STOP,
  CALL_HANDLER_UNREGISTER,
  CALL_WIFI_DEINIT,
  CALL_NETIF_CLEAR,
  CALL_NETIF_DESTROY
};

struct WifiFakeState {
  esp_err_t netifInitResult;
  esp_err_t eventLoopResult;
  bool createNetifResult;
  esp_err_t netifAttachResult;
  esp_err_t defaultHandlersResult;
  esp_err_t wifiInitResult;
  esp_err_t storageResult;
  esp_err_t modeResult;
  esp_err_t configResult;
  esp_err_t handlerRegisterResult;
  esp_err_t ipHandlerRegisterResult;
  esp_err_t wifiStartResult;
  esp_err_t wifiStopResult;
  esp_err_t handlerUnregisterResult;
  esp_err_t wifiDeinitResult;
  esp_err_t netifClearResult;
  bool emitApStart;
  int netifInitCalls;
  int eventLoopCalls;
  int netifNewCalls;
  int netifAttachCalls;
  int defaultHandlersCalls;
  int netifDestroyDefaultCalls;
  int netifDestroyCalls;
  int clearDefaultCalls;
  int wifiInitCalls;
  int storageCalls;
  int modeCalls;
  int configCalls;
  int handlerRegisterCalls;
  int wifiStartCalls;
  int wifiStopCalls;
  int handlerUnregisterCalls;
  int wifiDeinitCalls;
  int waitCalls;
  int storageMode;
  int wifiMode;
  int wifiInterface;
  wifi_config_t config;
  esp_netif_config_t netifConfig;
  bool randomReadyDuringConfig;
  bool randomReadyDuringStart;
  bool randomReadyDuringStop;
  bool eventGroupDeleted;
  EventGroupHandle_t startEvents;
  Call calls[40];
  size_t callCount;
  WifiFakeState()
      : netifInitResult(ESP_OK), eventLoopResult(ESP_OK), createNetifResult(true),
        netifAttachResult(ESP_OK), defaultHandlersResult(ESP_OK),
        wifiInitResult(ESP_OK), storageResult(ESP_OK), modeResult(ESP_OK),
        configResult(ESP_OK), handlerRegisterResult(ESP_OK),
        ipHandlerRegisterResult(ESP_OK), wifiStartResult(ESP_OK), wifiStopResult(ESP_OK),
        handlerUnregisterResult(ESP_OK), wifiDeinitResult(ESP_OK),
        netifClearResult(ESP_OK),
        emitApStart(true), netifInitCalls(0), eventLoopCalls(0),
        netifNewCalls(0), netifAttachCalls(0), defaultHandlersCalls(0),
        netifDestroyDefaultCalls(0), netifDestroyCalls(0), clearDefaultCalls(0),
        wifiInitCalls(0), storageCalls(0),
        modeCalls(0), configCalls(0), handlerRegisterCalls(0),
        wifiStartCalls(0), wifiStopCalls(0), handlerUnregisterCalls(0),
        wifiDeinitCalls(0), waitCalls(0), storageMode(-1), wifiMode(-1),
        wifiInterface(-1), config{}, netifConfig{}, randomReadyDuringConfig(false),
        randomReadyDuringStart(false), randomReadyDuringStop(false),
        eventGroupDeleted(false), startEvents(nullptr), calls{}, callCount(0) {}
  void record(Call call) {
    if (callCount < sizeof calls / sizeof calls[0]) calls[callCount++] = call;
  }
  int indexOf(Call call) const {
    for (size_t i = 0; i < callCount; ++i) {
      if (calls[i] == call) return static_cast<int>(i);
    }
    return -1;
  }
};

static WifiFakeState gWifi;
static Esp32SecureRandom* gRandom = nullptr;
static esp_event_handler_t gHandler = nullptr;
static void* gHandlerArg = nullptr;
static int32_t gHandlerEvent = -1;
static esp_event_handler_t gIpHandler = nullptr;
static void* gIpHandlerArg = nullptr;
static int32_t gIpHandlerEvent = -1;
static esp_netif_t gNetif = {1};
static esp_netif_t gOtherNetif = {2};
const esp_netif_inherent_config_t gEspNetifBaseDefaultAp = {1};
const esp_netif_netstack_config_t gEspNetifNetstackDefaultAp = {1};
esp_event_base_t WIFI_EVENT = "WIFI_EVENT";
esp_event_base_t IP_EVENT = "IP_EVENT";

// This suite links esp32_secure_random.cpp through Esp32WifiAp. PlatformIO
// compiles each test suite as a separate executable, so the RF-RNG suite's
// fake is not visible here. Keep this deterministic implementation local to
// the AP suite; it is a host-test shim, never firmware code.
static int gEspFillRandomCalls = 0;
extern "C" void esp_fill_random(void* buffer, size_t length) {
  ++gEspFillRandomCalls;
  uint8_t* out = static_cast<uint8_t*>(buffer);
  for (size_t i = 0; i < length; ++i) {
    out[i] = static_cast<uint8_t>(0xA0u + i);
  }
}

esp_err_t esp_netif_init(void) {
  ++gWifi.netifInitCalls; gWifi.record(CALL_NETIF_INIT); return gWifi.netifInitResult;
}
esp_err_t esp_event_loop_create_default(void) {
  ++gWifi.eventLoopCalls; gWifi.record(CALL_EVENT_LOOP); return gWifi.eventLoopResult;
}
esp_netif_t* esp_netif_new(const esp_netif_config_t* config) {
  TEST_ASSERT_NOT_NULL(config);
  ++gWifi.netifNewCalls; gWifi.record(CALL_NETIF_NEW);
  if (config != nullptr) gWifi.netifConfig = *config;
  return gWifi.createNetifResult ? &gNetif : nullptr;
}
esp_err_t esp_netif_attach_wifi_ap(esp_netif_t* netif) {
  TEST_ASSERT_EQUAL_PTR(&gNetif, netif);
  ++gWifi.netifAttachCalls; gWifi.record(CALL_NETIF_ATTACH);
  return gWifi.netifAttachResult;
}
esp_err_t esp_wifi_set_default_wifi_ap_handlers(void) {
  ++gWifi.defaultHandlersCalls; gWifi.record(CALL_DEFAULT_HANDLERS);
  return gWifi.defaultHandlersResult;
}
esp_err_t esp_wifi_clear_default_wifi_driver_and_handlers(void* netif) {
  TEST_ASSERT_EQUAL_PTR(&gNetif, netif);
  ++gWifi.clearDefaultCalls; gWifi.record(CALL_NETIF_CLEAR);
  return gWifi.netifClearResult;
}
void esp_netif_destroy(esp_netif_t* netif) {
  TEST_ASSERT_EQUAL_PTR(&gNetif, netif);
  ++gWifi.netifDestroyCalls; gWifi.record(CALL_NETIF_DESTROY);
}
esp_err_t esp_wifi_init(const wifi_init_config_t* config) {
  TEST_ASSERT_NOT_NULL(config);
  ++gWifi.wifiInitCalls; gWifi.record(CALL_WIFI_INIT); return gWifi.wifiInitResult;
}
esp_err_t esp_wifi_set_storage(wifi_storage_t storage) {
  ++gWifi.storageCalls; gWifi.storageMode = static_cast<int>(storage);
  gWifi.record(CALL_WIFI_STORAGE); return gWifi.storageResult;
}
esp_err_t esp_wifi_set_mode(wifi_mode_t mode) {
  ++gWifi.modeCalls; gWifi.wifiMode = static_cast<int>(mode);
  gWifi.record(CALL_WIFI_MODE); return gWifi.modeResult;
}
esp_err_t esp_wifi_set_config(wifi_interface_t interface, const wifi_config_t* config) {
  ++gWifi.configCalls; gWifi.wifiInterface = static_cast<int>(interface);
  if (config != nullptr) gWifi.config = *config;
  gWifi.randomReadyDuringConfig = gRandom != nullptr && gRandom->rfStarted();
  gWifi.record(CALL_WIFI_CONFIG); return gWifi.configResult;
}
esp_err_t esp_event_handler_register(esp_event_base_t eventBase, int32_t eventId,
                                     esp_event_handler_t handler, void* handlerArg) {
  ++gWifi.handlerRegisterCalls; gWifi.record(CALL_HANDLER_REGISTER);
  if (eventBase == IP_EVENT) {
    if (gWifi.ipHandlerRegisterResult == ESP_OK) {
      gIpHandler = handler; gIpHandlerArg = handlerArg; gIpHandlerEvent = eventId;
    }
    return gWifi.ipHandlerRegisterResult;
  }
  TEST_ASSERT_EQUAL_PTR(WIFI_EVENT, eventBase);
  if (gWifi.handlerRegisterResult == ESP_OK) {
    gHandler = handler; gHandlerArg = handlerArg; gHandlerEvent = eventId;
  }
  return gWifi.handlerRegisterResult;
}
esp_err_t esp_event_handler_unregister(esp_event_base_t eventBase, int32_t eventId,
                                       esp_event_handler_t handler) {
  (void)handler;
  ++gWifi.handlerUnregisterCalls; gWifi.record(CALL_HANDLER_UNREGISTER);
  if (eventBase == IP_EVENT) {
    TEST_ASSERT_EQUAL_INT(IP_EVENT_AP_STAIPASSIGNED, eventId);
    if (gWifi.handlerUnregisterResult == ESP_OK) {
      gIpHandler = nullptr; gIpHandlerArg = nullptr; gIpHandlerEvent = -1;
    }
    return gWifi.handlerUnregisterResult;
  }
  TEST_ASSERT_EQUAL_PTR(WIFI_EVENT, eventBase);
  TEST_ASSERT_EQUAL_INT(WIFI_EVENT_AP_START, eventId);
  if (gWifi.handlerUnregisterResult == ESP_OK) {
    gHandler = nullptr; gHandlerArg = nullptr; gHandlerEvent = -1;
  }
  return gWifi.handlerUnregisterResult;
}
esp_err_t esp_wifi_start(void) {
  ++gWifi.wifiStartCalls; gWifi.record(CALL_WIFI_START);
  gWifi.randomReadyDuringStart = gRandom != nullptr && gRandom->rfStarted();
  if (gWifi.wifiStartResult == ESP_OK && gWifi.emitApStart && gHandler != nullptr &&
      gHandlerEvent == WIFI_EVENT_AP_START) {
    gHandler(gHandlerArg, WIFI_EVENT, WIFI_EVENT_AP_START, nullptr);
  }
  return gWifi.wifiStartResult;
}
esp_err_t esp_wifi_stop(void) {
  ++gWifi.wifiStopCalls; gWifi.record(CALL_WIFI_STOP);
  gWifi.randomReadyDuringStop = gRandom != nullptr && gRandom->rfStarted();
  return gWifi.wifiStopResult;
}
esp_err_t esp_wifi_deinit(void) {
  ++gWifi.wifiDeinitCalls; gWifi.record(CALL_WIFI_DEINIT); return gWifi.wifiDeinitResult;
}
EventGroupHandle_t xEventGroupCreate(void) {
  gWifi.startEvents = new FakeEventGroup{0};
  return gWifi.startEvents;
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bitsToWaitFor,
                                BaseType_t clearOnExit, BaseType_t waitForAll,
                                TickType_t ticksToWait) {
  (void)waitForAll; (void)ticksToWait;
  ++gWifi.waitCalls;
  if (group == nullptr) return 0;
  const EventBits_t result = group->bits & bitsToWaitFor;
  if (clearOnExit) group->bits &= ~bitsToWaitFor;
  return result;
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bitsToSet) {
  if (group == nullptr) return 0;
  group->bits |= bitsToSet;
  return group->bits;
}
EventBits_t xEventGroupGetBits(EventGroupHandle_t group) {
  return group == nullptr ? 0 : group->bits;
}
void vEventGroupDelete(EventGroupHandle_t group) {
  delete group;
  if (gWifi.startEvents == group) gWifi.startEvents = nullptr;
  gWifi.eventGroupDeleted = true;
}

void setUp() {
  gWifi = WifiFakeState(); gRandom = nullptr; gHandler = nullptr;
  gHandlerArg = nullptr; gHandlerEvent = -1; gIpHandler = nullptr;
  gIpHandlerArg = nullptr; gIpHandlerEvent = -1; gEspFillRandomCalls = 0;
}
void tearDown() {}

static const char kSsid[] = "ATT-DEVICE-01";
static const char kPass[] = "schoolPassphrase42";

void test_config_is_set_before_radio_start_and_rng_waits_for_ap_start_event() {
  Esp32SecureRandom random; gRandom = &random;
  uint8_t randomBytes[4] = {0};
  TEST_ASSERT_FALSE(random.fill(randomBytes, sizeof randomBytes));
  TEST_ASSERT_EQUAL_INT(0, gEspFillRandomCalls);

  Esp32WifiAp ap(random);
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 6));
  TEST_ASSERT_TRUE(ap.ready());
  TEST_ASSERT_TRUE(random.fill(randomBytes, sizeof randomBytes));
  TEST_ASSERT_EQUAL_INT(1, gEspFillRandomCalls);
  for (size_t i = 0; i < sizeof randomBytes; ++i) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(0xA0u + i), randomBytes[i]);
  }
  TEST_ASSERT_TRUE(random.rfStarted());
  TEST_ASSERT_NOT_NULL(gHandlerArg);
  TEST_ASSERT_NOT_NULL(gIpHandlerArg);
  TEST_ASSERT_EQUAL_INT(WIFI_EVENT_AP_START, gHandlerEvent);
  TEST_ASSERT_EQUAL_INT(IP_EVENT_AP_STAIPASSIGNED, gIpHandlerEvent);
  TEST_ASSERT_FALSE(ap.hasAssignedClientIp());
  ip_event_ap_staipassigned_t assigned = {};
  assigned.esp_netif = &gOtherNetif;
  gIpHandler(gIpHandlerArg, IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &assigned);
  TEST_ASSERT_FALSE(ap.hasAssignedClientIp());
  assigned.esp_netif = &gNetif;
  gIpHandler(gIpHandlerArg, IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &assigned);
  TEST_ASSERT_TRUE(ap.hasAssignedClientIp());
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifInitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.eventLoopCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifNewCalls);
  TEST_ASSERT_EQUAL_PTR(ESP_NETIF_BASE_DEFAULT_WIFI_AP, gWifi.netifConfig.base);
  TEST_ASSERT_NULL(gWifi.netifConfig.driver);
  TEST_ASSERT_EQUAL_PTR(ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP, gWifi.netifConfig.stack);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifAttachCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.defaultHandlersCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiInitCalls);
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_NETIF_NEW) < gWifi.indexOf(CALL_NETIF_ATTACH));
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_NETIF_ATTACH) < gWifi.indexOf(CALL_DEFAULT_HANDLERS));
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_DEFAULT_HANDLERS) < gWifi.indexOf(CALL_WIFI_INIT));
  TEST_ASSERT_EQUAL_INT(WIFI_STORAGE_RAM, gWifi.storageMode);
  TEST_ASSERT_EQUAL_INT(WIFI_MODE_AP, gWifi.wifiMode);
  TEST_ASSERT_EQUAL_INT(WIFI_IF_AP, gWifi.wifiInterface);
  TEST_ASSERT_EQUAL_INT(1, gWifi.configCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiStartCalls);
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_WIFI_MODE) < gWifi.indexOf(CALL_WIFI_CONFIG));
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_WIFI_CONFIG) < gWifi.indexOf(CALL_WIFI_START));
  TEST_ASSERT_FALSE(gWifi.randomReadyDuringConfig);
  TEST_ASSERT_FALSE(gWifi.randomReadyDuringStart);
  TEST_ASSERT_EQUAL_INT(WIFI_AUTH_WPA2_PSK, gWifi.config.ap.authmode);
  TEST_ASSERT_EQUAL_INT(WIFI_CIPHER_TYPE_CCMP, gWifi.config.ap.pairwise_cipher);
  TEST_ASSERT_EQUAL_INT(2, gWifi.config.ap.max_connection);
  TEST_ASSERT_EQUAL_INT(0, gWifi.config.ap.ssid_hidden);
  TEST_ASSERT_EQUAL_INT(6, gWifi.config.ap.channel);
  TEST_ASSERT_EQUAL_INT(sizeof kSsid - 1, gWifi.config.ap.ssid_len);
  TEST_ASSERT_EQUAL_MEMORY(kSsid, gWifi.config.ap.ssid, sizeof kSsid - 1);
  TEST_ASSERT_EQUAL_MEMORY(kPass, gWifi.config.ap.password, sizeof kPass - 1);
  TEST_ASSERT_TRUE(ap.stop());
  TEST_ASSERT_FALSE(ap.hasAssignedClientIp());
  TEST_ASSERT_FALSE(random.fill(randomBytes, sizeof randomBytes));
  TEST_ASSERT_EQUAL_INT(1, gEspFillRandomCalls);
}

void test_config_failure_never_starts_radio_and_cleans_initialized_resources() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.configResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiStartCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_TRUE(gWifi.eventGroupDeleted);
  TEST_ASSERT_TRUE(ap.faulted());
  TEST_ASSERT_EQUAL_INT(EWAP_CONFIG_FAILED, ap.code());
}

void test_netif_allocation_failure_returns_without_aborting_or_starting_wifi() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.createNetifResult = false;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifNewCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.netifAttachCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.netifDestroyCalls);
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.faulted());
}

void test_netif_attach_failure_is_rolled_back_without_wifi_start() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.netifAttachResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifAttachCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.defaultHandlersCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.clearDefaultCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.faulted());
}

void test_default_handler_registration_failure_is_rolled_back() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.defaultHandlersResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.defaultHandlersCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.clearDefaultCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.faulted());
}

void test_driver_start_failure_keeps_rng_blocked_and_cleans_resources() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.wifiStartResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiStartCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiStopCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_TRUE(ap.faulted());
  TEST_ASSERT_EQUAL_INT(EWAP_START_FAILED, ap.code());
}

void test_missing_ap_start_event_times_out_and_stops_driver() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.emitApStart = false;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_EQUAL_INT(1, gWifi.waitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiStopCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_TRUE(gWifi.eventGroupDeleted);
  TEST_ASSERT_TRUE(ap.faulted());
  TEST_ASSERT_EQUAL_INT(EWAP_START_TIMEOUT, ap.code());
}

void test_global_netif_and_event_loop_already_initialized_are_accepted() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.netifInitResult = ESP_ERR_INVALID_STATE;
  gWifi.eventLoopResult = ESP_ERR_INVALID_STATE;
  Esp32WifiAp ap(random);
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_TRUE(ap.ready());
  TEST_ASSERT_TRUE(ap.stop());
}

void test_netif_initialization_failure_never_initializes_wifi_driver() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.netifInitResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.faulted());
}

void test_invalid_null_empty_or_embedded_nul_ssid_is_rejected_before_driver() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  const char embeddedNul[] = {'A', 'B', '\0', 'C'};
  TEST_ASSERT_FALSE(ap.start(nullptr, 4, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(ap.start(kSsid, 0, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(ap.start(embeddedNul, sizeof embeddedNul, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
  TEST_ASSERT_EQUAL_INT(EWAP_INVALID_ARGUMENT, ap.code());
}

void test_ssid_over_32_bytes_is_rejected_without_truncation() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  char ssid[33]; memset(ssid, 'S', sizeof ssid);
  TEST_ASSERT_FALSE(ap.start(ssid, sizeof ssid, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
}

void test_passphrase_length_and_printable_ascii_policy_is_enforced() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, "short7", 6, 1));
  char longPass[64]; memset(longPass, 'P', sizeof longPass);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, longPass, sizeof longPass, 1));
  const char invalidPass[] = {'p','a','s','s','w','o','r','d', 0x7F};
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, invalidPass, sizeof invalidPass, 1));
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
}

void test_maximum_ssid_and_passphrase_lengths_are_accepted() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  char ssid[32]; char passphrase[63];
  memset(ssid, 'S', sizeof ssid); memset(passphrase, 'P', sizeof passphrase);
  TEST_ASSERT_TRUE(ap.start(ssid, sizeof ssid, passphrase, sizeof passphrase, 13));
  TEST_ASSERT_EQUAL_INT(32, gWifi.config.ap.ssid_len);
  TEST_ASSERT_EQUAL_MEMORY(ssid, gWifi.config.ap.ssid, sizeof ssid);
  TEST_ASSERT_EQUAL_MEMORY(passphrase, gWifi.config.ap.password, sizeof passphrase);
  TEST_ASSERT_TRUE(ap.stop());
}

void test_channel_outside_one_to_13_is_rejected() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 0));
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 14));
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiInitCalls);
}

void test_stop_disables_rng_before_wifi_stop_and_releases_resources() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_TRUE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.stop());
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_FALSE(gWifi.randomReadyDuringStop);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiStopCalls);
  TEST_ASSERT_EQUAL_INT(2, gWifi.handlerUnregisterCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.netifDestroyCalls);
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_WIFI_STOP) < gWifi.indexOf(CALL_HANDLER_UNREGISTER));
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_WIFI_DEINIT) < gWifi.indexOf(CALL_NETIF_CLEAR));
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_NETIF_CLEAR) < gWifi.indexOf(CALL_NETIF_DESTROY));
  TEST_ASSERT_TRUE(gWifi.eventGroupDeleted);
  TEST_ASSERT_EQUAL_INT(EWAP_STOPPED, ap.code());
}

void test_stop_failure_keeps_rng_blocked_and_does_not_destroy_live_resources() {
  Esp32SecureRandom random; gRandom = &random;
  EventGroupHandle_t retainedGroup = nullptr;
  void* retainedContext = nullptr;
  {
    Esp32WifiAp ap(random);
    TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
    gWifi.wifiStopResult = ESP_FAIL;
    TEST_ASSERT_FALSE(ap.stop());
    TEST_ASSERT_FALSE(random.rfStarted());
    TEST_ASSERT_TRUE(ap.faulted());
    TEST_ASSERT_EQUAL_INT(0, gWifi.wifiDeinitCalls);
    TEST_ASSERT_EQUAL_INT(0, gWifi.netifDestroyCalls);
    TEST_ASSERT_EQUAL_INT(EWAP_STOP_FAILED, ap.code());
    retainedGroup = gWifi.startEvents;
    retainedContext = gHandlerArg;
  }
  TEST_ASSERT_NOT_NULL(retainedGroup);
  TEST_ASSERT_NOT_NULL(retainedContext);
  // A failed esp_wifi_stop() leaves both handlers registered in production;
  // prove that their independent context remains safe after owner destruction.
  gHandler(retainedContext, WIFI_EVENT, WIFI_EVENT_AP_START, nullptr);
  ip_event_ap_staipassigned_t assigned = {};
  assigned.esp_netif = &gNetif;
  gIpHandler(retainedContext, IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &assigned);
  TEST_ASSERT_TRUE((retainedGroup->bits & BIT0) != 0);
  TEST_ASSERT_TRUE((retainedGroup->bits & BIT1) != 0);
  delete retainedGroup;
  gWifi.startEvents = nullptr;
  free(retainedContext);
  gHandler = nullptr; gHandlerArg = nullptr; gHandlerEvent = -1;
  gIpHandler = nullptr; gIpHandlerArg = nullptr; gIpHandlerEvent = -1;
}

void test_netif_clear_failure_retains_netif_and_latches_fault() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  gWifi.netifClearResult = ESP_FAIL;
  TEST_ASSERT_FALSE(ap.stop());
  TEST_ASSERT_FALSE(random.rfStarted());
  TEST_ASSERT_TRUE(ap.faulted());
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiStopCalls);
  TEST_ASSERT_EQUAL_INT(2, gWifi.handlerUnregisterCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.clearDefaultCalls);
  TEST_ASSERT_TRUE(gWifi.indexOf(CALL_WIFI_DEINIT) < gWifi.indexOf(CALL_NETIF_CLEAR));
  TEST_ASSERT_EQUAL_INT(-1, gWifi.indexOf(CALL_NETIF_DESTROY));
  TEST_ASSERT_EQUAL_INT(0, gWifi.netifDestroyCalls);
  TEST_ASSERT_EQUAL_INT(EWAP_STOP_FAILED, ap.code());
}

void test_double_start_is_refused_without_reinitializing_driver() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiInitCalls);
  TEST_ASSERT_TRUE(ap.ready());
  TEST_ASSERT_EQUAL_INT(EWAP_ALREADY_ACTIVE, ap.code());
  TEST_ASSERT_TRUE(ap.stop());
}

void test_invalid_arguments_do_not_consume_one_shot_start() {
  Esp32SecureRandom random; gRandom = &random;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 14));
  TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiInitCalls);
  TEST_ASSERT_TRUE(ap.ready());
  TEST_ASSERT_TRUE(ap.stop());
}

void test_unregister_failure_keeps_callback_context_safe_after_owner_destruction() {
  Esp32SecureRandom random; gRandom = &random;
  EventGroupHandle_t retainedGroup = nullptr;
  {
    Esp32WifiAp ap(random);
    TEST_ASSERT_TRUE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
    TEST_ASSERT_NOT_NULL(gHandlerArg);
    TEST_ASSERT_EQUAL_PTR(gHandlerArg, gIpHandlerArg);
    gWifi.handlerUnregisterResult = ESP_FAIL;
    TEST_ASSERT_FALSE(ap.stop());
    TEST_ASSERT_FALSE(gWifi.eventGroupDeleted);
    TEST_ASSERT_EQUAL_INT(0, gWifi.wifiDeinitCalls);
    TEST_ASSERT_EQUAL_INT(0, gWifi.netifDestroyCalls);
    TEST_ASSERT_TRUE(ap.faulted());
    retainedGroup = gWifi.startEvents;
  }
  TEST_ASSERT_NOT_NULL(retainedGroup);
  TEST_ASSERT_NOT_NULL(gHandler);
  // The registered callback no longer dereferences the destroyed owner.
  gHandler(gHandlerArg, WIFI_EVENT, WIFI_EVENT_AP_START, nullptr);
  TEST_ASSERT_TRUE((retainedGroup->bits & BIT0) != 0);
  ip_event_ap_staipassigned_t assigned = {};
  assigned.esp_netif = &gNetif;
  gIpHandler(gIpHandlerArg, IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &assigned);
  TEST_ASSERT_TRUE((retainedGroup->bits & BIT1) != 0);
  // In production the context intentionally survives until reboot after an
  // unregister failure. This unit test has now exercised both still-registered
  // callbacks, so it can explicitly reclaim the retained fake resources.
  void* retainedContext = gHandlerArg;
  delete retainedGroup;
  gWifi.startEvents = nullptr;
  free(retainedContext);
  gHandler = nullptr; gHandlerArg = nullptr; gHandlerEvent = -1;
  gIpHandler = nullptr; gIpHandlerArg = nullptr; gIpHandlerEvent = -1;
}

void test_ip_handler_registration_failure_blocks_radio_start() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.ipHandlerRegisterResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(1, gWifi.handlerRegisterCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiStartCalls);
  TEST_ASSERT_EQUAL_INT(0, gWifi.handlerUnregisterCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.wifiDeinitCalls);
  TEST_ASSERT_TRUE(gWifi.eventGroupDeleted);
  TEST_ASSERT_NULL(gIpHandler);
  TEST_ASSERT_FALSE(ap.hasAssignedClientIp());
}

void test_wifi_handler_registration_failure_unregisters_ip_handler() {
  Esp32SecureRandom random; gRandom = &random;
  gWifi.handlerRegisterResult = ESP_FAIL;
  Esp32WifiAp ap(random);
  TEST_ASSERT_FALSE(ap.start(kSsid, sizeof kSsid - 1, kPass, sizeof kPass - 1, 1));
  TEST_ASSERT_EQUAL_INT(2, gWifi.handlerRegisterCalls);
  TEST_ASSERT_EQUAL_INT(1, gWifi.handlerUnregisterCalls);
  TEST_ASSERT_NULL(gIpHandler);
  TEST_ASSERT_EQUAL_INT(0, gWifi.wifiStartCalls);
  TEST_ASSERT_TRUE(gWifi.eventGroupDeleted);
  TEST_ASSERT_TRUE(ap.faulted());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_config_is_set_before_radio_start_and_rng_waits_for_ap_start_event);
  RUN_TEST(test_config_failure_never_starts_radio_and_cleans_initialized_resources);
  RUN_TEST(test_driver_start_failure_keeps_rng_blocked_and_cleans_resources);
  RUN_TEST(test_missing_ap_start_event_times_out_and_stops_driver);
  RUN_TEST(test_global_netif_and_event_loop_already_initialized_are_accepted);
  RUN_TEST(test_netif_initialization_failure_never_initializes_wifi_driver);
  RUN_TEST(test_invalid_null_empty_or_embedded_nul_ssid_is_rejected_before_driver);
  RUN_TEST(test_ssid_over_32_bytes_is_rejected_without_truncation);
  RUN_TEST(test_passphrase_length_and_printable_ascii_policy_is_enforced);
  RUN_TEST(test_maximum_ssid_and_passphrase_lengths_are_accepted);
  RUN_TEST(test_channel_outside_one_to_13_is_rejected);
  RUN_TEST(test_stop_disables_rng_before_wifi_stop_and_releases_resources);
  RUN_TEST(test_stop_failure_keeps_rng_blocked_and_does_not_destroy_live_resources);
  RUN_TEST(test_netif_clear_failure_retains_netif_and_latches_fault);
  RUN_TEST(test_double_start_is_refused_without_reinitializing_driver);
  RUN_TEST(test_invalid_arguments_do_not_consume_one_shot_start);
  RUN_TEST(test_unregister_failure_keeps_callback_context_safe_after_owner_destruction);
  RUN_TEST(test_netif_allocation_failure_returns_without_aborting_or_starting_wifi);
  RUN_TEST(test_netif_attach_failure_is_rolled_back_without_wifi_start);
  RUN_TEST(test_default_handler_registration_failure_is_rolled_back);
  RUN_TEST(test_ip_handler_registration_failure_blocks_radio_start);
  RUN_TEST(test_wifi_handler_registration_failure_unregisters_ip_handler);
  return UNITY_END();
}
