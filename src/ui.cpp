#include "ui.h"
#include "coordinates.h"
#include "encoder.h"
#include "offsets.h"
#include "tools.h"
#include "config.h"
#include "mono_font.h"
#include "leadscrew.h"
#include "endstops.h"

#include <cstring>

// Safely disable LVGL theme transitions (press animations) by providing
// a valid transition descriptor with 0ms duration.
static const lv_style_prop_t NO_TRANS_PROPS[] = {0};
static const lv_style_transition_dsc_t NO_TRANSITION = {
	.props = NO_TRANS_PROPS,
	.user_data = nullptr,
	.path_xcb = lv_anim_path_linear,
	.time = 0,
	.delay = 0,
};

static void apply_button_common_style(lv_obj_t *btn) {
    if (!btn) return;

    // Prevent theme press visuals from being clipped in tight rows.
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

    // Neutralize any pressed-state transform/translate from the default theme.
    lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
	// Default theme uses transform_width/height for the "grow" press effect.
	lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_translate_x(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    // Some themes bump border width on press; force it stable to avoid clipping.
	lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_PRESSED);

	// Some themes define a more specific PRESSED|CHECKED style; override that too.
	lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_translate_x(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);

	// Disable animations (safer than touching transition descriptors).
	lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN);
	lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);

	// Also disable theme transitions explicitly.
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN);
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
}

lv_obj_t *UIManager::lbl_x = nullptr;
lv_obj_t *UIManager::lbl_z = nullptr;
lv_obj_t *UIManager::lbl_c = nullptr;

lv_obj_t *UIManager::lbl_x_unit = nullptr;
lv_obj_t *UIManager::lbl_z_unit = nullptr;
lv_obj_t *UIManager::lbl_c_unit = nullptr;

lv_obj_t *UIManager::lbl_units_mode = nullptr;
lv_obj_t *UIManager::lbl_pitch = nullptr;
lv_obj_t *UIManager::lbl_pitch_mode = nullptr;
lv_obj_t *UIManager::lbl_els = nullptr;
lv_obj_t *UIManager::btn_jog_l = nullptr;
lv_obj_t *UIManager::btn_jog_r = nullptr;
lv_obj_t *UIManager::btn_els_ptr = nullptr;
lv_obj_t *UIManager::btn_endstop_min_ptr = nullptr;
lv_obj_t *UIManager::btn_endstop_max_ptr = nullptr;
bool UIManager::els_latched = false;
bool UIManager::endstop_min_long_pressed = false;
bool UIManager::endstop_max_long_pressed = false;

