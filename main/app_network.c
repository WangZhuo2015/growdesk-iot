#include "app_network.h"
#include "app_provisioning.h"

#include <string.h>
#include <stdio.h>

#ifndef TEST_HOST
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include "esp_err.h"
#endif

#ifndef TEST_HOST
static const char *TAG = "app_network";
#endif

static app_network_status_t s_status = APP_NETWORK_DISCONNECTED;
static char s_ip_str[16] = {0};
static bool s_auto_reconnect = true;
static bool s_initialized = false;
static app_network_event_cb_t s_event_cb = NULL;
static int s_retry_count = 0;
#define MAX_RETRY_BACKOFF 5

#ifndef TEST_HOST
static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_wifi_handler_instance;
static esp_event_handler_instance_t s_ip_handler_instance;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi STA started");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Wi-Fi STA connected to AP, waiting for IP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (app_provisioning_is_active() && s_status == APP_NETWORK_CONNECTING) {
                app_provisioning_notify_failed();
            }
            s_status = APP_NETWORK_DISCONNECTED;
            s_ip_str[0] = '\0';
            ESP_LOGW(TAG, "Wi-Fi STA disconnected");
            if (s_event_cb) {
                s_event_cb(s_status, NULL);
            }
            if (s_auto_reconnect && !app_provisioning_is_active()) {
                s_retry_count++;
                ESP_LOGI(TAG, "Auto reconnecting... (attempt %d)", s_retry_count);
                esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_SCAN_DONE:
            app_provisioning_on_scan_done();
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            s_status = APP_NETWORK_CONNECTED;
            s_retry_count = 0;
            ESP_LOGI(TAG, "Wi-Fi Got IP: %s", s_ip_str);
            if (app_provisioning_is_active()) {
                app_provisioning_notify_connected();
            }
            if (s_event_cb) {
                s_event_cb(s_status, s_ip_str);
            }
        }
    }
}
#endif

esp_err_t app_network_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

#ifndef TEST_HOST
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %d", err);
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", err);
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "Failed to create default wifi STA netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 4;
    cfg.dynamic_rx_buf_num = 16;
    cfg.dynamic_tx_buf_num = 16;
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", err);
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL,
                                              &s_wifi_handler_instance);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL,
                                              &s_ip_handler_instance);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_storage failed: %d", err);
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
#endif

    s_initialized = true;
    s_status = APP_NETWORK_DISCONNECTED;
    return ESP_OK;
}

esp_err_t app_network_set_event_callback(app_network_event_cb_t cb)
{
    s_event_cb = cb;
    return ESP_OK;
}

esp_err_t app_network_connect(const char *ssid, const char *password)
{
    if (!s_initialized) {
        esp_err_t err = app_network_init();
        if (err != ESP_OK) {
            return err;
        }
    }

    if (!ssid || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_status = APP_NETWORK_CONNECTING;
    if (s_event_cb) {
        s_event_cb(s_status, NULL);
    }

#ifndef TEST_HOST
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    }
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;

    // NEVER log password
    ESP_LOGI(TAG, "Connecting to SSID: %s (password_len: %zu)", ssid, password ? strlen(password) : 0);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) {
        s_status = APP_NETWORK_FAILED;
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        s_status = APP_NETWORK_FAILED;
        return err;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_status = APP_NETWORK_FAILED;
        return err;
    }
#else
    // Test mock behavior
    (void)password;
    s_status = APP_NETWORK_CONNECTED;
    snprintf(s_ip_str, sizeof(s_ip_str), "192.168.1.100");
    if (s_event_cb) {
        s_event_cb(s_status, s_ip_str);
    }
#endif

    return ESP_OK;
}

esp_err_t app_network_disconnect(void)
{
    s_auto_reconnect = false;
    s_status = APP_NETWORK_DISCONNECTED;
    s_ip_str[0] = '\0';

#ifndef TEST_HOST
    esp_err_t err = esp_wifi_disconnect();
    return err;
#else
    if (s_event_cb) {
        s_event_cb(s_status, NULL);
    }
    return ESP_OK;
#endif
}

esp_err_t app_network_start_sta(void)
{
    if (!s_initialized) {
        esp_err_t err = app_network_init();
        if (err != ESP_OK) return err;
    }
#ifndef TEST_HOST
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return esp_wifi_connect();
#else
    return ESP_OK;
#endif
}

bool app_network_is_connected(void)
{
    return s_status == APP_NETWORK_CONNECTED;
}

app_network_status_t app_network_get_status(void)
{
    return s_status;
}

const char *app_network_get_ip(void)
{
    return s_ip_str;
}

void app_network_set_auto_reconnect(bool enabled)
{
    s_auto_reconnect = enabled;
}

esp_err_t app_network_reconnect(void)
{
    s_retry_count = 0;
#ifndef TEST_HOST
    return esp_wifi_connect();
#else
    return ESP_OK;
#endif
}
