#pragma once
#include "esp_err.h"
#include "esp_event.h"
#include <stdint.h>
struct esp_netif_t { int marker; };
struct esp_netif_inherent_config_t { int marker; };
struct esp_netif_netstack_config_t { int marker; };
typedef struct {
  const esp_netif_inherent_config_t* base;
  void* driver;
  const esp_netif_netstack_config_t* stack;
} esp_netif_config_t;
extern const esp_netif_inherent_config_t gEspNetifBaseDefaultAp;
extern const esp_netif_netstack_config_t gEspNetifNetstackDefaultAp;
#define IP_EVENT_AP_STAIPASSIGNED 2
typedef struct {
  esp_netif_t* esp_netif;
  uint32_t ip;
  uint8_t mac[6];
} ip_event_ap_staipassigned_t;

esp_err_t esp_netif_init(void);
esp_netif_t* esp_netif_new(const esp_netif_config_t* config);
void esp_netif_destroy(esp_netif_t* netif);
