#include "ui_ui.h"
#include "coordinates_ui.h"
#include "encoder_proxy.h"
#include "offsets_ui.h"
#include "tools_ui.h"
#include "config_ui.h"
#include "mono_font_ui.h"
#include "leadscrew_proxy.h"
#include "endstop_proxy.h"
#include "sync_proxy.h"
#include "ota_proxy.h"
#include "spi_master.h"
#include "button_style_ui.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

static constexpr int32_t UI_BUTTON_RADIUS = 6;

static lv_color_t endstop_active_color()
{
	return lv_color_mix(lv_palette_main(LV_PALETTE_GREY), lv_palette_main(LV_PALETTE_RED), 180);
}

static lv_color_t endstop_hit_color()
{
	return lv_color_mix(lv_palette_main(LV_PALETTE_GREY), lv_palette_main(LV_PALETTE_GREEN), 180);
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
lv_obj_t *UIManager::btn_endstop_min_ptr = nullptr;
lv_obj_t *UIManager::btn_endstop_max_ptr = nullptr;
lv_obj_t *UIManager::btn_els_estop_ptr = nullptr;
lv_obj_t *UIManager::lbl_els_estop_ptr = nullptr;
lv_obj_t *UIManager::btn_sync_ptr = nullptr;
bool UIManager::endstop_min_long_pressed = false;
bool UIManager::endstop_max_long_pressed = false;
bool UIManager::endstop_min_hit = false;
bool UIManager::endstop_max_hit = false;
uint32_t UIManager::endstop_min_hit_ms = 0;
uint32_t UIManager::endstop_max_hit_ms = 0;

// Sync UX: avoid showing green immediately on enable before spindle moves.
static bool g_sync_seen_motion_since_enable = false;
static int32_t g_sync_enable_c_ticks = 0;

void UIManager::init() { createUI(); }

void UIManager::createUI() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 4, 0);
    lv_obj_set_style_pad_row(scr, 4, 0);
    lv_obj_set_style_pad_column(scr, 4, 0);

	ToolManager::init();
	OffsetManager::init();

    static int32_t scr_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t scr_row_dsc[] = {LV_GRID_FR(1), 48, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(scr, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(scr, scr_col_dsc, scr_row_dsc);

    lv_obj_t *main_row = lv_obj_create(scr);
    lv_obj_set_grid_cell(main_row, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_clear_flag(main_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 0, 0);

	const int scr_pad = 4;
	const int tool_row_pad = 2;
	const int tool_gap = 4;
	const int tool_btn_w = (SCREEN_W - (2 * scr_pad) - (2 * tool_row_pad) - (tool_gap * (TOOL_COUNT - 1))) / TOOL_COUNT;
	const int feature_w = (tool_btn_w * OFFSET_COUNT) + (tool_gap * (OFFSET_COUNT - 1)) + tool_row_pad;
	static int32_t main_col_dsc[] = {LV_GRID_FR(1), 0, LV_GRID_TEMPLATE_LAST};
	main_col_dsc[1] = feature_w;
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
    lv_obj_set_style_pad_row(dro_col, 2, 0);

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
	lv_obj_set_style_pad_right(feature_col, tool_row_pad, 0);
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
		lv_obj_t *btn = UiStyle::createButtonStripped(offset_row);
        lv_obj_set_height(btn, 44);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, OffsetManager::onOffsetSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);
		lv_obj_add_event_cb(btn, OffsetManager::onOffsetLongPress, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
		UiStyle::applyButtonCommonStyle(btn, UI_BUTTON_RADIUS);
        OffsetManager::registerButton(i, btn);
        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8]; snprintf(txt, sizeof(txt), "G%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
		OffsetManager::registerLabel(i, lbl);
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

	lv_obj_t *btn_units = UiStyle::createButtonStripped(units_row);
    lv_obj_set_height(btn_units, 44);
    lv_obj_set_flex_grow(btn_units, 1);
    lv_obj_clear_flag(btn_units, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_units, onToggleUnits, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_units, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_units, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	UiStyle::applyButtonCommonStyle(btn_units, UI_BUTTON_RADIUS);
    lbl_units_mode = lv_label_create(btn_units);
    lv_label_set_text(lbl_units_mode, CoordinateSystem::isLinearInchMode() ? "INCH" : "MM");
    lv_obj_center(lbl_units_mode);

	lv_obj_t *btn_pitch_mode = UiStyle::createButtonStripped(units_row);
	lv_obj_set_height(btn_pitch_mode, 44);
	lv_obj_set_flex_grow(btn_pitch_mode, 1);
	lv_obj_clear_flag(btn_pitch_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_pitch_mode, onTogglePitchMode, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_pitch_mode, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_pitch_mode, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	UiStyle::applyButtonCommonStyle(btn_pitch_mode, UI_BUTTON_RADIUS);
    lbl_pitch_mode = lv_label_create(btn_pitch_mode);
    lv_label_set_text(lbl_pitch_mode, LeadscrewProxy::isPitchTpiMode() ? "TPI" : "PITCH");
    lv_obj_center(lbl_pitch_mode);

	// Pitch row
	lv_obj_t *pitch_row = lv_obj_create(feature_col);
	lv_obj_set_width(pitch_row, LV_PCT(100));
    lv_obj_set_height(pitch_row, 52);
    lv_obj_clear_flag(pitch_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pitch_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pitch_row, 0, 0);
    lv_obj_set_style_pad_all(pitch_row, 0, 0);
    lv_obj_set_style_pad_gap(pitch_row, 4, 0);
    lv_obj_set_flex_flow(pitch_row, LV_FLEX_FLOW_ROW);

	// Pitch value button
	lv_obj_t *btn_pitch = UiStyle::createButtonStripped(pitch_row);
    lv_obj_set_height(btn_pitch, LV_PCT(100));
    lv_obj_set_flex_grow(btn_pitch, 1);
    lv_obj_clear_flag(btn_pitch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_pitch, onEditPitch, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_opa(btn_pitch, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_pitch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	UiStyle::applyButtonCommonStyle(btn_pitch, UI_BUTTON_RADIUS);
    lbl_pitch = lv_label_create(btn_pitch);
	lv_obj_set_style_text_font(lbl_pitch, &lv_font_montserrat_28, 0);
	char pbuf[32];
	LeadscrewProxy::formatPitchLabel(pbuf, sizeof(pbuf));
	lv_label_set_text(lbl_pitch, pbuf);
    lv_obj_center(lbl_pitch);

	// Sync button
	btn_sync_ptr = UiStyle::createButtonStripped(pitch_row);
	lv_obj_set_height(btn_sync_ptr, LV_PCT(100));
	lv_obj_set_width(btn_sync_ptr, 52);
	lv_obj_clear_flag(btn_sync_ptr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn_sync_ptr, onEditSync, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_add_event_cb(btn_sync_ptr, onLongPressSync, LV_EVENT_LONG_PRESSED, nullptr);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_sync_ptr, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_2);
	lv_obj_set_style_bg_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN | LV_STATE_USER_2);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN | LV_STATE_USER_2);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_2);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_2 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN | LV_STATE_USER_2 | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_sync_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN | LV_STATE_USER_2 | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_2 | LV_STATE_CHECKED);
	// Light-red state used when Sync is enabled but ELS is not.
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_3);
	lv_obj_set_style_bg_color(btn_sync_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_USER_3);
	lv_obj_set_style_border_color(btn_sync_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_USER_3);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_3);
	lv_obj_set_style_bg_opa(btn_sync_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_3 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_sync_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_USER_3 | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_sync_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_USER_3 | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_sync_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_3 | LV_STATE_CHECKED);
	UiStyle::applyButtonCommonStyle(btn_sync_ptr, UI_BUTTON_RADIUS);
	lv_obj_t *lbl_sync = lv_label_create(btn_sync_ptr);
	lv_label_set_text(lbl_sync, "|•|");
	lv_obj_set_style_text_font(lbl_sync, &lv_font_montserrat_20, 0);
	lv_obj_center(lbl_sync);

	// Endstop row
	lv_obj_t *els_row = lv_obj_create(feature_col);
	lv_obj_set_width(els_row, LV_PCT(100));
	lv_obj_set_flex_grow(els_row, 1);
	lv_obj_clear_flag(els_row, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(els_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(els_row, 0, 0);
	lv_obj_set_style_pad_all(els_row, 0, 0);
	lv_obj_set_style_pad_gap(els_row, 4, 0);
	lv_obj_set_flex_flow(els_row, LV_FLEX_FLOW_ROW);

	// Z left endstop button
	btn_endstop_min_ptr = UiStyle::createButtonStripped(els_row);
	lv_obj_set_height(btn_endstop_min_ptr, LV_PCT(100));
	lv_obj_set_flex_grow(btn_endstop_min_ptr, 1);
	lv_obj_clear_flag(btn_endstop_min_ptr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn_endstop_min_ptr, onEditEndstopMin, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_add_event_cb(btn_endstop_min_ptr, onLongPressEndstopMin, LV_EVENT_LONG_PRESSED, nullptr);
	lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_endstop_min_ptr, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_endstop_min_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_min_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_min_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_color(btn_endstop_min_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_border_color(btn_endstop_min_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_opa(btn_endstop_min_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_min_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_min_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_min_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	UiStyle::applyButtonCommonStyle(btn_endstop_min_ptr, UI_BUTTON_RADIUS);
	lv_obj_t *lbl_emin = lv_label_create(btn_endstop_min_ptr);
	lv_label_set_text(lbl_emin, "|<");
	lv_obj_set_style_text_font(lbl_emin, &lv_font_montserrat_20, 0);
	lv_obj_center(lbl_emin);

	// Center ELS pseudo e-stop button (disabled unless ELS is enabled)
	btn_els_estop_ptr = UiStyle::createButtonStripped(els_row);
	lv_obj_set_height(btn_els_estop_ptr, LV_PCT(100));
	// Small button: just wide enough for "<" / ">".
	lv_obj_set_width(btn_els_estop_ptr, 44);
	lv_obj_clear_flag(btn_els_estop_ptr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn_els_estop_ptr, onElsEStop, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_set_style_bg_opa(btn_els_estop_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_els_estop_ptr, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_els_estop_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_els_estop_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	// Disabled/greyed
	lv_obj_set_style_bg_opa(btn_els_estop_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_bg_color(btn_els_estop_ptr, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_border_color(btn_els_estop_ptr, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN | LV_STATE_DISABLED);
	lv_obj_set_style_text_color(btn_els_estop_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_DISABLED);
	UiStyle::applyButtonCommonStyle(btn_els_estop_ptr, UI_BUTTON_RADIUS);
	lbl_els_estop_ptr = lv_label_create(btn_els_estop_ptr);
	// Hidden label while disabled; direction arrow is shown when enabled.
	lv_label_set_text(lbl_els_estop_ptr, "•");
	lv_obj_set_style_text_font(lbl_els_estop_ptr, &lv_font_montserrat_20, 0);
	lv_obj_center(lbl_els_estop_ptr);
	lv_obj_add_state(btn_els_estop_ptr, LV_STATE_DISABLED);

	// Z right endstop button
	btn_endstop_max_ptr = UiStyle::createButtonStripped(els_row);
	lv_obj_set_height(btn_endstop_max_ptr, LV_PCT(100));
	lv_obj_set_flex_grow(btn_endstop_max_ptr, 1);
	lv_obj_clear_flag(btn_endstop_max_ptr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_event_cb(btn_endstop_max_ptr, onEditEndstopMax, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_add_event_cb(btn_endstop_max_ptr, onLongPressEndstopMax, LV_EVENT_LONG_PRESSED, nullptr);
	lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_endstop_max_ptr, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_endstop_max_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_max_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_max_ptr, endstop_active_color(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_color(btn_endstop_max_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_border_color(btn_endstop_max_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1);
	lv_obj_set_style_bg_opa(btn_endstop_max_ptr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_max_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_max_ptr, endstop_hit_color(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_max_ptr, lv_color_white(), LV_PART_MAIN | LV_STATE_USER_1 | LV_STATE_CHECKED);
	UiStyle::applyButtonCommonStyle(btn_endstop_max_ptr, UI_BUTTON_RADIUS);
	lv_obj_t *lbl_emax = lv_label_create(btn_endstop_max_ptr);
	lv_label_set_text(lbl_emax, ">|");
	lv_obj_set_style_text_font(lbl_emax, &lv_font_montserrat_20, 0);
	lv_obj_center(lbl_emax);

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
    lv_obj_set_size(tool_row, LV_PCT(100), 48);
    lv_obj_clear_flag(tool_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(tool_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tool_row, 0, 0);
    lv_obj_set_style_pad_all(tool_row, 2, 0);
    lv_obj_set_style_pad_gap(tool_row, 4, 0);
    lv_obj_set_flex_flow(tool_row, LV_FLEX_FLOW_ROW);

    for (int i = 0; i < TOOL_COUNT; i++) {
		lv_obj_t *btn = UiStyle::createButtonStripped(tool_row);
        lv_obj_set_height(btn, 44);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_event_cb(btn, ToolManager::onToolSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);
		lv_obj_add_event_cb(btn, ToolManager::onToolLongPress, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

		// Override default theme focus styling (often blue).
		lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);
		lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_FOCUSED);
		lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_FOCUSED);
		lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
		lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_FOCUS_KEY);
		lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_FOCUS_KEY);

		// Avoid LVGL theme "pressed" color flash (default theme uses blue).
		// Keep the press feedback purely as a transform (see apply_button_common_style).
		lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);

        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUSED);
		lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUSED);
		lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUSED);
		lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUSED);
		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
		lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
		lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);
		lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_FOCUS_KEY);

		// When the selected tool is pressed, keep it visually selected.
		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
		lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
		lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
		lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
		UiStyle::applyButtonCommonStyle(btn, UI_BUTTON_RADIUS);
        ToolManager::registerButton(i, btn);
        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8]; snprintf(txt, sizeof(txt), "T%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
		ToolManager::registerLabel(i, lbl);
    }

    ToolManager::setCurrentTool(ToolManager::getCurrentTool());
    OffsetManager::setCurrentOffset(OffsetManager::getCurrentOffset());
}

lv_obj_t *UIManager::makeAxisRow(lv_obj_t *parent, const char *name,
								 lv_obj_t **out_value_label, lv_event_cb_t zero_cb,
								 lv_obj_t **out_name_label)
{
	lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 1, 0);
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
        // Initial unit based on current mode
        bool showRpm = EncoderProxy::shouldShowRpm();
        lv_label_set_text(lbl_unit, showRpm ? "rpm" : "deg");
        lv_obj_set_style_text_color(lbl_unit, showRpm ?
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
    lv_obj_add_event_cb(lbl_val, zero_cb, LV_EVENT_SHORT_CLICKED, nullptr);
    if (strcmp(name, "Z") == 0) {
        lv_obj_add_event_cb(lbl_val, onLongPressZ, LV_EVENT_LONG_PRESSED, nullptr);
    } else if (strcmp(name, "C") == 0) {
        lv_obj_add_event_cb(lbl_val, onLongPressC, LV_EVENT_LONG_PRESSED, nullptr);
    }

    if (out_value_label) *out_value_label = lbl_val;
	if (out_name_label)
		*out_name_label = lbl_name;
	return row;
}

void UIManager::update() {
    if (!lbl_x || !lbl_z || !lbl_c) return;

	static constexpr uint32_t ENDSTOP_HIT_HOLD_MS = 1000;

	// Keep ELS-dependent UI (center pseudo e-stop) in sync with motion state.
	{
		static bool prev_els_enabled = false;
		static int8_t prev_els_dir_mul = 0;
		const bool els_enabled = LeadscrewProxy::isEnabled();
		const int8_t dir_mul = LeadscrewProxy::getDirectionMul();
		if (els_enabled != prev_els_enabled || (els_enabled && dir_mul != prev_els_dir_mul))
		{
			updateEndstopButtonStates();
			prev_els_enabled = els_enabled;
			prev_els_dir_mul = dir_mul;
		}
	}

    // Check bounds exceeded from motion board
    if (LeadscrewProxy::wasBoundsExceeded()) {
        LeadscrewProxy::clearBoundsExceeded();
		const int32_t z_um = CoordinateSystem::z_raw_um;
		if (EndstopProxy::isMinEnabled() && z_um <= EndstopProxy::getMinMachineUm()) {
			endstop_min_hit = true;
			endstop_min_hit_ms = millis();
		}
		if (EndstopProxy::isMaxEnabled() && z_um >= EndstopProxy::getMaxMachineUm()) {
			endstop_max_hit = true;
			endstop_max_hit_ms = millis();
		}
        forceElsOff();
		updateEndstopButtonStates();
    }

	// Clear the green "hit" indication after a short hold.
	// This keeps the UI informative without keeping a stale latched state.
	bool endstop_state_changed = false;
	const uint32_t now_ms = millis();
	if (endstop_min_hit && (now_ms - endstop_min_hit_ms) > ENDSTOP_HIT_HOLD_MS)
	{
		endstop_min_hit = false;
		endstop_state_changed = true;
	}
	if (endstop_max_hit && (now_ms - endstop_max_hit_ms) > ENDSTOP_HIT_HOLD_MS)
	{
		endstop_max_hit = false;
		endstop_state_changed = true;
	}
	if (endstop_state_changed)
		updateEndstopButtonStates();

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

	// C axis display: explicit modes
	// RPM_CONTROL mode: show RPM (target when stopped, actual when running)
	// JOG_C mode: show degrees (red text)
	// JOG_Z mode: show degrees (normal)
	MpgModeProto mpgModeC = SpiMaster::getMpgMode();
	bool isJogC = (mpgModeC == MpgModeProto::JOG_C);
	bool spindleRunning = EncoderProxy::isSpindleRunning();

	if (EncoderProxy::shouldShowRpm())
	{
		// RPM_CONTROL mode - show RPM
		if (spindleRunning)
		{
			// Show actual RPM in white when enabled/toggled on
			// Quantize to nearest valid display step based on segment
			const int32_t rpm_x10_raw = EncoderProxy::getRpmSigned();
			const int32_t abs_rpm_x10_raw = (rpm_x10_raw < 0) ? -rpm_x10_raw : rpm_x10_raw;
			const int32_t abs_rpm_x10 = quantizeRpmX10ForDisplay(abs_rpm_x10_raw);
			const int32_t rpm_int = abs_rpm_x10 / 10;
			const int32_t rpm_frac = abs_rpm_x10 % 10;
			snprintf(buf, sizeof(buf), "%s%ld.%01ld", (rpm_x10_raw < 0) ? "-" : "", (long)rpm_int, (long)rpm_frac);
			lv_label_set_text(lbl_c, buf);
			lv_obj_set_style_text_color(lbl_c, lv_color_white(), LV_PART_MAIN);
		}
		else
		{
			// Show target RPM in blue-grey when stopped
			// Quantize to nearest valid display step based on segment
			int16_t targetRpm_x10_raw = EncoderProxy::getTargetRpm();
			if (targetRpm_x10_raw < 0) targetRpm_x10_raw = 0;
			const int32_t targetRpm_x10 = quantizeRpmX10ForDisplay(targetRpm_x10_raw);
			const int32_t tgt_int = targetRpm_x10 / 10;
			const int32_t tgt_frac = targetRpm_x10 % 10;
			snprintf(buf, sizeof(buf), "%ld.%01ld", (long)tgt_int, (long)tgt_frac);
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
		// JOG mode - show degrees
		int32_t c_phase = CoordinateSystem::getDisplayC(EncoderProxy::getRawTicks(), tool);
		CoordinateSystem::formatDeg(buf, sizeof(buf), CoordinateSystem::ticksToDegX100(c_phase));
		lv_label_set_text(lbl_c, buf);
		// Value color: red in JOG_C mode, white otherwise
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

	updateSyncButtonStates();
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

void UIManager::onEditSync(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	SyncProxy::toggleEnabled();
	if (SyncProxy::isEnabled())
	{
		g_sync_seen_motion_since_enable = EncoderProxy::isSpindleRunning();
		g_sync_enable_c_ticks = EncoderProxy::getRawTicks();
	}
	else
	{
		g_sync_seen_motion_since_enable = false;
		g_sync_enable_c_ticks = 0;
	}
	updateSyncButtonStates();
}

void UIManager::onTogglePitchMode(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    LeadscrewProxy::togglePitchTpiMode();
    update();
}

void UIManager::onLongPressSync(lv_event_t *e)
{
	(void)e;
	OtaProxy::start();
	forceElsOff();
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
	// Toggle between RPM_CONTROL and JOG_C modes
	MpgModeProto current = SpiMaster::getMpgMode();
	MpgModeProto newMode = (current == MpgModeProto::JOG_C) ? MpgModeProto::RPM_CONTROL : MpgModeProto::JOG_C;
	SpiMaster::setMpgMode(newMode);
	Serial.printf("[UI] MPG mode -> %s (via C unit tap)\n", newMode == MpgModeProto::JOG_C ? "JOG_C" : "RPM");
	update();
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
	if (!EndstopProxy::isMinEnabled()) endstop_min_hit = false;
    updateEndstopButtonStates();
}

void UIManager::onLongPressEndstopMax(lv_event_t *e) {
    (void)e;
    EndstopProxy::toggleMaxEnabled();
	if (!EndstopProxy::isMaxEnabled()) endstop_max_hit = false;
    updateEndstopButtonStates();
}

void UIManager::onElsEStop(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	// Only allow UI pseudo e-stop when ELS is actually enabled.
	if (!LeadscrewProxy::isEnabled())
		return;
	SpiMaster::requestDisableEls();
}

void UIManager::onLongPressZ(lv_event_t *e)
{
	(void)e;
	// Toggle between RPM_CONTROL and JOG_Z mode
	MpgModeProto currentMode = SpiMaster::getMpgMode();
	MpgModeProto newMode = (currentMode == MpgModeProto::JOG_Z) ? MpgModeProto::RPM_CONTROL : MpgModeProto::JOG_Z;
	if (newMode == MpgModeProto::JOG_Z)
	{
		forceElsOff();
	}
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

	Serial.printf("[UI] MPG mode -> %s\n", newMode == MpgModeProto::JOG_C ? "JOG_C" : "RPM");
}

void UIManager::updateEndstopButtonStates() {
	// Update ELS pseudo e-stop button state
	if (btn_els_estop_ptr && lbl_els_estop_ptr)
	{
		static bool prev_els_enabled = false;
		static int8_t prev_dir_mul = 0;
		const bool els_enabled = LeadscrewProxy::isEnabled();
		const int8_t dir_mul = LeadscrewProxy::getDirectionMul();

		if (els_enabled != prev_els_enabled || (els_enabled && dir_mul != prev_dir_mul))
		{
			if (!els_enabled)
			{
				lv_obj_add_state(btn_els_estop_ptr, LV_STATE_DISABLED);
				lv_obj_set_style_bg_opa(btn_els_estop_ptr, LV_OPA_TRANSP, LV_PART_MAIN);
				lv_obj_set_style_border_color(btn_els_estop_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
				lv_obj_set_style_text_color(btn_els_estop_ptr, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
				lv_label_set_text(lbl_els_estop_ptr, "|");
			}
			else
			{
				lv_obj_clear_state(btn_els_estop_ptr, LV_STATE_DISABLED);
				lv_obj_set_style_bg_opa(btn_els_estop_ptr, LV_OPA_COVER, LV_PART_MAIN);
				lv_obj_set_style_bg_color(btn_els_estop_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
				lv_obj_set_style_border_color(btn_els_estop_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
				lv_obj_set_style_text_color(btn_els_estop_ptr, lv_color_white(), LV_PART_MAIN);
				lv_label_set_text(lbl_els_estop_ptr, (dir_mul < 0) ? "<" : ">");
			}
			prev_els_enabled = els_enabled;
			prev_dir_mul = dir_mul;
		}
	}

    if (btn_endstop_min_ptr) {
		if (!EndstopProxy::hasMinValue() || !EndstopProxy::isMinEnabled()) endstop_min_hit = false;
        if (EndstopProxy::isMinEnabled()) lv_obj_add_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
		if (endstop_min_hit) lv_obj_add_state(btn_endstop_min_ptr, LV_STATE_USER_1);
		else lv_obj_clear_state(btn_endstop_min_ptr, LV_STATE_USER_1);
    }
    if (btn_endstop_max_ptr) {
		if (!EndstopProxy::hasMaxValue() || !EndstopProxy::isMaxEnabled()) endstop_max_hit = false;
        if (EndstopProxy::isMaxEnabled()) lv_obj_add_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
		if (endstop_max_hit) lv_obj_add_state(btn_endstop_max_ptr, LV_STATE_USER_1);
		else lv_obj_clear_state(btn_endstop_max_ptr, LV_STATE_USER_1);
    }
}

void UIManager::updateSyncButtonStates()
{
	if (!btn_sync_ptr)
		return;

	if (!SyncProxy::isEnabled())
	{
		lv_obj_clear_state(btn_sync_ptr, LV_STATE_CHECKED);
		lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_1);
		lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_2);
		lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_3);
		return;
	}

	// Sync enabled: show red/orange/green based on current speed correction.
	// Color meaning is based on actual sync error magnitude (microns).
	lv_obj_add_state(btn_sync_ptr, LV_STATE_CHECKED);
	// Clear overlay states first
	lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_1);
	lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_2);
	lv_obj_clear_state(btn_sync_ptr, LV_STATE_USER_3);

	// If Sync is ON but ELS is OFF, show light red to avoid misleading green.
	if (!LeadscrewProxy::isEnabled())
	{
		lv_obj_add_state(btn_sync_ptr, LV_STATE_USER_3);
		return;
	}

	// Default to red until we've seen spindle motion since enabling sync.
	if (!g_sync_seen_motion_since_enable)
	{
		if (EncoderProxy::isSpindleRunning() || (EncoderProxy::getRawTicks() != g_sync_enable_c_ticks))
			g_sync_seen_motion_since_enable = true;
		else
			return;
	}

	const uint16_t abs_err_um = SyncProxy::getAbsErrorUm();
	static constexpr uint16_t GREEN_ERR_UM = 100;
	static constexpr uint16_t ORANGE_ERR_UM = 500;

	if (abs_err_um <= GREEN_ERR_UM)
	{
		lv_obj_add_state(btn_sync_ptr, LV_STATE_USER_2);
	}
	else if (abs_err_um <= ORANGE_ERR_UM)
	{
		lv_obj_add_state(btn_sync_ptr, LV_STATE_USER_1);
	}
	else
	{
		// Red (base checked style)
		// (no overlay state)
	}
}

void UIManager::forceElsOff() {
	// ELS enable/jog are physical-only on the motion board.
	// Keep this function as a safe no-op for legacy call-sites.
}

// Global function for modal callback
void updateEndstopButtonStates() { UIManager::updateEndstopButtonStates(); }
void updateSyncButtonStates() { UIManager::updateSyncButtonStates(); }
