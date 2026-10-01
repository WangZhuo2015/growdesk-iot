#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "app_cards.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_WS_DISCONNECTED = 0,
    APP_WS_CONNECTING,
    APP_WS_CONNECTED,
} app_ws_state_t;

typedef struct {
    void (*on_ready)(const char *device_id, const char *baby_id, const char *baby_name);
    void (*on_asr_final)(const char *turn_id, const char *text);
    void (*on_assistant_final)(const char *turn_id, const char *text);
    void (*on_card_present)(const passport_proposal_t *proposal);
    void (*on_card_saved)(const char *run_id, const char *entity_type, const char *record_id);
    void (*on_tts_start)(const char *turn_id, int sample_rate, int channels);
    void (*on_tts_chunk)(const uint8_t *data, size_t len);
    void (*on_tts_end)(const char *turn_id);
    void (*on_error)(const char *code, const char *message);
    void (*on_disconnected)(void);
} app_ws_callbacks_t;

esp_err_t app_ws_init(const app_ws_callbacks_t *callbacks);
esp_err_t app_ws_connect(const char *ws_url, const char *jwt_token);
esp_err_t app_ws_disconnect(void);
bool app_ws_is_connected(void);

// Outgoing messages
esp_err_t app_ws_send_audio_start(const char *turn_id);
esp_err_t app_ws_send_audio_chunk(const uint8_t *pcm_data, size_t len);
esp_err_t app_ws_send_audio_end(const char *turn_id);
esp_err_t app_ws_send_card_confirm(const char *run_id, const char *plan_hash, const char *action_id);
esp_err_t app_ws_send_tts_interrupt(const char *turn_id);

#ifdef __cplusplus
}
#endif
