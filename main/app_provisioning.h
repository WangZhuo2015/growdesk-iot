#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_provisioning_cb_t)(bool success, const char *ssid);

esp_err_t app_provisioning_init(void);
esp_err_t app_provisioning_start(const char *device_name, app_provisioning_cb_t cb);
esp_err_t app_provisioning_stop(void);
bool app_provisioning_is_active(void);
esp_err_t app_provisioning_notify_connected(void);
esp_err_t app_provisioning_notify_failed(void);
void app_provisioning_on_scan_done(void);

#ifdef __cplusplus
}
#endif
