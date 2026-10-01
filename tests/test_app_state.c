#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/app_state.c"

static int s_state_changes = 0;
static app_state_t s_last_old = APP_STATE_BOOT;
static app_state_t s_last_new = APP_STATE_BOOT;

static void on_state_change(app_state_t old_state, app_state_t new_state, void *user_data)
{
    assert(user_data == (void *)0x1234);
    s_last_old = old_state;
    s_last_new = new_state;
    s_state_changes++;
}

int main(void)
{
    app_state_init(on_state_change, (void *)0x1234);
    assert(app_state_get() == APP_STATE_BOOT);
    assert(strcmp(app_state_name(APP_STATE_BOOT), "BOOT") == 0);

    // Boot -> Pairing
    assert(app_state_set(APP_STATE_PAIRING) == true);
    assert(app_state_get() == APP_STATE_PAIRING);
    assert(s_state_changes == 1);
    assert(s_last_old == APP_STATE_BOOT && s_last_new == APP_STATE_PAIRING);

    // Pairing -> Idle
    assert(app_state_set(APP_STATE_IDLE) == true);
    assert(app_state_get() == APP_STATE_IDLE);

    // Idle -> Listening (PTT pressed)
    assert(app_state_set(APP_STATE_LISTENING) == true);
    assert(app_state_get() == APP_STATE_LISTENING);

    // Listening -> Thinking (PTT released)
    assert(app_state_set(APP_STATE_THINKING) == true);
    assert(app_state_get() == APP_STATE_THINKING);

    // Thinking -> Proposal Card
    assert(app_state_set(APP_STATE_PROPOSAL_CARD) == true);
    assert(app_state_get() == APP_STATE_PROPOSAL_CARD);

    // Proposal Card -> Confirming (OK pressed)
    assert(app_state_set(APP_STATE_CONFIRMING) == true);
    assert(app_state_get() == APP_STATE_CONFIRMING);

    // Confirming -> Saved
    assert(app_state_set(APP_STATE_SAVED) == true);
    assert(app_state_get() == APP_STATE_SAVED);

    // Saved -> Idle
    assert(app_state_set(APP_STATE_IDLE) == true);
    assert(app_state_get() == APP_STATE_IDLE);

    // Idle -> Speaking
    assert(app_state_set(APP_STATE_SPEAKING) == true);
    // Speaking -> Listening (interrupt)
    assert(app_state_set(APP_STATE_LISTENING) == true);

    // Invalid transition: Listening directly to Saved
    assert(app_state_can_transition(APP_STATE_LISTENING, APP_STATE_SAVED) == false);
    assert(app_state_set(APP_STATE_SAVED) == false);
    assert(app_state_get() == APP_STATE_LISTENING);

    // Any state can transition to Error or Offline
    assert(app_state_set(APP_STATE_OFFLINE) == true);
    assert(app_state_get() == APP_STATE_OFFLINE);

    puts("GrowDesk app state machine tests: PASS");
    return 0;
}
