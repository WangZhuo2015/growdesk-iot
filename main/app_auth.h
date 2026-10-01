#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PAIR_CODE_LEN   16
#define MAX_POLL_TOKEN_LEN  128
#define MAX_PAIRING_ID_LEN  64
#define MAX_JWT_TOKEN_LEN   1024

typedef struct {
    char pairing_id[MAX_PAIRING_ID_LEN];
    char pair_code[MAX_PAIR_CODE_LEN];
    char poll_token[MAX_POLL_TOKEN_LEN];
    int64_t expires_at;
} app_auth_pairing_info_t;

esp_err_t app_auth_init(const char *server_base_url);
esp_err_t app_auth_set_server_url(const char *server_base_url);
const char *app_auth_get_server_url(void);

// Pairing flow
esp_err_t app_auth_request_pairing(app_auth_pairing_info_t *out_info);
esp_err_t app_auth_poll_pairing_status(const char *pairing_id, const char *poll_token,
                                      bool *out_completed, bool *out_expired);

// Token management
esp_err_t app_auth_refresh_token(void);
bool app_auth_has_valid_token(void);
const char *app_auth_get_access_token(void);
void app_auth_invalidate_token(void);

// Device credentials check
bool app_auth_is_paired(void);

#ifdef __cplusplus
}
#endif
