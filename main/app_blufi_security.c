#include "app_blufi_security.h"

#include <stdlib.h>
#include <string.h>

#ifndef TEST_HOST
#include "esp_blufi_api.h"
#include "esp_crc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"

static const char *TAG = "blufi_sec";

#define SEC_TYPE_DH_PARAM_LEN  0x00
#define SEC_TYPE_DH_PARAM_DATA 0x01
#define SEC_TYPE_DH_P          0x02
#define SEC_TYPE_DH_G          0x03
#define SEC_TYPE_DH_PUBLIC     0x04
#define DH_KEY_LEN              128
#define DH_PARAM_LEN_MAX        1024
#define PSK_LEN                 16

typedef struct {
    uint8_t public_key[DH_KEY_LEN];
    uint8_t shared_key[DH_KEY_LEN];
    size_t shared_len;
    uint8_t psk[PSK_LEN];
    uint8_t *dh_param;
    int dh_param_len;
    uint8_t iv[16];
    mbedtls_dhm_context dhm;
    mbedtls_aes_context aes;
} blufi_security_t;

static blufi_security_t *s_security;

extern void btc_blufi_report_error(esp_blufi_error_state_t state);

static int random_bytes(void *state, unsigned char *output, size_t len)
{
    (void)state;
    esp_fill_random(output, len);
    return 0;
}

