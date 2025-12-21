#include "ui_ui.h"
#include "coordinates_ui.h"
#include "encoder_proxy.h"
#include "offsets_ui.h"
#include "tools_ui.h"
#include "config_ui.h"
#include "mono_font_ui.h"
#include "leadscrew_proxy.h"
#include "endstop_proxy.h"
#include "spi_master.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

static const lv_style_prop_t NO_TRANS_PROPS[] = {0};
static const lv_style_transition_dsc_t NO_TRANSITION = {
    .props = NO_TRANS_PROPS, .user_data = nullptr,
    .path_xcb = lv_anim_path_linear, .time = 0, .delay = 0,
};

static void apply_button_common_style(lv_obj_t *btn) {
    if (!btn) return;
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN);
    lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_PRESSED);
}

lv_obj_t *UIManager::lbl_x = nullptr;
lv_obj_t *UIManager::lbl_z = nullptr;
lv_obj_t *UIManager::lbl_c = nullptr;
lv_obj_t *UIManager::lbl_x_unit = nullptr;
lv_obj_t *UIManager::lbl_z_unit = nullptr;
lv_obj_t *UIManager::lbl_c_unit = nullptr;
lv_obj_t *UIManager::lbl_x_name = nullptr;
lv_obj_t *UIManager::lbl_z_name = nullptr;
lv_obj_t *UIManager::lbl_c_name = nullptr;
lv_obj_t *UIManager::lbl_units_mode = nullptr;
lv_obj_t *UIManager::lbl_pitch = nullptr;
lv_obj_t *UIManager::lbl_pitch_mode = nullptr;
lv_obj_t *UIManager::btn_jog_l = nullptr;
lv_obj_t *UIManager::btn_jog_r = nullptr;
lv_obj_t *UIManager::btn_endstop_min_ptr = nullptr;
lv_obj_t *UIManager::btn_endstop_max_ptr = nullptr;
bool UIManager::els_latched = false;
bool UIManager::endstop_min_long_pressed = false;
bool UIManager::endstop_max_long_pressed = false;
bool UIManager::btn_left_last = true;  // HIGH (not pressed) initially
bool UIManager::btn_right_last = true; // HIGH (not pressed) initially

void UIManager::updateJogAvailability() {
	// When ELS is active on one button, show other as blue-grey (still clickable for e-stop)
	const bool l_checked = lv_obj_has_state(btn_jog_l, LV_STATE_CHECKED);
	const bool r_checked = lv_obj_has_state(btn_jog_r, LV_STATE_CHECKED);

	// Set right button color based on left button state
	if (l_checked)
	{
		// Left is active (red) - right should be blue-grey
		lv_obj_set_style_bg_color(btn_jog_r, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_jog_r, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), LV_PART_MAIN);
	}
	else if (!r_checked)
	{
		// Neither active - right should be green
		lv_obj_set_style_bg_color(btn_jog_r, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_jog_r, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	}

	// Set left button color based on right button state
	if (r_checked)
	{
		// Right is active (red) - left should be blue-grey
		lv_obj_set_style_bg_color(btn_jog_l, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_jog_l, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), LV_PART_MAIN);
	}
	else if (!l_checked)
	{
		// Neither active - left should be green
		lv_obj_set_style_bg_color(btn_jog_l, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_jog_l, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	}
}

void UIManager::init() { createUI(); }

