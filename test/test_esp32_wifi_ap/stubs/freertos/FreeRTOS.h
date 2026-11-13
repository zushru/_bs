#pragma once
#include <stdint.h>
typedef uint32_t EventBits_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define BIT0 ((EventBits_t)1u)
#define BIT1 ((EventBits_t)2u)
