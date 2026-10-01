#include "app_ws.h"
#include "app_protocol.h"
#include "app_auth.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TEST_HOST
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

static const char *TAG = "app_ws";
static esp_websocket_client_handle_t s_client = NULL;
#endif

static app_ws_callbacks_t s_callbacks = {0};
static app_ws_state_t s_state = APP_WS_DISCONNECTED;

#ifndef TEST_HOST
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        s_state = APP_WS_CONNECTED;
        // Send hello frame
        {
            char hello_buf[256];
            size_t len = app_protocol_build_hello(hello_buf, sizeof(hello_buf), "1.0.0");
            esp_websocket_client_send_text(s_client, hello_buf, len, portMAX_DELAY);
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        s_state = APP_WS_DISCONNECTED;
        if (s_callbacks.on_disconnected) {
            s_callbacks.on_disconnected();
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x01) {
            // Text frame (JSON)
            char text_buf[MAX_WS_CONTROL_FRAME_SIZE];
            int copy_len = data->data_len < (int)sizeof(text_buf) - 1 ? data->data_len : (int)sizeof(text_buf) - 1;
            memcpy(text_buf, data->data_ptr, copy_len);
            text_buf[copy_len] = '\0';

            char type[32] = {0};
            if (!app_protocol_get_type(text_buf, type, sizeof(type))) {
                break;
            }

            if (strcmp(type, "ready") == 0) {
                char dev_id[64] = {0}, baby_id[64] = {0}, baby_name[64] = {0};
                if (app_protocol_parse_ready(text_buf, dev_id, sizeof(dev_id), baby_id, sizeof(baby_id), baby_name, sizeof(baby_name))) {
                    if (s_callbacks.on_ready) s_callbacks.on_ready(dev_id, baby_id, baby_name);
                }
            } else if (strcmp(type, "asr.final") == 0) {
                char asr_text[512] = {0};
                bool is_final = false;
                if (app_protocol_parse_asr(text_buf, asr_text, sizeof(asr_text), &is_final)) {
                    char turn_id[64] = {0};
                    app_json_get_string(text_buf, "turnId", turn_id, sizeof(turn_id));
                    if (s_callbacks.on_asr_final) s_callbacks.on_asr_final(turn_id, asr_text);
                }
            } else if (strcmp(type, "assistant.final") == 0) {
                char asst_text[1024] = {0};
                bool is_final = false;
                if (app_protocol_parse_assistant(text_buf, asst_text, sizeof(asst_text), &is_final)) {
                    char turn_id[64] = {0};
                    app_json_get_string(text_buf, "turnId", turn_id, sizeof(turn_id));
                    if (s_callbacks.on_assistant_final) s_callbacks.on_assistant_final(turn_id, asst_text);
                }
            } else if (strcmp(type, "card.present") == 0) {
                passport_proposal_t prop;
                if (app_protocol_parse_card(text_buf, &prop)) {
                    if (s_callbacks.on_card_present) s_callbacks.on_card_present(&prop);
                }
            } else if (strcmp(type, "card.saved") == 0) {
                char run_id[64] = {0}, entity_type[32] = {0}, record_id[64] = {0};
                app_json_get_string(text_buf, "runId", run_id, sizeof(run_id));
                app_json_get_string(text_buf, "entityType", entity_type, sizeof(entity_type));
                app_json_get_string(text_buf, "recordId", record_id, sizeof(record_id));
                if (s_callbacks.on_card_saved) s_callbacks.on_card_saved(run_id, entity_type, record_id);
            } else if (strcmp(type, "tts.start") == 0) {
                char turn_id[64] = {0};
                int sample_rate = 24000, channels = 1;
                if (app_protocol_parse_tts_start(text_buf, turn_id, sizeof(turn_id), &sample_rate, &channels)) {
                    if (s_callbacks.on_tts_start) s_callbacks.on_tts_start(turn_id, sample_rate, channels);
                }
            } else if (strcmp(type, "tts.end") == 0) {
                char turn_id[64] = {0};
                if (app_protocol_parse_tts_end(text_buf, turn_id, sizeof(turn_id))) {
                    if (s_callbacks.on_tts_end) s_callbacks.on_tts_end(turn_id);
                }
            } else if (strcmp(type, "error") == 0) {
                char code[64] = {0}, msg[256] = {0};
                if (app_protocol_parse_error(text_buf, code, sizeof(code), msg, sizeof(msg))) {
                    if (s_callbacks.on_error) s_callbacks.on_error(code, msg);
                }
            }
        } else if (data->op_code == 0x02) {
            // Binary frame (TTS audio stream PCM)
            if (s_callbacks.on_tts_chunk && data->data_len > 0) {
                s_callbacks.on_tts_chunk((const uint8_t *)data->data_ptr, data->data_len);
            }
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        break;
    }
}
#endif

