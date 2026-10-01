#include "app_provisioning.h"
#include "app_blufi_security.h"
#include "app_network.h"

#include <string.h>
#include <stdio.h>

#ifndef TEST_HOST
#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#else
#include "esp_err.h"
#endif

static const char *TAG = "app_prov";

static bool s_active = false;
static bool s_ble_connected = false;
static bool s_profile_initialized = false;
static bool s_gatt_initialized = false;
static bool s_btc_initialized = false;
static bool s_host_running = false;
static SemaphoreHandle_t s_host_stopped = NULL;
static app_provisioning_cb_t s_callback = NULL;
static char s_target_ssid[33] = {0};
static char s_device_name[32] = "BLUFI_FoloPassport";

#ifndef TEST_HOST
static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

static esp_blufi_callbacks_t s_blufi_callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = app_blufi_negotiate,
    .encrypt_func = app_blufi_encrypt,
    .decrypt_func = app_blufi_decrypt,
    .checksum_func = app_blufi_checksum,
};

static void blufi_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
}

static void blufi_sync(void)
{
    int rc = esp_blufi_profile_init();
    if (rc == 0) {
        s_profile_initialized = true;
        ESP_LOGI(TAG, "BLUFI profile initialized successfully");
    } else {
        ESP_LOGE(TAG, "esp_blufi_profile_init failed: %d", rc);
    }
}

static void blufi_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLUFI host task started");
    nimble_port_run();
    if (s_host_stopped) {
        xSemaphoreGive(s_host_stopped);
    }
    nimble_port_freertos_deinit();
}

static char s_target_passwd[65] = {0};

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "BLUFI init finished, advertising as %s...", s_device_name);
        esp_blufi_adv_start_with_name(s_device_name);
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "BLUFI BLE connected");
        s_ble_connected = true;
        esp_blufi_adv_stop();
        app_blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "BLUFI BLE disconnected");
        s_ble_connected = false;
        app_blufi_security_deinit();
        if (s_active) {
            esp_blufi_adv_start_with_name(s_device_name);
        }
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        esp_wifi_set_mode(WIFI_MODE_STA);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        if (param->sta_ssid.ssid_len < sizeof(s_target_ssid)) {
            memset(s_target_ssid, 0, sizeof(s_target_ssid));
            memcpy(s_target_ssid, param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            ESP_LOGI(TAG, "BLUFI received SSID: %s", s_target_ssid);
        }
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD: {
        if (param->sta_passwd.passwd_len < sizeof(s_target_passwd)) {
            memset(s_target_passwd, 0, sizeof(s_target_passwd));
            memcpy(s_target_passwd, param->sta_passwd.passwd, param->sta_passwd.passwd_len);
            // CRITICAL: NEVER log password
            ESP_LOGI(TAG, "BLUFI received password (len: %d)", param->sta_passwd.passwd_len);
        }
        break;
    }
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_LOGI(TAG, "BLUFI requested connect to AP: %s", s_target_ssid);
        if (s_target_ssid[0] != '\0') {
            app_network_connect(s_target_ssid, s_target_passwd);
            memset(s_target_passwd, 0, sizeof(s_target_passwd));
        }
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        ESP_LOGI(TAG, "BLUFI requested disconnect from AP");
        app_network_disconnect();
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        esp_blufi_extra_info_t info = { 0 };
        info.sta_ssid = (uint8_t *)s_target_ssid;
        info.sta_ssid_len = strlen(s_target_ssid);
        esp_blufi_sta_conn_state_t st = ESP_BLUFI_STA_CONN_FAIL;
        if (app_network_get_status() == APP_NETWORK_CONNECTED) {
            st = ESP_BLUFI_STA_CONN_SUCCESS;
        } else if (app_network_get_status() == APP_NETWORK_CONNECTING) {
            st = ESP_BLUFI_STA_CONNECTING;
        }
        esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, st, 0, &info);
        break;
    }
    case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
        ESP_LOGI(TAG, "BLUFI requested Wi-Fi AP scan list");
        wifi_scan_config_t scan_cfg = { 0 };
        if (esp_wifi_scan_start(&scan_cfg, false) != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_scan_start failed");
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        ESP_LOGI(TAG, "BLUFI requested BLE disconnect");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        ESP_LOGI(TAG, "BLUFI requested STA deauthenticate");
        app_network_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        ESP_LOGW(TAG, "BLUFI report error: %d", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    default:
        break;
    }
}

