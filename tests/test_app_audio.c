#include "app_audio.h"
#include <assert.h>
#include <stdio.h>

static int s_chunks_received = 0;

static void test_chunk_cb(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    s_chunks_received++;
}

int main(void)
{
    printf("Running test_app_audio...\n");

    assert(app_audio_init() == ESP_OK);
    assert(!app_audio_is_recording());
    assert(!app_audio_is_playing());

    // Start recording
    assert(app_audio_start_recording(test_chunk_cb) == ESP_OK);
    assert(app_audio_is_recording());
    assert(!app_audio_is_playing());

    // Start playback (should automatically stop recording)
    assert(app_audio_start_playback(24000, 1) == ESP_OK);
    assert(app_audio_is_playing());
    assert(!app_audio_is_recording());

    // Play chunk
    uint8_t dummy_pcm[320] = {0};
    assert(app_audio_play_chunk(dummy_pcm, sizeof(dummy_pcm)) == ESP_OK);

    // Stop playback
    assert(app_audio_stop_playback() == ESP_OK);
    assert(!app_audio_is_playing());

    // Stop recording
    assert(app_audio_stop_recording() == ESP_OK);
    assert(!app_audio_is_recording());

    printf("test_app_audio: PASS\n");
    return 0;
}
