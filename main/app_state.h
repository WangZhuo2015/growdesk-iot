#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_WIFI_PROVISIONING,
    APP_STATE_PAIRING,
    APP_STATE_IDLE,
    APP_STATE_LISTENING,
    APP_STATE_THINKING,
    APP_STATE_SPEAKING,
    APP_STATE_PROPOSAL_CARD,
    APP_STATE_CONFIRMING,
    APP_STATE_SAVED,
    APP_STATE_OFFLINE,
    APP_STATE_AUTH_EXPIRED,
    APP_STATE_PAIRING_REQUIRED,
    APP_STATE_ERROR,
    APP_STATE_RESULT_UNKNOWN,
} app_state_t;

typedef void (*app_state_change_cb_t)(app_state_t old_state, app_state_t new_state, void *user_data);

void app_state_init(app_state_change_cb_t cb, void *user_data);
app_state_t app_state_get(void);
bool app_state_set(app_state_t new_state);
const char *app_state_name(app_state_t state);
bool app_state_can_transition(app_state_t from, app_state_t to);

#ifdef __cplusplus
}
#endif
