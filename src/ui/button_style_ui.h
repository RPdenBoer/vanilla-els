#pragma once

#include <lvgl.h>

// Helpers to make button styling deterministic.
//
// We strip LVGL theme styles from buttons at creation time so theme state
// transitions (e.g. default blue focus/pressed) can't flash through.

namespace UiStyle {

inline void stripThemeFromButton(lv_obj_t *btn)
{
	if (!btn) return;
	lv_obj_remove_style_all(btn);

	// These are touch-first UIs; focus visuals tend to clash.
	// Also helps avoid transient theme focus styling.
#ifdef LV_OBJ_FLAG_CLICK_FOCUSABLE
	lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
#endif
}

inline lv_obj_t *createButtonStripped(lv_obj_t *parent)
{
	lv_obj_t *btn = lv_btn_create(parent);
	stripThemeFromButton(btn);
	return btn;
}

// Common non-modal button behavior: no theme transitions and no press animation
// other than the explicit transform we set.
inline void applyButtonCommonStyle(lv_obj_t *btn, int32_t radius)
{
	if (!btn) return;

	static const lv_style_prop_t NO_TRANS_PROPS[] = {0};
	static const lv_style_transition_dsc_t NO_TRANSITION = {
		.props = NO_TRANS_PROPS,
		.user_data = nullptr,
		.path_xcb = lv_anim_path_linear,
		.time = 0,
		.delay = 0,
	};

	lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
	lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(btn, radius, LV_PART_MAIN);
	lv_obj_set_style_radius(btn, radius, LV_PART_MAIN | LV_STATE_PRESSED);

	// Keep press feedback subtle and consistent.
	lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

	// Some themes add borders on pressed; make it deterministic.
	lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_PRESSED);

	// Disable transitions to avoid any state-flash.
	lv_obj_set_style_anim_duration(btn, 0, LV_PART_MAIN);
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN);
	lv_obj_set_style_transition(btn, &NO_TRANSITION, LV_PART_MAIN | LV_STATE_PRESSED);
}

} // namespace UiStyle
