#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef const char* esp_event_base_t;
typedef void (*esp_event_handler_t)(void*, esp_event_base_t, int32_t, void*);
extern esp_event_base_t WIFI_EVENT;
extern esp_event_base_t IP_EVENT;
#define WIFI_EVENT_AP_START 1
#define WIFI_EVENT_AP_STOP 2
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t event_base, int32_t event_id,
                                     esp_event_handler_t event_handler, void* event_handler_arg);
esp_err_t esp_event_handler_unregister(esp_event_base_t event_base, int32_t event_id,
                                       esp_event_handler_t event_handler);
