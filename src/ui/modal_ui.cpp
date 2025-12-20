#include "modal_ui.h"
#include "coordinates_ui.h"
#include "tools_ui.h"
#include "offsets_ui.h"
#include "encoder_proxy.h"
#include "config_ui.h"
#include "leadscrew_proxy.h"
#include "endstop_proxy.h"

#include <cstring>
#include <cstdio>
#include <cmath>

// Disable LVGL theme transitions
static const lv_style_prop_t NO_TRANS_PROPS[] = {0};
static const lv_style_transition_dsc_t NO_TRANSITION = {
    .props = NO_TRANS_PROPS, .user_data = nullptr,
    .path_xcb = lv_anim_path_linear, .time = 0, .delay = 0,
};

static lv_color_t modal_accent_blue_grey() { return lv_palette_darken(LV_PALETTE_BLUE_GREY, 4); }

static bool ta_select_all_pending = false;
static lv_obj_t *ta_select_all_target = nullptr;

static void apply_visual_selection(lv_obj_t *ta, bool selected) {
    if (!ta) return;
    lv_obj_t *label = lv_textarea_get_label(ta);
    if (!label) return;
	// Set selection background color to blue-grey (matching modal buttons)
	lv_obj_set_style_bg_color(label, modal_accent_blue_grey(), LV_PART_SELECTED);
	lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_SELECTED);
	if (selected) {
        const char *txt = lv_textarea_get_text(ta);
        uint32_t len = txt ? (uint32_t)strlen(txt) : 0;
        lv_label_set_text_selection_start(label, 0);
        lv_label_set_text_selection_end(label, len);
    } else {
        lv_label_set_text_selection_start(label, LV_DRAW_LABEL_NO_TXT_SEL);
        lv_label_set_text_selection_end(label, LV_DRAW_LABEL_NO_TXT_SEL);
    }
}

static void mark_select_all(lv_obj_t *ta) {
    if (!ta) return;
    ta_select_all_pending = true;
    ta_select_all_target = ta;
    lv_textarea_set_text_selection(ta, true);
    apply_visual_selection(ta, true);
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
}

static void clear_select_all() {
    if (ta_select_all_pending && ta_select_all_target)
        apply_visual_selection(ta_select_all_target, false);
    ta_select_all_pending = false;
    ta_select_all_target = nullptr;
}

static void onTextareaClicked(lv_event_t *e) { (void)e; clear_select_all(); }

static void trim_trailing_zeros_inplace(char *s) {
    if (!s || s[0] == '\0') return;
    char *dot = strchr(s, '.');
    if (!dot) return;
    char *end = s + strlen(s) - 1;
    while (end > dot && *end == '0') *end-- = '\0';
    if (end == dot) *end = '\0';
    if (strcmp(s, "-0") == 0) { s[0] = '0'; s[1] = '\0'; }
}

static void apply_modal_button_common_style(lv_obj_t *btn);

static void apply_numpad_key_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_image_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, modal_accent_blue_grey(), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE_GREY, 3), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    apply_modal_button_common_style(btn);
}