void app_provisioning_on_scan_done(void)
{
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        ESP_LOGI(TAG, "No APs found during scan");
        return;
    }
    if (ap_count > 16) {
        ap_count = 16;
    }

    wifi_ap_record_t *records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!records) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP scan records");
        esp_wifi_clear_ap_list();
        return;
    }

    esp_err_t err = esp_wifi_scan_get_ap_records(&ap_count, records);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records failed: %d", err);
        free(records);
        esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        return;
    }

    esp_blufi_ap_record_t *list = (esp_blufi_ap_record_t *)malloc(sizeof(esp_blufi_ap_record_t) * ap_count);
    if (!list) {
        ESP_LOGE(TAG, "Failed to allocate memory for blufi AP records");
        free(records);
        return;
    }

    for (uint16_t i = 0; i < ap_count; i++) {
        list[i].rssi = records[i].rssi;
        memcpy(list[i].ssid, records[i].ssid, sizeof(list[i].ssid));
    }

    if (s_ble_connected) {
        ESP_LOGI(TAG, "Sending %u AP records to BLUFI client", ap_count);
        esp_blufi_send_wifi_list(ap_count, list);
    } else {
        ESP_LOGI(TAG, "BLE not connected, discarding scan results");
    }

    esp_wifi_scan_stop();
    free(records);
    free(list);
}
#else
void app_provisioning_on_scan_done(void) {}
#endif

esp_err_t app_provisioning_init(void)
{
    return ESP_OK;
}

esp_err_t app_provisioning_start(const char *device_name, app_provisioning_cb_t cb)
{
    if (s_active) {
        return ESP_OK;
    }

    s_callback = cb;
    if (device_name && strlen(device_name) > 0) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
    } else {
        strncpy(s_device_name, "BLUFI_FoloPassport", sizeof(s_device_name) - 1);
    }

#ifndef TEST_HOST
    esp_err_t err = esp_blufi_register_callbacks(&s_blufi_callbacks);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_blufi_register_callbacks failed: %d", err);
        return err;
    }

    err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
        return err;
    }

    s_host_stopped = xSemaphoreCreateBinary();

    ble_hs_cfg.reset_cb = blufi_reset;
    ble_hs_cfg.sync_cb = blufi_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;

    int rc = esp_blufi_gatt_svr_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "esp_blufi_gatt_svr_init failed: %d", rc);
        return ESP_FAIL;
    }
    s_gatt_initialized = true;

    rc = ble_svc_gap_device_name_set(s_device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        return ESP_FAIL;
    }

    esp_blufi_btc_init();
    s_btc_initialized = true;

    err = esp_nimble_enable(blufi_host_task);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_nimble_enable failed: %d", err);
        return err;
    }
    s_host_running = true;
#endif

    s_active = true;
    return ESP_OK;
}

esp_err_t app_provisioning_stop(void)
{
    if (!s_active) {
        return ESP_OK;
    }

#ifndef TEST_HOST
    s_ble_connected = false;
    app_blufi_security_deinit();

    if (s_profile_initialized) {
        esp_blufi_adv_stop();
    }
    if (s_gatt_initialized) {
        esp_blufi_gatt_svr_deinit();
        s_gatt_initialized = false;
    }
    if (s_host_running) {
        nimble_port_stop();
        if (s_host_stopped) {
            xSemaphoreTake(s_host_stopped, pdMS_TO_TICKS(1000));
        }
        s_host_running = false;
    }
    nimble_port_deinit();

    if (s_profile_initialized) {
        esp_blufi_profile_deinit();
        s_profile_initialized = false;
    }
    if (s_btc_initialized) {
        esp_blufi_btc_deinit();
        s_btc_initialized = false;
    }
    if (s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
#endif

    s_active = false;
    return ESP_OK;
}

bool app_provisioning_is_active(void)
{
    return s_active;
}

esp_err_t app_provisioning_notify_connected(void)
{
    if (!s_active) {
        return ESP_OK;
    }

#ifndef TEST_HOST
    if (s_ble_connected) {
        esp_blufi_extra_info_t info = { 0 };
        info.sta_ssid = (uint8_t *)s_target_ssid;
        info.sta_ssid_len = strlen(s_target_ssid);
        esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
    }
#endif

    if (s_callback) {
        s_callback(true, s_target_ssid);
    }

    return ESP_OK;
}

esp_err_t app_provisioning_notify_failed(void)
{
    if (!s_active) {
        return ESP_OK;
    }

#ifndef TEST_HOST
    if (s_ble_connected) {
        esp_blufi_extra_info_t info = { 0 };
        info.sta_ssid = (uint8_t *)s_target_ssid;
        info.sta_ssid_len = strlen(s_target_ssid);
        esp_blufi_send_wifi_conn_report(WIFI_MODE_STA, ESP_BLUFI_STA_CONN_FAIL, 0, &info);
    }
#endif

    if (s_callback) {
        s_callback(false, s_target_ssid);
    }

    return ESP_OK;
}
