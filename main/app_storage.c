#include "app_storage.h"
#include <string.h>

#if !defined(HOST_TEST) && !defined(TEST_HOST)
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "app_storage";
static const char *NVS_NAMESPACE = "growdesk";

esp_err_t app_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t get_str(const char *key, char *buf, size_t max_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t required = max_len;
    err = nvs_get_str(handle, key, buf, &required);
    nvs_close(handle);
    return err;
}

static esp_err_t set_str(const char *key, const char *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, key, val);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

bool app_storage_has_device_credentials(void)
{
    char did[64] = {0};
    char cred[128] = {0};
    if (get_str("dev_id", did, sizeof(did)) == ESP_OK && strlen(did) > 0 &&
        get_str("dev_cred", cred, sizeof(cred)) == ESP_OK && strlen(cred) > 0) {
        return true;
    }
    return false;
}

esp_err_t app_storage_get_device_credentials(char *dev_id, size_t id_len, char *dev_cred, size_t cred_len)
{
    esp_err_t err = get_str("dev_id", dev_id, id_len);
    if (err != ESP_OK) return err;
    return get_str("dev_cred", dev_cred, cred_len);
}

esp_err_t app_storage_save_device_credentials(const char *dev_id, const char *dev_cred)
{
    esp_err_t err = set_str("dev_id", dev_id);
    if (err != ESP_OK) return err;
    return set_str("dev_cred", dev_cred);
}

esp_err_t app_storage_get_baby_info(char *baby_id, size_t id_len, char *baby_name, size_t name_len)
{
    esp_err_t err = get_str("baby_id", baby_id, id_len);
    if (err != ESP_OK) return err;
    return get_str("baby_name", baby_name, name_len);
}

esp_err_t app_storage_save_baby_info(const char *baby_id, const char *baby_name)
{
    esp_err_t err = set_str("baby_id", baby_id);
    if (err != ESP_OK) return err;
    return set_str("baby_name", baby_name);
}

esp_err_t app_storage_get_server_url(char *url, size_t len)
{
    esp_err_t err = get_str("server_url", url, len);
    if (err != ESP_OK || strlen(url) == 0) {
        strncpy(url, DEFAULT_GROWDESK_SERVER_URL, len - 1);
        url[len - 1] = '\0';
        return ESP_OK;
    }
    return ESP_OK;
}

esp_err_t app_storage_save_server_url(const char *url)
{
    return set_str("server_url", url);
}

esp_err_t app_storage_clear_pairing(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_key(handle, "dev_id");
    nvs_erase_key(handle, "dev_cred");
    nvs_erase_key(handle, "baby_id");
    nvs_erase_key(handle, "baby_name");
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Pairing credentials cleared from NVS");
    return ESP_OK;
}

#else
// Host test in-memory mock implementation
static char s_mock_dev_id[64] = {0};
static char s_mock_dev_cred[128] = {0};
static char s_mock_baby_id[64] = {0};
static char s_mock_baby_name[64] = {0};
static char s_mock_server_url[128] = {0};

esp_err_t app_storage_init(void) { return ESP_OK; }

bool app_storage_has_device_credentials(void)
{
    return strlen(s_mock_dev_id) > 0 && strlen(s_mock_dev_cred) > 0;
}

esp_err_t app_storage_get_device_credentials(char *dev_id, size_t id_len, char *dev_cred, size_t cred_len)
{
    if (strlen(s_mock_dev_id) == 0) return ESP_ERR_NOT_FOUND;
    strncpy(dev_id, s_mock_dev_id, id_len - 1);
    dev_id[id_len - 1] = '\0';
    strncpy(dev_cred, s_mock_dev_cred, cred_len - 1);
    dev_cred[cred_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t app_storage_save_device_credentials(const char *dev_id, const char *dev_cred)
{
    strncpy(s_mock_dev_id, dev_id, sizeof(s_mock_dev_id) - 1);
    strncpy(s_mock_dev_cred, dev_cred, sizeof(s_mock_dev_cred) - 1);
    return ESP_OK;
}

esp_err_t app_storage_get_baby_info(char *baby_id, size_t id_len, char *baby_name, size_t name_len)
{
    if (strlen(s_mock_baby_id) == 0) return ESP_ERR_NOT_FOUND;
    strncpy(baby_id, s_mock_baby_id, id_len - 1);
    baby_id[id_len - 1] = '\0';
    strncpy(baby_name, s_mock_baby_name, name_len - 1);
    baby_name[name_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t app_storage_save_baby_info(const char *baby_id, const char *baby_name)
{
    strncpy(s_mock_baby_id, baby_id, sizeof(s_mock_baby_id) - 1);
    strncpy(s_mock_baby_name, baby_name, sizeof(s_mock_baby_name) - 1);
    return ESP_OK;
}

esp_err_t app_storage_get_server_url(char *url, size_t len)
{
    if (strlen(s_mock_server_url) == 0) {
        strncpy(url, DEFAULT_GROWDESK_SERVER_URL, len - 1);
        url[len - 1] = '\0';
    } else {
        strncpy(url, s_mock_server_url, len - 1);
        url[len - 1] = '\0';
    }
    return ESP_OK;
}

esp_err_t app_storage_save_server_url(const char *url)
{
    strncpy(s_mock_server_url, url, sizeof(s_mock_server_url) - 1);
    return ESP_OK;
}

esp_err_t app_storage_clear_pairing(void)
{
    memset(s_mock_dev_id, 0, sizeof(s_mock_dev_id));
    memset(s_mock_dev_cred, 0, sizeof(s_mock_dev_cred));
    memset(s_mock_baby_id, 0, sizeof(s_mock_baby_id));
    memset(s_mock_baby_name, 0, sizeof(s_mock_baby_name));
    return ESP_OK;
}
#endif