static lv_obj_t *create_numpad(lv_obj_t *parent) {
    lv_obj_t *pad_row = lv_obj_create(parent);
    lv_obj_set_width(pad_row, LV_PCT(100));
    lv_obj_set_flex_grow(pad_row, 1);
    lv_obj_clear_flag(pad_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pad_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pad_row, 0, 0);
    lv_obj_set_style_pad_all(pad_row, 0, 0);
    lv_obj_set_style_pad_gap(pad_row, 2, 0);
    lv_obj_set_flex_flow(pad_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *grid = lv_obj_create(pad_row);
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_height(grid, LV_PCT(100));
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 2, 0);
    lv_obj_set_style_pad_column(grid, 2, 0);

    static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    auto make_key = [&](int col, int row, const char *txt, int keycode) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, ModalManager::onNumpadKey, LV_EVENT_CLICKED, (void *)(intptr_t)keycode);
        apply_numpad_key_style(btn);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    };

    make_key(0, 0, "7", '7'); make_key(1, 0, "8", '8'); make_key(2, 0, "9", '9');
    make_key(0, 1, "4", '4'); make_key(1, 1, "5", '5'); make_key(2, 1, "6", '6');
    make_key(0, 2, "1", '1'); make_key(1, 2, "2", '2'); make_key(2, 2, "3", '3');
    make_key(0, 3, "+/-", '-'); make_key(1, 3, "0", '0'); make_key(2, 3, ".", '.');

    lv_obj_t *right_col = lv_obj_create(pad_row);
    lv_obj_set_width(right_col, 92);
    lv_obj_set_height(right_col, LV_PCT(100));
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);
    lv_obj_set_style_pad_all(right_col, 0, 0);
    lv_obj_set_style_pad_gap(right_col, 2, 0);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *btn_bs = lv_btn_create(right_col);
    lv_obj_set_width(btn_bs, LV_PCT(100));
    lv_obj_set_flex_grow(btn_bs, 1);
    lv_obj_add_event_cb(btn_bs, ModalManager::onNumpadBackspace, LV_EVENT_CLICKED, nullptr);
    apply_numpad_key_style(btn_bs);
    lv_obj_t *lblbs = lv_label_create(btn_bs);
    lv_label_set_text(lblbs, "DEL");
    lv_obj_center(lblbs);

    lv_obj_t *btn_clear = lv_btn_create(right_col);
    lv_obj_set_width(btn_clear, LV_PCT(100));
    lv_obj_set_flex_grow(btn_clear, 1);
    lv_obj_add_event_cb(btn_clear, ModalManager::onNumpadClear, LV_EVENT_CLICKED, nullptr);
    apply_numpad_key_style(btn_clear);
    lv_obj_t *lblc = lv_label_create(btn_clear);
    lv_label_set_text(lblc, "CLEAR");
    lv_obj_center(lblc);

    return pad_row;
}

static void apply_modal_button_common_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN);
    lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_PRESSED);
}

// Forward declaration for UI update
void updateEndstopButtonStates();

lv_obj_t *ModalManager::modal_bg = nullptr;
lv_obj_t *ModalManager::modal_win = nullptr;
lv_obj_t *ModalManager::ta_value = nullptr;
lv_obj_t *ModalManager::kb = nullptr;
AxisSel ModalManager::active_axis = AXIS_X;
bool ModalManager::pitch_modal = false;
bool ModalManager::endstop_modal = false;
bool ModalManager::endstop_is_max = false;

