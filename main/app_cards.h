#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CARD_FIELDS 8

typedef struct {
    char label[32];
    char value[64];
    char unit[16];
    bool emphasis;
} passport_card_field_t;

typedef struct {
    uint8_t schema_version;
    char kind[32];
    char entity_type[32];
    char title[64];
    char status[32];
    char footer[64];
    uint8_t field_count;
    passport_card_field_t fields[MAX_CARD_FIELDS];
} passport_card_t;

typedef struct {
    char run_id[40];
    char plan_hash[70];
    char action_id[40];
    char expires_at[32];
    passport_card_t card;
    bool valid;
} passport_proposal_t;

void app_cards_init(passport_proposal_t *proposal);
bool app_cards_parse_proposal(const char *json, passport_proposal_t *out);
void app_cards_invalidate(passport_proposal_t *proposal);

#ifdef __cplusplus
}
#endif
