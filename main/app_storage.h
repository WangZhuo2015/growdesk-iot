#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_GROWDESK_SERVER_URL "https://ampere.zwang.fun:8443"
#define DEFAULT_GROWDESK_WS_URL     "wss://ampere.zwang.fun:8443"

esp_err_t app_storage_init(void);

bool app_storage_has_device_credentials(void);
esp_err_t app_storage_get_device_credentials(char *dev_id, size_t id_len, char *dev_cred, size_t cred_len);
esp_err_t app_storage_save_device_credentials(const char *dev_id, const char *dev_cred);

esp_err_t app_storage_get_baby_info(char *baby_id, size_t id_len, char *baby_name, size_t name_len);
esp_err_t app_storage_save_baby_info(const char *baby_id, const char *baby_name);

esp_err_t app_storage_get_server_url(char *url, size_t len);
esp_err_t app_storage_save_server_url(const char *url);

esp_err_t app_storage_clear_pairing(void);

#ifdef __cplusplus
}
#endif
