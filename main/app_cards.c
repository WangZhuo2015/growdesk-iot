#include "app_cards.h"
#include "app_protocol.h"
#include <string.h>

void app_cards_init(passport_proposal_t *proposal)
{
    if (proposal) {
        memset(proposal, 0, sizeof(*proposal));
    }
}

bool app_cards_parse_proposal(const char *json, passport_proposal_t *out)
{
    return app_protocol_parse_card(json, out);
}

void app_cards_invalidate(passport_proposal_t *proposal)
{
    if (proposal) {
        proposal->valid = false;
        strncpy(proposal->card.status, "superseded", sizeof(proposal->card.status) - 1);
    }
}
