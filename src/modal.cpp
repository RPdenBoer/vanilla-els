#include "modal.h"
#include "coordinates.h"
#include "tools.h"
#include "offsets.h"
#include "encoder.h"
#include "config.h"
#include "leadscrew.h"

static void apply_modal_button_common_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN);

    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    // Some themes add focus outlines that also clip.
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN | LV_STATE_FOCUS_KEY);

    lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_x(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    // Keep border stable on press to avoid clipping.
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_PRESSED);
}

lv_obj_t *ModalManager::modal_bg = nullptr;
lv_obj_t *ModalManager::modal_win = nullptr;
lv_obj_t *ModalManager::ta_value = nullptr;
lv_obj_t *ModalManager::kb = nullptr;
AxisSel ModalManager::active_axis = AXIS_X;
bool ModalManager::pitch_modal = false;

void ModalManager::showOffsetModal(AxisSel axis) {
    if (modal_bg) return;
    active_axis = axis;
    pitch_modal = false;

    modal_bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_70, 0);
    lv_obj_add_flag(modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modal_bg, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(modal_bg, 0, 0);

    modal_win = lv_obj_create(modal_bg);
    lv_obj_set_size(modal_win, 460, 250);
    lv_obj_center(modal_win);
    lv_obj_clear_flag(modal_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modal_win, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_pad_all(modal_win, 10, 0);
    lv_obj_set_style_pad_gap(modal_win, 8, 0);
    lv_obj_set_style_bg_color(modal_win, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(modal_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(modal_win, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_border_width(modal_win, 2, 0);
    lv_obj_set_style_radius(modal_win, 10, 0);

    lv_obj_set_flex_flow(modal_win, LV_FLEX_FLOW_COLUMN);

    // Title
    lv_obj_t *title = lv_label_create(modal_win);
    const char *lin_unit = CoordinateSystem::isLinearInchMode() ? "in" : "mm";
    if (axis == AXIS_X) {
        const char *xmode = CoordinateSystem::isXRadiusMode() ? "rad" : "dia";
        char tbuf[48];
        snprintf(tbuf, sizeof(tbuf), "Set X (%s %s)", lin_unit, xmode);
        lv_label_set_text(title, tbuf);
    }
    else if (axis == AXIS_Z) {
        const char *zmode = CoordinateSystem::isZInverted() ? "neg" : "pos";
        char tbuf[48];
        snprintf(tbuf, sizeof(tbuf), "Set Z (%s %s)", lin_unit, zmode);
        lv_label_set_text(title, tbuf);
    }
    else {
        lv_label_set_text(title, "Set C (deg)");
    }
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // Row: textarea + X/T/G
    lv_obj_t *row = lv_obj_create(modal_win);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 44);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ta_value = lv_textarea_create(row);
    lv_obj_set_height(ta_value, 44);
    lv_obj_set_flex_grow(ta_value, 1);
    lv_textarea_set_one_line(ta_value, true);
    lv_textarea_set_cursor_click_pos(ta_value, true);
    lv_obj_clear_flag(ta_value, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ta_value, LV_SCROLLBAR_MODE_OFF);

    // Prefill zero (fastest path = press T or G)
    if (axis == AXIS_C) lv_textarea_set_text(ta_value, "0.00");
    else               lv_textarea_set_text(ta_value, CoordinateSystem::isLinearInchMode() ? "0.0000" : "0.000");

    lv_obj_t *btn_x = lv_btn_create(row);
    lv_obj_set_size(btn_x, 44, 44);
    lv_obj_add_event_cb(btn_x, onCancel, LV_EVENT_CLICKED, nullptr);
    // Ghost style
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_x, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *lblx = lv_label_create(btn_x);
    lv_label_set_text(lblx, "X");
    lv_obj_center(lblx);

    apply_modal_button_common_style(btn_x);

    lv_obj_t *btn_t = lv_btn_create(row);
    lv_obj_set_size(btn_t, 52, 44);
    lv_obj_add_event_cb(btn_t, onSetTool, LV_EVENT_CLICKED, nullptr);
    // Filled red
    lv_obj_set_style_bg_opa(btn_t, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_t, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_t, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_t, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_t, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblt = lv_label_create(btn_t);
    char ttxt[8];
    snprintf(ttxt, sizeof(ttxt), "T%d", ToolManager::getCurrentTool() + 1);
    lv_label_set_text(lblt, ttxt);
    lv_obj_center(lblt);

    apply_modal_button_common_style(btn_t);

    lv_obj_t *btn_g = lv_btn_create(row);
    lv_obj_set_size(btn_g, 52, 44);
    lv_obj_add_event_cb(btn_g, onSetGlobal, LV_EVENT_CLICKED, nullptr);
    // Filled blue
    lv_obj_set_style_bg_opa(btn_g, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_g, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_g, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_g, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_g, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblg = lv_label_create(btn_g);
    char gtxt[8];
    snprintf(gtxt, sizeof(gtxt), "G%d", OffsetManager::getCurrentOffset() + 1);
    lv_label_set_text(lblg, gtxt);
    lv_obj_center(lblg);

    apply_modal_button_common_style(btn_g);

    // Keyboard
    kb = lv_keyboard_create(modal_win);
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_height(kb, 140);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(kb, LV_SCROLLBAR_MODE_OFF);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta_value);

    // Disable pressed transforms on keyboard keys (btnmatrix items) to prevent clipping.
    lv_obj_set_style_transform_scale_x(kb, 256, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(kb, 256, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_translate_x(kb, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(kb, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(kb, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_outline_pad(kb, 0, LV_PART_ITEMS | LV_STATE_PRESSED);
}

void ModalManager::showPitchModal() {
    if (modal_bg) return;
    pitch_modal = true;

    modal_bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_70, 0);
    lv_obj_add_flag(modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modal_bg, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(modal_bg, 0, 0);

    modal_win = lv_obj_create(modal_bg);
    lv_obj_set_size(modal_win, 460, 250);
    lv_obj_center(modal_win);
    lv_obj_clear_flag(modal_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modal_win, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_pad_all(modal_win, 10, 0);
    lv_obj_set_style_pad_gap(modal_win, 8, 0);
    lv_obj_set_style_bg_color(modal_win, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(modal_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(modal_win, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_border_width(modal_win, 2, 0);
    lv_obj_set_style_radius(modal_win, 10, 0);

    lv_obj_set_flex_flow(modal_win, LV_FLEX_FLOW_COLUMN);

    // Title
    lv_obj_t *title = lv_label_create(modal_win);
    if (LeadscrewManager::isPitchTpiMode()) {
        lv_label_set_text(title, "Pitch (TPI)");
    } else {
        lv_label_set_text(title, CoordinateSystem::isLinearInchMode() ? "Pitch (in)" : "Pitch (mm)");
    }
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    // Row: textarea + X/OK
    lv_obj_t *row = lv_obj_create(modal_win);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 44);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ta_value = lv_textarea_create(row);
    lv_obj_set_height(ta_value, 44);
    lv_obj_set_flex_grow(ta_value, 1);
    lv_textarea_set_one_line(ta_value, true);
    lv_textarea_set_cursor_click_pos(ta_value, true);
    lv_obj_clear_flag(ta_value, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ta_value, LV_SCROLLBAR_MODE_OFF);

    // Prefill current pitch in active unit
    char pbuf[32];
    if (LeadscrewManager::isPitchTpiMode()) {
        const float pitch = (float)LeadscrewManager::getPitchUm();
        float tpi = 25400.0f / (float)fabsf(pitch);
        if (pitch < 0) tpi = -tpi;
        snprintf(pbuf, sizeof(pbuf), "%.2f", tpi);
    } else {
        if (CoordinateSystem::isLinearInchMode()) {
            float inches = (float)LeadscrewManager::getPitchUm() / 25400.0f;
            snprintf(pbuf, sizeof(pbuf), "%.4f", inches);
        } else {
            float mm = (float)LeadscrewManager::getPitchUm() / 1000.0f;
            snprintf(pbuf, sizeof(pbuf), "%.3f", mm);
        }
    }
    lv_textarea_set_text(ta_value, pbuf);

    lv_obj_t *btn_x = lv_btn_create(row);
    lv_obj_set_size(btn_x, 44, 44);
    lv_obj_add_event_cb(btn_x, onCancel, LV_EVENT_CLICKED, nullptr);
    // Ghost style
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_x, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *lblx = lv_label_create(btn_x);
    lv_label_set_text(lblx, "X");
    lv_obj_center(lblx);

    apply_modal_button_common_style(btn_x);

    lv_obj_t *btn_ok = lv_btn_create(row);
    lv_obj_set_size(btn_ok, 64, 44);
    lv_obj_add_event_cb(btn_ok, onPitchOk, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_ok, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_ok, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_ok, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_ok, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblo = lv_label_create(btn_ok);
    lv_label_set_text(lblo, "OK");
    lv_obj_center(lblo);

    apply_modal_button_common_style(btn_ok);

    kb = lv_keyboard_create(modal_win);
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_height(kb, 140);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(kb, LV_SCROLLBAR_MODE_OFF);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta_value);
}

void ModalManager::closeModal() {
    if (modal_bg) {
        lv_obj_del(modal_bg);
        modal_bg = nullptr;
        modal_win = nullptr;
        ta_value = nullptr;
        kb = nullptr;
    }
}

void ModalManager::applyToolOffset() {
    if (!ta_value) return;

    const char *txt = lv_textarea_get_text(ta_value);
    const int tool = ToolManager::getCurrentTool();
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    if (active_axis == AXIS_X) {
        int32_t target_um = 0;
        if (!CoordinateSystem::parseLinearToUm(txt, &target_um)) target_um = 0;
        int32_t target_machine_um = CoordinateSystem::xUserToMachineUm(target_um);
        CoordinateSystem::x_tool_um[tool] = CoordinateSystem::x_raw_um - 
            CoordinateSystem::x_global_um[off] - target_machine_um;

    } else if (active_axis == AXIS_Z) {
        int32_t target_um = 0;
        if (!CoordinateSystem::parseLinearToUm(txt, &target_um)) target_um = 0;
        int32_t target_machine_um = CoordinateSystem::zUserToMachineUm(target_um);
        CoordinateSystem::z_tool_um[tool] = CoordinateSystem::z_raw_um - 
            CoordinateSystem::z_global_um[off] - target_machine_um;

    } else {
        int32_t target_deg_x100 = 0;
        if (!CoordinateSystem::parseDegToDegX100(txt, &target_deg_x100)) target_deg_x100 = 0;
        int32_t target_ticks = CoordinateSystem::degX100ToTicks(target_deg_x100);

        int32_t raw = CoordinateSystem::wrap01599(EncoderManager::getRawTicks());
        CoordinateSystem::c_tool_ticks[tool] = 
            CoordinateSystem::wrap01599(raw - CoordinateSystem::c_global_ticks[off] - target_ticks);
    }
}

void ModalManager::applyGlobalOffset() {
    if (!ta_value) return;

    const char *txt = lv_textarea_get_text(ta_value);
    const int tool = ToolManager::getCurrentTool();
    int off = OffsetManager::getCurrentOffset();
    if (off < 0 || off >= OFFSET_COUNT) off = 0;

    if (active_axis == AXIS_X) {
        int32_t target_um = 0;
        if (!CoordinateSystem::parseLinearToUm(txt, &target_um)) target_um = 0;
        int32_t target_machine_um = CoordinateSystem::xUserToMachineUm(target_um);
        CoordinateSystem::x_global_um[off] = CoordinateSystem::x_raw_um - 
            CoordinateSystem::x_tool_um[tool] - target_machine_um;

    } else if (active_axis == AXIS_Z) {
        int32_t target_um = 0;
        if (!CoordinateSystem::parseLinearToUm(txt, &target_um)) target_um = 0;
        int32_t target_machine_um = CoordinateSystem::zUserToMachineUm(target_um);
        CoordinateSystem::z_global_um[off] = CoordinateSystem::z_raw_um - 
            CoordinateSystem::z_tool_um[tool] - target_machine_um;

    } else {
        int32_t target_deg_x100 = 0;
        if (!CoordinateSystem::parseDegToDegX100(txt, &target_deg_x100)) target_deg_x100 = 0;
        int32_t target_ticks = CoordinateSystem::degX100ToTicks(target_deg_x100);

        int32_t raw = CoordinateSystem::wrap01599(EncoderManager::getRawTicks());
        CoordinateSystem::c_global_ticks[off] = 
            CoordinateSystem::wrap01599(raw - CoordinateSystem::c_tool_ticks[tool] - target_ticks);
    }
}

void ModalManager::onCancel(lv_event_t *e) { 
    (void)e; 
    closeModal(); 
}

void ModalManager::onSetTool(lv_event_t *e) { 
    (void)e; 
    applyToolOffset(); 
    closeModal(); 
}

void ModalManager::onSetGlobal(lv_event_t *e) { 
    (void)e; 
    applyGlobalOffset(); 
    closeModal(); 
}

void ModalManager::applyPitch() {
    if (!ta_value) return;
    const char *txt = lv_textarea_get_text(ta_value);

    float v = atof(txt);
    if (v == 0.0f) return;

    if (LeadscrewManager::isPitchTpiMode()) {
        // Interpret as TPI
        float tpi = v;
        int32_t pitch_um = (int32_t)lroundf(25400.0f / tpi);
        LeadscrewManager::setPitchUm(pitch_um);
    } else {
        // Interpret as pitch length in current DRO units
        if (CoordinateSystem::isLinearInchMode()) {
            float inches = v;
            int32_t pitch_um = (int32_t)lroundf(inches * 25400.0f);
            LeadscrewManager::setPitchUm(pitch_um);
        } else {
            float mm = v;
            int32_t pitch_um = (int32_t)lroundf(mm * 1000.0f);
            LeadscrewManager::setPitchUm(pitch_um);
        }
    }
}

void ModalManager::onPitchOk(lv_event_t *e) {
    (void)e;
    applyPitch();
    closeModal();
}