void app_blufi_negotiate(uint8_t *data, int len, uint8_t **output_data,
                         int *output_len, bool *need_free)
{
    ESP_LOGI(TAG, "Negotiate called: len=%d", len);
    if (!data || len < 3) {
        ESP_LOGE(TAG, "Negotiate: invalid data format (len=%d)", len);
        btc_blufi_report_error(ESP_BLUFI_DATA_FORMAT_ERROR);
        return;
    }
    if (!s_security) {
        ESP_LOGE(TAG, "Negotiate: security context is NULL");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    uint8_t type = data[0];
    ESP_LOGI(TAG, "Negotiate packet: type=0x%02x, len=%d", type, len);

    switch (type) {
    case SEC_TYPE_DH_PARAM_LEN: {
        int param_len = (data[1] << 8) | data[2];
        ESP_LOGI(TAG, "SEC_TYPE_DH_PARAM_LEN: param_len=%d", param_len);
        if (param_len <= 0 || param_len > DH_PARAM_LEN_MAX) {
            ESP_LOGE(TAG, "Invalid DH param len: %d (max %d)", param_len, DH_PARAM_LEN_MAX);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }
        if (s_security->dh_param) {
            free(s_security->dh_param);
            s_security->dh_param = NULL;
        }
        s_security->dh_param = (uint8_t *)malloc(param_len);
        if (!s_security->dh_param) {
            s_security->dh_param_len = 0;
            ESP_LOGE(TAG, "malloc failed for param_len=%d", param_len);
            btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            return;
        }
        s_security->dh_param_len = param_len;
        break;
    }

    case SEC_TYPE_DH_PARAM_DATA: {
        ESP_LOGI(TAG, "SEC_TYPE_DH_PARAM_DATA: len=%d, expected=%d", len, s_security->dh_param_len);
        if (!s_security->dh_param) {
            ESP_LOGE(TAG, "dh_param is NULL when receiving PARAM_DATA");
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }
        if (len < s_security->dh_param_len + 1) {
            ESP_LOGE(TAG, "PARAM_DATA len=%d < expected=%d + 1", len, s_security->dh_param_len);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }

        memcpy(s_security->dh_param, data + 1, s_security->dh_param_len);
        uint8_t *param = s_security->dh_param;
        int rc = mbedtls_dhm_read_params(&s_security->dhm, &param,
                                         param + s_security->dh_param_len);
        free(s_security->dh_param);
        s_security->dh_param = NULL;
        if (rc != 0) {
            ESP_LOGE(TAG, "read DH parameters failed: -0x%04x", -rc);
            btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
            return;
        }

        const int dh_len = (int)mbedtls_dhm_get_len(&s_security->dhm);
        ESP_LOGI(TAG, "DH parsed successfully: dh_len=%d", dh_len);
        if (dh_len > DH_KEY_LEN) {
            ESP_LOGE(TAG, "DH key len %d > max %d", dh_len, DH_KEY_LEN);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }

        ESP_LOGI(TAG, "Heap before make_public: free=%u, largest_8bit=%u",
                 (unsigned int)esp_get_free_heap_size(),
                 (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        int x_size = (dh_len > 32) ? 32 : dh_len;
        rc = mbedtls_dhm_make_public(&s_security->dhm, x_size,
                                     s_security->public_key, dh_len,
                                     random_bytes, NULL);
        if (rc != 0) {
            ESP_LOGW(TAG, "make public with x_size=%d returned -0x%04x, retrying with %d...", x_size, -rc, dh_len);
            rc = mbedtls_dhm_make_public(&s_security->dhm, dh_len,
                                         s_security->public_key, dh_len,
                                         random_bytes, NULL);
        }
        if (rc != 0) {
            ESP_LOGE(TAG, "make public failed: -0x%04x", -rc);
            btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
            return;
        }

        rc = mbedtls_dhm_calc_secret(&s_security->dhm, s_security->shared_key,
                                     DH_KEY_LEN, &s_security->shared_len,
                                     random_bytes, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "calc secret failed: -0x%04x", -rc);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }

        rc = mbedtls_md5(s_security->shared_key, s_security->shared_len,
                         s_security->psk);
        if (rc != 0) {
            ESP_LOGE(TAG, "MD5 hash failed: %d", rc);
            btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
            return;
        }

        rc = mbedtls_aes_setkey_enc(&s_security->aes, s_security->psk,
                                    PSK_LEN * 8);
        if (rc != 0) {
            ESP_LOGE(TAG, "AES setkey failed: %d", rc);
            btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
            return;
        }

        *output_data = s_security->public_key;
        *output_len = dh_len;
        *need_free = false;
        ESP_LOGI(TAG, "DH security handshake SUCCESS! Sent %d bytes public key", dh_len);
        break;
    }

    case SEC_TYPE_DH_P:
    case SEC_TYPE_DH_G:
    case SEC_TYPE_DH_PUBLIC:
        ESP_LOGI(TAG, "Received optional DH packet type 0x%02x (ignored)", type);
        break;

    default:
        ESP_LOGW(TAG, "Unknown negotiate type 0x%02x", type);
        break;
    }
}

static int crypt(bool encrypt, uint8_t iv8, uint8_t *data, int len)
{
    if (!s_security || !data || len < 0) return -1;
    size_t offset = 0;
    uint8_t iv[16];
    memcpy(iv, s_security->iv, sizeof(iv));
    iv[0] = iv8;
    int mode = encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    return mbedtls_aes_crypt_cfb128(&s_security->aes, mode, len, &offset,
                                    iv, data, data) == 0 ? len : -1;
}

int app_blufi_encrypt(uint8_t iv8, uint8_t *data, int len)
{
    return crypt(true, iv8, data, len);
}

int app_blufi_decrypt(uint8_t iv8, uint8_t *data, int len)
{
    return crypt(false, iv8, data, len);
}

uint16_t app_blufi_checksum(uint8_t iv8, uint8_t *data, int len)
{
    (void)iv8;
    return esp_crc16_be(0, data, len);
}

int app_blufi_security_init(void)
{
    app_blufi_security_deinit();
    s_security = (blufi_security_t *)calloc(1, sizeof(*s_security));
    if (!s_security) return -1;
    mbedtls_dhm_init(&s_security->dhm);
    mbedtls_aes_init(&s_security->aes);
    return 0;
}

void app_blufi_security_deinit(void)
{
    if (!s_security) return;
    free(s_security->dh_param);
    mbedtls_dhm_free(&s_security->dhm);
    mbedtls_aes_free(&s_security->aes);
    memset(s_security, 0, sizeof(*s_security));
    free(s_security);
    s_security = NULL;
}

#else
// TEST_HOST Stubs
void app_blufi_negotiate(uint8_t *data, int len, uint8_t **output_data,
                         int *output_len, bool *need_free)
{
    (void)data; (void)len; (void)output_data; (void)output_len; (void)need_free;
}
int app_blufi_encrypt(uint8_t iv8, uint8_t *data, int len)
{
    (void)iv8; (void)data; return len;
}
int app_blufi_decrypt(uint8_t iv8, uint8_t *data, int len)
{
    (void)iv8; (void)data; return len;
}
uint16_t app_blufi_checksum(uint8_t iv8, uint8_t *data, int len)
{
    (void)iv8; (void)data; (void)len; return 0;
}
int app_blufi_security_init(void) { return 0; }
void app_blufi_security_deinit(void) {}
#endif