void UIManager::updateJogAvailability() {
    if (!btn_jog_l || !btn_jog_r) return;
    if (els_latched) {
        lv_obj_add_state(btn_jog_l, LV_STATE_DISABLED);
        lv_obj_add_state(btn_jog_r, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(btn_jog_l, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_jog_r, LV_STATE_DISABLED);
    }
}
void UIManager::init() {
    createUI();
}

void UIManager::createUI() {
    lv_obj_t *scr = lv_screen_active();

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 4, 0);
    lv_obj_set_style_pad_row(scr, 4, 0);
    lv_obj_set_style_pad_column(scr, 4, 0);

    // Use a 2-row grid so the main area can't be clipped by flex sizing
    static int32_t scr_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t scr_row_dsc[] = {LV_GRID_FR(1), 44, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(scr, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(scr, scr_col_dsc, scr_row_dsc);

    // Main content: left DRO column + right features column
    lv_obj_t *main_row = lv_obj_create(scr);
    lv_obj_set_grid_cell(main_row, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_size(main_row, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(main_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 0, 0);

    // Grid inside the main content: DRO column + feature column
    static int32_t main_col_dsc[] = {LV_GRID_FR(1), 188, LV_GRID_TEMPLATE_LAST};
    static int32_t main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(main_row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(main_row, main_col_dsc, main_row_dsc);
	lv_obj_set_style_pad_column(main_row, 10, 0);

	lv_obj_t *dro_col = lv_obj_create(main_row);
    lv_obj_set_grid_cell(dro_col, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_size(dro_col, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(dro_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(dro_col, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(dro_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dro_col, 0, 0);
    lv_obj_set_style_pad_all(dro_col, 0, 0);
    lv_obj_set_style_pad_row(dro_col, 4, 0);

    // Make the DRO area a deterministic 3-row grid so rows fill the height
    static int32_t dro_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t dro_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(dro_col, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(dro_col, dro_col_dsc, dro_row_dsc);

    lv_obj_t *feature_col = lv_obj_create(main_row);
    lv_obj_set_grid_cell(feature_col, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_size(feature_col, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(feature_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(feature_col, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(feature_col, LV_OPA_TRANSP, 0);
    // Keep the right column flush (no nested padding box)
    lv_obj_set_style_border_width(feature_col, 0, 0);
    lv_obj_set_style_pad_all(feature_col, 0, 0);

    // Feature column layout
    lv_obj_set_flex_flow(feature_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(feature_col, 6, 0);

    // Work offset selector (G1/G2/G3)
    lv_obj_t *offset_row = lv_obj_create(feature_col);
    lv_obj_set_width(offset_row, LV_PCT(100));
    lv_obj_set_height(offset_row, 44);
    lv_obj_clear_flag(offset_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(offset_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(offset_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(offset_row, 0, 0);
    lv_obj_set_style_pad_all(offset_row, 0, 0);
    lv_obj_set_style_pad_gap(offset_row, 4, 0);
    lv_obj_set_flex_flow(offset_row, LV_FLEX_FLOW_ROW);
	// Center buttons vertically so we can use a consistent height across T/G buttons.
	lv_obj_set_flex_align(offset_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	for (int i = 0; i < OFFSET_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(offset_row);
		lv_obj_set_height(btn, 40);
		lv_obj_set_flex_grow(btn, 1);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
		// Consistent with rest of UI: action on release.
		lv_obj_add_event_cb(btn, OffsetManager::onOffsetSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Ghost by default; selected = blue
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
		lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
		lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

		// Press feedback: subtle tint (no size change).
		lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_border_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_text_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);

		lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);

		lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
		lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);

		apply_button_common_style(btn);

        OffsetManager::registerButton(i, btn);

        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8];
        snprintf(txt, sizeof(txt), "G%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    }

    // Units + pitch mode row: [MM/INCH] [TPI/PITCH]
    lv_obj_t *units_row = lv_obj_create(feature_col);
    lv_obj_set_width(units_row, LV_PCT(100));
    lv_obj_set_height(units_row, 44);
    lv_obj_clear_flag(units_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(units_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(units_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(units_row, 0, 0);
    lv_obj_set_style_pad_all(units_row, 0, 0);
    lv_obj_set_style_pad_gap(units_row, 4, 0);
    lv_obj_set_flex_flow(units_row, LV_FLEX_FLOW_ROW);

    // Units toggle (MM/INCH)
    lv_obj_t *btn_units = lv_btn_create(units_row);
    lv_obj_set_height(btn_units, 44);
    lv_obj_set_flex_grow(btn_units, 1);
    lv_obj_clear_flag(btn_units, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(btn_units, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(btn_units, onToggleUnits, LV_EVENT_CLICKED, nullptr);

    // Ghost style
    lv_obj_set_style_bg_opa(btn_units, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_units, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_units, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_units, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_units, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_units, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	apply_button_common_style(btn_units);

    lv_obj_t *lbl_units = lv_label_create(btn_units);
    lbl_units_mode = lbl_units;
    lv_label_set_text(lbl_units, CoordinateSystem::isLinearInchMode() ? "INCH" : "MM");
    lv_obj_center(lbl_units);

    // Pitch mode toggle (TPI/PITCH)
    lv_obj_t *btn_pitch_mode = lv_btn_create(units_row);
    lv_obj_set_width(btn_pitch_mode, 80);
    lv_obj_set_height(btn_pitch_mode, 44);
    lv_obj_clear_flag(btn_pitch_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(btn_pitch_mode, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(btn_pitch_mode, onTogglePitchMode, LV_EVENT_CLICKED, nullptr);

    // Ghost style
    lv_obj_set_style_bg_opa(btn_pitch_mode, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_pitch_mode, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_pitch_mode, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_pitch_mode, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_pitch_mode, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_pitch_mode, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	apply_button_common_style(btn_pitch_mode);

    lv_obj_t *lblpm = lv_label_create(btn_pitch_mode);
    lbl_pitch_mode = lblpm;
    lv_label_set_text(lblpm, LeadscrewManager::isPitchTpiMode() ? "TPI" : "PITCH");
    lv_obj_center(lblpm);

	// Pitch row: [min endstop] [pitch value] [max endstop]
	lv_obj_t *pitch_row = lv_obj_create(feature_col);
	lv_obj_set_width(pitch_row, LV_PCT(100));
	lv_obj_set_height(pitch_row, 52);
	lv_obj_clear_flag(pitch_row, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(pitch_row, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_style_bg_opa(pitch_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(pitch_row, 0, 0);
	lv_obj_set_style_pad_all(pitch_row, 0, 0);
	lv_obj_set_style_pad_gap(pitch_row, 4, 0);
	lv_obj_set_flex_flow(pitch_row, LV_FLEX_FLOW_ROW);

	// Min endstop button "["
	lv_obj_t *btn_endstop_min = lv_btn_create(pitch_row);
	btn_endstop_min_ptr = btn_endstop_min;
	lv_obj_set_width(btn_endstop_min, 32);
	lv_obj_set_height(btn_endstop_min, LV_PCT(100));
	lv_obj_clear_flag(btn_endstop_min, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(btn_endstop_min, LV_SCROLLBAR_MODE_OFF);
	lv_obj_add_event_cb(btn_endstop_min, onEditEndstopMin, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_add_event_cb(btn_endstop_min, onLongPressEndstopMin, LV_EVENT_LONG_PRESSED, nullptr);
	lv_obj_set_style_pad_all(btn_endstop_min, 0, LV_PART_MAIN);
	// Ghost style (disabled/no value)
	lv_obj_set_style_bg_opa(btn_endstop_min, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_endstop_min, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_endstop_min, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_endstop_min, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_endstop_min, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_endstop_min, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_endstop_min, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_endstop_min, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	// Checked state (enabled) = red
	lv_obj_set_style_bg_opa(btn_endstop_min, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_min, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_min, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_min, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_min, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_min, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	apply_button_common_style(btn_endstop_min);
	lv_obj_t *lbl_emin = lv_label_create(btn_endstop_min);
	lv_label_set_text(lbl_emin, "[");
	lv_obj_center(lbl_emin);

	// Pitch value button (larger font)
	lv_obj_t *btn_pitch = lv_btn_create(pitch_row);
	lv_obj_set_height(btn_pitch, LV_PCT(100));
	lv_obj_set_flex_grow(btn_pitch, 1);
	lv_obj_clear_flag(btn_pitch, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(btn_pitch, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(btn_pitch, onEditPitch, LV_EVENT_CLICKED, nullptr);

    // Ghost style
    lv_obj_set_style_bg_opa(btn_pitch, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_pitch, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_pitch, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_pitch, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_pitch, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_pitch, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	apply_button_common_style(btn_pitch);

    lv_obj_t *lblp = lv_label_create(btn_pitch);
    lbl_pitch = lblp;
    lv_obj_set_style_text_font(lblp, &lv_font_montserrat_24, 0);
    {
        char pbuf[32];
        LeadscrewManager::formatPitchLabel(pbuf, sizeof(pbuf));
        lv_label_set_text(lblp, pbuf);
    }
    lv_obj_center(lblp);

	// Max endstop button "]"
	lv_obj_t *btn_endstop_max = lv_btn_create(pitch_row);
	btn_endstop_max_ptr = btn_endstop_max;
	lv_obj_set_width(btn_endstop_max, 32);
	lv_obj_set_height(btn_endstop_max, LV_PCT(100));
	lv_obj_clear_flag(btn_endstop_max, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(btn_endstop_max, LV_SCROLLBAR_MODE_OFF);
	lv_obj_add_event_cb(btn_endstop_max, onEditEndstopMax, LV_EVENT_SHORT_CLICKED, nullptr);
	lv_obj_add_event_cb(btn_endstop_max, onLongPressEndstopMax, LV_EVENT_LONG_PRESSED, nullptr);
	lv_obj_set_style_pad_all(btn_endstop_max, 0, LV_PART_MAIN);
	// Ghost style (disabled/no value)
	lv_obj_set_style_bg_opa(btn_endstop_max, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_endstop_max, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_endstop_max, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_endstop_max, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_endstop_max, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_endstop_max, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_endstop_max, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_endstop_max, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	// Checked state (enabled) = red
	lv_obj_set_style_bg_opa(btn_endstop_max, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_max, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_max, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_endstop_max, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_endstop_max, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_endstop_max, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	apply_button_common_style(btn_endstop_max);
	lv_obj_t *lbl_emax = lv_label_create(btn_endstop_max);
	lv_label_set_text(lbl_emax, "]");
	lv_obj_center(lbl_emax);

	// ELS row: [jog left] [ELS on/off] [jog right]
	lv_obj_t *els_row = lv_obj_create(feature_col);
    lv_obj_set_width(els_row, LV_PCT(100));
    lv_obj_set_flex_grow(els_row, 1);
    lv_obj_clear_flag(els_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(els_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(els_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(els_row, 0, 0);
    lv_obj_set_style_pad_all(els_row, 0, 0);
    lv_obj_set_style_pad_gap(els_row, 4, 0);
    lv_obj_set_flex_flow(els_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *btn_jog_l = lv_btn_create(els_row);
    UIManager::btn_jog_l = btn_jog_l;
    lv_obj_set_width(btn_jog_l, 60);
    lv_obj_set_height(btn_jog_l, LV_PCT(100));
    lv_obj_clear_flag(btn_jog_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(btn_jog_l, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(btn_jog_l, onJog, LV_EVENT_PRESSED, (void*)(intptr_t)1);
    lv_obj_add_event_cb(btn_jog_l, onJog, LV_EVENT_RELEASED, (void*)(intptr_t)1);
    lv_obj_add_event_cb(btn_jog_l, onJog, LV_EVENT_PRESS_LOST, (void*)(intptr_t)1);
    lv_obj_set_style_bg_opa(btn_jog_l, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_jog_l, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_jog_l, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_jog_l, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_jog_l, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_jog_l, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_jog_l, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_jog_l, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	apply_button_common_style(btn_jog_l);
    lv_obj_t *lbll = lv_label_create(btn_jog_l);
    lv_label_set_text(lbll, "<");
    lv_obj_center(lbll);

    lv_obj_t *btn_els = lv_btn_create(els_row);
	btn_els_ptr = btn_els;
	lv_obj_set_height(btn_els, LV_PCT(100));
	lv_obj_set_flex_grow(btn_els, 1);
    lv_obj_clear_flag(btn_els, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(btn_els, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(btn_els, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_els, onToggleEls, LV_EVENT_CLICKED, btn_els);

    // Off = green, On = red
    lv_obj_set_style_bg_opa(btn_els, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(btn_els, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_els, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_els, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	lv_obj_set_style_text_color(btn_els, lv_color_white(), LV_PART_MAIN);

	lv_obj_set_style_bg_color(btn_els, lv_palette_darken(LV_PALETTE_GREEN, 3), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_els, lv_palette_darken(LV_PALETTE_GREEN, 3), LV_PART_MAIN | LV_STATE_PRESSED);

	lv_obj_set_style_bg_opa(btn_els, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(btn_els, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_els, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_text_color(btn_els, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);

	lv_obj_set_style_bg_color(btn_els, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
	lv_obj_set_style_border_color(btn_els, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);

	apply_button_common_style(btn_els);

    lv_obj_t *lble = lv_label_create(btn_els);
    lbl_els = lble;
    lv_label_set_text(lble, "ELS");
    lv_obj_center(lble);

    lv_obj_t *btn_jog_r = lv_btn_create(els_row);
    UIManager::btn_jog_r = btn_jog_r;
    lv_obj_set_width(btn_jog_r, 60);
    lv_obj_set_height(btn_jog_r, LV_PCT(100));
    lv_obj_clear_flag(btn_jog_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(btn_jog_r, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(btn_jog_r, onJog, LV_EVENT_PRESSED, (void*)(intptr_t)-1);
    lv_obj_add_event_cb(btn_jog_r, onJog, LV_EVENT_RELEASED, (void*)(intptr_t)-1);
    lv_obj_add_event_cb(btn_jog_r, onJog, LV_EVENT_PRESS_LOST, (void*)(intptr_t)-1);
    lv_obj_set_style_bg_opa(btn_jog_r, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(btn_jog_r, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(btn_jog_r, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_jog_r, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn_jog_r, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_color(btn_jog_r, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_border_color(btn_jog_r, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_text_color(btn_jog_r, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
	apply_button_common_style(btn_jog_r);
    lv_obj_t *lblr = lv_label_create(btn_jog_r);
    lv_label_set_text(lblr, ">");
    lv_obj_center(lblr);

    // Axis rows (left column)
    lv_obj_t *row_x = makeAxisRow(dro_col, "X", &lbl_x, onZeroX);
    lv_obj_t *row_z = makeAxisRow(dro_col, "Z", &lbl_z, onZeroZ);
    lv_obj_t *row_c = makeAxisRow(dro_col, "C", &lbl_c, onZeroC);

    lv_obj_set_grid_cell(row_x, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(row_z, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(row_c, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    // Tool row (bottom)
    lv_obj_t *tool_row = lv_obj_create(scr);
    lv_obj_set_grid_cell(tool_row, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_size(tool_row, LV_PCT(100), 44);
    lv_obj_clear_flag(tool_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(tool_row, LV_SCROLLBAR_MODE_OFF);
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
        lv_obj_set_scrollbar_mode(btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
		// Consistent with rest of UI: action on release.
		lv_obj_add_event_cb(btn, ToolManager::onToolSelect, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Ghost by default; selected = orange
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
		lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
		lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

		lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_border_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);
		lv_obj_set_style_text_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_PRESSED);

        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 2), LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);

        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_palette_darken(LV_PALETTE_ORANGE, 3), LV_PART_MAIN | LV_STATE_PRESSED | LV_STATE_CHECKED);

		apply_button_common_style(btn);

        ToolManager::registerButton(i, btn);

        lv_obj_t *lbl = lv_label_create(btn);
        char txt[8];
        snprintf(txt, sizeof(txt), "T%d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    }

    ToolManager::setCurrentTool(0); // Set tool 1 as default
    OffsetManager::setCurrentOffset(0); // Set G1 as default

    updateJogAvailability();
}

lv_obj_t* UIManager::makeAxisRow(lv_obj_t *parent,
                                const char *name,
                                lv_obj_t **out_value_label,
                                lv_event_cb_t zero_cb) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
	// No border around axis rows (cleaner, less visual clutter)
	lv_obj_set_style_border_width(row, 0, 0);

	// 3-column grid: axis letter | unit superscript | value
    // Keep the letter and superscript columns tight so they read as one unit.
    static int32_t col_dsc[] = {30, 22, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(row, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(row, 0, 0);

    // Larger hit target for axis letter + superscript (improves tapping)
    lv_obj_t *hitbox = nullptr;
    if (strcmp(name, "X") == 0) {
        hitbox = lv_obj_create(row);
        lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hitbox, 0, 0);
        lv_obj_set_style_pad_all(hitbox, 0, 0);
        lv_obj_clear_flag(hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(hitbox, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_grid_cell(hitbox, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_event_cb(hitbox, onToggleXMode, LV_EVENT_CLICKED, nullptr);
    } else if (strcmp(name, "Z") == 0) {
        hitbox = lv_obj_create(row);
        lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hitbox, 0, 0);
        lv_obj_set_style_pad_all(hitbox, 0, 0);
        lv_obj_clear_flag(hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(hitbox, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_grid_cell(hitbox, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);
        lv_obj_add_event_cb(hitbox, onToggleZPolarity, LV_EVENT_CLICKED, nullptr);
	}
	else if (strcmp(name, "C") == 0)
	{
		hitbox = lv_obj_create(row);
		lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(hitbox, 0, 0);
		lv_obj_set_style_pad_all(hitbox, 0, 0);
		lv_obj_clear_flag(hitbox, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_scrollbar_mode(hitbox, LV_SCROLLBAR_MODE_OFF);
		lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_set_grid_cell(hitbox, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);
		lv_obj_add_event_cb(hitbox, onToggleCMode, LV_EVENT_CLICKED, nullptr);
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
        if (CoordinateSystem::isXRadiusMode()) {
            lv_label_set_text(lbl_unit, "rad");
            lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        } else {
            lv_label_set_text(lbl_unit, "dia");
            lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        }
        lbl_x_unit = lbl_unit;
    } else if (strcmp(name, "Z") == 0) {
        lv_label_set_text(lbl_unit, "pos");
        lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        lbl_z_unit = lbl_unit;
    } else if (strcmp(name, "C") == 0) {
		// Show current mode: deg (default) or rpm (manual selection or auto-threshold)
		if (EncoderManager::isManualRpmMode())
		{
			lv_label_set_text(lbl_unit, "rpm");
			lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
		}
		else
		{
			lv_label_set_text(lbl_unit, "deg");
			lv_obj_set_style_text_color(lbl_unit, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
		}
		lbl_c_unit = lbl_unit;
    } else {
        lv_label_set_text(lbl_unit, "");
    }

    lv_obj_t *lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, "0");
    lv_obj_set_style_text_font(lbl_val, get_mono_numeric_font_44(), 0);
    lv_obj_set_style_text_color(lbl_val, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_RIGHT, 0);
    // Keep a little breathing room from the right border
    lv_obj_set_style_pad_right(lbl_val, 6, 0);
    lv_obj_set_grid_cell(lbl_val, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_flag(lbl_val, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_val, zero_cb, LV_EVENT_CLICKED, nullptr);

    if (out_value_label) {
        *out_value_label = lbl_val;
    }

    return row;
}

void UIManager::update() {
    if (!lbl_x || !lbl_z || !lbl_c) return;

	// Check if ELS was disabled due to bounds exceeded (from step task on other core)
	if (LeadscrewManager::wasBoundsExceeded())
	{
		LeadscrewManager::clearBoundsExceeded();
		forceElsOff();
	}

	updateJogAvailability();

	// Keep X/Z superscripts in sync with current modes
    if (lbl_x_unit) {
        if (CoordinateSystem::isXRadiusMode()) {
            lv_label_set_text(lbl_x_unit, "rad");
            lv_obj_set_style_text_color(lbl_x_unit, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        } else {
            lv_label_set_text(lbl_x_unit, "dia");
			lv_obj_set_style_text_color(lbl_x_unit, lv_palette_darken(LV_PALETTE_BLUE, 2), LV_PART_MAIN);
		}
    }

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
    
    int32_t x_um = CoordinateSystem::getDisplayX(tool);
    int32_t z_um = CoordinateSystem::getDisplayZ(tool);
    int32_t c_phase = CoordinateSystem::getDisplayC(EncoderManager::getRawTicks(), tool);

    static char buf[48];

    CoordinateSystem::formatLinear(buf, sizeof(buf), x_um);
    lv_label_set_text(lbl_x, buf);

    CoordinateSystem::formatLinear(buf, sizeof(buf), z_um);
    lv_label_set_text(lbl_z, buf);

    if (EncoderManager::shouldShowRpm()) {
		snprintf(buf, sizeof(buf), "%ld", (long)EncoderManager::getRpmSigned());
		lv_label_set_text(lbl_c, buf);
        if (lbl_c_unit) {
			lv_label_set_text(lbl_c_unit, "rpm");
			lv_obj_set_style_text_color(lbl_c_unit, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
		}
    } else {
        int32_t deg_x100 = CoordinateSystem::ticksToDegX100(c_phase);
        CoordinateSystem::formatDeg(buf, sizeof(buf), deg_x100);
        lv_label_set_text(lbl_c, buf);

        if (lbl_c_unit) {
            lv_label_set_text(lbl_c_unit, "deg");
			lv_obj_set_style_text_color(lbl_c_unit, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
		}
    }

    if (lbl_units_mode) {
        lv_label_set_text(lbl_units_mode, CoordinateSystem::isLinearInchMode() ? "INCH" : "MM");
    }

    if (lbl_pitch) {
        char pbuf[32];
        LeadscrewManager::formatPitchLabel(pbuf, sizeof(pbuf));
        lv_label_set_text(lbl_pitch, pbuf);
    }

    if (lbl_pitch_mode) {
        lv_label_set_text(lbl_pitch_mode, LeadscrewManager::isPitchTpiMode() ? "TPI" : "PITCH");
    }
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
    LeadscrewManager::togglePitchTpiMode();
    update();
}

void UIManager::onToggleEls(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    // Keep visual checked state in sync
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_user_data(e);
    const bool now_on = lv_obj_has_state(btn, LV_STATE_CHECKED);
    els_latched = now_on;
    LeadscrewManager::setEnabled(now_on);
    LeadscrewManager::setDirectionMul(1);
    updateJogAvailability();
}

void UIManager::onJog(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    const int dir = (int)(intptr_t)lv_event_get_user_data(e);

    // Jog is only available when the main ELS toggle is OFF.
    if (els_latched) return;

    if (code == LV_EVENT_PRESSED) {
        // Momentary ELS: equivalent to enabling ELS while held.
        // Left = normal feed, Right = reversed feed (equivalent to negative pitch).
        LeadscrewManager::setDirectionMul((int8_t)dir);
        LeadscrewManager::setEnabled(true);
		// Show ELS button as active (red) during jog
		setElsButtonActive(true);
		return;
	}
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        LeadscrewManager::setEnabled(false);
        LeadscrewManager::setDirectionMul(1);
		// Restore ELS button to inactive (green)
		setElsButtonActive(false);
		return;
	}
}

void UIManager::onZeroX(lv_event_t *e) { 
    (void)e; 
    ModalManager::showOffsetModal(AXIS_X); 
}

void UIManager::onZeroZ(lv_event_t *e) { 
    (void)e; 
    ModalManager::showOffsetModal(AXIS_Z); 
}

void UIManager::onZeroC(lv_event_t *e) { 
    (void)e; 
    ModalManager::showOffsetModal(AXIS_C); 
}

void UIManager::onToggleXMode(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    CoordinateSystem::toggleXRadiusMode();
    update();
}

void UIManager::onToggleZPolarity(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    CoordinateSystem::toggleZInverted();
    update();
}

void UIManager::onToggleCMode(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_CLICKED)
		return;
	// Only toggle if below auto-RPM threshold
	if (EncoderManager::canToggleManualMode())
	{
		EncoderManager::toggleManualRpmMode();
		update();
	}
}

void UIManager::onEditEndstopMin(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	ModalManager::showEndstopModal(false); // false = min endstop
}

void UIManager::onEditEndstopMax(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED)
		return;
	ModalManager::showEndstopModal(true); // true = max endstop
}

void UIManager::onLongPressEndstopMin(lv_event_t *e)
{
	(void)e;
	// Toggle min endstop enabled state (only if value has been set)
	EndstopManager::toggleMinEnabled();
	updateEndstopButtonStates();
}

void UIManager::onLongPressEndstopMax(lv_event_t *e)
{
	(void)e;
	// Toggle max endstop enabled state (only if value has been set)
	EndstopManager::toggleMaxEnabled();
	updateEndstopButtonStates();
}

void UIManager::updateEndstopButtonStates()
{
	// Update button visual state to match enabled status
	if (btn_endstop_min_ptr)
	{
		if (EndstopManager::isMinEnabled())
		{
			lv_obj_add_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
		}
		else
		{
			lv_obj_clear_state(btn_endstop_min_ptr, LV_STATE_CHECKED);
		}
	}
	if (btn_endstop_max_ptr)
	{
		if (EndstopManager::isMaxEnabled())
		{
			lv_obj_add_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
		}
		else
		{
			lv_obj_clear_state(btn_endstop_max_ptr, LV_STATE_CHECKED);
		}
	}
}

void UIManager::forceElsOff()
{
	// Called when Z goes out of bounds - force ELS off and update UI.
	if (els_latched)
	{
		els_latched = false;
		if (btn_els_ptr)
		{
			lv_obj_clear_state(btn_els_ptr, LV_STATE_CHECKED);
		}
	}
	LeadscrewManager::setEnabled(false);
	LeadscrewManager::setDirectionMul(1);
	setElsButtonActive(false);
	updateJogAvailability();
}

void UIManager::setElsButtonActive(bool active)
{
	// Update ELS button visual state without changing the latched toggle state.
	// This is used for jog momentary feedback and bounds-exceeded indication.
	if (!btn_els_ptr)
		return;

	if (active && !els_latched)
	{
		// Temporarily show as red (active) without setting CHECKED state
		lv_obj_set_style_bg_color(btn_els_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_els_ptr, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_MAIN);
	}
	else if (!active && !els_latched)
	{
		// Restore to green (inactive)
		lv_obj_set_style_bg_color(btn_els_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
		lv_obj_set_style_border_color(btn_els_ptr, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_PART_MAIN);
	}
	// If els_latched is true, the CHECKED state styling handles the color
}