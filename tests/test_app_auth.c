#include "app_auth.h"
#include "app_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Forward declaration of mock helper from app_auth.c
void test_auth_set_mock_response(int status_code, const char *resp);

int main(void)
{
    printf("Running test_app_auth...\n");

    assert(app_storage_init() == ESP_OK);
    app_storage_clear_pairing();

    assert(app_auth_init("https://test.growdesk.local") == ESP_OK);
    assert(strcmp(app_auth_get_server_url(), "https://test.growdesk.local") == 0);
    assert(!app_auth_is_paired());
    assert(!app_auth_has_valid_token());
    assert(app_auth_get_access_token() == NULL);

    // 1. Test pairing request
    const char *mock_pairing_resp =
        "{\"data\":{\"pairingId\":\"pair-uuid-1234\",\"pairCode\":\"7K4P-M2QF\","
        "\"pollToken\":\"poll-token-secret\",\"expiresAt\":\"2026-10-01T00:00:00Z\"}}";
    test_auth_set_mock_response(201, mock_pairing_resp);

    app_auth_pairing_info_t pairing_info;
    assert(app_auth_request_pairing(&pairing_info) == ESP_OK);
    assert(strcmp(pairing_info.pairing_id, "pair-uuid-1234") == 0);
    assert(strcmp(pairing_info.pair_code, "7K4P-M2QF") == 0);
    assert(strcmp(pairing_info.poll_token, "poll-token-secret") == 0);

    // 2. Test poll pending
    const char *mock_poll_pending = "{\"data\":{\"status\":\"pending\"}}";
    test_auth_set_mock_response(200, mock_poll_pending);
    bool completed = false, expired = false;
    assert(app_auth_poll_pairing_status("pair-uuid-1234", "poll-token-secret", &completed, &expired) == ESP_OK);
    assert(!completed);
    assert(!expired);

    // 3. Test poll completed
    const char *mock_poll_completed =
        "{\"data\":{\"status\":\"completed\",\"deviceId\":\"dev-001\","
        "\"deviceCredential\":\"super-secret-cred-32-bytes-123456\","
        "\"familyId\":\"fam-001\",\"babyId\":\"baby-001\"}}";
    test_auth_set_mock_response(200, mock_poll_completed);
    assert(app_auth_poll_pairing_status("pair-uuid-1234", "poll-token-secret", &completed, &expired) == ESP_OK);
    assert(completed);
    assert(!expired);

    // Verify credentials saved to storage
    assert(app_auth_is_paired());

    // 4. Test token refresh
    const char *mock_token_resp =
        "{\"data\":{\"accessToken\":\"mock-jwt-access-token-12345\","
        "\"expiresIn\":900,\"tokenType\":\"Bearer\"}}";
    test_auth_set_mock_response(200, mock_token_resp);
    assert(app_auth_refresh_token() == ESP_OK);
    assert(app_auth_has_valid_token());
    assert(strcmp(app_auth_get_access_token(), "mock-jwt-access-token-12345") == 0);

    // 5. Test token invalidation
    app_auth_invalidate_token();
    assert(!app_auth_has_valid_token());
    assert(app_auth_get_access_token() == NULL);

    printf("test_app_auth: PASS\n");
    return 0;
}
