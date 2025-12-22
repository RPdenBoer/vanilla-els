#pragma once

#include <lvgl.h>
#include <stdint.h>
#include "modal_ui.h"

class UIManager {
public:
    static void init();
    static void update();
    
    static void onZeroX(lv_event_t *e);
    static void onZeroZ(lv_event_t *e);
    static void onZeroC(lv_event_t *e);
    static void onToggleXMode(lv_event_t *e);
    static void onToggleZPolarity(lv_event_t *e);
    static void onToggleCMode(lv_event_t *e);
    static void onToggleUnits(lv_event_t *e);
    static void onEditPitch(lv_event_t *e);
    static void onTogglePitchMode(lv_event_t *e);
    static void onEditSync(lv_event_t *e);
    static void onLongPressSync(lv_event_t *e);
    static void onToggleEls(lv_event_t *e);
    static void onJog(lv_event_t *e);
    static void onEditEndstopMin(lv_event_t *e);
    static void onEditEndstopMax(lv_event_t *e);
    static void onLongPressEndstopMin(lv_event_t *e);
    static void onLongPressEndstopMax(lv_event_t *e);
	static void onLongPressZ(lv_event_t *e);
	static void onLongPressC(lv_event_t *e);
	static void updateEndstopButtonStates();
	static void updateSyncButtonStates();
    static void forceElsOff();
    static void setElsButtonActive(bool active);
	static void initPhysicalButtons();
	static void pollPhysicalButtons();
	static void triggerElsLeft();
	static void triggerElsRight();

private:
    static lv_obj_t *lbl_x;
    static lv_obj_t *lbl_z;
    static lv_obj_t *lbl_c;
    static lv_obj_t *btn_jog_l;
	static lv_obj_t *btn_jog_r;
	static lv_obj_t *btn_endstop_min_ptr;
    static lv_obj_t *btn_endstop_max_ptr;
    static lv_obj_t *btn_sync_ptr;
    static lv_obj_t *lbl_x_unit;
    static lv_obj_t *lbl_z_unit;
    static lv_obj_t *lbl_c_unit;
	static lv_obj_t *lbl_x_name;
	static lv_obj_t *lbl_z_name;
	static lv_obj_t *lbl_c_name;
	static lv_obj_t *lbl_units_mode;
    static lv_obj_t *lbl_pitch;
	static lv_obj_t *lbl_pitch_mode;
	static bool els_latched;
    static bool endstop_min_long_pressed;
    static bool endstop_max_long_pressed;
    static bool btn_left_down;
    static bool btn_right_down;
    static bool btn_left_long_handled;
    static bool btn_right_long_handled;
    static uint32_t btn_left_down_ms;
    static uint32_t btn_right_down_ms;

	static void createUI();
    static void updateJogAvailability();
	static lv_obj_t *makeAxisRow(lv_obj_t *parent, const char *name,
								 lv_obj_t **out_value_label, lv_event_cb_t zero_cb,
								 lv_obj_t **out_name_label);
};

// Global function for modal callback
void updateEndstopButtonStates();
void updateSyncButtonStates();