esp_err_t app_ws_init(const app_ws_callbacks_t *callbacks)
{
    if (callbacks) {
        s_callbacks = *callbacks;
    }
    s_state = APP_WS_DISCONNECTED;
    return ESP_OK;
}

esp_err_t app_ws_connect(const char *ws_url, const char *jwt_token)
{
    if (!ws_url || !jwt_token || strlen(jwt_token) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

#ifndef TEST_HOST
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s/api/v1/passport/ws", ws_url);

    char auth_header[MAX_JWT_TOKEN_LEN + 32];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s\r\n", jwt_token);

    esp_websocket_client_config_t config = {
        .uri = full_url,
        .headers = auth_header,
        .buffer_size = 4096,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    if (s_client) {
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
    }

    s_client = esp_websocket_client_init(&config);
    if (!s_client) {
        return ESP_FAIL;
    }

    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    s_state = APP_WS_CONNECTING;
    return esp_websocket_client_start(s_client);
#else
    s_state = APP_WS_CONNECTED;
    if (s_callbacks.on_ready) {
        s_callbacks.on_ready("dev-001", "baby-001", "好好");
    }
    return ESP_OK;
#endif
}

esp_err_t app_ws_disconnect(void)
{
#ifndef TEST_HOST
    if (s_client) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
    }
#endif
    s_state = APP_WS_DISCONNECTED;
    return ESP_OK;
}

bool app_ws_is_connected(void)
{
    return s_state == APP_WS_CONNECTED;
}

esp_err_t app_ws_send_audio_start(const char *turn_id)
{
    if (s_state != APP_WS_CONNECTED) return ESP_ERR_INVALID_STATE;
    char buf[256];
    size_t len = app_protocol_build_audio_start(buf, sizeof(buf), turn_id);
#ifndef TEST_HOST
    int res = esp_websocket_client_send_text(s_client, buf, len, portMAX_DELAY);
    return (res >= 0) ? ESP_OK : ESP_FAIL;
#else
    (void)len;
    return ESP_OK;
#endif
}

esp_err_t app_ws_send_audio_chunk(const uint8_t *pcm_data, size_t len)
{
    if (s_state != APP_WS_CONNECTED || !pcm_data || len == 0) return ESP_ERR_INVALID_STATE;
#ifndef TEST_HOST
    int res = esp_websocket_client_send_bin(s_client, (const char *)pcm_data, len, portMAX_DELAY);
    return (res >= 0) ? ESP_OK : ESP_FAIL;
#else
    return ESP_OK;
#endif
}

esp_err_t app_ws_send_audio_end(const char *turn_id)
{
    if (s_state != APP_WS_CONNECTED) return ESP_ERR_INVALID_STATE;
    char buf[256];
    size_t len = app_protocol_build_audio_end(buf, sizeof(buf), turn_id);
#ifndef TEST_HOST
    int res = esp_websocket_client_send_text(s_client, buf, len, portMAX_DELAY);
    return (res >= 0) ? ESP_OK : ESP_FAIL;
#else
    (void)len;
    return ESP_OK;
#endif
}

esp_err_t app_ws_send_card_confirm(const char *run_id, const char *plan_hash, const char *action_id)
{
    if (s_state != APP_WS_CONNECTED) return ESP_ERR_INVALID_STATE;
    char buf[512];
    size_t len = app_protocol_build_card_confirm(buf, sizeof(buf), run_id, plan_hash, action_id, "req-1");
#ifndef TEST_HOST
    int res = esp_websocket_client_send_text(s_client, buf, len, portMAX_DELAY);
    return (res >= 0) ? ESP_OK : ESP_FAIL;
#else
    (void)len;
    return ESP_OK;
#endif
}

esp_err_t app_ws_send_tts_interrupt(const char *turn_id)
{
    if (s_state != APP_WS_CONNECTED) return ESP_ERR_INVALID_STATE;
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "{\"v\":1,\"type\":\"tts.interrupt\",\"turnId\":\"%s\"}", turn_id ? turn_id : "");
#ifndef TEST_HOST
    int res = esp_websocket_client_send_text(s_client, buf, len, portMAX_DELAY);
    return (res >= 0) ? ESP_OK : ESP_FAIL;
#else
    (void)len;
    return ESP_OK;
#endif
}
