#include "app_ws.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool s_ready_called = false;
static char s_ready_baby_name[64] = {0};

static void test_on_ready(const char *dev_id, const char *baby_id, const char *baby_name)
{
    (void)dev_id; (void)baby_id;
    s_ready_called = true;
    if (baby_name) {
        strncpy(s_ready_baby_name, baby_name, sizeof(s_ready_baby_name) - 1);
    }
}

int main(void)
{
    printf("Running test_app_ws...\n");

    app_ws_callbacks_t cbs = {
        .on_ready = test_on_ready,
    };
    assert(app_ws_init(&cbs) == ESP_OK);
    assert(!app_ws_is_connected());

    // Connect with mock
    assert(app_ws_connect("wss://example.com", "mock-token-xyz") == ESP_OK);
    assert(app_ws_is_connected());
    assert(s_ready_called);
    assert(strcmp(s_ready_baby_name, "好好") == 0);

    // Test send messages
    assert(app_ws_send_audio_start("turn-01") == ESP_OK);
    uint8_t pcm[320] = {0};
    assert(app_ws_send_audio_chunk(pcm, sizeof(pcm)) == ESP_OK);
    assert(app_ws_send_audio_end("turn-01") == ESP_OK);
    assert(app_ws_send_card_confirm("run-01", "hash-01", "act-01") == ESP_OK);
    assert(app_ws_send_tts_interrupt("turn-01") == ESP_OK);

    // Disconnect
    assert(app_ws_disconnect() == ESP_OK);
    assert(!app_ws_is_connected());

    printf("test_app_ws: PASS\n");
    return 0;
}
