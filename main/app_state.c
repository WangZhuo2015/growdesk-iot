#include "app_state.h"
#include <stddef.h>

static app_state_t s_current_state = APP_STATE_BOOT;
static app_state_change_cb_t s_cb = NULL;
static void *s_user_data = NULL;

void app_state_init(app_state_change_cb_t cb, void *user_data)
{
    s_current_state = APP_STATE_BOOT;
    s_cb = cb;
    s_user_data = user_data;
}

app_state_t app_state_get(void)
{
    return s_current_state;
}

const char *app_state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:               return "BOOT";
    case APP_STATE_WIFI_PROVISIONING:  return "WIFI_PROVISIONING";
    case APP_STATE_PAIRING:            return "PAIRING";
    case APP_STATE_IDLE:               return "IDLE";
    case APP_STATE_LISTENING:          return "LISTENING";
    case APP_STATE_THINKING:           return "THINKING";
    case APP_STATE_SPEAKING:           return "SPEAKING";
    case APP_STATE_PROPOSAL_CARD:      return "PROPOSAL_CARD";
    case APP_STATE_CONFIRMING:         return "CONFIRMING";
    case APP_STATE_SAVED:              return "SAVED";
    case APP_STATE_OFFLINE:            return "OFFLINE";
    case APP_STATE_AUTH_EXPIRED:       return "AUTH_EXPIRED";
    case APP_STATE_PAIRING_REQUIRED:   return "PAIRING_REQUIRED";
    case APP_STATE_ERROR:              return "ERROR";
    case APP_STATE_RESULT_UNKNOWN:     return "RESULT_UNKNOWN";
    default:                           return "UNKNOWN";
    }
}

bool app_state_can_transition(app_state_t from, app_state_t to)
{
    if (from == to) {
        return true;
    }

    // Any state can go to OFFLINE, ERROR, or PAIRING_REQUIRED
    if (to == APP_STATE_OFFLINE || to == APP_STATE_ERROR || to == APP_STATE_PAIRING_REQUIRED) {
        return true;
    }

    switch (from) {
    case APP_STATE_BOOT:
        return (to == APP_STATE_WIFI_PROVISIONING ||
                to == APP_STATE_PAIRING ||
                to == APP_STATE_IDLE ||
                to == APP_STATE_AUTH_EXPIRED);

    case APP_STATE_WIFI_PROVISIONING:
        return (to == APP_STATE_PAIRING ||
                to == APP_STATE_IDLE ||
                to == APP_STATE_BOOT);

    case APP_STATE_PAIRING:
        return (to == APP_STATE_IDLE ||
                to == APP_STATE_WIFI_PROVISIONING);

    case APP_STATE_IDLE:
        return (to == APP_STATE_LISTENING ||
                to == APP_STATE_PROPOSAL_CARD ||
                to == APP_STATE_SPEAKING ||
                to == APP_STATE_AUTH_EXPIRED ||
                to == APP_STATE_WIFI_PROVISIONING);

    case APP_STATE_LISTENING:
        return (to == APP_STATE_THINKING ||
                to == APP_STATE_IDLE);

    case APP_STATE_THINKING:
        return (to == APP_STATE_SPEAKING ||
                to == APP_STATE_PROPOSAL_CARD ||
                to == APP_STATE_IDLE);

    case APP_STATE_SPEAKING:
        return (to == APP_STATE_IDLE ||
                to == APP_STATE_LISTENING ||
                to == APP_STATE_PROPOSAL_CARD);

    case APP_STATE_PROPOSAL_CARD:
        return (to == APP_STATE_CONFIRMING ||
                to == APP_STATE_LISTENING ||
                to == APP_STATE_IDLE);

    case APP_STATE_CONFIRMING:
        return (to == APP_STATE_SAVED ||
                to == APP_STATE_RESULT_UNKNOWN ||
                to == APP_STATE_IDLE);

    case APP_STATE_SAVED:
        return (to == APP_STATE_IDLE ||
                to == APP_STATE_SPEAKING);

    case APP_STATE_OFFLINE:
        return (to == APP_STATE_BOOT ||
                to == APP_STATE_IDLE ||
                to == APP_STATE_PAIRING ||
                to == APP_STATE_WIFI_PROVISIONING ||
                to == APP_STATE_RESULT_UNKNOWN);

    case APP_STATE_AUTH_EXPIRED:
        return (to == APP_STATE_IDLE ||
                to == APP_STATE_PAIRING);

    case APP_STATE_PAIRING_REQUIRED:
        return (to == APP_STATE_PAIRING ||
                to == APP_STATE_WIFI_PROVISIONING);

    case APP_STATE_ERROR:
        return (to == APP_STATE_IDLE ||
                to == APP_STATE_BOOT);

    case APP_STATE_RESULT_UNKNOWN:
        return (to == APP_STATE_SAVED ||
                to == APP_STATE_IDLE ||
                to == APP_STATE_ERROR);

    default:
        return false;
    }
}

bool app_state_set(app_state_t new_state)
{
    if (!app_state_can_transition(s_current_state, new_state)) {
        return false;
    }

    app_state_t old_state = s_current_state;
    s_current_state = new_state;

    if (s_cb != NULL && old_state != new_state) {
        s_cb(old_state, new_state, s_user_data);
    }
    return true;
}
