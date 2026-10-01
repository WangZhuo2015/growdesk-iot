#include "app_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *skip_whitespace(const char *p)
{
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    return p;
}

// Return the byte after a complete JSON string, skipping escaped quotes.
static const char *json_string_end(const char *p)
{
    if (!p || *p != '"') return NULL;
    for (p++; *p; p++) {
        if (*p == '"') return p + 1;
        if ((unsigned char)*p < 0x20) return NULL;
        if (*p == '\\') {
            p++;
            if (!*p) return NULL;
        }
    }
    return NULL;
}

// Bound every lookup to its own container. In particular, a card field must
// never borrow an optional unit/emphasis from a following field. A small fixed
// stack also rejects excessive nesting without heap allocation or recursion.
static const char *json_container_end(const char *p)
{
    char closing[16];
    size_t depth = 0;
    if (!p || (*p != '{' && *p != '[')) return NULL;
    while (*p) {
        if (*p == '"') {
            p = json_string_end(p);
            if (!p) return NULL;
            continue;
        }
        if (*p == '{' || *p == '[') {
            if (depth == sizeof(closing)) return NULL;
            closing[depth++] = (*p == '{') ? '}' : ']';
        } else if (*p == '}' || *p == ']') {
            if (depth == 0 || closing[depth - 1] != *p) return NULL;
            if (--depth == 0) return p + 1;
        }
        p++;
    }
    return NULL;
}

static const char *find_key(const char *json, const char *key)
{
    if (!json || !key) return NULL;
    const char *p = skip_whitespace(json);
    const char *end = json_container_end(p);
    if (!end) return NULL;

    const size_t key_len = strlen(key);
    const char *nested_match = NULL;
    size_t depth = 0;
    while (p < end) {
        if (*p == '"') {
            const char *after_string = json_string_end(p);
            if (!after_string) return NULL;
            const char *after = skip_whitespace(after_string);
            if (*after == ':' && (size_t)(after_string - p - 2) == key_len &&
                memcmp(p + 1, key, key_len) == 0) {
                const char *value = skip_whitespace(after + 1);
                if (depth == 1) return value;
                if (!nested_match) nested_match = value;
            }
            p = after_string;
            continue;
        }
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        p++;
    }
    // Preserve lookups through HTTP data envelopes and nested audioFormat.
    return nested_match;
}

bool app_json_get_string(const char *json, const char *key, char *out, size_t max_len)
{
    if (!out || max_len == 0) return false;
    out[0] = '\0';

    const char *val = find_key(json, key);
    if (!val || *val != '"') return false;
    val++; // skip leading quote

    size_t written = 0;
    while (*val && *val != '"' && written + 1 < max_len) {
        if (*val == '\\' && *(val + 1)) {
            val++;
            switch (*val) {
            case '"':  out[written++] = '"'; break;
            case '\\': out[written++] = '\\'; break;
            case '/':  out[written++] = '/'; break;
            case 'n':  out[written++] = '\n'; break;
            case 'r':  out[written++] = '\r'; break;
            case 't':  out[written++] = '\t'; break;
            default:   out[written++] = *val; break;
            }
        } else {
            out[written++] = *val;
        }
        val++;
    }
    if (*val != '"') {
        out[0] = '\0';
        return false;
    }
    out[written] = '\0';
    return true;
}

bool app_json_get_int(const char *json, const char *key, int *out)
{
    if (!out) return false;
    const char *val = find_key(json, key);
    if (!val) return false;

    char *endptr;
    long n = strtol(val, &endptr, 10);
    if (endptr == val) return false;
    *out = (int)n;
    return true;
}

