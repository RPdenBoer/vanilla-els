#pragma once

#include <lvgl.h>
#include "modal.h"

class UIManager {
public:
    static void init();
    static void update();
    
    // Axis event handlers
    static void onZeroX(lv_event_t *e);
    static void onZeroZ(lv_event_t *e);
    static void onZeroC(lv_event_t *e);

    static void onToggleXMode(lv_event_t *e);
    static void onToggleZPolarity(lv_event_t *e);
    static void onToggleUnits(lv_event_t *e);
    static void onEditPitch(lv_event_t *e);
    static void onTogglePitchMode(lv_event_t *e);
    static void onToggleEls(lv_event_t *e);
    static void onJog(lv_event_t *e);
    
private:
    static lv_obj_t *lbl_x;
    static lv_obj_t *lbl_z;
    static lv_obj_t *lbl_c;

    static lv_obj_t *btn_jog_l;
    static lv_obj_t *btn_jog_r;

    static lv_obj_t *lbl_x_unit;
    static lv_obj_t *lbl_z_unit;
    static lv_obj_t *lbl_c_unit;

    static lv_obj_t *lbl_units_mode;

    static lv_obj_t *lbl_pitch;
    static lv_obj_t *lbl_pitch_mode;
    static lv_obj_t *lbl_els;

    static bool els_latched;
    
    static void createUI();
    static void updateJogAvailability();
    static lv_obj_t* makeAxisRow(lv_obj_t *parent,
                                const char *name,
                                lv_obj_t **out_value_label,
                                lv_event_cb_t zero_cb);
};