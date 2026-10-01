#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_NETWORK_DISCONNECTED = 0,
    APP_NETWORK_CONNECTING,
    APP_NETWORK_CONNECTED,
    APP_NETWORK_FAILED,
} app_network_status_t;

typedef void (*app_network_event_cb_t)(app_network_status_t status, const char *ip);

esp_err_t app_network_init(void);
esp_err_t app_network_set_event_callback(app_network_event_cb_t cb);

esp_err_t app_network_connect(const char *ssid, const char *password);
esp_err_t app_network_disconnect(void);
esp_err_t app_network_start_sta(void);

bool app_network_is_connected(void);
app_network_status_t app_network_get_status(void);
const char *app_network_get_ip(void);

void app_network_set_auto_reconnect(bool enabled);
esp_err_t app_network_reconnect(void);

#ifdef __cplusplus
}
#endif
