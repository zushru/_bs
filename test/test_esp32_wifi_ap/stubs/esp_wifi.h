#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { WIFI_MODE_NULL = 0, WIFI_MODE_AP = 2 } wifi_mode_t;
typedef enum { WIFI_IF_AP = 1 } wifi_interface_t;
typedef enum { WIFI_STORAGE_FLASH = 0, WIFI_STORAGE_RAM = 1 } wifi_storage_t;
typedef enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 } wifi_auth_mode_t;
typedef enum { WIFI_CIPHER_TYPE_NONE = 0, WIFI_CIPHER_TYPE_CCMP = 4 } wifi_cipher_type_t;

typedef struct {
  uint8_t ssid[32];
  uint8_t password[64];
  uint8_t ssid_len;
  uint8_t channel;
  uint8_t max_connection;
  uint8_t ssid_hidden;
  bool ftm_responder;
  wifi_auth_mode_t authmode;
  wifi_cipher_type_t pairwise_cipher;
} wifi_ap_config_t;

typedef struct { wifi_ap_config_t ap; } wifi_config_t;
typedef struct { uint32_t magic; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() { 0x12345678u }

esp_err_t esp_wifi_init(const wifi_init_config_t* config);
esp_err_t esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_set_config(wifi_interface_t interface, const wifi_config_t* config);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_deinit(void);
