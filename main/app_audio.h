#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_audio_chunk_cb_t)(const uint8_t *pcm_data, size_t len);

esp_err_t app_audio_init(void);

// Microphone recording (16kHz, 16bit, mono)
esp_err_t app_audio_start_recording(app_audio_chunk_cb_t cb);
esp_err_t app_audio_stop_recording(void);
bool app_audio_is_recording(void);

// Speaker playback (supports 16kHz or 24kHz, 16bit, mono)
esp_err_t app_audio_start_playback(uint32_t sample_rate, uint8_t channels);
esp_err_t app_audio_play_chunk(const uint8_t *pcm_data, size_t len);
esp_err_t app_audio_stop_playback(void);
bool app_audio_is_playing(void);

void app_audio_set_volume(uint8_t percent);

#ifdef __cplusplus
}
#endif
