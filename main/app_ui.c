#include "app_ui.h"
#include <stdio.h>
#include <string.h>

#ifndef HOST_TEST
#include "lvgl.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_log.h"

static const char *TAG = "app_ui";

// Color palette
#define COLOR_BG          0xF8FAFC  // Slate 50
#define COLOR_CARD_BG     0xFFFFFF  // White
#define COLOR_PRIMARY     0x059669  // Emerald 600
#define COLOR_PRIMARY_LGT 0xD1FAE5  // Emerald 100
#define COLOR_TEXT_DARK   0x0F172A  // Slate 900
#define COLOR_TEXT_MUTED  0x64748B  // Slate 500
#define COLOR_ACCENT      0x2563EB  // Blue 600
#define COLOR_ERROR       0xDC2626  // Red 600
#define COLOR_BORDER      0xE2E8F0  // Slate 200

static lv_obj_t *s_scr = NULL;

// Top bar
static lv_obj_t *s_topbar = NULL;
static lv_obj_t *s_label_baby = NULL;
static lv_obj_t *s_label_wifi = NULL;
static lv_obj_t *s_label_battery = NULL;

// Content container
static lv_obj_t *s_content = NULL;

// Views
static lv_obj_t *s_view_idle = NULL;
static lv_obj_t *s_label_idle_recent_feeding = NULL;
static lv_obj_t *s_label_idle_recent_sleep = NULL;

static lv_obj_t *s_view_listening = NULL;
static lv_obj_t *s_bars[5];

static lv_obj_t *s_view_thinking = NULL;
static lv_obj_t *s_view_speaking = NULL;
static lv_obj_t *s_label_speaking_text = NULL;

static lv_obj_t *s_view_proposal = NULL;
static lv_obj_t *s_label_card_title = NULL;
static lv_obj_t *s_label_card_fields[MAX_CARD_FIELDS];
static lv_obj_t *s_label_card_footer = NULL;

static lv_obj_t *s_view_saved = NULL;
static lv_obj_t *s_view_pairing = NULL;
static lv_obj_t *s_label_pair_code = NULL;

static lv_obj_t *s_view_provisioning = NULL;

static lv_obj_t *s_view_error = NULL;
static lv_obj_t *s_label_err_title = NULL;
static lv_obj_t *s_label_err_detail = NULL;
static lv_obj_t *s_label_err_saved = NULL;

#ifndef HOST_TEST
LV_FONT_DECLARE(growdesk_font_16);
static lv_font_t s_growdesk_font_with_fallback;
static bool s_font_initialized = false;
#endif

static const lv_font_t *get_cjk_font(void)
{
#ifndef HOST_TEST
    if (!s_font_initialized) {
        s_growdesk_font_with_fallback = growdesk_font_16;
        s_growdesk_font_with_fallback.fallback = &lv_font_montserrat_14;
        s_font_initialized = true;
    }
    return &s_growdesk_font_with_fallback;
#else
    return NULL;
#endif
}

