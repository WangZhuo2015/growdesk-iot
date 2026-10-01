#include "app_network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int s_callback_invoked = 0;
static app_network_status_t s_last_status = APP_NETWORK_DISCONNECTED;
static char s_last_ip[32] = {0};

static void test_network_cb(app_network_status_t status, const char *ip)
{
    s_callback_invoked++;
    s_last_status = status;
    if (ip) {
        strncpy(s_last_ip, ip, sizeof(s_last_ip) - 1);
    } else {
        s_last_ip[0] = '\0';
    }
}

int main(void)
{
    printf("Running test_app_network...\n");

    assert(app_network_init() == ESP_OK);
    assert(!app_network_is_connected());
    assert(app_network_get_status() == APP_NETWORK_DISCONNECTED);

    assert(app_network_set_event_callback(test_network_cb) == ESP_OK);

    // Invalid arguments
    assert(app_network_connect(NULL, "password") == ESP_ERR_INVALID_ARG);
    assert(app_network_connect("", "password") == ESP_ERR_INVALID_ARG);

    // Successful connection
    assert(app_network_connect("MyHomeWifi", "secret123") == ESP_OK);
    assert(app_network_is_connected());
    assert(app_network_get_status() == APP_NETWORK_CONNECTED);
    assert(strcmp(app_network_get_ip(), "192.168.1.100") == 0);
    assert(s_callback_invoked > 0);
    assert(s_last_status == APP_NETWORK_CONNECTED);
    assert(strcmp(s_last_ip, "192.168.1.100") == 0);

    // Disconnect
    assert(app_network_disconnect() == ESP_OK);
    assert(!app_network_is_connected());
    assert(app_network_get_status() == APP_NETWORK_DISCONNECTED);
    assert(s_last_status == APP_NETWORK_DISCONNECTED);

    printf("test_app_network: PASS\n");
    return 0;
}
