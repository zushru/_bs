#pragma once
#include <stddef.h>
#include <stdint.h>

typedef int32_t esp_err_t;
typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY = 0, NVS_READWRITE = 1 } nvs_open_mode_t;

#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_TYPE_MISMATCH 0x1103
#define ESP_ERR_NVS_NOT_INITIALIZED 0x1101
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE 0x1105
#define ESP_ERR_NVS_INVALID_LENGTH 0x110C
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110D
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110

#ifdef __cplusplus
extern "C" {
#endif
esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode,
                   nvs_handle_t* out_handle);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value,
                       size_t* length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key,
                       const void* value, size_t length);
esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key, uint16_t* out_value);
esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key, uint16_t value);
esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out_value,
                      size_t* length);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
#ifdef __cplusplus
}
#endif
