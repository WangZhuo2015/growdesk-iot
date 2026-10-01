// main/main.c —— GrowDesk x FoloToy AI Passport: Cloud Direct AI Companion
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"

#include "app_state.h"
#include "app_storage.h"
#include "app_network.h"
#include "app_provisioning.h"
#include "app_auth.h"
#include "app_audio.h"
#include "app_ws.h"
#include "app_ui.h"
#include "app_cards.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "growdesk_main";

#define INPUT_QUEUE_DEPTH 16

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue = NULL;
static passport_proposal_t s_active_proposal;
static char s_current_turn_id[64] = {0};
static char s_last_assistant_text[512] = {0};

// Forward declarations of WebSocket callbacks
static void on_ws_ready(const char *device_id, const char *baby_id, const char *baby_name);
static void on_ws_asr_final(const char *turn_id, const char *text);
static void on_ws_assistant_final(const char *turn_id, const char *text);
static void on_ws_card_present(const passport_proposal_t *proposal);
static void on_ws_card_saved(const char *run_id, const char *entity_type, const char *record_id);
static void on_ws_tts_start(const char *turn_id, int sample_rate, int channels);
static void on_ws_tts_chunk(const uint8_t *data, size_t len);
static void on_ws_tts_end(const char *turn_id);
static void on_ws_error(const char *code, const char *message);
static void on_ws_disconnected(void);

static app_ws_callbacks_t s_ws_callbacks = {
    .on_ready = on_ws_ready,
    .on_asr_final = on_ws_asr_final,
    .on_assistant_final = on_ws_assistant_final,
    .on_card_present = on_ws_card_present,
    .on_card_saved = on_ws_card_saved,
    .on_tts_start = on_ws_tts_start,
    .on_tts_chunk = on_ws_tts_chunk,
    .on_tts_end = on_ws_tts_end,
    .on_error = on_ws_error,
    .on_disconnected = on_ws_disconnected,
};

static void set_state(app_state_t new_state)
{
    app_state_set(new_state);
    app_ui_set_state(new_state);
}

static void audio_chunk_recorded(const uint8_t *pcm_data, size_t len)
{
    if (app_state_get() == APP_STATE_LISTENING) {
        app_ws_send_audio_chunk(pcm_data, len);
    }
}

// Button ISR callback: strictly non-blocking queue send
static void button_callback(bsp_btn_t btn, bsp_btn_ev_t event, void *user_data)
{
    (void)user_data;
    input_event_t ev = { .btn = btn, .event = event };
    if (s_input_queue) {
        xQueueSendFromISR(s_input_queue, &ev, NULL);
    }
}

static void generate_turn_id(char *out, size_t max_len)
{
    static uint32_t counter = 0;
    snprintf(out, max_len, "turn-%lld-%" PRIu32, (long long)esp_timer_get_time(), ++counter);
}

// Audio playback completion timer callback
static void saved_delay_timer_cb(void *arg)
{
    (void)arg;
    if (app_state_get() == APP_STATE_SAVED) {
        set_state(APP_STATE_IDLE);
    }
}

