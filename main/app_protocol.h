#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "app_cards.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WS_CONTROL_FRAME_SIZE (4096)

// JSON String extraction helpers
bool app_json_get_string(const char *json, const char *key, char *out, size_t max_len);
bool app_json_get_int(const char *json, const char *key, int *out);
bool app_json_get_bool(const char *json, const char *key, bool *out);

// Outgoing frames
size_t app_protocol_build_hello(char *buf, size_t max_len, const char *firmware_version);
size_t app_protocol_build_audio_start(char *buf, size_t max_len, const char *turn_id);
size_t app_protocol_build_audio_end(char *buf, size_t max_len, const char *turn_id);
size_t app_protocol_build_card_confirm(char *buf, size_t max_len, const char *run_id,
                                      const char *plan_hash, const char *action_id,
                                      const char *client_request_id);
size_t app_protocol_build_resume(char *buf, size_t max_len, const char *run_id, int64_t after_seq);

// Incoming frames
bool app_protocol_get_type(const char *json, char *type_buf, size_t max_len);
bool app_protocol_parse_ready(const char *json, char *device_id, size_t id_len,
                              char *baby_id, size_t bid_len, char *baby_name, size_t name_len);
bool app_protocol_parse_asr(const char *json, char *text_buf, size_t text_len, bool *is_final);
bool app_protocol_parse_assistant(const char *json, char *text_buf, size_t text_len, bool *is_final);
bool app_protocol_parse_card(const char *json, passport_proposal_t *proposal);
bool app_protocol_parse_tts_start(const char *json, char *turn_id, size_t id_len, int *sample_rate, int *channels);
bool app_protocol_parse_tts_end(const char *json, char *turn_id, size_t id_len);
bool app_protocol_parse_error(const char *json, char *code_buf, size_t code_len, char *msg_buf, size_t msg_len);

#ifdef __cplusplus
}
#endif
