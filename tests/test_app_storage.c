#include <assert.h>
#include <stdio.h>
#include <string.h>

#define HOST_TEST 1
#include "../main/app_storage.c"

int main(void)
{
    assert(app_storage_init() == ESP_OK);
    assert(app_storage_has_device_credentials() == false);

    // Save and retrieve credentials
    assert(app_storage_save_device_credentials("dev-uuid-1234", "cred-secret-5678") == ESP_OK);
    assert(app_storage_has_device_credentials() == true);

    char dev_id[64] = {0};
    char dev_cred[128] = {0};
    assert(app_storage_get_device_credentials(dev_id, sizeof(dev_id), dev_cred, sizeof(dev_cred)) == ESP_OK);
    assert(strcmp(dev_id, "dev-uuid-1234") == 0);
    assert(strcmp(dev_cred, "cred-secret-5678") == 0);

    // Save and retrieve baby info
    char baby_id[64] = {0};
    char baby_name[64] = {0};
    assert(app_storage_save_baby_info("baby-uuid-9999", "好好") == ESP_OK);
    assert(app_storage_get_baby_info(baby_id, sizeof(baby_id), baby_name, sizeof(baby_name)) == ESP_OK);
    assert(strcmp(baby_id, "baby-uuid-9999") == 0);
    assert(strcmp(baby_name, "好好") == 0);

    // Server URL
    char url[128] = {0};
    assert(app_storage_get_server_url(url, sizeof(url)) == ESP_OK);
    assert(strcmp(url, DEFAULT_GROWDESK_SERVER_URL) == 0);

    assert(app_storage_save_server_url("https://custom.growdesk.example") == ESP_OK);
    assert(app_storage_get_server_url(url, sizeof(url)) == ESP_OK);
    assert(strcmp(url, "https://custom.growdesk.example") == 0);

    // Clear pairing
    assert(app_storage_clear_pairing() == ESP_OK);
    assert(app_storage_has_device_credentials() == false);

    puts("GrowDesk app storage tests: PASS");
    return 0;
}