// Input processing task
static void input_task(void *param)
{
    (void)param;
    input_event_t ev;

    while (1) {
        if (xQueueReceive(s_input_queue, &ev, portMAX_DELAY)) {
            app_state_t state = app_state_get();

            if (ev.btn == BSP_BTN_OK) {
                if (ev.event == BSP_BTN_PRESS) {
                    if (state == APP_STATE_SPEAKING) {
                        // Half-duplex interruption: stop playback immediately
                        app_audio_stop_playback();
                        app_ws_send_tts_interrupt(s_current_turn_id);
                        // Fallthrough to start listening
                        state = APP_STATE_IDLE;
                    }

                    if (state == APP_STATE_IDLE) {
                        // Start recording (PTT Press)
                        generate_turn_id(s_current_turn_id, sizeof(s_current_turn_id));
                        set_state(APP_STATE_LISTENING);
                        app_ws_send_audio_start(s_current_turn_id);
                        app_audio_start_recording(audio_chunk_recorded);
                    }
                } else if (ev.event == BSP_BTN_RELEASE) {
                    if (state == APP_STATE_LISTENING) {
                        // Stop recording and send audio.end (PTT Release)
                        app_audio_stop_recording();
                        app_ws_send_audio_end(s_current_turn_id);
                        set_state(APP_STATE_THINKING);
                    }
                } else if (ev.event == BSP_BTN_CLICK) {
                    if (state == APP_STATE_PROPOSAL_CARD) {
                        // Human confirmation via physical OK short-press!
                        set_state(APP_STATE_CONFIRMING);
                        app_ws_send_card_confirm(s_active_proposal.run_id,
                                                s_active_proposal.plan_hash,
                                                s_active_proposal.action_id);
                    }
                } else if (ev.event == BSP_BTN_LONG) {
                    if (state == APP_STATE_PROPOSAL_CARD || state == APP_STATE_CONFIRMING) {
                        // Cancel card confirmation
                        set_state(APP_STATE_IDLE);
                    }
                }
            } else if (ev.btn == BSP_BTN_UP || ev.btn == BSP_BTN_DOWN) {
                if (state == APP_STATE_PROPOSAL_CARD && ev.event == BSP_BTN_CLICK) {
                    // Navigate card fields if needed
                }
            }
        }
    }
}

// WebSocket event handlers
static void on_ws_ready(const char *device_id, const char *baby_id, const char *baby_name)
{
    (void)device_id;
    ESP_LOGI(TAG, "GrowDesk Voice Gateway Ready for baby: %s", baby_name);
    app_storage_save_baby_info(baby_id, baby_name);
    app_ui_set_baby_name(baby_name);
    app_ui_set_wifi_status(true, NULL);
    set_state(APP_STATE_IDLE);
}

static void on_ws_asr_final(const char *turn_id, const char *text)
{
    (void)turn_id;
    ESP_LOGI(TAG, "ASR transcript: %s", text);
}

static void on_ws_assistant_final(const char *turn_id, const char *text)
{
    (void)turn_id;
    ESP_LOGI(TAG, "Assistant reply: %s", text);
    if (text) {
        strncpy(s_last_assistant_text, text, sizeof(s_last_assistant_text) - 1);
        app_ui_set_assistant_text(text);
    }
}

static void on_ws_card_present(const passport_proposal_t *proposal)
{
    if (!proposal) return;
    ESP_LOGI(TAG, "Proposal card received: %s (%s)", proposal->card.title, proposal->card.entity_type);
    s_active_proposal = *proposal;
    set_state(APP_STATE_PROPOSAL_CARD);
    app_ui_set_proposal_card(&proposal->card);
}

static void on_ws_card_saved(const char *run_id, const char *entity_type, const char *record_id)
{
    (void)run_id; (void)entity_type; (void)record_id;
    ESP_LOGI(TAG, "Proposal confirmed and SAVED! Record ID: %s", record_id);
    set_state(APP_STATE_SAVED);

    // Return to IDLE after 2.5 seconds
    esp_timer_create_args_t timer_args = {
        .callback = saved_delay_timer_cb,
        .name = "saved_timer",
    };
    esp_timer_handle_t timer;
    if (esp_timer_create(&timer_args, &timer) == ESP_OK) {
        esp_timer_start_once(timer, 2500000); // 2.5s
    }
}

static void on_ws_tts_start(const char *turn_id, int sample_rate, int channels)
{
    (void)turn_id;
    ESP_LOGI(TAG, "TTS stream started (%d Hz, %d ch)", sample_rate, channels);
    set_state(APP_STATE_SPEAKING);
    app_audio_start_playback(sample_rate, channels);
}

static void on_ws_tts_chunk(const uint8_t *data, size_t len)
{
    if (app_state_get() == APP_STATE_SPEAKING) {
        app_audio_play_chunk(data, len);
    }
}

static void on_ws_tts_end(const char *turn_id)
{
    (void)turn_id;
    ESP_LOGI(TAG, "TTS stream ended");
    app_audio_stop_playback();
    if (app_state_get() == APP_STATE_SPEAKING) {
        set_state(APP_STATE_IDLE);
    }
}