static void hide_all_views(void)
{
    if (s_view_idle)         lv_obj_add_flag(s_view_idle, LV_OBJ_FLAG_HIDDEN);
    if (s_view_listening)    lv_obj_add_flag(s_view_listening, LV_OBJ_FLAG_HIDDEN);
    if (s_view_thinking)     lv_obj_add_flag(s_view_thinking, LV_OBJ_FLAG_HIDDEN);
    if (s_view_speaking)     lv_obj_add_flag(s_view_speaking, LV_OBJ_FLAG_HIDDEN);
    if (s_view_proposal)     lv_obj_add_flag(s_view_proposal, LV_OBJ_FLAG_HIDDEN);
    if (s_view_saved)        lv_obj_add_flag(s_view_saved, LV_OBJ_FLAG_HIDDEN);
    if (s_view_pairing)      lv_obj_add_flag(s_view_pairing, LV_OBJ_FLAG_HIDDEN);
    if (s_view_provisioning) lv_obj_add_flag(s_view_provisioning, LV_OBJ_FLAG_HIDDEN);
    if (s_view_error)        lv_obj_add_flag(s_view_error, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_init(void)
{
    if (!bsp_lvgl_lock(1000)) {
        return;
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 240, 320);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    const lv_font_t *font_cjk = get_cjk_font();

    // 1. Top bar
    s_topbar = lv_obj_create(s_scr);
    lv_obj_set_size(s_topbar, 240, 32);
    lv_obj_set_pos(s_topbar, 0, 0);
    lv_obj_set_style_bg_color(s_topbar, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_side(s_topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_topbar, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(s_topbar, 1, 0);
    lv_obj_set_style_pad_all(s_topbar, 4, 0);
    lv_obj_clear_flag(s_topbar, LV_OBJ_FLAG_SCROLLABLE);

    s_label_baby = lv_label_create(s_topbar);
    lv_obj_align(s_label_baby, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_text_font(s_label_baby, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_baby, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_text(s_label_baby, "好好");

    s_label_wifi = lv_label_create(s_topbar);
    lv_obj_align(s_label_wifi, LV_ALIGN_CENTER, 20, 0);
    lv_obj_set_style_text_font(s_label_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_wifi, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(s_label_wifi, "Wi-Fi");

    s_label_battery = lv_label_create(s_topbar);
    lv_obj_align(s_label_battery, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_text_font(s_label_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_battery, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(s_label_battery, "85%");

    // 2. Content container
    s_content = lv_obj_create(s_scr);
    lv_obj_set_size(s_content, 240, 288);
    lv_obj_set_pos(s_content, 0, 32);
    lv_obj_set_style_bg_color(s_content, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 8, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // 3. IDLE View
    s_view_idle = lv_obj_create(s_content);
    lv_obj_set_size(s_view_idle, 224, 272);
    lv_obj_set_style_bg_opa(s_view_idle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_idle, 0, 0);
    lv_obj_set_style_pad_all(s_view_idle, 4, 0);
    lv_obj_clear_flag(s_view_idle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_idle = lv_label_create(s_view_idle);
    lv_obj_align(title_idle, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_font(title_idle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_idle, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(title_idle, "GrowDesk");

    lv_obj_t *btn_speak = lv_obj_create(s_view_idle);
    lv_obj_set_size(btn_speak, 180, 48);
    lv_obj_align(btn_speak, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(btn_speak, lv_color_hex(COLOR_PRIMARY_LGT), 0);
    lv_obj_set_style_border_color(btn_speak, lv_color_hex(COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(btn_speak, 1, 0);
    lv_obj_set_style_radius(btn_speak, 24, 0);

    lv_obj_t *lbl_speak = lv_label_create(btn_speak);
    lv_obj_align(lbl_speak, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_speak, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_speak, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(lbl_speak, "按住 ● 说话");

    lv_obj_t *panel_recent = lv_obj_create(s_view_idle);
    lv_obj_set_size(panel_recent, 216, 120);
    lv_obj_align(panel_recent, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_color(panel_recent, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_color(panel_recent, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(panel_recent, 1, 0);
    lv_obj_set_style_radius(panel_recent, 12, 0);

    lv_obj_t *lbl_rec_hdr = lv_label_create(panel_recent);
    lv_obj_align(lbl_rec_hdr, LV_ALIGN_TOP_LEFT, 6, 4);
    lv_obj_set_style_text_font(lbl_rec_hdr, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_rec_hdr, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(lbl_rec_hdr, "最近记录");

    s_label_idle_recent_feeding = lv_label_create(panel_recent);
    lv_obj_align(s_label_idle_recent_feeding, LV_ALIGN_TOP_LEFT, 6, 28);
    lv_obj_set_style_text_font(s_label_idle_recent_feeding, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_idle_recent_feeding, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_text(s_label_idle_recent_feeding, "奶量 140 mL · 09:32");

    s_label_idle_recent_sleep = lv_label_create(panel_recent);
    lv_obj_align(s_label_idle_recent_sleep, LV_ALIGN_TOP_LEFT, 6, 56);
    lv_obj_set_style_text_font(s_label_idle_recent_sleep, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_idle_recent_sleep, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_text(s_label_idle_recent_sleep, "睡眠 1h 12m · 07:15");

    lv_obj_t *lbl_idle_footer = lv_label_create(s_view_idle);
    lv_obj_align(lbl_idle_footer, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_text_font(lbl_idle_footer, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_idle_footer, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(lbl_idle_footer, "↑ 历史   ● 说话   ↓");

    // 4. Listening View
    s_view_listening = lv_obj_create(s_content);
    lv_obj_set_size(s_view_listening, 224, 272);
    lv_obj_set_style_bg_opa(s_view_listening, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_listening, 0, 0);
    lv_obj_clear_flag(s_view_listening, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_list_title = lv_label_create(s_view_listening);
    lv_obj_align(lbl_list_title, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_font(lbl_list_title, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_list_title, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(lbl_list_title, "正在听…");

    // 5 waveform bars
    for (int i = 0; i < 5; i++) {
        s_bars[i] = lv_obj_create(s_view_listening);
        lv_obj_set_size(s_bars[i], 12, 30);
        lv_obj_set_pos(s_bars[i], 56 + i * 24, 110);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(COLOR_PRIMARY), 0);
        lv_obj_set_style_radius(s_bars[i], 6, 0);
        lv_obj_set_style_border_width(s_bars[i], 0, 0);
    }

    lv_obj_t *lbl_list_footer = lv_label_create(s_view_listening);
    lv_obj_align(lbl_list_footer, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(lbl_list_footer, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_list_footer, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(lbl_list_footer, "松开结束");

    // 5. Thinking View
    s_view_thinking = lv_obj_create(s_content);
    lv_obj_set_size(s_view_thinking, 224, 272);
    lv_obj_set_style_bg_opa(s_view_thinking, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_thinking, 0, 0);
    lv_obj_clear_flag(s_view_thinking, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_think_title = lv_label_create(s_view_thinking);
    lv_obj_align(lbl_think_title, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(lbl_think_title, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_think_title, lv_color_hex(COLOR_ACCENT), 0);
    lv_label_set_text(lbl_think_title, "正在整理…");

    lv_obj_t *lbl_dots = lv_label_create(s_view_thinking);
    lv_obj_align(lbl_dots, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_color(lbl_dots, lv_color_hex(COLOR_ACCENT), 0);
    lv_label_set_text(lbl_dots, "●   ●   ●");

    // 6. Speaking View
    s_view_speaking = lv_obj_create(s_content);
    lv_obj_set_size(s_view_speaking, 224, 272);
    lv_obj_set_style_bg_opa(s_view_speaking, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_speaking, 0, 0);
    lv_obj_clear_flag(s_view_speaking, LV_OBJ_FLAG_SCROLLABLE);

    s_label_speaking_text = lv_label_create(s_view_speaking);
    lv_obj_set_width(s_label_speaking_text, 210);
    lv_obj_align(s_label_speaking_text, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(s_label_speaking_text, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_speaking_text, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_long_mode(s_label_speaking_text, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_label_speaking_text, "我整理成一张喂奶记录，请确认。");

    lv_obj_t *lbl_speak_footer = lv_label_create(s_view_speaking);
    lv_obj_align(lbl_speak_footer, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(lbl_speak_footer, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_speak_footer, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(lbl_speak_footer, "● 按键打断");

    // 7. Proposal Card View
    s_view_proposal = lv_obj_create(s_content);
    lv_obj_set_size(s_view_proposal, 220, 260);
    lv_obj_align(s_view_proposal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_view_proposal, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_color(s_view_proposal, lv_color_hex(COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(s_view_proposal, 2, 0);
    lv_obj_set_style_radius(s_view_proposal, 12, 0);

    s_label_card_title = lv_label_create(s_view_proposal);
    lv_obj_align(s_label_card_title, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_set_style_text_font(s_label_card_title, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_card_title, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(s_label_card_title, "🍼 喂奶记录");

    for (int i = 0; i < MAX_CARD_FIELDS; i++) {
        s_label_card_fields[i] = lv_label_create(s_view_proposal);
        lv_obj_set_pos(s_label_card_fields[i], 6, 36 + i * 26);
        lv_obj_set_style_text_font(s_label_card_fields[i], font_cjk, 0);
        lv_obj_set_style_text_color(s_label_card_fields[i], lv_color_hex(COLOR_TEXT_DARK), 0);
        lv_label_set_text(s_label_card_fields[i], "");
    }

    s_label_card_footer = lv_label_create(s_view_proposal);
    lv_obj_align(s_label_card_footer, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_text_font(s_label_card_footer, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_card_footer, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(s_label_card_footer, "● 确认保存    长按取消");

    // 8. Saved View
    s_view_saved = lv_obj_create(s_content);
    lv_obj_set_size(s_view_saved, 224, 272);
    lv_obj_set_style_bg_opa(s_view_saved, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_saved, 0, 0);
    lv_obj_clear_flag(s_view_saved, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_saved = lv_label_create(s_view_saved);
    lv_obj_align(lbl_saved, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_font(lbl_saved, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_saved, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(lbl_saved, "✓ 已保存");

    // 9. Pairing View
    s_view_pairing = lv_obj_create(s_content);
    lv_obj_set_size(s_view_pairing, 224, 272);
    lv_obj_set_style_bg_opa(s_view_pairing, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_pairing, 0, 0);
    lv_obj_clear_flag(s_view_pairing, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_pair_hdr = lv_label_create(s_view_pairing);
    lv_obj_align(lbl_pair_hdr, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(lbl_pair_hdr, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_pair_hdr, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_text(lbl_pair_hdr, "连接 GrowDesk");

    s_label_pair_code = lv_label_create(s_view_pairing);
    lv_obj_align(s_label_pair_code, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_font(s_label_pair_code, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_pair_code, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(s_label_pair_code, "----\n----");

    lv_obj_t *lbl_pair_tip = lv_label_create(s_view_pairing);
    lv_obj_align(lbl_pair_tip, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(lbl_pair_tip, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_pair_tip, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(lbl_pair_tip, "请在 GrowDesk 中添加设备");

    // 10. Provisioning View
    s_view_provisioning = lv_obj_create(s_content);
    lv_obj_set_size(s_view_provisioning, 224, 272);
    lv_obj_set_style_bg_opa(s_view_provisioning, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_provisioning, 0, 0);
    lv_obj_clear_flag(s_view_provisioning, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_prov_hdr = lv_label_create(s_view_provisioning);
    lv_obj_align(lbl_prov_hdr, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_font(lbl_prov_hdr, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_prov_hdr, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_text(lbl_prov_hdr, "蓝牙配网");

    lv_obj_t *lbl_prov_body = lv_label_create(s_view_provisioning);
    lv_obj_set_width(lbl_prov_body, 200);
    lv_obj_align(lbl_prov_body, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_prov_body, font_cjk, 0);
    lv_obj_set_style_text_color(lbl_prov_body, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_long_mode(lbl_prov_body, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_prov_body, "请打开微信小程序\n「蓝牙配网-FoloToy AI PASSPORT」\n为设备配置 Wi-Fi");

    // 11. Error View
    s_view_error = lv_obj_create(s_content);
    lv_obj_set_size(s_view_error, 224, 272);
    lv_obj_set_style_bg_opa(s_view_error, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_view_error, 0, 0);
    lv_obj_clear_flag(s_view_error, LV_OBJ_FLAG_SCROLLABLE);

    s_label_err_title = lv_label_create(s_view_error);
    lv_obj_align(s_label_err_title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(s_label_err_title, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_err_title, lv_color_hex(COLOR_ERROR), 0);
    lv_label_set_text(s_label_err_title, "出错了");

    s_label_err_detail = lv_label_create(s_view_error);
    lv_obj_set_width(s_label_err_detail, 200);
    lv_obj_align(s_label_err_detail, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_font(s_label_err_detail, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_err_detail, lv_color_hex(COLOR_TEXT_DARK), 0);
    lv_label_set_long_mode(s_label_err_detail, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_label_err_detail, "网络连接断开");

    s_label_err_saved = lv_label_create(s_view_error);
    lv_obj_align(s_label_err_saved, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(s_label_err_saved, font_cjk, 0);
    lv_obj_set_style_text_color(s_label_err_saved, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_label_set_text(s_label_err_saved, "没有保存任何记录");

    lv_scr_load(s_scr);
    app_ui_set_state(APP_STATE_BOOT);
    bsp_lvgl_unlock();
}

void app_ui_set_state(app_state_t state)
{
    if (!bsp_lvgl_lock(500)) return;

    hide_all_views();

    switch (state) {
    case APP_STATE_BOOT:
        if (s_view_idle) lv_obj_clear_flag(s_view_idle, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_WIFI_PROVISIONING:
        if (s_view_provisioning) lv_obj_clear_flag(s_view_provisioning, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_PAIRING:
    case APP_STATE_PAIRING_REQUIRED:
        if (s_view_pairing) lv_obj_clear_flag(s_view_pairing, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_IDLE:
        if (s_view_idle) lv_obj_clear_flag(s_view_idle, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_LISTENING:
        if (s_view_listening) lv_obj_clear_flag(s_view_listening, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_THINKING:
        if (s_view_thinking) lv_obj_clear_flag(s_view_thinking, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_SPEAKING:
        if (s_view_speaking) lv_obj_clear_flag(s_view_speaking, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_PROPOSAL_CARD:
    case APP_STATE_CONFIRMING:
        if (s_view_proposal) lv_obj_clear_flag(s_view_proposal, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_SAVED:
        if (s_view_saved) lv_obj_clear_flag(s_view_saved, LV_OBJ_FLAG_HIDDEN);
        break;
    case APP_STATE_OFFLINE:
    case APP_STATE_AUTH_EXPIRED:
    case APP_STATE_ERROR:
    case APP_STATE_RESULT_UNKNOWN:
        if (s_view_error) lv_obj_clear_flag(s_view_error, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    bsp_lvgl_unlock();
}

void app_ui_set_baby_name(const char *name)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_baby && name) {
        lv_label_set_text(s_label_baby, name);
    }
    bsp_lvgl_unlock();
}

void app_ui_set_wifi_status(bool connected, const char *ssid)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_wifi) {
        if (connected) {
            lv_label_set_text(s_label_wifi, ssid ? ssid : "Wi-Fi");
            lv_obj_set_style_text_color(s_label_wifi, lv_color_hex(COLOR_PRIMARY), 0);
        } else {
            lv_label_set_text(s_label_wifi, "离线");
            lv_obj_set_style_text_color(s_label_wifi, lv_color_hex(COLOR_ERROR), 0);
        }
    }
    bsp_lvgl_unlock();
}

void app_ui_set_battery(int percent)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_battery) {
        char buf[16];
        if (percent >= 0) {
            snprintf(buf, sizeof(buf), "%d%%", percent);
        } else {
            snprintf(buf, sizeof(buf), "--%%");
        }
        lv_label_set_text(s_label_battery, buf);
    }
    bsp_lvgl_unlock();
}

void app_ui_set_pairing_code(const char *code)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_pair_code && code) {
        char formatted[32] = {0};
        const char *dash = strchr(code, '-');
        if (dash && (size_t)(dash - code) == 4) {
            snprintf(formatted, sizeof(formatted), "%.4s\n%s", code, dash + 1);
        } else if (strlen(code) == 8) {
            snprintf(formatted, sizeof(formatted), "%.4s\n%s", code, code + 4);
        } else {
            strncpy(formatted, code, sizeof(formatted) - 1);
        }
        lv_label_set_text(s_label_pair_code, formatted);
    }
    bsp_lvgl_unlock();
}

void app_ui_set_recent_records(const char *feeding_info, const char *sleep_info)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_idle_recent_feeding && feeding_info) {
        lv_label_set_text(s_label_idle_recent_feeding, feeding_info);
    }
    if (s_label_idle_recent_sleep && sleep_info) {
        lv_label_set_text(s_label_idle_recent_sleep, sleep_info);
    }
    bsp_lvgl_unlock();
}

void app_ui_set_assistant_text(const char *text)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_speaking_text && text) {
        lv_label_set_text(s_label_speaking_text, text);
    }
    bsp_lvgl_unlock();
}

void app_ui_set_proposal_card(const passport_card_t *card)
{
    if (!bsp_lvgl_lock(500)) return;
    if (card) {
        if (s_label_card_title) {
            lv_label_set_text(s_label_card_title, card->title);
        }
        for (int i = 0; i < MAX_CARD_FIELDS; i++) {
            if (s_label_card_fields[i]) {
                if (i < card->field_count) {
                    char field_buf[128];
                    const passport_card_field_t *f = &card->fields[i];
                    if (strlen(f->unit) > 0) {
                        snprintf(field_buf, sizeof(field_buf), "%s: %s %s", f->label, f->value, f->unit);
                    } else {
                        snprintf(field_buf, sizeof(field_buf), "%s: %s", f->label, f->value);
                    }
                    lv_label_set_text(s_label_card_fields[i], field_buf);
                    if (f->emphasis) {
                        lv_obj_set_style_text_color(s_label_card_fields[i], lv_color_hex(COLOR_PRIMARY), 0);
                    } else {
                        lv_obj_set_style_text_color(s_label_card_fields[i], lv_color_hex(COLOR_TEXT_DARK), 0);
                    }
                } else {
                    lv_label_set_text(s_label_card_fields[i], "");
                }
            }
        }
        if (s_label_card_footer && strlen(card->footer) > 0) {
            lv_label_set_text(s_label_card_footer, card->footer);
        }
    }
    bsp_lvgl_unlock();
}

void app_ui_set_error(const char *title, const char *detail, bool saved)
{
    if (!bsp_lvgl_lock(200)) return;
    if (s_label_err_title && title) lv_label_set_text(s_label_err_title, title);
    if (s_label_err_detail && detail) lv_label_set_text(s_label_err_detail, detail);
    if (s_label_err_saved) {
        lv_label_set_text(s_label_err_saved, saved ? "✓ 已保存" : "没有保存记录");
    }
    bsp_lvgl_unlock();
}

void app_ui_update_visualizer(int volume_level)
{
    if (!bsp_lvgl_lock(100)) return;
    int base_heights[5] = {12, 20, 36, 20, 12};
    for (int i = 0; i < 5; i++) {
        if (s_bars[i]) {
            int h = (base_heights[i] * (volume_level + 10)) / 20;
            if (h < 6) h = 6;
            if (h > 60) h = 60;
            lv_obj_set_height(s_bars[i], h);
        }
    }
    bsp_lvgl_unlock();
}

#else
// Host test stub
void app_ui_init(void) {}
void app_ui_set_state(app_state_t state) { (void)state; }
void app_ui_set_baby_name(const char *name) { (void)name; }
void app_ui_set_wifi_status(bool connected, const char *ssid) { (void)connected; (void)ssid; }
void app_ui_set_battery(int percent) { (void)percent; }
void app_ui_set_pairing_code(const char *code) { (void)code; }
void app_ui_set_recent_records(const char *f, const char *s) { (void)f; (void)s; }
void app_ui_set_assistant_text(const char *t) { (void)t; }
void app_ui_set_proposal_card(const passport_card_t *c) { (void)c; }
void app_ui_set_error(const char *t, const char *d, bool s) { (void)t; (void)d; (void)s; }
void app_ui_update_visualizer(int v) { (void)v; }
#endif