void UIManager::createUI() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 4, 0);
    lv_obj_set_style_pad_row(scr, 4, 0);
    lv_obj_set_style_pad_column(scr, 4, 0);

    static int32_t scr_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t scr_row_dsc[] = {LV_GRID_FR(1), 44, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(scr, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(scr, scr_col_dsc, scr_row_dsc);

    lv_obj_t *main_row = lv_obj_create(scr);
    lv_obj_set_grid_cell(main_row, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_clear_flag(main_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 0, 0);

    static int32_t main_col_dsc[] = {LV_GRID_FR(1), 188, LV_GRID_TEMPLATE_LAST};
    static int32_t main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(main_row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(main_row, main_col_dsc, main_row_dsc);
    lv_obj_set_style_pad_column(main_row, 10, 0);

    lv_obj_t *dro_col = lv_obj_create(main_row);
    lv_obj_set_grid_cell(dro_col, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_clear_flag(dro_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(dro_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dro_col, 0, 0);
    lv_obj_set_style_pad_all(dro_col, 0, 0);
    lv_obj_set_style_pad_row(dro_col, 4, 0);

    static int32_t dro_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t dro_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(dro_col, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(dro_col, dro_col_dsc, dro_row_dsc);

    lv_obj_t *feature_col = lv_obj_create(main_row);
    lv_obj_set_grid_cell(feature_col, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_clear_flag(feature_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(feature_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(feature_col, 0, 0);
    lv_obj_set_style_pad_all(feature_col, 0, 0);
    lv_obj_set_flex_flow(feature_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(feature_col, 6, 0);

    // Work offset selector (G1/G2/G3)
    lv_obj_t *offset_row = lv_obj_create(feature_col);
    lv_obj_set_width(offset_row, LV_PCT(100));
    lv_obj_set_height(offset_row, 44);
    lv_obj_clear_flag(offset_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(offset_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(offset_row, 0, 0);
    lv_obj_set_style_pad_all(offset_row, 0, 0);
    lv_obj_set_style_pad_gap(offset_row, 4, 0);
    lv_obj_set_flex_flow(offset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(offset_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < OFFSET_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(offset_row);
        lv_obj_set_height(btn, 40);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, OffsetManager::onOffsetSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
        apply_button_common_style(btn);
        OffsetManager::registerButton(i, btn);
        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8]; snprintf(txt, sizeof(txt), "G%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    }

    // Units + pitch mode row
    lv_obj_t *units_row = lv_obj_create(feature_col);
    lv_obj_set_width(units_row, LV_PCT(100));
    lv_obj_set_height(units_row, 44);
    lv_obj_clear_flag(units_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(units_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(units_row, 0, 0);
    lv_obj_set_style_pad_all(units_row, 0, 0);
    lv_obj_set_style_pad_gap(units_row, 4, 0);
    lv_obj_set_flex_flow(units_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *btn_units = lv_btn_create(units_row);
    lv_obj_set_height(btn_units, 44);
    lv_obj_set_flex_grow(btn_units, 1);
    lv_obj_clear_flag(btn_units, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_units, onToggleUnits, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_units, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_units, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    apply_button_common_style(btn_units);
    lbl_units_mode = lv_label_create(btn_units);
    lv_label_set_text(lbl_units_mode, CoordinateSystem::isLinearInchMode() ? "INCH" : "MM");
    lv_obj_center(lbl_units_mode);

	lv_obj_t *btn_pitch_mode = lv_btn_create(units_row);
	lv_obj_set_height(btn_pitch_mode, 44);
	lv_obj_set_flex_grow(btn_pitch_mode, 1);
	lv_obj_clear_flag(btn_pitch_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_pitch_mode, onTogglePitchMode, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_pitch_mode, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_pitch_mode, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    apply_button_common_style(btn_pitch_mode);
    lbl_pitch_mode = lv_label_create(btn_pitch_mode);
    lv_label_set_text(lbl_pitch_mode, LeadscrewProxy::isPitchTpiMode() ? "TPI" : "PITCH");
    lv_obj_center(lbl_pitch_mode);

    // Pitch row with endstop buttons
    lv_obj_t *pitch_row = lv_obj_create(feature_col);
    lv_obj_set_width(pitch_row, LV_PCT(100));
    lv_obj_set_height(pitch_row, 52);
    lv_obj_clear_flag(pitch_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pitch_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pitch_row, 0, 0);
    lv_obj_set_style_pad_all(pitch_row, 0, 0);
    lv_obj_set_style_pad_gap(pitch_row, 4, 0);
    lv_obj_set_flex_flow(pitch_row, LV_FLEX_FLOW_ROW);

    // Min endstop button
    btn_endstop_min_ptr = lv_btn_create(pitch_row);
    lv_obj_set_width(btn_endstop_min_ptr, 32);
    lv_obj_set_height(btn_endstop_min_ptr, LV_PCT(100));
    lv_obj_clear_flag(btn_endstop_min_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_endstop_min_ptr, onEditEndstopMin, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(btn_endstop_min_ptr, onLongPressEndstopMin, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_endstop_min_ptr, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_endstop_min_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btn_endstop_min_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btn_endstop_min_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
    apply_button_common_style(btn_endstop_min_ptr);
    lv_obj_t *lbl_emin = lv_label_create(btn_endstop_min_ptr);
    lv_label_set_text(lbl_emin, "[");
    lv_obj_center(lbl_emin);

    // Pitch value button
    lv_obj_t *btn_pitch = lv_btn_create(pitch_row);
    lv_obj_set_height(btn_pitch, LV_PCT(100));
    lv_obj_set_flex_grow(btn_pitch, 1);
    lv_obj_clear_flag(btn_pitch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_pitch, onEditPitch, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_pitch, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_pitch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    apply_button_common_style(btn_pitch);
    lbl_pitch = lv_label_create(btn_pitch);
    lv_obj_set_style_text_font(lbl_pitch, &lv_font_montserrat_24, 0);
    char pbuf[32]; LeadscrewProxy::formatPitchLabel(pbuf, sizeof(pbuf));
    lv_label_set_text(lbl_pitch, pbuf);
    lv_obj_center(lbl_pitch);

    // Max endstop button
    btn_endstop_max_ptr = lv_btn_create(pitch_row);
    lv_obj_set_width(btn_endstop_max_ptr, 32);
    lv_obj_set_height(btn_endstop_max_ptr, LV_PCT(100));
    lv_obj_clear_flag(btn_endstop_max_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_endstop_max_ptr, onEditEndstopMax, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(btn_endstop_max_ptr, onLongPressEndstopMax, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_endstop_max_ptr, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_endstop_max_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btn_endstop_max_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btn_endstop_max_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
    apply_button_common_style(btn_endstop_max_ptr);
    lv_obj_t *lbl_emax = lv_label_create(btn_endstop_max_ptr);
    lv_label_set_text(lbl_emax, "]");
    lv_obj_center(lbl_emax);

    // ELS row
    lv_obj_t *els_row = lv_obj_create(feature_col);
    lv_obj_set_width(els_row, LV_PCT(100));
    lv_obj_set_flex_grow(els_row, 1);
    lv_obj_clear_flag(els_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(els_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(els_row, 0, 0);
    lv_obj_set_style_pad_all(els_row, 0, 0);
    lv_obj_set_style_pad_gap(els_row, 4, 0);
    lv_obj_set_flex_flow(els_row, LV_FLEX_FLOW_ROW);

	// Left ELS button (positive direction)
	btn_jog_l = lv_btn_create(els_row);
	lv_obj_set_height(btn_jog_l, LV_PCT(100));
	lv_obj_set_flex_grow(btn_jog_l, 1);
	lv_obj_clear_flag(btn_jog_l, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(btn_jog_l, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_event_cb(btn_jog_l, onToggleEls, LV_EVENT_CLICKED, (void *)(intptr_t)1);
	lv_obj_set_style_bg_opa(btn_jog_l, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(btn_jog_l, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_jog_l, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_jog_l, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_jog_l, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_jog_l, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_jog_l, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_jog_l, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	apply_button_common_style(btn_jog_l);
    lv_obj_t *lbll = lv_label_create(btn_jog_l);
	lv_label_set_text(lbll, "<ELS");
	lv_obj_center(lbll);

	// Right ELS button (negative direction)
	btn_jog_r = lv_btn_create(els_row);
	lv_obj_set_height(btn_jog_r, LV_PCT(100));
	lv_obj_set_flex_grow(btn_jog_r, 1);
	lv_obj_clear_flag(btn_jog_r, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(btn_jog_r, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_add_event_cb(btn_jog_r, onToggleEls, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
	lv_obj_set_style_bg_opa(btn_jog_r, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(btn_jog_r, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_jog_r, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_jog_r, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_jog_r, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_jog_r, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_jog_r, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_jog_r, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	apply_button_common_style(btn_jog_r);
    lv_obj_t *lblr = lv_label_create(btn_jog_r);
	lv_label_set_text(lblr, "ELS>");
	lv_obj_center(lblr);

    // Axis rows
	lv_obj_t *row_x = makeAxisRow(dro_col, "X", &lbl_x, onZeroX, &lbl_x_name);
	lv_obj_t *row_z = makeAxisRow(dro_col, "Z", &lbl_z, onZeroZ, &lbl_z_name);
	lv_obj_t *row_c = makeAxisRow(dro_col, "C", &lbl_c, onZeroC, &lbl_c_name);
	lv_obj_set_grid_cell(row_x, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(row_z, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(row_c, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    // Tool row
    lv_obj_t *tool_row = lv_obj_create(scr);
    lv_obj_set_grid_cell(tool_row, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_size(tool_row, LV_PCT(100), 44);
    lv_obj_clear_flag(tool_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(tool_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tool_row, 0, 0);
    lv_obj_set_style_pad_all(tool_row, 2, 0);
    lv_obj_set_style_pad_gap(tool_row, 4, 0);
    lv_obj_set_flex_flow(tool_row, LV_FLEX_FLOW_ROW);

    for (int i = 0; i < TOOL_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(tool_row);
        lv_obj_set_height(btn, 40);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, ToolManager::onToolSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
        apply_button_common_style(btn);
        ToolManager::registerButton(i, btn);
        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8]; snprintf(txt, sizeof(txt), "T%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    }

    ToolManager::setCurrentTool(0);
    OffsetManager::setCurrentOffset(0);
    updateJogAvailability();
}

lv_obj_t *UIManager::makeAxisRow(lv_obj_t *parent, const char *name,
								 lv_obj_t **out_value_label, lv_event_cb_t zero_cb,
								 lv_obj_t **out_name_label)
{
	lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);

    static int32_t col_dsc[] = {30, 22, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(row, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(row, 0, 0);

	// Hitbox for toggling modes (short click) and MPG jog (long press for Z/C)
	lv_obj_t *hitbox = lv_obj_create(row);
    lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hitbox, 0, 0);
    lv_obj_set_style_pad_all(hitbox, 0, 0);
    lv_obj_clear_flag(hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_grid_cell(hitbox, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);
	if (strcmp(name, "X") == 0)
	{
		lv_obj_add_event_cb(hitbox, onToggleXMode, LV_EVENT_CLICKED, nullptr);
	}
	else if (strcmp(name, "Z") == 0)
	{
		// Use SHORT_CLICKED so long press doesn't also trigger the toggle
		lv_obj_add_event_cb(hitbox, onToggleZPolarity, LV_EVENT_SHORT_CLICKED, nullptr);
		lv_obj_add_event_cb(hitbox, onLongPressZ, LV_EVENT_LONG_PRESSED, nullptr);
	}
	else if (strcmp(name, "C") == 0)
	{
		// Use SHORT_CLICKED so long press doesn't also trigger the toggle
		lv_obj_add_event_cb(hitbox, onToggleCMode, LV_EVENT_SHORT_CLICKED, nullptr);
		lv_obj_add_event_cb(hitbox, onLongPressC, LV_EVENT_LONG_PRESSED, nullptr);
	}

	lv_obj_t *lbl_name = lv_label_create(row);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_name, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_grid_cell(lbl_name, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t *lbl_unit = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_grid_cell(lbl_unit, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_y(lbl_unit, 6);

    if (strcmp(name, "X") == 0) {
        lv_label_set_text(lbl_unit, CoordinateSystem::isXRadiusMode() ? "rad" : "dia");
        lv_obj_set_style_text_color(lbl_unit, CoordinateSystem::isXRadiusMode() ? 
            lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        lbl_x_unit = lbl_unit;
    } else if (strcmp(name, "Z") == 0) {
        lv_label_set_text(lbl_unit, "pos");
        lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        lbl_z_unit = lbl_unit;
    } else if (strcmp(name, "C") == 0) {
        lv_label_set_text(lbl_unit, EncoderProxy::isManualRpmMode() ? "rpm" : "deg");
        lv_obj_set_style_text_color(lbl_unit, EncoderProxy::isManualRpmMode() ?
            lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
        lbl_c_unit = lbl_unit;
    }

    lv_obj_t *lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, "0");
    lv_obj_set_style_text_font(lbl_val, get_mono_numeric_font_44(), 0);
    lv_obj_set_style_text_color(lbl_val, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_right(lbl_val, 6, 0);
    lv_obj_set_grid_cell(lbl_val, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_flag(lbl_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_val, zero_cb, LV_EVENT_CLICKED, nullptr);

    if (out_value_label) *out_value_label = lbl_val;
	if (out_name_label)
		*out_name_label = lbl_name;
	return row;
}

void UIManager::update() {
    if (!lbl_x || !lbl_z || !lbl_c) return;

    // Check bounds exceeded from motion board
    if (LeadscrewProxy::wasBoundsExceeded()) {
        LeadscrewProxy::clearBoundsExceeded();
        forceElsOff();
    }

    updateJogAvailability();

    // Update X superscript
    if (lbl_x_unit) {
        if (CoordinateSystem::isXRadiusMode()) {
            lv_label_set_text(lbl_x_unit, "rad");
            lv_obj_set_style_text_color(lbl_x_unit, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        } else {
            lv_label_set_text(lbl_x_unit, "dia");
            lv_obj_set_style_text_color(lbl_x_unit, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
        }
    }

	// Update Z superscript (unchanged by jog mode)
	if (lbl_z_unit) {
        if (CoordinateSystem::isZInverted()) {
            lv_label_set_text(lbl_z_unit, "neg");
            lv_obj_set_style_text_color(lbl_z_unit, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        } else {
            lv_label_set_text(lbl_z_unit, "pos");
            lv_obj_set_style_text_color(lbl_z_unit, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
        }
    }

    const int tool = ToolManager::getCurrentTool();
    static char buf[48];

    CoordinateSystem::formatLinear(buf, sizeof(buf), CoordinateSystem::getDisplayX(tool));
    lv_label_set_text(lbl_x, buf);

    CoordinateSystem::formatLinear(buf, sizeof(buf), CoordinateSystem::getDisplayZ(tool));
    lv_label_set_text(lbl_z, buf);

	// Z value color: red when in jog mode
	MpgModeProto mpgModeZ = SpiMaster::getMpgMode();
	bool isJogZ = (mpgModeZ == MpgModeProto::JOG_Z);
	lv_obj_set_style_text_color(lbl_z, isJogZ ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), LV_PART_MAIN);

	// C axis display: RPM when moving, degrees when stopped, grey target RPM when in RPM mode but stopped
	bool isJogC = (SpiMaster::getMpgMode() == MpgModeProto::JOG_C);
	bool spindleMoving = (abs(EncoderProxy::getRpmSigned()) > 10);

	if (EncoderProxy::shouldShowRpm() && !isJogC)
	{
		// Normal RPM display (not in jog mode)
		if (spindleMoving)
		{
			// Show actual RPM in white when spinning
			snprintf(buf, sizeof(buf), "%ld", (long)EncoderProxy::getRpmSigned());
			lv_label_set_text(lbl_c, buf);
			lv_obj_set_style_text_color(lbl_c, lv_color_white(), LV_PART_MAIN);
		}
		else
		{
			// Show target RPM in blue-grey when stopped (stepper spindle mode)
			int16_t targetRpm = EncoderProxy::getTargetRpm();
			if (targetRpm > 0)
			{
				snprintf(buf, sizeof(buf), "%d", targetRpm);
			}
			else
			{
				snprintf(buf, sizeof(buf), "0");
			}
			lv_label_set_text(lbl_c, buf);
			lv_obj_set_style_text_color(lbl_c, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), LV_PART_MAIN);
		}
		if (lbl_c_unit)
		{
			lv_label_set_text(lbl_c_unit, "rpm");
			lv_obj_set_style_text_color(lbl_c_unit, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
		}
	}
	else
	{
		// Degrees display (normal or jog mode - jog forces degrees)
		int32_t c_phase = CoordinateSystem::getDisplayC(EncoderProxy::getRawTicks(), tool);
		CoordinateSystem::formatDeg(buf, sizeof(buf), CoordinateSystem::ticksToDegX100(c_phase));
		lv_label_set_text(lbl_c, buf);
		// Value color: red in jog mode, white otherwise
		lv_obj_set_style_text_color(lbl_c, isJogC ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), LV_PART_MAIN);
		if (lbl_c_unit) {
            lv_label_set_text(lbl_c_unit, "deg");
            lv_obj_set_style_text_color(lbl_c_unit, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
        }
	}

	if (lbl_units_mode) lv_label_set_text(lbl_units_mode, CoordinateSystem::isLinearInchMode() ? "INCH" : "MM");

    if (lbl_pitch) {
        char pbuf[32]; LeadscrewProxy::formatPitchLabel(pbuf, sizeof(pbuf));
        lv_label_set_text(lbl_pitch, pbuf);
    }

    if (lbl_pitch_mode) lv_label_set_text(lbl_pitch_mode, LeadscrewProxy::isPitchTpiMode() ? "TPI" : "PITCH");
}

void UIManager::onToggleUnits(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    CoordinateSystem::toggleLinearInchMode();
    update();
}

void UIManager::onEditPitch(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ModalManager::showPitchModal();
}

void UIManager::onTogglePitchMode(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    LeadscrewProxy::togglePitchTpiMode();
    update();
}

void UIManager::onToggleEls(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
	lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
	const int dir = (int)(intptr_t)lv_event_get_user_data(e);

	// If ELS is currently running, pressing either button turns it off (e-stop)
	if (els_latched)
	{
		// Turn off - clear both buttons
		lv_obj_clear_state(btn_jog_l, LV_STATE_CHECKED);
		lv_obj_clear_state(btn_jog_r, LV_STATE_CHECKED);
		els_latched = false;
		LeadscrewProxy::setEnabled(false);
		LeadscrewProxy::setDirectionMul(1);
		updateJogAvailability();
		return;
	}

	// ELS is off - turn it on with the specified direction
	const bool now_on = lv_obj_has_state(btn, LV_STATE_CHECKED);

	// If turning on this button, turn off the other
	if (now_on)
	{
		lv_obj_t *other = (btn == btn_jog_l) ? btn_jog_r : btn_jog_l;
		if (other && lv_obj_has_state(other, LV_STATE_CHECKED))
		{
			lv_obj_clear_state(other, LV_STATE_CHECKED);
		}
	}

	els_latched = now_on;
    LeadscrewProxy::setEnabled(now_on);
	LeadscrewProxy::setDirectionMul(now_on ? (int8_t)dir : 1);
	updateJogAvailability();
}

void UIManager::onJog(lv_event_t *e) {
	// Unused - kept for potential future use
	(void)e;
}

void UIManager::onZeroX(lv_event_t *e) { (void)e; ModalManager::showOffsetModal(AXIS_X); }
void UIManager::onZeroZ(lv_event_t *e) { (void)e; ModalManager::showOffsetModal(AXIS_Z); }
void UIManager::onZeroC(lv_event_t *e) { (void)e; ModalManager::showOffsetModal(AXIS_C); }

void UIManager::onToggleXMode(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    CoordinateSystem::toggleXRadiusMode();
    update();
}

void UIManager::onToggleZPolarity(lv_event_t *e) {
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	CoordinateSystem::toggleZInverted();
    update();
}

void UIManager::onToggleCMode(lv_event_t *e) {
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	if (EncoderProxy::canToggleManualMode()) {
        EncoderProxy::toggleManualRpmMode();
        update();
    }
}

void UIManager::onEditEndstopMin(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED) return;
    ModalManager::showEndstopModal(false);
}

void UIManager::onEditEndstopMax(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED) return;
    ModalManager::showEndstopModal(true);
}

void UIManager::onLongPressEndstopMin(lv_event_t *e) {
    (void)e;
    EndstopProxy::toggleMinEnabled();
    updateEndstopButtonStates();
}

void UIManager::onLongPressEndstopMax(lv_event_t *e) {
    (void)e;
    EndstopProxy::toggleMaxEnabled();
    updateEndstopButtonStates();
}

void UIManager::onLongPressZ(lv_event_t *e)
{
	(void)e;
	// Toggle between RPM_CONTROL and JOG_Z mode
	MpgModeProto currentMode = SpiMaster::getMpgMode();
	MpgModeProto newMode = (currentMode == MpgModeProto::JOG_Z) ? MpgModeProto::RPM_CONTROL : MpgModeProto::JOG_Z;
	SpiMaster::setMpgMode(newMode);
	Serial.printf("[UI] MPG mode -> %s\n", newMode == MpgModeProto::JOG_Z ? "JOG_Z" : "RPM");
}

void UIManager::onLongPressC(lv_event_t *e)
{
	(void)e;
	// Toggle between RPM_CONTROL and JOG_C mode
	MpgModeProto currentMode = SpiMaster::getMpgMode();
	MpgModeProto newMode = (currentMode == MpgModeProto::JOG_C) ? MpgModeProto::RPM_CONTROL : MpgModeProto::JOG_C;
	SpiMaster::setMpgMode(newMode);

	// When entering jog mode, force degrees display (exit manual RPM mode if active)
	if (newMode == MpgModeProto::JOG_C && EncoderProxy::isManualRpmMode())
	{
		EncoderProxy::toggleManualRpmMode(); // Turn off manual RPM mode
	}

	Serial.printf("[UI] MPG mode -> %s\n", newMode == MpgModeProto::JOG_C ? "JOG_C" : "RPM");
}

void UIManager::updateEndstopButtonStates() {
    if (btn_endstop_min_ptr) {
        if (EndstopProxy::isMinEnabled()) lv_obj_add_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
    }
    if (btn_endstop_max_ptr) {
        if (EndstopProxy::isMaxEnabled()) lv_obj_add_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
    }
}

void UIManager::forceElsOff() {
	LeadscrewProxy::setEnabled(false);
	LeadscrewProxy::setDirectionMul(1);
	els_latched = false;
	if (btn_jog_l)
		lv_obj_clear_state(btn_jog_l, LV_STATE_CHECKED);
	if (btn_jog_r)
		lv_obj_clear_state(btn_jog_r, LV_STATE_CHECKED);
	updateJogAvailability();
}

void UIManager::setElsButtonActive(bool active)
{
	// No longer needed - buttons handle their own checked state
	(void)active;
}

void UIManager::initPhysicalButtons()
{
	pinMode(ELS_BTN_LEFT_PIN, INPUT_PULLUP);
	pinMode(ELS_BTN_RIGHT_PIN, INPUT_PULLUP);
	btn_left_last = digitalRead(ELS_BTN_LEFT_PIN);
	btn_right_last = digitalRead(ELS_BTN_RIGHT_PIN);
	Serial.printf("[UI] Physical buttons init: L=%d, R=%d\\n", ELS_BTN_LEFT_PIN, ELS_BTN_RIGHT_PIN);
}

void UIManager::pollPhysicalButtons()
{
	bool btn_left = digitalRead(ELS_BTN_LEFT_PIN);
	bool btn_right = digitalRead(ELS_BTN_RIGHT_PIN);

	// Detect falling edge (button press, active LOW)
	if (btn_left_last && !btn_left)
	{
		triggerElsLeft();
	}
	if (btn_right_last && !btn_right)
	{
		triggerElsRight();
	}

	btn_left_last = btn_left;
	btn_right_last = btn_right;
}

void UIManager::triggerElsLeft()
{
	// Same logic as pressing the left ELS button
	if (els_latched)
	{
		// E-stop: turn off ELS
		if (btn_jog_l)
			lv_obj_clear_state(btn_jog_l, LV_STATE_CHECKED);
		if (btn_jog_r)
			lv_obj_clear_state(btn_jog_r, LV_STATE_CHECKED);
		els_latched = false;
		LeadscrewProxy::setEnabled(false);
		LeadscrewProxy::setDirectionMul(1);
	}
	else
	{
		// Turn on ELS with positive direction
		if (btn_jog_l)
			lv_obj_add_state(btn_jog_l, LV_STATE_CHECKED);
		if (btn_jog_r)
			lv_obj_clear_state(btn_jog_r, LV_STATE_CHECKED);
		els_latched = true;
		LeadscrewProxy::setEnabled(true);
		LeadscrewProxy::setDirectionMul(1);
	}
	updateJogAvailability();
}

void UIManager::triggerElsRight()
{
	// Same logic as pressing the right ELS button
	if (els_latched)
	{
		// E-stop: turn off ELS
		if (btn_jog_l)
			lv_obj_clear_state(btn_jog_l, LV_STATE_CHECKED);
		if (btn_jog_r)
			lv_obj_clear_state(btn_jog_r, LV_STATE_CHECKED);
		els_latched = false;
		LeadscrewProxy::setEnabled(false);
		LeadscrewProxy::setDirectionMul(1);
	}
	else
	{
		// Turn on ELS with negative direction
		if (btn_jog_l)
			lv_obj_clear_state(btn_jog_l, LV_STATE_CHECKED);
		if (btn_jog_r)
			lv_obj_add_state(btn_jog_r, LV_STATE_CHECKED);
		els_latched = true;
		LeadscrewProxy::setEnabled(true);
		LeadscrewProxy::setDirectionMul(-1);
	}
	updateJogAvailability();
}

// Global function for modal callback
void updateEndstopButtonStates() { UIManager::updateEndstopButtonStates(); }