static void on_ws_error(const char *code, const char *message)
{
    ESP_LOGE(TAG, "Cloud error: [%s] %s", code, message);
    set_state(APP_STATE_ERROR);
    app_ui_set_error(code, message, false);
}

static void on_ws_disconnected(void)
{
    ESP_LOGW(TAG, "WebSocket disconnected from Voice Gateway");
    app_ui_set_wifi_status(false, NULL);
    set_state(APP_STATE_OFFLINE);
}

static void on_blufi_provisioned(bool success, const char *ssid)
{
    if (success) {
        ESP_LOGI(TAG, "Wi-Fi provisioned successfully to SSID: %s", ssid);
    }
}

// Background network & coordinator task
static void coordinator_task(void *param)
{
    (void)param;

    // Check Wi-Fi connection
    if (!app_network_is_connected()) {
        ESP_LOGI(TAG, "Attempting Wi-Fi connection from storage...");
        app_network_start_sta();
        for (int i = 0; i < 10 && !app_network_is_connected(); i++) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    if (!app_network_is_connected()) {
        ESP_LOGI(TAG, "No Wi-Fi connected, entering BLUFI provisioning...");
        set_state(APP_STATE_WIFI_PROVISIONING);
        app_provisioning_start("BLUFI_FoloPassport", on_blufi_provisioned);

        while (!app_network_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        app_provisioning_notify_connected();
        vTaskDelay(pdMS_TO_TICKS(1500));
        app_provisioning_stop();
    }

    // Initialize audio now that Wi-Fi is connected and BluFi has freed BLE memory
    ESP_LOGI(TAG, "Initializing audio subsystem...");
    app_audio_init();

    app_ui_set_wifi_status(true, NULL);

    // Pairing flow
    while (!app_auth_is_paired()) {
        set_state(APP_STATE_PAIRING);
        app_auth_pairing_info_t pairing;
        esp_err_t err = app_auth_request_pairing(&pairing);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to request pairing, retrying in 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        app_ui_set_pairing_code(pairing.pair_code);
        ESP_LOGI(TAG, "Pairing code: %s. Awaiting user claim in GrowDesk app...", pairing.pair_code);

        bool completed = false, expired = false;
        while (!completed && !expired) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            err = app_auth_poll_pairing_status(pairing.pairing_id, pairing.poll_token, &completed, &expired);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Poll pairing status error: %d", err);
            }
        }

        if (completed) {
            ESP_LOGI(TAG, "Device successfully paired to GrowDesk!");
            break;
        }
    }

    // Connect to Cloud Voice Gateway over WebSocket
    while (1) {
        if (!app_auth_has_valid_token()) {
            ESP_LOGI(TAG, "Refreshing Passport device access token...");
            esp_err_t err = app_auth_refresh_token();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to exchange auth token: %d. Retrying in 10s...", err);
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue;
            }
        }

        const char *token = app_auth_get_access_token();
        ESP_LOGI(TAG, "Connecting to GrowDesk Voice Gateway: %s...", DEFAULT_GROWDESK_WS_URL);
        app_ws_connect(DEFAULT_GROWDESK_WS_URL, token);

        // Keep-alive loop
        while (app_ws_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(30000));
            // Proactively refresh token before 15-minute expiry
            if (!app_auth_has_valid_token()) {
                app_auth_refresh_token();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Starting GrowDesk AI Passport ===");

    // 1. Hardware peripherals
    bsp_i2c_init();
    if (bsp_display_init() == ESP_OK) {
        if (!bsp_lvgl_init()) {
            ESP_LOGE(TAG, "LVGL initialization failed!");
        }
        bsp_display_backlight(100);
    }
    bsp_battery_init();

    // 2. Storage & UI
    app_storage_init();
    app_ui_init();

    // 3. Network & Auth
    app_network_init();
    app_auth_init(NULL);
    app_ws_init(&s_ws_callbacks);

    // 4. Register button callbacks
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    bsp_button_init(button_callback, NULL);

    // 5. Start FreeRTOS worker tasks
    xTaskCreate(input_task, "input_task", 2048, NULL, 10, NULL);
    xTaskCreate(coordinator_task, "coord_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "=== GrowDesk AI Passport initialized successfully ===");
}
