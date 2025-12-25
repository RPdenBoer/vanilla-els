#pragma once

#include <lvgl.h>
#include "shared/config_shared.h"

class OffsetManager {
public:
    static void init();
    static int getCurrentOffset() { return g_offset_index; }
    static int getCurrentOffsetVariant() { return offset_b[g_offset_index] ? 1 : 0; }
    static bool isOffsetBActive(int index);
    // Returns the current pixel width of a main UI G-button (G1/G2/G3).
    // Used to keep modal button sizing consistent with the main interface.
    static int getMainOffsetButtonWidth();
    static void setCurrentOffset(int index);
    static void registerButton(int index, lv_obj_t *btn);
    static void registerLabel(int index, lv_obj_t *label);
    static void onOffsetSelect(lv_event_t *e);
    static void onOffsetLongPress(lv_event_t *e);

private:
    static void updateLabel(int index);

    static int g_offset_index;
    static lv_obj_t *offset_btns[OFFSET_COUNT];
    static lv_obj_t *offset_labels[OFFSET_COUNT];
    static bool offset_b[OFFSET_COUNT];
};