void ModalManager::showOffsetModal(AxisSel axis) {
    if (modal_bg) return;
    active_axis = axis;
    pitch_modal = false;
    endstop_modal = false;

    modal_bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_bg, 0, LV_PART_MAIN);
    lv_obj_add_flag(modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(modal_bg, 0, 0);

    modal_win = lv_obj_create(modal_bg);
    lv_obj_set_size(modal_win, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(modal_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(modal_win, 6, 0);
    lv_obj_set_style_pad_gap(modal_win, 6, 0);
    lv_obj_set_style_bg_color(modal_win, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_win, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_win, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(modal_win, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(modal_win, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(modal_win);
    const char *lin_unit = CoordinateSystem::isLinearInchMode() ? "in" : "mm";
    char tbuf[48];
    if (axis == AXIS_X) {
        snprintf(tbuf, sizeof(tbuf), "Set X (%s %s)", lin_unit, CoordinateSystem::isXRadiusMode() ? "rad" : "dia");
    } else if (axis == AXIS_Z) {
        snprintf(tbuf, sizeof(tbuf), "Set Z (%s %s)", lin_unit, CoordinateSystem::isZInverted() ? "neg" : "pos");
    } else {
        snprintf(tbuf, sizeof(tbuf), "Set C (deg)");
    }
    lv_label_set_text(title, tbuf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, modal_accent_blue_grey(), 0);

    lv_obj_t *row = lv_obj_create(modal_win);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 56);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ta_value = lv_textarea_create(row);
    lv_obj_set_height(ta_value, 56);
    lv_obj_set_flex_grow(ta_value, 1);
    lv_textarea_set_one_line(ta_value, true);
    lv_textarea_set_cursor_click_pos(ta_value, true);
    lv_obj_clear_flag(ta_value, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ta_value, onTextareaClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_text_font(ta_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta_value, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ta_value, 0, LV_PART_MAIN);
	// Selection styling - blue-grey to match modal buttons
	lv_obj_set_style_bg_color(ta_value, modal_accent_blue_grey(), LV_PART_SELECTED);
	lv_obj_set_style_bg_opa(ta_value, LV_OPA_COVER, LV_PART_SELECTED);

	char buf[32];
    const int tool = ToolManager::getCurrentTool();
    if (axis == AXIS_X) {
        CoordinateSystem::formatLinear(buf, sizeof(buf), CoordinateSystem::getDisplayX(tool));
    } else if (axis == AXIS_Z) {
        CoordinateSystem::formatLinear(buf, sizeof(buf), CoordinateSystem::getDisplayZ(tool));
    } else {
        int32_t ticks = CoordinateSystem::getDisplayC(EncoderProxy::getRawTicks(), tool);
        CoordinateSystem::formatDeg(buf, sizeof(buf), CoordinateSystem::ticksToDegX100(ticks));
    }
    trim_trailing_zeros_inplace(buf);
    lv_textarea_set_text(ta_value, buf);
    mark_select_all(ta_value);

    const int main_g_btn_w = OffsetManager::getMainOffsetButtonWidth();

    // T button
    lv_obj_t *btn_t = lv_btn_create(row);
    lv_obj_set_size(btn_t, main_g_btn_w, 40);
    lv_obj_add_event_cb(btn_t, onSetTool, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(btn_t, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_t, lv_color_white(), LV_PART_MAIN);
    char ttxt[8]; snprintf(ttxt, sizeof(ttxt), "T%d", ToolManager::getCurrentTool() + 1);
    lv_obj_t *lblt = lv_label_create(btn_t);
    lv_label_set_text(lblt, ttxt);
    lv_obj_center(lblt);
    apply_modal_button_common_style(btn_t);

    // G button
    lv_obj_t *btn_g = lv_btn_create(row);
    lv_obj_set_size(btn_g, main_g_btn_w, 40);
    lv_obj_add_event_cb(btn_g, onSetGlobal, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(btn_g, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_g, lv_color_white(), LV_PART_MAIN);
    char gtxt[8]; snprintf(gtxt, sizeof(gtxt), "G%d", OffsetManager::getCurrentOffset() + 1);
    lv_obj_t *lblg = lv_label_create(btn_g);
    lv_label_set_text(lblg, gtxt);
    lv_obj_center(lblg);
    apply_modal_button_common_style(btn_g);

    // X button
    lv_obj_t *btn_x = lv_btn_create(row);
    lv_obj_set_size(btn_x, main_g_btn_w, 40);
    lv_obj_add_event_cb(btn_x, onCancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_x, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *lblx = lv_label_create(btn_x);
    lv_label_set_text(lblx, "X");
    lv_obj_center(lblx);
    apply_modal_button_common_style(btn_x);

    kb = create_numpad(modal_win);
}

void ModalManager::showPitchModal() {
    if (modal_bg) return;
    pitch_modal = true;
    endstop_modal = false;

    modal_bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_bg, 0, LV_PART_MAIN);
    lv_obj_add_flag(modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);

    modal_win = lv_obj_create(modal_bg);
    lv_obj_set_size(modal_win, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(modal_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(modal_win, 6, 0);
    lv_obj_set_style_pad_gap(modal_win, 6, 0);
    lv_obj_set_style_bg_color(modal_win, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_win, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_win, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(modal_win, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(modal_win);
    if (LeadscrewProxy::isPitchTpiMode()) {
        lv_label_set_text(title, "Pitch (TPI)");
    } else {
        lv_label_set_text(title, CoordinateSystem::isLinearInchMode() ? "Pitch (in)" : "Pitch (mm)");
    }
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, modal_accent_blue_grey(), 0);

    lv_obj_t *row = lv_obj_create(modal_win);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 56);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);

    ta_value = lv_textarea_create(row);
    lv_obj_set_height(ta_value, 56);
    lv_obj_set_flex_grow(ta_value, 1);
    lv_textarea_set_one_line(ta_value, true);
    lv_obj_clear_flag(ta_value, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(ta_value, onTextareaClicked, LV_EVENT_CLICKED, nullptr);
	lv_obj_set_style_text_font(ta_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta_value, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ta_value, 0, LV_PART_MAIN);
	// Selection styling - blue-grey to match modal buttons
	lv_obj_set_style_bg_color(ta_value, modal_accent_blue_grey(), LV_PART_SELECTED);
	lv_obj_set_style_bg_opa(ta_value, LV_OPA_COVER, LV_PART_SELECTED);

	char pbuf[32];
    if (LeadscrewProxy::isPitchTpiMode()) {
        float tpi = 25400.0f / fabsf((float)LeadscrewProxy::getPitchUm());
        snprintf(pbuf, sizeof(pbuf), "%.2f", tpi);
    } else if (CoordinateSystem::isLinearInchMode()) {
        snprintf(pbuf, sizeof(pbuf), "%.3f", (float)LeadscrewProxy::getPitchUm() / 25400.0f);
    } else {
        snprintf(pbuf, sizeof(pbuf), "%.2f", (float)LeadscrewProxy::getPitchUm() / 1000.0f);
    }
    trim_trailing_zeros_inplace(pbuf);
    lv_textarea_set_text(ta_value, pbuf);
    mark_select_all(ta_value);

    const int btn_w = OffsetManager::getMainOffsetButtonWidth();

    lv_obj_t *btn_ok = lv_btn_create(row);
    lv_obj_set_size(btn_ok, btn_w, 40);
    lv_obj_add_event_cb(btn_ok, onPitchOk, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(btn_ok, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_ok, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblo = lv_label_create(btn_ok);
    lv_label_set_text(lblo, "OK");
    lv_obj_center(lblo);
    apply_modal_button_common_style(btn_ok);

    lv_obj_t *btn_x = lv_btn_create(row);
    lv_obj_set_size(btn_x, btn_w, 40);
    lv_obj_add_event_cb(btn_x, onCancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_x, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *lblx = lv_label_create(btn_x);
    lv_label_set_text(lblx, "X");
    lv_obj_center(lblx);
    apply_modal_button_common_style(btn_x);

    kb = create_numpad(modal_win);
}

void ModalManager::showEndstopModal(bool is_max) {
    if (modal_bg) return;
    pitch_modal = false;
    endstop_modal = true;
    endstop_is_max = is_max;

    modal_bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_bg, 0, LV_PART_MAIN);
    lv_obj_add_flag(modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);

    modal_win = lv_obj_create(modal_bg);
    lv_obj_set_size(modal_win, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(modal_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(modal_win, 6, 0);
    lv_obj_set_style_pad_gap(modal_win, 6, 0);
    lv_obj_set_style_bg_color(modal_win, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal_win, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_win, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(modal_win, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(modal_win);
    const char *unit = CoordinateSystem::isLinearInchMode() ? "in" : "mm";
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "Z %s %s (%s)", is_max ? "Max" : "Min", is_max ? "]" : "[", unit);
    lv_label_set_text(title, tbuf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, modal_accent_blue_grey(), 0);

    lv_obj_t *row = lv_obj_create(modal_win);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 56);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);

    ta_value = lv_textarea_create(row);
    lv_obj_set_height(ta_value, 56);
    lv_obj_set_flex_grow(ta_value, 1);
    lv_textarea_set_one_line(ta_value, true);
    lv_obj_clear_flag(ta_value, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(ta_value, onTextareaClicked, LV_EVENT_CLICKED, nullptr);
	lv_obj_set_style_text_font(ta_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta_value, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ta_value, 0, LV_PART_MAIN);
	// Selection styling - blue-grey to match modal buttons
	lv_obj_set_style_bg_color(ta_value, modal_accent_blue_grey(), LV_PART_SELECTED);
	lv_obj_set_style_bg_opa(ta_value, LV_OPA_COVER, LV_PART_SELECTED);

	const int tool = ToolManager::getCurrentTool();
    char pbuf[32];
    int32_t display_um = EndstopProxy::areEndstopsSet()
        ? (is_max ? EndstopProxy::getMaxDisplayUm(tool) : EndstopProxy::getMinDisplayUm(tool))
        : CoordinateSystem::getDisplayZ(tool);
    CoordinateSystem::formatLinear(pbuf, sizeof(pbuf), display_um);
    trim_trailing_zeros_inplace(pbuf);
    lv_textarea_set_text(ta_value, pbuf);
    mark_select_all(ta_value);

    const int btn_w = 50;

    lv_obj_t *btn_ok = lv_btn_create(row);
    lv_obj_set_size(btn_ok, btn_w, 40);
    lv_obj_add_event_cb(btn_ok, onEndstopOk, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(btn_ok, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_ok, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblo = lv_label_create(btn_ok);
    lv_label_set_text(lblo, "OK");
    lv_obj_center(lblo);
    apply_modal_button_common_style(btn_ok);

    lv_obj_t *btn_clr = lv_btn_create(row);
    lv_obj_set_size(btn_clr, btn_w, 40);
    lv_obj_add_event_cb(btn_clr, onEndstopClear, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(btn_clr, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_clr, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lblclr = lv_label_create(btn_clr);
    lv_label_set_text(lblclr, "CLR");
    lv_obj_center(lblclr);
    apply_modal_button_common_style(btn_clr);

    lv_obj_t *btn_x = lv_btn_create(row);
    lv_obj_set_size(btn_x, btn_w, 40);
    lv_obj_add_event_cb(btn_x, onCancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_x, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_x, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_x, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *lblx = lv_label_create(btn_x);
    lv_label_set_text(lblx, "X");
    lv_obj_center(lblx);
    apply_modal_button_common_style(btn_x);

    kb = create_numpad(modal_win);
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
        CoordinateSystem::parseLinearToUm(txt, &target_um);
        int32_t target_machine_um = CoordinateSystem::xUserToMachineUm(target_um);
        CoordinateSystem::x_tool_um[tool] = CoordinateSystem::x_raw_um - CoordinateSystem::x_global_um[off] - target_machine_um;
    } else if (active_axis == AXIS_Z) {
        int32_t target_um = 0;
        CoordinateSystem::parseLinearToUm(txt, &target_um);
        int32_t target_machine_um = CoordinateSystem::zUserToMachineUm(target_um);
        CoordinateSystem::z_tool_um[tool] = CoordinateSystem::z_raw_um - CoordinateSystem::z_global_um[off] - target_machine_um;
    } else {
        int32_t target_deg_x100 = 0;
        CoordinateSystem::parseDegToDegX100(txt, &target_deg_x100);
        int32_t target_ticks = CoordinateSystem::degX100ToTicks(target_deg_x100);
        int32_t raw = CoordinateSystem::wrap01599(EncoderProxy::getRawTicks());
        CoordinateSystem::c_tool_ticks[tool] = CoordinateSystem::wrap01599(raw - CoordinateSystem::c_global_ticks[off] - target_ticks);
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
        CoordinateSystem::parseLinearToUm(txt, &target_um);
        int32_t target_machine_um = CoordinateSystem::xUserToMachineUm(target_um);
        CoordinateSystem::x_global_um[off] = CoordinateSystem::x_raw_um - CoordinateSystem::x_tool_um[tool] - target_machine_um;
    } else if (active_axis == AXIS_Z) {
        int32_t target_um = 0;
        CoordinateSystem::parseLinearToUm(txt, &target_um);
        int32_t target_machine_um = CoordinateSystem::zUserToMachineUm(target_um);
        CoordinateSystem::z_global_um[off] = CoordinateSystem::z_raw_um - CoordinateSystem::z_tool_um[tool] - target_machine_um;
    } else {
        int32_t target_deg_x100 = 0;
        CoordinateSystem::parseDegToDegX100(txt, &target_deg_x100);
        int32_t target_ticks = CoordinateSystem::degX100ToTicks(target_deg_x100);
        int32_t raw = CoordinateSystem::wrap01599(EncoderProxy::getRawTicks());
        CoordinateSystem::c_global_ticks[off] = CoordinateSystem::wrap01599(raw - CoordinateSystem::c_tool_ticks[tool] - target_ticks);
    }
}

void ModalManager::onCancel(lv_event_t *e) { (void)e; closeModal(); }
void ModalManager::onNumpadClear(lv_event_t *e) { (void)e; if (ta_value) lv_textarea_set_text(ta_value, ""); }
void ModalManager::onNumpadBackspace(lv_event_t *e) { (void)e; if (ta_value) lv_textarea_delete_char(ta_value); }

void ModalManager::onNumpadKey(lv_event_t *e) {
    if (!ta_value) return;
    int key = (int)(intptr_t)lv_event_get_user_data(e);

    if (ta_select_all_pending && key >= '0' && key <= '9') {
        lv_textarea_set_text(ta_value, "");
        clear_select_all();
    }

    if (key >= '0' && key <= '9') { lv_textarea_add_char(ta_value, (char)key); return; }

    const char *cur = lv_textarea_get_text(ta_value);
    if (!cur) cur = "";

    if (key == '.') {
        if (ta_select_all_pending) { lv_textarea_set_text(ta_value, "0."); clear_select_all(); return; }
        if (strchr(cur, '.')) return;
        if (cur[0] == '\0') { lv_textarea_set_text(ta_value, "0."); return; }
        if (strcmp(cur, "-") == 0) { lv_textarea_set_text(ta_value, "-0."); return; }
        lv_textarea_add_char(ta_value, '.');
        return;
    }

    if (key == '-') {
        if (ta_select_all_pending) { lv_textarea_set_text(ta_value, "-"); clear_select_all(); return; }
        char buf[64];
        if (cur[0] == '-') strncpy(buf, cur + 1, sizeof(buf) - 1);
        else { buf[0] = '-'; strncpy(buf + 1, cur, sizeof(buf) - 2); }
        buf[sizeof(buf) - 1] = '\0';
        lv_textarea_set_text(ta_value, buf);
    }
}

void ModalManager::onSetTool(lv_event_t *e) { (void)e; applyToolOffset(); closeModal(); }
void ModalManager::onSetGlobal(lv_event_t *e) { (void)e; applyGlobalOffset(); closeModal(); }

void ModalManager::applyPitch() {
    if (!ta_value) return;
    float v = atof(lv_textarea_get_text(ta_value));
    if (v == 0.0f) return;

    int32_t pitch_um;
    if (LeadscrewProxy::isPitchTpiMode()) {
        pitch_um = (int32_t)lroundf(25400.0f / v);
    } else if (CoordinateSystem::isLinearInchMode()) {
        pitch_um = (int32_t)lroundf(v * 25400.0f);
    } else {
        pitch_um = (int32_t)lroundf(v * 1000.0f);
    }
    LeadscrewProxy::setPitchUm(pitch_um);
}

void ModalManager::onPitchOk(lv_event_t *e) { (void)e; applyPitch(); closeModal(); }

void ModalManager::applyEndstop() {
    if (!ta_value) return;
    const int tool = ToolManager::getCurrentTool();
    int32_t target_um = 0;
    if (!CoordinateSystem::parseLinearToUm(lv_textarea_get_text(ta_value), &target_um))
        target_um = CoordinateSystem::getDisplayZ(tool);

    if (endstop_is_max) EndstopProxy::setMaxFromDisplayUm(target_um, tool);
    else EndstopProxy::setMinFromDisplayUm(target_um, tool);
    updateEndstopButtonStates();
}

void ModalManager::onEndstopOk(lv_event_t *e) { (void)e; applyEndstop(); closeModal(); }

void ModalManager::onEndstopClear(lv_event_t *e) {
    (void)e;
    if (endstop_is_max) EndstopProxy::clearMax();
    else EndstopProxy::clearMin();
    updateEndstopButtonStates();
    closeModal();
}
