#pragma once
#include "FreeRTOS.h"
#include <stddef.h>
struct FakeEventGroup { EventBits_t bits; };
typedef FakeEventGroup* EventGroupHandle_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bitsToWaitFor,
                                BaseType_t clearOnExit, BaseType_t waitForAll,
                                TickType_t ticksToWait);
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bitsToSet);
EventBits_t xEventGroupGetBits(EventGroupHandle_t group);
void vEventGroupDelete(EventGroupHandle_t group);
