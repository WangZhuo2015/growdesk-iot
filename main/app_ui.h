#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_state.h"
#include "app_cards.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the GrowDesk Assistant UI
void app_ui_init(void);

// State and screen updates
void app_ui_set_state(app_state_t state);

// Top bar updates
void app_ui_set_baby_name(const char *name);
void app_ui_set_wifi_status(bool connected, const char *ssid);
void app_ui_set_battery(int percent);

// Content updates for specific states
void app_ui_set_pairing_code(const char *code);
void app_ui_set_recent_records(const char *feeding_info, const char *sleep_info);
void app_ui_set_assistant_text(const char *text);
void app_ui_set_proposal_card(const passport_card_t *card);
void app_ui_set_error(const char *title, const char *detail, bool saved);
void app_ui_update_visualizer(int volume_level);

#ifdef __cplusplus
}
#endif
