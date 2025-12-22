#pragma once

#include <lvgl.h>

enum AxisSel { AXIS_X = 0, AXIS_Z = 1, AXIS_C = 2 };

class ModalManager {
public:
    static void showOffsetModal(AxisSel axis);
    static void showPitchModal();
    static void showEndstopModal(bool is_max); // false = min "[", true = max "]"
    static void showSyncModal();
    static void closeModal();
    
    static void onCancel(lv_event_t *e);
    static void onSetTool(lv_event_t *e);
    static void onSetGlobal(lv_event_t *e);
    static void onPitchOk(lv_event_t *e);
    static void onEndstopOk(lv_event_t *e);
    static void onEndstopClear(lv_event_t *e);
    static void onSyncOk(lv_event_t *e);
    static void onNumpadKey(lv_event_t *e);
    static void onNumpadClear(lv_event_t *e);
    static void onNumpadBackspace(lv_event_t *e);
    static void onNumpadCurrent(lv_event_t *e);

private:
    static lv_obj_t *modal_bg;
    static lv_obj_t *modal_win;
    static lv_obj_t *ta_value;
    static lv_obj_t *kb;
    static AxisSel active_axis;
    static bool pitch_modal;
    static bool endstop_modal;
    static bool endstop_is_max;
    static bool sync_modal;

    static void applyToolOffset();
    static void applyGlobalOffset();
    static void applyPitch();
    static void applyEndstop();
    static void applySync();
};
