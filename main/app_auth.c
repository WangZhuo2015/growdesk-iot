#include "app_auth.h"
#include "app_storage.h"
#include "app_protocol.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef TEST_HOST
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#else
#include "esp_err.h"
#endif

#ifndef TEST_HOST
static const char *TAG = "app_auth";
#endif

static char s_server_url[128] = DEFAULT_GROWDESK_SERVER_URL;
static char s_access_token[MAX_JWT_TOKEN_LEN] = {0};
static int64_t s_token_expires_at = 0;

esp_err_t app_auth_init(const char *server_base_url)
{
    if (server_base_url && strlen(server_base_url) > 0) {
        strncpy(s_server_url, server_base_url, sizeof(s_server_url) - 1);
        s_server_url[sizeof(s_server_url) - 1] = '\0';
    } else {
        char stored_url[128] = {0};
        if (app_storage_get_server_url(stored_url, sizeof(stored_url)) == ESP_OK && strlen(stored_url) > 0) {
            strncpy(s_server_url, stored_url, sizeof(s_server_url) - 1);
        }
    }
    s_access_token[0] = '\0';
    s_token_expires_at = 0;
    return ESP_OK;
}

esp_err_t app_auth_set_server_url(const char *server_base_url)
{
    if (!server_base_url || strlen(server_base_url) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_server_url, server_base_url, sizeof(s_server_url) - 1);
    s_server_url[sizeof(s_server_url) - 1] = '\0';
    return app_storage_save_server_url(s_server_url);
}

const char *app_auth_get_server_url(void)
{
    return s_server_url;
}

bool app_auth_is_paired(void)
{
    return app_storage_has_device_credentials();
}

#ifndef TEST_HOST
typedef struct {
    char *buffer;
    size_t buffer_len;
    size_t written;
} http_resp_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *res = (http_resp_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && res && res->buffer) {
        if (res->written + evt->data_len < res->buffer_len) {
            memcpy(res->buffer + res->written, evt->data, evt->data_len);
            res->written += evt->data_len;
            res->buffer[res->written] = '\0';
        }
    }
    return ESP_OK;
}