bool app_json_get_bool(const char *json, const char *key, bool *out)
{
    if (!out) return false;
    const char *val = find_key(json, key);
    if (!val) return false;

    if (strncmp(val, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(val, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

size_t app_protocol_build_hello(char *buf, size_t max_len, const char *firmware_version)
{
    int ret = snprintf(buf, max_len,
        "{\"v\":1,\"type\":\"hello\",\"firmwareVersion\":\"%s\","
        "\"audioInput\":{\"codec\":\"pcm_s16le\",\"sampleRate\":16000,\"channels\":1}}",
        firmware_version ? firmware_version : "1.0.0");
    return (ret > 0 && (size_t)ret < max_len) ? (size_t)ret : 0;
}

size_t app_protocol_build_audio_start(char *buf, size_t max_len, const char *turn_id)
{
    int ret = snprintf(buf, max_len,
        "{\"v\":1,\"type\":\"audio.start\",\"turnId\":\"%s\"}",
        turn_id ? turn_id : "");
    return (ret > 0 && (size_t)ret < max_len) ? (size_t)ret : 0;
}

size_t app_protocol_build_audio_end(char *buf, size_t max_len, const char *turn_id)
{
    int ret = snprintf(buf, max_len,
        "{\"v\":1,\"type\":\"audio.end\",\"turnId\":\"%s\"}",
        turn_id ? turn_id : "");
    return (ret > 0 && (size_t)ret < max_len) ? (size_t)ret : 0;
}

size_t app_protocol_build_card_confirm(char *buf, size_t max_len, const char *run_id,
                                      const char *plan_hash, const char *action_id,
                                      const char *client_request_id)
{
    int ret = snprintf(buf, max_len,
        "{\"v\":1,\"type\":\"card.confirm\",\"runId\":\"%s\",\"planHash\":\"%s\","
        "\"actionIds\":[\"%s\"],\"clientRequestId\":\"%s\"}",
        run_id ? run_id : "",
        plan_hash ? plan_hash : "",
        action_id ? action_id : "",
        client_request_id ? client_request_id : "");
    return (ret > 0 && (size_t)ret < max_len) ? (size_t)ret : 0;
}

size_t app_protocol_build_resume(char *buf, size_t max_len, const char *run_id, int64_t after_seq)
{
    int ret = snprintf(buf, max_len,
        "{\"type\":\"resume\",\"runId\":\"%s\",\"afterSeq\":\"%lld\"}",
        run_id ? run_id : "", (long long)after_seq);
    return (ret > 0 && (size_t)ret < max_len) ? (size_t)ret : 0;
}

bool app_protocol_get_type(const char *json, char *type_buf, size_t max_len)
{
    return app_json_get_string(json, "type", type_buf, max_len);
}

bool app_protocol_parse_ready(const char *json, char *device_id, size_t id_len,
                              char *baby_id, size_t bid_len, char *baby_name, size_t name_len)
{
    if (!json) return false;
    if (device_id) app_json_get_string(json, "deviceId", device_id, id_len);

    const char *baby_obj = find_key(json, "baby");
    if (baby_obj && *baby_obj == '{') {
        if (baby_id) app_json_get_string(baby_obj, "id", baby_id, bid_len);
        if (baby_name) app_json_get_string(baby_obj, "name", baby_name, name_len);
        return true;
    }
    return false;
}

bool app_protocol_parse_asr(const char *json, char *text_buf, size_t text_len, bool *is_final)
{
    char type[32] = {0};
    if (!app_protocol_get_type(json, type, sizeof(type))) return false;

    if (strcmp(type, "asr.final") == 0) {
        if (is_final) *is_final = true;
        return app_json_get_string(json, "text", text_buf, text_len);
    }
    if (strcmp(type, "asr.partial") == 0) {
        if (is_final) *is_final = false;
        return app_json_get_string(json, "text", text_buf, text_len);
    }
    return false;
}

bool app_protocol_parse_assistant(const char *json, char *text_buf, size_t text_len, bool *is_final)
{
    char type[32] = {0};
    if (!app_protocol_get_type(json, type, sizeof(type))) return false;

    if (strcmp(type, "assistant.final") == 0) {
        if (is_final) *is_final = true;
        return app_json_get_string(json, "text", text_buf, text_len);
    }
    if (strcmp(type, "assistant.delta") == 0) {
        if (is_final) *is_final = false;
        return app_json_get_string(json, "text", text_buf, text_len);
    }
    return false;
}

bool app_protocol_parse_card(const char *json, passport_proposal_t *proposal)
{
    if (!json || !proposal) return false;
    memset(proposal, 0, sizeof(*proposal));

    if (!app_json_get_string(json, "runId", proposal->run_id, sizeof(proposal->run_id))) {
        return false;
    }

    const char *conf = find_key(json, "confirmation");
    if (conf && *conf == '{') {
        app_json_get_string(conf, "planHash", proposal->plan_hash, sizeof(proposal->plan_hash));
        app_json_get_string(conf, "expiresAt", proposal->expires_at, sizeof(proposal->expires_at));

        const char *actions = find_key(conf, "actionIds");
        if (actions && *actions == '[') {
            const char *act = actions + 1;
            while (*act && *act != '"' && *act != ']') act++;
            if (*act == '"') {
                act++;
                size_t w = 0;
                while (*act && *act != '"' && w + 1 < sizeof(proposal->action_id)) {
                    proposal->action_id[w++] = *act++;
                }
                if (*act != '"') return false;
                proposal->action_id[w] = '\0';
            }
        }
    }

    const char *card = find_key(json, "card");
    if (!card || *card != '{') return false;

    int schema_ver = 1;
    app_json_get_int(card, "schemaVersion", &schema_ver);
    proposal->card.schema_version = (uint8_t)schema_ver;

    app_json_get_string(card, "kind", proposal->card.kind, sizeof(proposal->card.kind));
    app_json_get_string(card, "entityType", proposal->card.entity_type, sizeof(proposal->card.entity_type));
    app_json_get_string(card, "title", proposal->card.title, sizeof(proposal->card.title));
    app_json_get_string(card, "status", proposal->card.status, sizeof(proposal->card.status));
    app_json_get_string(card, "footer", proposal->card.footer, sizeof(proposal->card.footer));

    const char *fields = find_key(card, "fields");
    if (fields && *fields == '[') {
        const char *p = fields + 1;
        while (*p && *p != ']' && proposal->card.field_count < MAX_CARD_FIELDS) {
            p = skip_whitespace(p);
            if (*p == ']') break;
            if (*p == '{') {
                passport_card_field_t *f = &proposal->card.fields[proposal->card.field_count];
                memset(f, 0, sizeof(*f));
                app_json_get_string(p, "label", f->label, sizeof(f->label));
                app_json_get_string(p, "value", f->value, sizeof(f->value));
                app_json_get_string(p, "unit", f->unit, sizeof(f->unit));
                app_json_get_bool(p, "emphasis", &f->emphasis);
                proposal->card.field_count++;

                p = json_container_end(p);
                if (!p) return false;
            } else {
                p++;
            }
        }
    }

    proposal->valid = (proposal->run_id[0] != '\0' &&
                       proposal->plan_hash[0] != '\0' &&
                       proposal->action_id[0] != '\0');
    return proposal->valid;
}

bool app_protocol_parse_tts_start(const char *json, char *turn_id, size_t id_len, int *sample_rate, int *channels)
{
    if (!json) return false;
    if (turn_id) app_json_get_string(json, "turnId", turn_id, id_len);
    if (sample_rate) {
        if (!app_json_get_int(json, "sampleRate", sample_rate)) {
            *sample_rate = 24000;
        }
    }
    if (channels) {
        if (!app_json_get_int(json, "channels", channels)) {
            *channels = 1;
        }
    }
    return true;
}

bool app_protocol_parse_tts_end(const char *json, char *turn_id, size_t id_len)
{
    if (!json) return false;
    if (turn_id) app_json_get_string(json, "turnId", turn_id, id_len);
    return true;
}

bool app_protocol_parse_error(const char *json, char *code_buf, size_t code_len, char *msg_buf, size_t msg_len)
{
    if (!json) return false;
    if (code_buf) app_json_get_string(json, "code", code_buf, code_len);
    if (msg_buf) app_json_get_string(json, "message", msg_buf, msg_len);
    return true;
}
