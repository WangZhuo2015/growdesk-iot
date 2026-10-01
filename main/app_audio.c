#include "app_audio.h"

#include <string.h>

#ifndef TEST_HOST
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_audio";

#define RECORD_CHUNK_BYTES (1280) // 40ms at 16kHz 16-bit mono

static bool s_recording = false;
static bool s_playing = false;
static TaskHandle_t s_record_task_handle = NULL;
static app_audio_chunk_cb_t s_chunk_cb = NULL;

static void record_task(void *param)
{
    (void)param;
    uint8_t buffer[RECORD_CHUNK_BYTES];

    // Ensure 16kHz mono format for recording
    bsp_audio_set_format(16000, 16, 1);

    while (s_recording) {
        esp_err_t err = bsp_audio_read(buffer, sizeof(buffer));
        if (err == ESP_OK && s_recording) {
            if (s_chunk_cb) {
                s_chunk_cb(buffer, sizeof(buffer));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    s_record_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t app_audio_init(void)
{
    esp_err_t err = bsp_audio_init();
    if (err == ESP_OK) {
        bsp_audio_set_volume(70);
    }
    return err;
}

esp_err_t app_audio_start_recording(app_audio_chunk_cb_t cb)
{
    if (s_recording) {
        return ESP_OK;
    }
    // Stop playback if active (Half-duplex PTT)
    app_audio_stop_playback();

    s_chunk_cb = cb;
    s_recording = true;

    BaseType_t ret = xTaskCreate(record_task, "rec_task", 4096, NULL, 5, &s_record_task_handle);
    if (ret != pdPASS) {
        s_recording = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t app_audio_stop_recording(void)
{
    s_recording = false;
    return ESP_OK;
}

bool app_audio_is_recording(void)
{
    return s_recording;
}

esp_err_t app_audio_start_playback(uint32_t sample_rate, uint8_t channels)
{
    // Stop recording if active
    app_audio_stop_recording();

    esp_err_t err = bsp_audio_set_format(sample_rate, 16, channels);
    if (err == ESP_OK) {
        s_playing = true;
    }
    return err;
}

esp_err_t app_audio_play_chunk(const uint8_t *pcm_data, size_t len)
{
    if (!s_playing || !pcm_data || len == 0) {
        return ESP_OK;
    }
    return bsp_audio_write(pcm_data, len);
}

esp_err_t app_audio_stop_playback(void)
{
    s_playing = false;
    return ESP_OK;
}

bool app_audio_is_playing(void)
{
    return s_playing;
}

void app_audio_set_volume(uint8_t percent)
{
    bsp_audio_set_volume(percent);
}

#else
// TEST_HOST mock
static bool s_mock_rec = false;
static bool s_mock_play = false;
static app_audio_chunk_cb_t s_mock_chunk_cb = NULL;

esp_err_t app_audio_init(void) { return ESP_OK; }
esp_err_t app_audio_start_recording(app_audio_chunk_cb_t cb)
{
    s_mock_chunk_cb = cb;
    s_mock_rec = true;
    s_mock_play = false;
    return ESP_OK;
}
esp_err_t app_audio_stop_recording(void)
{
    s_mock_rec = false;
    return ESP_OK;
}
bool app_audio_is_recording(void) { return s_mock_rec; }
esp_err_t app_audio_start_playback(uint32_t sample_rate, uint8_t channels)
{
    (void)sample_rate; (void)channels;
    s_mock_play = true;
    s_mock_rec = false;
    return ESP_OK;
}
esp_err_t app_audio_play_chunk(const uint8_t *pcm_data, size_t len)
{
    (void)pcm_data; (void)len;
    return ESP_OK;
}
esp_err_t app_audio_stop_playback(void)
{
    s_mock_play = false;
    return ESP_OK;
}
bool app_audio_is_playing(void) { return s_mock_play; }
void app_audio_set_volume(uint8_t percent) { (void)percent; }
#endif
