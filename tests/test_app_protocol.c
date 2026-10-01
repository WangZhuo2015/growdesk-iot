#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/app_protocol.c"
#include "../main/app_cards.c"

int main(void)
{
    char buf[1024];

    // 1. Build hello
    size_t len = app_protocol_build_hello(buf, sizeof(buf), "1.0.0");
    assert(len > 0);
    assert(strstr(buf, "\"type\":\"hello\"") != NULL);
    assert(strstr(buf, "\"firmwareVersion\":\"1.0.0\"") != NULL);

    // 2. Parse ready
    const char *ready_json = "{\"v\":1,\"type\":\"ready\",\"deviceId\":\"dev-1234\",\"baby\":{\"id\":\"baby-5678\",\"name\":\"好好\"}}";
    char dev_id[64] = {0};
    char baby_id[64] = {0};
    char baby_name[64] = {0};
    assert(app_protocol_parse_ready(ready_json, dev_id, sizeof(dev_id), baby_id, sizeof(baby_id), baby_name, sizeof(baby_name)));
    assert(strcmp(dev_id, "dev-1234") == 0);
    assert(strcmp(baby_id, "baby-5678") == 0);
    assert(strcmp(baby_name, "好好") == 0);

    // 3. Audio start / end
    len = app_protocol_build_audio_start(buf, sizeof(buf), "turn-999");
    assert(len > 0);
    assert(strstr(buf, "\"type\":\"audio.start\"") != NULL);
    assert(strstr(buf, "\"turnId\":\"turn-999\"") != NULL);

    len = app_protocol_build_audio_end(buf, sizeof(buf), "turn-999");
    assert(len > 0);
    assert(strstr(buf, "\"type\":\"audio.end\"") != NULL);

    // 4. ASR parse
    const char *asr_json = "{\"type\":\"asr.final\",\"turnId\":\"turn-999\",\"text\":\"宝宝喝了140毫升\"}";
    char asr_text[128] = {0};
    bool is_final = false;
    assert(app_protocol_parse_asr(asr_json, asr_text, sizeof(asr_text), &is_final));
    assert(is_final == true);
    assert(strcmp(asr_text, "宝宝喝了140毫升") == 0);

    // 5. Assistant parse
    const char *asst_json = "{\"type\":\"assistant.final\",\"text\":\"整理成喂奶卡片\"}";
    char asst_text[128] = {0};
    assert(app_protocol_parse_assistant(asst_json, asst_text, sizeof(asst_text), &is_final));
    assert(is_final == true);
    assert(strcmp(asst_text, "整理成喂奶卡片") == 0);

    // 6. Card present parse
    const char *card_json = "{\"v\":1,\"type\":\"card.present\",\"runId\":\"run-abc\","
        "\"card\":{\"schemaVersion\":1,\"kind\":\"record_proposal\",\"entityType\":\"feeding\","
        "\"title\":\"喂奶记录\",\"status\":\"awaiting_confirmation\","
        "\"fields\":[{\"label\":\"奶量\",\"value\":\"140\",\"unit\":\"mL\",\"emphasis\":true},"
        "{\"label\":\"类型\",\"value\":\"配方奶\"},{\"label\":\"时间\",\"value\":\"09:32\"}],"
        "\"footer\":\"按 OK 保存\"},"
        "\"confirmation\":{\"planHash\":\"hash-xyz\",\"actionIds\":[\"act-1\"],\"expiresAt\":\"2026-05-02T03:04:05Z\"}}";

    passport_proposal_t proposal;
    assert(app_protocol_parse_card(card_json, &proposal));
    assert(proposal.valid == true);
    assert(strcmp(proposal.run_id, "run-abc") == 0);
    assert(strcmp(proposal.plan_hash, "hash-xyz") == 0);
    assert(strcmp(proposal.action_id, "act-1") == 0);
    assert(strcmp(proposal.card.title, "喂奶记录") == 0);
    assert(strcmp(proposal.card.entity_type, "feeding") == 0);
    assert(proposal.card.field_count == 3);
    assert(strcmp(proposal.card.fields[0].label, "奶量") == 0);
    assert(strcmp(proposal.card.fields[0].value, "140") == 0);
    assert(strcmp(proposal.card.fields[0].unit, "mL") == 0);
    assert(proposal.card.fields[0].emphasis == true);
    assert(strcmp(proposal.card.fields[1].label, "类型") == 0);
    assert(strcmp(proposal.card.fields[1].value, "配方奶") == 0);
    assert(proposal.card.fields[1].emphasis == false);

    // 7. Card confirm build
    len = app_protocol_build_card_confirm(buf, sizeof(buf), proposal.run_id, proposal.plan_hash, proposal.action_id, "req-1");
    assert(len > 0);
    assert(strstr(buf, "\"type\":\"card.confirm\"") != NULL);
    assert(strstr(buf, "\"runId\":\"run-abc\"") != NULL);
    assert(strstr(buf, "\"planHash\":\"hash-xyz\"") != NULL);
    assert(strstr(buf, "\"actionIds\":[\"act-1\"]") != NULL);

    // 8. Card invalidate
    app_cards_invalidate(&proposal);
    assert(proposal.valid == false);
    assert(strcmp(proposal.card.status, "superseded") == 0);

    // 9. TTS start & end
    const char *tts_start_json = "{\"type\":\"tts.start\",\"turnId\":\"turn-1\",\"sampleRate\":24000,\"channels\":1}";
    char turn_id[64] = {0};
    int sample_rate = 0, channels = 0;
    assert(app_protocol_parse_tts_start(tts_start_json, turn_id, sizeof(turn_id), &sample_rate, &channels));
    assert(strcmp(turn_id, "turn-1") == 0);
    assert(sample_rate == 24000);
    assert(channels == 1);

    // 10. Error parse
    const char *err_json = "{\"type\":\"error\",\"code\":\"VOICE_TOO_LONG\",\"message\":\"Recording exceeded 30 seconds\"}";
    char code[64] = {0}, msg[128] = {0};
    assert(app_protocol_parse_error(err_json, code, sizeof(code), msg, sizeof(msg)));
    assert(strcmp(code, "VOICE_TOO_LONG") == 0);
    assert(strcmp(msg, "Recording exceeded 30 seconds") == 0);

    puts("GrowDesk protocol & card parser tests: PASS");
    return 0;
}