static esp_err_t http_post_json(const char *url, const char *auth_header, const char *post_data,
                                char *out_buf, size_t out_buf_len, int *out_status_code)
{
    if (out_buf && out_buf_len > 0) {
        out_buf[0] = '\0';
    }

    http_resp_buf_t res_buf = {
        .buffer = out_buf,
        .buffer_len = out_buf_len,
        .written = 0,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &res_buf,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (auth_header && strlen(auth_header) > 0) {
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    if (post_data) {
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        *out_status_code = esp_http_client_get_status_code(client);
    } else {
        ESP_LOGE(TAG, "HTTP POST to %s failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
#else
// TEST_HOST Mock
static int s_test_mock_status_code = 200;
static char s_test_mock_response[2048] = {0};

void test_auth_set_mock_response(int status_code, const char *resp)
{
    s_test_mock_status_code = status_code;
    if (resp) {
        strncpy(s_test_mock_response, resp, sizeof(s_test_mock_response) - 1);
    } else {
        s_test_mock_response[0] = '\0';
    }
}

static esp_err_t http_post_json(const char *url, const char *auth_header, const char *post_data,
                                char *out_buf, size_t out_buf_len, int *out_status_code)
{
    (void)url; (void)auth_header; (void)post_data;
    *out_status_code = s_test_mock_status_code;
    strncpy(out_buf, s_test_mock_response, out_buf_len - 1);
    out_buf[out_buf_len - 1] = '\0';
    return ESP_OK;
}
#endif

esp_err_t app_auth_request_pairing(app_auth_pairing_info_t *out_info)
{
    if (!out_info) return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/passport/pairings", s_server_url);

    const char *payload = "{\"hardware\":\"folo-ai-passport\",\"firmwareVersion\":\"1.0.0\","
                          "\"capabilities\":{\"display\":\"240x320\",\"microphone\":true,\"speaker\":true,\"buttons\":3}}";

    char *response = (char *)calloc(1, 2048);
    if (!response) return ESP_ERR_NO_MEM;

    int status_code = 0;
    esp_err_t err = http_post_json(url, NULL, payload, response, 2048, &status_code);
    if (err != ESP_OK || status_code != 201) {
        free(response);
        return (err != ESP_OK) ? err : ESP_FAIL;
    }

    if (!app_json_get_string(response, "pairingId", out_info->pairing_id, sizeof(out_info->pairing_id))) {
        free(response);
        return ESP_FAIL;
    }
    if (!app_json_get_string(response, "pairCode", out_info->pair_code, sizeof(out_info->pair_code))) {
        free(response);
        return ESP_FAIL;
    }
    if (!app_json_get_string(response, "pollToken", out_info->poll_token, sizeof(out_info->poll_token))) {
        free(response);
        return ESP_FAIL;
    }

    free(response);
    out_info->expires_at = (int64_t)time(NULL) + 600;
    return ESP_OK;
}

esp_err_t app_auth_poll_pairing_status(const char *pairing_id, const char *poll_token,
                                      bool *out_completed, bool *out_expired)
{
    if (!pairing_id || !poll_token || !out_completed || !out_expired) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_completed = false;
    *out_expired = false;

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/passport/pairings/%s/poll", s_server_url, pairing_id);

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"pollToken\":\"%s\"}", poll_token);

    char *response = (char *)calloc(1, 2048);
    if (!response) return ESP_ERR_NO_MEM;

    int status_code = 0;
    esp_err_t err = http_post_json(url, NULL, payload, response, 2048, &status_code);
    if (err != ESP_OK || status_code != 200) {
        free(response);
        return (err != ESP_OK) ? err : ESP_FAIL;
    }

    char status[32] = {0};
    if (!app_json_get_string(response, "status", status, sizeof(status))) {
        free(response);
        return ESP_FAIL;
    }

    if (strcmp(status, "expired") == 0) {
        free(response);
        *out_expired = true;
        return ESP_OK;
    }

    if (strcmp(status, "completed") == 0) {
        char dev_id[64] = {0};
        char dev_cred[128] = {0};
        char baby_id[64] = {0};

        if (!app_json_get_string(response, "deviceId", dev_id, sizeof(dev_id)) ||
            !app_json_get_string(response, "deviceCredential", dev_cred, sizeof(dev_cred))) {
            free(response);
            return ESP_FAIL;
        }

        app_json_get_string(response, "babyId", baby_id, sizeof(baby_id));

        // Save credentials to storage
        app_storage_save_device_credentials(dev_id, dev_cred);
        if (baby_id[0] != '\0') {
            app_storage_save_baby_info(baby_id, "");
        }

        free(response);
        *out_completed = true;
        return ESP_OK;
    }

    free(response);
    // status is pending or claimed
    return ESP_OK;
}

esp_err_t app_auth_refresh_token(void)
{
    char dev_id[64] = {0};
    char dev_cred[128] = {0};

    esp_err_t err = app_storage_get_device_credentials(dev_id, sizeof(dev_id), dev_cred, sizeof(dev_cred));
    if (err != ESP_OK || dev_id[0] == '\0' || dev_cred[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/api/v1/passport/auth/token", s_server_url);

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Device %s.%s", dev_id, dev_cred);

    char payload[256];
    snprintf(payload, sizeof(payload), "{\"deviceId\":\"%s\",\"deviceCredential\":\"%s\"}", dev_id, dev_cred);

    char *response = (char *)calloc(1, 2048);
    if (!response) return ESP_ERR_NO_MEM;

    int status_code = 0;
    err = http_post_json(url, auth_header, payload, response, 2048, &status_code);
    if (err != ESP_OK) {
        free(response);
        return err;
    }

    if (status_code == 401 || status_code == 403) {
        // Device revoked or invalid credentials
        free(response);
        app_auth_invalidate_token();
        return ESP_ERR_INVALID_STATE;
    }

    if (status_code != 200) {
        free(response);
        return ESP_FAIL;
    }

    char token[MAX_JWT_TOKEN_LEN] = {0};
    int expires_in = 900;
    if (!app_json_get_string(response, "accessToken", token, sizeof(token))) {
        free(response);
        return ESP_FAIL;
    }
    app_json_get_int(response, "expiresIn", &expires_in);

    strncpy(s_access_token, token, sizeof(s_access_token) - 1);
    s_access_token[sizeof(s_access_token) - 1] = '\0';
    // Expire 180 seconds earlier to refresh before expiration
    s_token_expires_at = (int64_t)time(NULL) + (expires_in > 180 ? expires_in - 180 : expires_in);

    free(response);
    return ESP_OK;
}

bool app_auth_has_valid_token(void)
{
    if (s_access_token[0] == '\0') {
        return false;
    }
    return (int64_t)time(NULL) < s_token_expires_at;
}

const char *app_auth_get_access_token(void)
{
    if (!app_auth_has_valid_token()) {
        return NULL;
    }
    return s_access_token;
}

void app_auth_invalidate_token(void)
{
    memset(s_access_token, 0, sizeof(s_access_token));
    s_token_expires_at = 0;
}
